#include "handset_tests.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"
#include "theme.h"
#include "translator.h"

namespace {
    bool write(const QString &path, const QByteArray &contents)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            return false;
        return file.write(contents) == contents.size();
    }

    QByteArray readAll(const QString &path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return {};
        return file.readAll();
    }

    QString installFake(const QString &dir)
    {
        const QString script = dir + "/mmcli";
        const QByteArray program =
            "#!/bin/sh\n"
            "dir=$HANDSET_FAKE\n"
            "[ -n \"$dir\" ] || exit 1\n"
            "printf '%s\\n' \"$*\" >> \"$dir/log\"\n"
            "if [ \"$1\" = \"-m\" ]; then\n"
            "  case $3 in\n"
            "    --voice-list-calls)\n"
            "      cat \"$dir/list\"\n"
            "      exit 0\n"
            "      ;;\n"
            "    --voice-create-call=*)\n"
            "      printf '%s\\n' \"$3\" >> \"$dir/created\"\n"
            "      if [ -f \"$dir/create-fail\" ]; then exit 1; fi\n"
            "      id=$(cat \"$dir/create-id\")\n"
            "      printf 'Successfully created new call: /org/freedesktop/ModemManager1/Call/%s\\n' \"$id\"\n"
            "      exit 0\n"
            "      ;;\n"
            "    --voice-delete-call=*)\n"
            "      printf '%s\\n' \"$3\" >> \"$dir/deleted\"\n"
            "      exit 0\n"
            "      ;;\n"
            "  esac\n"
            "  exit 1\n"
            "fi\n"
            "if [ \"$1\" = \"-o\" ]; then\n"
            "  printf '%s %s\\n' \"$2\" \"$3\" >> \"$dir/ops\"\n"
            "  if [ \"$3\" = \"-J\" ]; then\n"
            "    cat \"$dir/call-$2.json\"\n"
            "    exit 0\n"
            "  fi\n"
            "  if [ \"$3\" = \"--start\" ] && [ -f \"$dir/start-fail\" ]; then exit 1; fi\n"
            "  exit 0\n"
            "fi\n"
            "exit 1\n";
        write(script, program);
        write(dir + "/list", "No calls were found\n");
        QFile::setPermissions(script, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        return script;
    }

}

void HandsetTests::dialNumberAcceptsAPhoneNumber()
    {
        QCOMPARE(Backend::dialNumber("10086"), QString("10086"));
        QCOMPARE(Backend::dialNumber("+86 138-0013-8000"), QString("+8613800138000"));
        QCOMPARE(Backend::dialNumber("*#100#"), QString("*#100#"));
    }

void HandsetTests::dialNumberRejectsShellAndMarkup()
    {
        QCOMPARE(Backend::dialNumber("1"), QString());
        QCOMPARE(Backend::dialNumber("123;rm"), QString());
        QCOMPARE(Backend::dialNumber("$(id)"), QString());
        QCOMPARE(Backend::dialNumber("<img>"), QString());
    }

void HandsetTests::plainStripsMarkup()
    {
        QCOMPARE(Backend::plain("<img src=\"x\">", 80), QString("img src=\"x\""));
        QCOMPARE(Backend::plain("--"), QString());
        QCOMPARE(Backend::plain("--exec"), QString("--exec"));
        QCOMPARE(Backend::callIdFrom("/org/freedesktop/ModemManager1/Call/12"), QString("12"));
        QCOMPARE(Backend::callIdFrom("Call/nope"), QString());
    }

void HandsetTests::incomingRingWinsOverAnotherLiveCall()
    {
        Backend::ListedCall active;
        active.id = "2";
        active.state = "active";
        active.live = true;
        Backend::ListedCall ringing;
        ringing.id = "1";
        ringing.state = "ringing";
        ringing.incoming = true;
        ringing.live = true;
        QCOMPARE(Backend::chooseLive({active, ringing}).id, QString("1"));

        const auto marked = Backend::parseCallJson(
            "3", R"({"call":{"properties":{"number":"<img src=\"x\">","state":"ringing","direction":"incoming"}}})");
        QCOMPARE(marked.number, QString("img src=\"x\""));
        QVERIFY(marked.live);
        QVERIFY(marked.incoming);

        const auto unknown = Backend::parseCallJson(
            "4", R"({"call":{"properties":{"number":"10086","state":"--","direction":"outgoing"}}})");
        QVERIFY(!unknown.live);
        QCOMPARE(Backend::parseCallJson("nope", "{}").id, QString());
    }

void HandsetTests::contactsRoundTripAndRejectBadInput()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString id;
        {
            Backend backend(dir.path(), {}, nullptr, 2500, false);
            QVERIFY(backend.addContact("<Ada>", "100-86"));
            QCOMPARE(backend.contacts().size(), 1);
            const auto row = backend.contacts().at(0).toMap();
            QCOMPARE(row.value("name").toString(), QString("Ada"));
            QCOMPARE(row.value("number").toString(), QString("10086"));
            id = row.value("id").toString();
            QVERIFY(!backend.addContact("x", "1"));
            QVERIFY(!backend.addContact("", "10086"));
            QVERIFY(!backend.removeContact("../x"));
            QVERIFY(!backend.callNumber("123;rm"));
            QCOMPARE(backend.problem(), QString("invalid"));
        }
        {
            Backend again(dir.path(), {}, nullptr, 2500, false);
            QCOMPARE(again.contacts().size(), 1);
            QVERIFY(again.removeContact(id));
            QCOMPARE(again.contacts().size(), 0);
        }
        Backend empty(dir.path(), {}, nullptr, 2500, false);
        QCOMPARE(empty.contactsJson(), QByteArray("{\"contacts\":[]}"));
    }

