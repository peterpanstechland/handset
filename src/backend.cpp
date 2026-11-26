#include "backend.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

namespace {
const QStringList kLiveStates{"dialing", "ringing", "active", "held", "waiting"};
}

Backend::Backend(const QString &dataDirectory, const QString &mmcli, QObject *parent, int pollMs, bool watchModem)
    : QObject(parent),
      m_dataDirectory(dataDirectory),
      m_mmcli(mmcli.isEmpty() ? QStringLiteral("mmcli") : mmcli) {
    if (m_dataDirectory.isEmpty())
        m_dataDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    m_kill.setSingleShot(true);
    m_kill.setInterval(8000);
    connect(&m_kill, &QTimer::timeout, this, [this] {
        if (m_process.state() != QProcess::NotRunning)
            m_process.kill();
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        m_kill.stop();
        m_op = Op::None;
        setBusy(false);
        setProblem(QStringLiteral("modem"));
    });
    connect(&m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        m_kill.stop();
        const QByteArray raw = m_process.readAllStandardOutput() + m_process.readAllStandardError();
        const Op op = m_op;
        m_op = Op::None;
        finishOp(op, status == QProcess::NormalExit ? code : 1, raw);
    });

    loadStore();
    if (!watchModem)
        return;
    m_poll.setInterval(pollMs > 0 ? pollMs : 2500);
    connect(&m_poll, &QTimer::timeout, this, &Backend::refresh);
    m_poll.start();
    refresh();
}

Backend::~Backend() {
    m_poll.stop();
    m_kill.stop();
    m_process.disconnect(this);
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(1000);
    }
}

QString Backend::plain(const QString &value, int maxLength) {
    QString text = value;
    text.replace(QRegularExpression(QStringLiteral("[\\x00-\\x1f\\x7f]")), QStringLiteral(" "));
    text.replace(QRegularExpression(QStringLiteral("[<>&%]")), QString());
    text.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    text = text.trimmed();
    if (text == QLatin1String("--"))
        return {};
    if (maxLength > 0 && text.size() > maxLength)
        text.truncate(maxLength);
    return text;
}

QString Backend::dialNumber(const QString &value) {
    QString text = value;
    text.remove(QRegularExpression(QStringLiteral("[\\s-]")));
    static const QRegularExpression ok(QStringLiteral("^\\+?[0-9*#]{2,20}$"));
    return ok.match(text).hasMatch() ? text : QString();
}

QString Backend::callIdFrom(const QString &raw) {
    static const QRegularExpression expression(QStringLiteral("/Call/([0-9]{1,6})\\b"));
    const auto match = expression.match(raw);
    return match.hasMatch() ? match.captured(1) : QString();
}

QString Backend::callToken(const QString &value) {
    static const QRegularExpression expression(QStringLiteral("^[a-z]+"));
    const auto match = expression.match(value.trimmed().toLower());
    return match.hasMatch() ? match.captured(0) : QString();
}

bool Backend::idOk(const QString &id) {
    static const QRegularExpression expression(QStringLiteral("^[0-9]{1,6}$"));
    return expression.match(id).hasMatch();
}

QStringList Backend::callIndexes(const QString &listOutput) {
    static const QRegularExpression expression(QStringLiteral("/Call/([0-9]{1,6})\\b"));
    QStringList ids;
    auto it = expression.globalMatch(listOutput);
    while (it.hasNext() && ids.size() < 8) {
        const QString id = it.next().captured(1);
        if (!ids.contains(id))
            ids.append(id);
    }
    return ids;
}

Backend::ListedCall Backend::parseCallJson(const QString &id, const QByteArray &json) {
    ListedCall call;
    if (!idOk(id))
        return call;
    const auto props = QJsonDocument::fromJson(json).object().value(QStringLiteral("call")).toObject()
                           .value(QStringLiteral("properties")).toObject();
    if (props.isEmpty())
        return call;
    call.id = id;
    call.number = plain(props.value(QStringLiteral("number")).toString(), 24);
    call.state = callToken(props.value(QStringLiteral("state")).toString());
    call.direction = callToken(props.value(QStringLiteral("direction")).toString());
    call.incoming = call.direction == QLatin1String("incoming");
    call.live = kLiveStates.contains(call.state);
    return call;
}

