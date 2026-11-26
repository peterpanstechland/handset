// Phone for the uConsole 4G modem: dial, answer, hang up, contacts, and calls.

#include <QGuiApplication>
#include <QIcon>
#include <QLocalServer>
#include <QLocalSocket>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTextStream>
#include <QUrl>
#include <unistd.h>

#include "backend.h"
#include "theme.h"
#include "translator.h"

namespace {
bool wantsContacts(int argc, char **argv) {
    for (int i = 1; i < argc; ++i)
        if (QString::fromLocal8Bit(argv[i]) == QLatin1String("contacts"))
            return true;
    return false;
}
}

int main(int argc, char *argv[]) {
    const bool contactsOnly = wantsContacts(argc, argv);
    if (contactsOnly)
        qputenv("QT_QPA_PLATFORM", "offscreen");

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("handset"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));
    app.setDesktopFileName(QStringLiteral("handset"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("handset")));

    if (contactsOnly) {
        Backend backend({}, {}, nullptr, 2500, false);
        if (backend.storeBroken()) {
            QTextStream(stderr) << "Could not read the address book\n";
            return 1;
        }
        QTextStream(stdout) << backend.contactsJson() << "\n";
        return 0;
    }

    // A second launch raises the window that is already open, so an incoming
    // ring does not start two copies that both try to answer.
    const QString serverName = QStringLiteral("handset-%1").arg(::getuid());
    QLocalSocket probe;
    probe.connectToServer(serverName);
    if (probe.waitForConnected(300)) {
        probe.write("raise\n");
        probe.waitForBytesWritten(300);
        return 0;
    }
    QLocalServer::removeServer(serverName);

    QQuickStyle::setStyle(QStringLiteral("Material"));

    Translator translator;
    app.installTranslator(&translator);
    Theme theme;
    Backend backend;

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty(QStringLiteral("translator"), &translator);
    engine.rootContext()->setContextProperty(QStringLiteral("theme"), &theme);
    engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);

    QLocalServer server;
    QObject::connect(&server, &QLocalServer::newConnection, &backend, [&backend, &server] {
        while (auto *socket = server.nextPendingConnection()) {
            backend.requestRaise();
            socket->deleteLater();
        }
    });
    server.listen(serverName);

    engine.load(QUrl(QStringLiteral("qrc:/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;
    return app.exec();
}