void HandsetTests::placingACallCreatesThenStarts()
    {
        QTemporaryDir dir;
        const QString script = installFake(dir.path());
        write(dir.path() + "/create-id", "4\n");
        qputenv("HANDSET_FAKE", dir.path().toUtf8());

        Backend backend(dir.path() + "/data", script, nullptr, 100, true);
        QVERIFY(backend.callNumber("10086"));
        QTRY_VERIFY(QFile::exists(dir.path() + "/created"));
        const QString created = QString::fromUtf8(readAll(dir.path() + "/created"));
        QVERIFY(created.contains("--voice-create-call=number=10086"));
        QTRY_VERIFY(QString::fromUtf8(readAll(dir.path() + "/ops")).contains("4 --start"));
    }

void HandsetTests::aFailedStartDeletesTheUnstartedCall()
    {
        QTemporaryDir dir;
        const QString script = installFake(dir.path());
        write(dir.path() + "/create-id", "9\n");
        write(dir.path() + "/start-fail", "1\n");
        qputenv("HANDSET_FAKE", dir.path().toUtf8());

        Backend backend(dir.path() + "/data", script, nullptr, 100, true);
        QVERIFY(backend.callNumber("*#100#"));
        QTRY_VERIFY(QString::fromUtf8(readAll(dir.path() + "/deleted")).contains("--voice-delete-call=9"));
        QTRY_COMPARE(backend.problem(), QString("failed"));
        QTRY_COMPARE(backend.busy(), false);
    }

void HandsetTests::anIncomingCallIsListedAndThenClosed()
    {
        QTemporaryDir dir;
        const QString script = installFake(dir.path());
        write(dir.path() + "/list", "/org/freedesktop/ModemManager1/Call/2\n");
        write(dir.path() + "/call-2.json",
              R"({"call":{"properties":{"number":"10086","state":"ringing","direction":"incoming"}}})");
        write(dir.path() + "/data/contacts.json",
              R"({"contacts":[{"id":"123456","name":"Harbor Desk","number":"10086"}]})");
        qputenv("HANDSET_FAKE", dir.path().toUtf8());

        Backend backend(dir.path() + "/data", script, nullptr, 100, true);
        QTRY_COMPARE(backend.liveName(), QString("Harbor Desk"));
        QTRY_COMPARE(backend.liveIncoming(), true);
        QTRY_COMPARE(backend.recents().size(), 1);
        QCOMPARE(backend.recents().at(0).toMap().value("number").toString(), QString("10086"));

        write(dir.path() + "/list", "No calls were found\n");
        backend.refresh();
        QTRY_COMPARE(backend.hasLiveCall(), false);
        QTRY_COMPARE(backend.recents().at(0).toMap().value("state").toString(), QString("terminated"));
        QCOMPARE(backend.recents().at(0).toMap().value("answered").toBool(), false);
    }

void HandsetTests::themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }

void HandsetTests::themeReadsAccent()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        QVERIFY(write(dir.path() + "/theme/colors.toml", "accent = \"#112233\"\n"));

        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
    }

void HandsetTests::themeFollowsHyprlandRounding()
    {
        QTemporaryDir dir;
        const QString response = dir.filePath("response.json");
        const QString command = dir.filePath("hyprctl");
        QVERIFY(write(command, "#!/bin/sh\ncat '" + response.toUtf8() + "'\n"));
        QVERIFY(QFile::setPermissions(command, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        QVERIFY(write(response, "{\"int\":0}"));

        Theme theme(dir.path(), nullptr, command);
        QCOMPARE(theme.radius(), 0);
        QVERIFY(write(response, "{\"int\":6}"));
        QTRY_COMPARE(theme.radius(), 6);
    }

void HandsetTests::translatorFallsBackToEnglish()
    {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Save").isNull());
    }

void HandsetTests::translatorFollowsTheLanguageFile()
    {
        QTemporaryDir dir;
        QVERIFY(write(dir.path() + "/language", "pt-BR\n"));
        QVERIFY(write(dir.path() + "/pt.json", R"({"Save": "Guardar", "Open": "Abrir"})"));
        QVERIFY(write(dir.path() + "/pt-BR.json", R"({"Save": "Salvar"})"));

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("pt-BR"));
        QCOMPARE(translator.translate("Main", "Save"), QString("Salvar"));
        QCOMPARE(translator.translate("Main", "Open"), QString("Abrir"));
        QVERIFY(translator.translate("Main", "Quit").isNull());
    }

void HandsetTests::translatorPicksPluralForms()
    {
        QTemporaryDir dir;
        QVERIFY(write(dir.path() + "/language", "ru\n"));
        QVERIFY(write(dir.path() + "/en.json", R"({"%n clips": {"one": "%n clip", "other": "%n clips"}})"));
        QVERIFY(write(dir.path() + "/ru.json",
                      R"({"%n clips": {"one": "%n клип", "few": "%n клипа", "many": "%n клипов", "other": "%n клипа"}})"));

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 21), QString("%n клип"));
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 22), QString("%n клипа"));
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 25), QString("%n клипов"));
        QCOMPARE(Translator::pluralCategory("ar", 11), QString("many"));
        QCOMPARE(Translator::pluralCategory("ja", 1), QString("other"));
    }

QTEST_MAIN(HandsetTests)