Backend::ListedCall Backend::chooseLive(const QList<ListedCall> &calls) {
    ListedCall other;
    bool have = false;
    for (const auto &call : calls) {
        if (!call.live || call.id.isEmpty())
            continue;
        if (call.incoming && call.state == QLatin1String("ringing"))
            return call;
        if (!have) {
            other = call;
            have = true;
        }
    }
    return other;
}

void Backend::setBusy(bool busy) {
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

void Backend::setProblem(const QString &problem) {
    if (m_problem == problem)
        return;
    m_problem = problem;
    emit problemChanged();
}

void Backend::setNumber(const QString &number) {
    if (m_number == number)
        return;
    m_number = number;
    emit numberChanged();
}

void Backend::appendDigit(const QString &digit) {
    if (digit.size() != 1 || !QStringLiteral("0123456789*#+").contains(digit))
        return;
    if (digit == QLatin1String("+") && !m_number.isEmpty())
        return;
    if (m_number.size() >= 20)
        return;
    setNumber(m_number + digit);
}

void Backend::backspace() {
    if (m_number.isEmpty())
        return;
    setNumber(m_number.left(m_number.size() - 1));
}

void Backend::clearNumber() {
    setNumber({});
}

void Backend::primaryAction() {
    if (m_liveIncoming && m_liveState == QLatin1String("ringing"))
        answer();
    else if (!hasLiveCall())
        callNumber(m_number);
}

bool Backend::callNumber(const QString &number) {
    const QString safe = dialNumber(number.isEmpty() ? m_number : number);
    if (safe.isEmpty()) {
        setProblem(QStringLiteral("invalid"));
        return false;
    }
    setProblem({});
    setNumber(safe);
    if (m_process.state() != QProcess::NotRunning) {
        m_queued = QStringLiteral("call");
        m_queuedNumber = safe;
        return true;
    }
    beginCall(safe);
    return true;
}

void Backend::answer() {
    if (!m_liveIncoming || m_liveState != QLatin1String("ringing") || !idOk(m_liveId))
        return;
    setProblem({});
    if (m_process.state() != QProcess::NotRunning) {
        m_queued = QStringLiteral("answer");
        return;
    }
    m_actionId = m_liveId;
    beginSimple(Op::Accept, {QStringLiteral("-o"), m_liveId, QStringLiteral("--accept")});
}

void Backend::hangup() {
    if (!hasLiveCall() || !idOk(m_liveId))
        return;
    setProblem({});
    if (m_process.state() != QProcess::NotRunning) {
        m_queued = QStringLiteral("hangup");
        return;
    }
    m_actionId = m_liveId;
    beginSimple(Op::Hangup, {QStringLiteral("-o"), m_liveId, QStringLiteral("--hangup")});
}

void Backend::beginCall(const QString &safe) {
    m_actionId.clear();
    beginSimple(Op::Create, {QStringLiteral("-m"), QStringLiteral("0"),
                             QStringLiteral("--voice-create-call=number=") + safe});
}

void Backend::beginSimple(Op op, const QStringList &args) {
    startOp(op, args);
}

void Backend::refresh() {
    if (m_process.state() != QProcess::NotRunning) {
        m_refreshQueued = true;
        return;
    }
    m_listed.clear();
    m_pendingIds.clear();
    startOp(Op::List, {QStringLiteral("-m"), QStringLiteral("0"), QStringLiteral("--voice-list-calls")});
}

void Backend::startOp(Op op, const QStringList &args) {
    m_op = op;
    const bool action = op == Op::Create || op == Op::Start || op == Op::Accept || op == Op::Hangup || op == Op::Delete;
    setBusy(action);
    m_process.start(m_mmcli, args);
    m_kill.start();
}

void Backend::finishOp(Op op, int code, const QByteArray &raw) {
    if (handleOp(op, code, raw))
        return;
    setBusy(false);
    if (runQueue())
        return;
    if (m_refreshQueued) {
        m_refreshQueued = false;
        refresh();
    }
}

bool Backend::handleOp(Op op, int code, const QByteArray &raw) {
    const QString text = QString::fromUtf8(raw);
    if (op == Op::List) {
        if (code != 0 && !text.contains(QStringLiteral("No calls")) && !text.contains(QStringLiteral("No voice"))) {
            applyListed({}, false);
            return false;
        }
        if (m_problem == QLatin1String("modem"))
            setProblem({});
        m_pendingIds = callIndexes(text);
        m_listed.clear();
        if (m_pendingIds.isEmpty()) {
            applyListed({}, true);
            return false;
        }
        startDetail();
        return true;
    }
    if (op == Op::Detail) {
        if (code == 0) {
            const ListedCall call = parseCallJson(m_detailId, raw);
            if (!call.id.isEmpty())
                m_listed.append(call);
        }
        if (!m_pendingIds.isEmpty()) {
            startDetail();
            return true;
        }
        applyListed(m_listed, true);
        return false;
    }
    if (op == Op::Create) {
        const QString id = callIdFrom(text);
        if (code != 0 || !idOk(id)) {
            setProblem(QStringLiteral("failed"));
            setBusy(false);
            refresh();
            return true;
        }
        m_actionId = id;
        startOp(Op::Start, {QStringLiteral("-o"), id, QStringLiteral("--start")});
        return true;
    }
    if (op == Op::Start && code != 0 && idOk(m_actionId)) {
        setProblem(QStringLiteral("failed"));
        const QString id = m_actionId;
        m_actionId.clear();
        startOp(Op::Delete, {QStringLiteral("-m"), QStringLiteral("0"),
                             QStringLiteral("--voice-delete-call=") + id});
        return true;
    }
    m_actionId.clear();
    if (code != 0 && op != Op::Delete)
        setProblem(QStringLiteral("failed"));
    setBusy(false);
    refresh();
    return true;
}

bool Backend::runQueue() {
    const QString action = m_queued;
    const QString number = m_queuedNumber;
    m_queued.clear();
    m_queuedNumber.clear();
    if (action == QLatin1String("call")) {
        beginCall(number);
        return true;
    }
    if (action == QLatin1String("answer")) {
        answer();
        return m_process.state() != QProcess::NotRunning;
    }
    if (action == QLatin1String("hangup")) {
        hangup();
        return m_process.state() != QProcess::NotRunning;
    }
    return false;
}

void Backend::startDetail() {
    m_detailId = m_pendingIds.takeFirst();
    startOp(Op::Detail, {QStringLiteral("-o"), m_detailId, QStringLiteral("-J")});
}

void Backend::publishLive(const ListedCall &live) {
    const QString name = live.id.isEmpty() ? QString() : contactName(live.number);
    const bool changed = live.id != m_liveId || live.number != m_liveNumber || name != m_liveName
        || live.state != m_liveState || live.incoming != m_liveIncoming;
    m_liveId = live.id;
    m_liveNumber = live.number;
    m_liveName = name;
    m_liveState = live.state;
    m_liveIncoming = live.id.isEmpty() ? false : live.incoming;
    if (changed)
        emit liveChanged();
}

void Backend::applyListed(const QList<ListedCall> &calls, bool ok) {
    if (!ok) {
        setProblem(QStringLiteral("modem"));
        return;
    }
    publishLive(chooseLive(calls));
    if (hasLiveCall() && m_problem == QLatin1String("failed"))
        setProblem({});
    noteCalls(calls);
}

void Backend::noteCalls(const QList<ListedCall> &calls) {
    QHash<QString, bool> seen;
    bool changed = false;
    for (const auto &call : calls) {
        if (call.id.isEmpty() || call.state.isEmpty())
            continue;
        seen.insert(call.id, true);
        if (!m_openRecent.contains(call.id)) {
            QVariantMap row;
            const QString id = newId();
            row.insert(QStringLiteral("id"), id);
            row.insert(QStringLiteral("number"), call.number);
            row.insert(QStringLiteral("direction"), call.incoming ? QStringLiteral("incoming") : QStringLiteral("outgoing"));
            row.insert(QStringLiteral("state"), call.state);
            row.insert(QStringLiteral("answered"), call.state == QLatin1String("active"));
            row.insert(QStringLiteral("at"), QDateTime::currentDateTime().toString(Qt::ISODate));
            m_recents.prepend(row);
            m_openRecent.insert(call.id, id);
            changed = true;
            continue;
        }
        const QString recentId = m_openRecent.value(call.id);
        for (int i = 0; i < m_recents.size(); ++i) {
            QVariantMap row = m_recents.at(i).toMap();
            if (row.value(QStringLiteral("id")).toString() != recentId)
                continue;
            if (row.value(QStringLiteral("state")).toString() != call.state) {
                row.insert(QStringLiteral("state"), call.state);
                changed = true;
            }
            if (call.state == QLatin1String("active") && !row.value(QStringLiteral("answered")).toBool()) {
                row.insert(QStringLiteral("answered"), true);
                changed = true;
            }
            if (!call.number.isEmpty() && row.value(QStringLiteral("number")).toString() != call.number) {
                row.insert(QStringLiteral("number"), call.number);
                changed = true;
            }
            m_recents[i] = row;
            break;
        }
    }
    const auto openIds = m_openRecent.keys();
    for (const auto &callId : openIds) {
        if (seen.contains(callId))
            continue;
        const QString recentId = m_openRecent.take(callId);
        for (int i = 0; i < m_recents.size(); ++i) {
            QVariantMap row = m_recents.at(i).toMap();
            if (row.value(QStringLiteral("id")).toString() != recentId)
                continue;
            if (row.value(QStringLiteral("state")).toString() != QLatin1String("terminated")) {
                row.insert(QStringLiteral("state"), QStringLiteral("terminated"));
                m_recents[i] = row;
                changed = true;
            }
            break;
        }
    }
    while (m_recents.size() > 40) {
        m_recents.removeLast();
        changed = true;
    }
    if (!changed)
        return;
    saveRecents();
    emit recentsChanged();
}

QString Backend::newId() {
    return QString::number(QDateTime::currentMSecsSinceEpoch()) + QString::number(++m_idSeq);
}

QString Backend::contactName(const QString &number) const {
    for (const auto &item : m_contacts) {
        const auto row = item.toMap();
        if (row.value(QStringLiteral("number")).toString() == number)
            return row.value(QStringLiteral("name")).toString();
    }
    return {};
}

QString Backend::nameFor(const QString &number) const {
    return contactName(plain(number, 24));
}

bool Backend::addContact(const QString &name, const QString &number) {
    const QString cleanName = plain(name, 40);
    const QString cleanNumber = dialNumber(number);
    if (cleanName.isEmpty() || cleanNumber.isEmpty() || m_contacts.size() >= 200)
        return false;
    QVariantMap row;
    row.insert(QStringLiteral("id"), newId());
    row.insert(QStringLiteral("name"), cleanName);
    row.insert(QStringLiteral("number"), cleanNumber);
    m_contacts.append(row);
    saveContacts();
    emit contactsChanged();
    if (m_liveNumber == cleanNumber)
        publishLive(chooseLive({ListedCall{m_liveId, m_liveNumber, m_liveState, m_liveIncoming ? QStringLiteral("incoming") : QStringLiteral("outgoing"), m_liveIncoming, true}}));
    return true;
}

bool Backend::removeContact(const QString &id) {
    static const QRegularExpression expression(QStringLiteral("^[0-9]{6,24}$"));
    if (!expression.match(id).hasMatch())
        return false;
    for (int i = 0; i < m_contacts.size(); ++i) {
        if (m_contacts.at(i).toMap().value(QStringLiteral("id")).toString() != id)
            continue;
        m_contacts.removeAt(i);
        saveContacts();
        emit contactsChanged();
        if (hasLiveCall())
            publishLive(chooseLive({ListedCall{m_liveId, m_liveNumber, m_liveState,
                                               m_liveIncoming ? QStringLiteral("incoming") : QStringLiteral("outgoing"),
                                               m_liveIncoming, true}}));
        return true;
    }
    return false;
}

void Backend::loadStore() {
    auto load = [this](const QString &fileName, const QString &key) {
        QFile file(m_dataDirectory + "/" + fileName);
        if (!file.exists())
            return QJsonArray();
        if (!file.open(QIODevice::ReadOnly)) {
            m_storeBroken = true;
            return QJsonArray();
        }
        const auto doc = QJsonDocument::fromJson(file.readAll());
        if (!doc.isObject() || !doc.object().value(key).isArray()) {
            m_storeBroken = true;
            return QJsonArray();
        }
        return doc.object().value(key).toArray();
    };

    static const QRegularExpression contactId(QStringLiteral("^[0-9]{6,24}$"));
    for (const auto &item : load(QStringLiteral("contacts.json"), QStringLiteral("contacts"))) {
        const auto object = item.toObject();
        const QString id = object.value(QStringLiteral("id")).toString();
        const QString name = plain(object.value(QStringLiteral("name")).toString(), 40);
        const QString number = dialNumber(object.value(QStringLiteral("number")).toString());
        if (!contactId.match(id).hasMatch() || name.isEmpty() || number.isEmpty())
            continue;
        QVariantMap row;
        row.insert(QStringLiteral("id"), id);
        row.insert(QStringLiteral("name"), name);
        row.insert(QStringLiteral("number"), number);
        if (m_contacts.size() < 200)
            m_contacts.append(row);
    }

    for (const auto &item : load(QStringLiteral("recents.json"), QStringLiteral("recents"))) {
        const auto object = item.toObject();
        const QString id = object.value(QStringLiteral("id")).toString();
        const QString number = plain(object.value(QStringLiteral("number")).toString(), 24);
        const QString direction = object.value(QStringLiteral("direction")).toString();
        const QString state = callToken(object.value(QStringLiteral("state")).toString());
        const QString at = object.value(QStringLiteral("at")).toString();
        if (!contactId.match(id).hasMatch() || state.isEmpty())
            continue;
        if (direction != QLatin1String("incoming") && direction != QLatin1String("outgoing"))
            continue;
        if (at.size() < 16 || at.size() > 40)
            continue;
        QVariantMap row;
        row.insert(QStringLiteral("id"), id);
        row.insert(QStringLiteral("number"), number);
        row.insert(QStringLiteral("direction"), direction);
        row.insert(QStringLiteral("state"), state);
        row.insert(QStringLiteral("answered"), object.value(QStringLiteral("answered")).toBool());
        row.insert(QStringLiteral("at"), plain(at, 40));
        if (m_recents.size() < 40)
            m_recents.append(row);
    }
}

bool Backend::writeJson(const QString &path, const QJsonObject &object) const {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    return file.commit();
}

void Backend::saveContacts() {
    QJsonArray array;
    for (const auto &item : m_contacts) {
        const auto row = item.toMap();
        QJsonObject object;
        object.insert(QStringLiteral("id"), row.value(QStringLiteral("id")).toString());
        object.insert(QStringLiteral("name"), row.value(QStringLiteral("name")).toString());
        object.insert(QStringLiteral("number"), row.value(QStringLiteral("number")).toString());
        array.append(object);
    }
    QJsonObject root;
    root.insert(QStringLiteral("contacts"), array);
    if (!writeJson(m_dataDirectory + "/contacts.json", root))
        m_storeBroken = true;
}

void Backend::saveRecents() {
    QJsonArray array;
    for (const auto &item : m_recents) {
        const auto row = item.toMap();
        QJsonObject object;
        object.insert(QStringLiteral("id"), row.value(QStringLiteral("id")).toString());
        object.insert(QStringLiteral("number"), row.value(QStringLiteral("number")).toString());
        object.insert(QStringLiteral("direction"), row.value(QStringLiteral("direction")).toString());
        object.insert(QStringLiteral("state"), row.value(QStringLiteral("state")).toString());
        object.insert(QStringLiteral("answered"), row.value(QStringLiteral("answered")).toBool());
        object.insert(QStringLiteral("at"), row.value(QStringLiteral("at")).toString());
        array.append(object);
    }
    QJsonObject root;
    root.insert(QStringLiteral("recents"), array);
    if (!writeJson(m_dataDirectory + "/recents.json", root))
        m_storeBroken = true;
}

QByteArray Backend::contactsJson() const {
    QJsonArray array;
    for (const auto &item : m_contacts) {
        const auto row = item.toMap();
        QJsonObject object;
        object.insert(QStringLiteral("id"), row.value(QStringLiteral("id")).toString());
        object.insert(QStringLiteral("name"), row.value(QStringLiteral("name")).toString());
        object.insert(QStringLiteral("number"), row.value(QStringLiteral("number")).toString());
        array.append(object);
    }
    QJsonObject root;
    root.insert(QStringLiteral("contacts"), array);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

void Backend::requestRaise() {
    emit raiseRequested();
}
