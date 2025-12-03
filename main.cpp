#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QCommandLineParser>
#include <iostream>
#include <thread>
#include <atomic>

#include "collage.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

std::shared_ptr<spdlog::logger> logger;

void initLogging()
{
    // Create a console sink (stdout)
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    
    // Create logger with the sink
    logger = std::make_shared<spdlog::logger>("gav", console_sink);
    
    // Register as default logger
    spdlog::set_default_logger(logger);
    
    // https://github.com/gabime/spdlog/wiki/3.-Custom-formatting
#ifdef QT_DEBUG
    logger->set_pattern("[%x %H:%M:%S.%f] [%o ms] [%L] [%t] %v");
    logger->set_level(spdlog::level::debug);
#else
    logger->set_pattern("[%x %H:%M:%S] [%L] %v");
    logger->set_level(spdlog::level::info);
#endif
    
    // https://github.com/gabime/spdlog/wiki/7.-Flush-policy
    logger->flush_on(spdlog::level::info);
}

void logOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QByteArray localMsg = msg.toLocal8Bit();
    const char *file = context.file ? context.file : "";
    const char *function = context.function ? context.function : "";
    switch (type) {
    case QtDebugMsg:
        logger->debug("Debug: {} ({}:{}, {})", localMsg.constData(), file, context.line, function);
        break;
    case QtInfoMsg:
        logger->info("Info: {} ({}:{}, {})", localMsg.constData(), file, context.line, function);
        break;
    case QtWarningMsg:
        logger->warn("Warning: {} ({}:{}, {})", localMsg.constData(), file, context.line, function);
        break;
    case QtCriticalMsg:
    case QtFatalMsg:
        logger->critical("Critical: {} ({}:{}, {})", localMsg.constData(), file, context.line, function);
        break;
    }
}

int main(int argc, char *argv[]) {
    initLogging();
    qInstallMessageHandler(logOutput);
    
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

    QCommandLineOption sourceOption({"s", "source"}, "Source of the audio or video to play");
    parser.addOption(sourceOption);

    QCommandLineOption collageOption({"c", "collage"}, "Create a collage from video files");
    parser.addOption(collageOption);

    QCommandLineOption verboseOption("verbose", "Enable verbose logging");
    parser.addOption(verboseOption);

    parser.process(app);

    if (parser.isSet(verboseOption)) {
        logger->set_level(spdlog::level::debug);
        logger->debug("Verbose logging enabled");
    }

    QString sourceValue;
    if (parser.isSet(sourceOption)) {
        sourceValue = parser.value(sourceOption);
    }

    if (parser.isSet(collageOption)) {
        QStringList collagePaths = parser.positionalArguments();
        if (collagePaths.isEmpty() && !sourceValue.isEmpty()) {
            collagePaths.append(sourceValue);
        }

        QList<QUrl> urls;
        for (const QString &path: std::as_const(collagePaths)) {
            urls.append(QUrl::fromUserInput(path));
        }

        // Create collage instance
        Collage collage;

        // Track results
        int successCount = 0;
        int failCount = 0;
        QStringList successPaths;
        QStringList failPaths;

        // Connect signals to track progress
        QObject::connect(&collage, &Collage::collageCompleted, [&](int index, const QString& outputPath, bool success) {
            if (success) {
                successCount++;
                successPaths.append(outputPath);
            } else {
                failCount++;
                failPaths.append(outputPath.isEmpty() ? QString("Unknown") : outputPath);
            }
        });

        // Start spinner in a separate thread
        std::atomic isRunning(true);
        std::thread spinnerThread([&isRunning]() {
            std::vector spinner = {'|', '/', '-', '\\'};
            int i = 0;
            while (isRunning) {
                std::cout << "\rCreating collage... " << spinner[i % 4] << " ";
                std::cout.flush();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                i++;
            }
        });

        // Call the collage function
        collage.toCollage(urls);

        // Stop the spinner
        isRunning = false;
        spinnerThread.join();

        // Clear the loading line
        std::cout << "\r\033[K";

        // Display results
        std::cout << "\n=== Collage Creation Summary ===\n" << std::endl;

        for (int i = 0; i < successPaths.size(); ++i) {
            std::cout << "[" << i << "] ✓ Success: "
                     << successPaths[i].toStdString() << std::endl;
        }

        for (int i = 0; i < failPaths.size(); ++i) {
            std::cout << "[" << (successPaths.size() + i) << "] ✗ Failed: "
                     << failPaths[i].toStdString() << std::endl;
        }

        std::cout << "\n" << successCount << " collage(s) created successfully";
        if (failCount > 0) {
            std::cout << ", " << failCount << " failed";
        }
        std::cout << ".\n" << std::endl;

        return failCount > 0 ? 1 : 0;
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
    DWORD result = GetConsoleProcessList((LPDWORD) procIDs, maxCount);

    // If result == 1, only this process is attached to console (launched from Explorer)
    // If result > 1, there's a parent console (cmd/powershell) - keep it attached
    if (result == 1) {
        FreeConsole();
    }
#endif

    return app.exec();
}
