#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QCommandLineParser>

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
    parser.addPositionalArgument("source", QCoreApplication::translate("main", "Source file to play"));

    parser.process(app);

    const QStringList args = parser.positionalArguments();

    QQmlApplicationEngine engine;
    if (!args.isEmpty()) {
        QUrl u = QUrl::fromUserInput(args.first());
        if (!u.isEmpty() && u.isValid())
            engine.setInitialProperties({{"source", u}});
    }

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("gavqml", "Main");

    return app.exec();
}
