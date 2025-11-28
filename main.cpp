#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QCommandLineParser>
#include <iostream>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);

#ifdef APP_VERSION
    QCoreApplication::setApplicationVersion(QString(APP_VERSION));
#else
    QCoreApplication::setApplicationVersion(QString("dev"));
#endif

    QCoreApplication::setApplicationName(QCoreApplication::translate("gav",
                                                                     "GAV is a simple audio and video player, backed by FFmpeg and Qt6"));

    QCommandLineParser parser;
    parser.setApplicationDescription("GAV is a simple audio and video player, backed by FFmpeg and Qt6");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption sourceOption("source", "Source of the audio or video to play");
    parser.addOption(sourceOption);

    parser.process(app);

    QString sourceValue;
    if (parser.isSet(sourceOption)) {
        sourceValue = parser.value(sourceOption);
    }

    QQmlApplicationEngine engine;
    if (!sourceValue.isEmpty()) {
        QUrl sourceURL = QUrl::fromUserInput(sourceValue);
        if (!sourceURL.isEmpty() && sourceURL.isValid())
            engine.setInitialProperties({{"source", sourceURL}});
    }

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.loadFromModule("gavqml", "Main");

#ifdef Q_OS_WIN
    // Free console only if launched without a parent console (GUI double-click)
    // This prevents the console window from staying open when launched from Explorer
    DWORD procIDs[2];
    DWORD maxCount = 2;
    DWORD result = GetConsoleProcessList((LPDWORD)procIDs, maxCount);
    
    // If result == 1, only this process is attached to console (launched from Explorer)
    // If result > 1, there's a parent console (cmd/powershell) - keep it attached
    if (result == 1) {
        FreeConsole();
    }
#endif

    return app.exec();
}
