#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariantList>

// Voice calls, a local address book, and a call log for the 4G modem.
// Numbers that reach mmcli are limited to +, digits, * and #, and they are
// passed as one argument, never through a shell.
class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString number READ number NOTIFY numberChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString problem READ problem NOTIFY problemChanged)
    Q_PROPERTY(bool hasLiveCall READ hasLiveCall NOTIFY liveChanged)
    Q_PROPERTY(QString liveId READ liveId NOTIFY liveChanged)
    Q_PROPERTY(QString liveNumber READ liveNumber NOTIFY liveChanged)
    Q_PROPERTY(QString liveName READ liveName NOTIFY liveChanged)
    Q_PROPERTY(QString liveState READ liveState NOTIFY liveChanged)
    Q_PROPERTY(bool liveIncoming READ liveIncoming NOTIFY liveChanged)
    Q_PROPERTY(QVariantList contacts READ contacts NOTIFY contactsChanged)
    Q_PROPERTY(QVariantList recents READ recents NOTIFY recentsChanged)
    Q_PROPERTY(bool storeBroken READ storeBroken NOTIFY problemChanged)

public:
    explicit Backend(const QString &dataDirectory = {}, const QString &mmcli = {}, QObject *parent = nullptr,
                     int pollMs = 2500, bool watchModem = true);
    ~Backend() override;

    QString number() const { return m_number; }
    bool busy() const { return m_busy; }
    QString problem() const { return m_problem; }
    bool hasLiveCall() const { return !m_liveId.isEmpty(); }
    QString liveId() const { return m_liveId; }
    QString liveNumber() const { return m_liveNumber; }
    QString liveName() const { return m_liveName; }
    QString liveState() const { return m_liveState; }
    bool liveIncoming() const { return m_liveIncoming; }
    QVariantList contacts() const { return m_contacts; }
    QVariantList recents() const { return m_recents; }
    bool storeBroken() const { return m_storeBroken; }

    Q_INVOKABLE void appendDigit(const QString &digit);
    Q_INVOKABLE void backspace();
    Q_INVOKABLE void clearNumber();
    Q_INVOKABLE void primaryAction();
    Q_INVOKABLE bool callNumber(const QString &number = {});
    Q_INVOKABLE void answer();
    Q_INVOKABLE void hangup();
    Q_INVOKABLE bool addContact(const QString &name, const QString &number);
    Q_INVOKABLE bool removeContact(const QString &id);
    Q_INVOKABLE QString nameFor(const QString &number) const;
    Q_INVOKABLE void refresh();
    Q_INVOKABLE QByteArray contactsJson() const;
    void requestRaise();

    static QString plain(const QString &value, int maxLength = 80);
    static QString dialNumber(const QString &value);
    static QString callIdFrom(const QString &raw);
    static QString callToken(const QString &value);

    struct ListedCall {
        QString id;
        QString number;
        QString state;
        QString direction;
        bool incoming = false;
        bool live = false;
    };
    static QStringList callIndexes(const QString &listOutput);
    static ListedCall parseCallJson(const QString &id, const QByteArray &json);
    static ListedCall chooseLive(const QList<ListedCall> &calls);

signals:
    void numberChanged();
    void busyChanged();
    void problemChanged();
    void liveChanged();
    void contactsChanged();
    void recentsChanged();
    void raiseRequested();

private:
    enum class Op { None, List, Detail, Create, Start, Accept, Hangup, Delete };

    void setBusy(bool busy);
    void setProblem(const QString &problem);
    void setNumber(const QString &number);
    void startOp(Op op, const QStringList &args);
    void finishOp(Op op, int code, const QByteArray &raw);
    bool handleOp(Op op, int code, const QByteArray &raw);
    bool runQueue();
    void beginCall(const QString &safe);
    void beginSimple(Op op, const QStringList &args);
    void startDetail();
    void applyListed(const QList<ListedCall> &calls, bool ok);
    void noteCalls(const QList<ListedCall> &calls);
    void loadStore();
    void saveContacts();
    void saveRecents();
    bool writeJson(const QString &path, const QJsonObject &object) const;
    QString contactName(const QString &number) const;
    QString newId();
    void publishLive(const ListedCall &live);
    static bool idOk(const QString &id);

    QString m_dataDirectory;
    QString m_mmcli;
    QString m_number;
    bool m_busy = false;
    QString m_problem;
    bool m_storeBroken = false;
    QString m_liveId;
    QString m_liveNumber;
    QString m_liveName;
    QString m_liveState;
    bool m_liveIncoming = false;
    QVariantList m_contacts;
    QVariantList m_recents;
    QHash<QString, QString> m_openRecent;
    int m_idSeq = 0;

    Op m_op = Op::None;
    QString m_actionId;
    QString m_detailId;
    QStringList m_pendingIds;
    QList<ListedCall> m_listed;
    QString m_queued;
    QString m_queuedNumber;
    bool m_refreshQueued = false;

    QProcess m_process;
    QTimer m_poll;
    QTimer m_kill;
};
