#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QCommandLineParser>
#include <iostream>
#include <thread>
#include <atomic>

#include "collage.h"

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

    QCommandLineOption sourceOption({"s", "source"}, "Source of the audio or video to play");
    parser.addOption(sourceOption);

    QCommandLineOption collageOption({"c", "collage"}, "Create a collage from video files");
    parser.addOption(collageOption);

    parser.process(app);

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
        for (const QString &path: collagePaths) {
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
