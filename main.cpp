#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QCommandLineParser>
#include <iostream>

int main(int argc, char *argv[])
{
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

    if (!pa)
    // engine.loadFromModule("gavqml", "Main");

    return app.exec();
}
