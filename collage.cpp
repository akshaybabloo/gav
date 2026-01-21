#include "collage.h"
#include <QMediaPlayer>
#include <QMediaMetaData>
#include <QEventLoop>
#include <QVideoSink>
#include <QVideoFrame>
#include <QDebug>
#include <QPainter>
#include <QFontMetrics>
#include <QtConcurrent>
#include <QCoreApplication>
#include <QMutex>
#include <QMutexLocker>
#include <QTimer>
#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <atomic>

// Timeout for external process completion (5 minutes per video)
// This allows for processing of large video files on slower systems
static constexpr int PROCESS_TIMEOUT_MS = 300000;

// Static tracking for running child processes
//
// These variables manage the lifecycle of child processes spawned for collage creation.
// They are shared across all Collage instances in this process.
//
// Threading / locking:
//  - s_processMutex must be held whenever s_runningProcesses is modified or
//    iterated, to protect against concurrent access from worker threads.
//  - s_shuttingDown is an atomic flag that can be safely read from any thread.
//    It is set to true during application shutdown to signal that no new child
//    processes should be started and existing ones may be terminated.
//
// Ownership / lifetime:
//  - s_runningProcesses contains pointers to heap-allocated QProcess instances.
//    Each process is removed from the list when it finishes. On shutdown, any
//    remaining running processes are forcefully killed.
static QMutex s_processMutex;
static QList<QProcess *> s_runningProcesses;
static std::atomic<bool> s_shuttingDown{false};

Collage::Collage() {
    // Set up thread pool with limited concurrency
    m_threadPool.setMaxThreadCount(MAX_CONCURRENT);
    qDebug() << "Collage thread pool initialized with" << MAX_CONCURRENT << "max threads";

    // Connect watcher signals
    connect(&m_watcher, &QFutureWatcher<CollageResult>::resultReadyAt,
            this, [this](int resultIndex) {
                CollageResult result = m_watcher.resultAt(resultIndex);
                emit collageCompleted(result.index, result.outputPath, result.success);
            });

    connect(&m_watcher, &QFutureWatcher<CollageResult>::finished,
            this, &Collage::onFutureFinished);

    // Connect to application shutdown for true global shutdown handling
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [this]() {
        s_shuttingDown = true;
        m_shuttingDown = true;
    });
}

Collage::~Collage() {
    // Signal instance-level shutdown (does not affect other instances)
    m_shuttingDown = true;

    // Cancel pending work
    m_watcher.cancel();

    // Terminate all running child processes to allow quick shutdown
    // The worker threads are blocked on waitForFinished(), so we need to
    // kill the processes here to unblock them
    {
        QMutexLocker locker(&s_processMutex);
        for (QProcess *process : s_runningProcesses) {
            if (process && process->state() != QProcess::NotRunning) {
                qDebug() << "Terminating child process on shutdown";
                process->kill();
            }
        }
    }

    // Wait for our tasks to complete (they will exit quickly now that processes are killed)
    m_watcher.waitForFinished();
}

void Collage::toCollage(const QList<QUrl> &paths) {
    // Cancel any running operation
    if (m_watcher.isRunning()) {
        m_watcher.cancel();
        m_watcher.waitForFinished();
    }

    // Reset shutdown flag for new operation
    m_shuttingDown = false;

    m_currentPaths = paths;
    emit collageStarted(paths.size());

    if (paths.isEmpty()) {
        emit collageFinished(0, 0);
        return;
    }

    // Create list of index-path pairs for mapping
    QList<QPair<int, QUrl> > indexedPaths;
    for (int i = 0; i < paths.size(); ++i) {
        indexedPaths.append({i, paths[i]});
    }

    // Capture reference to instance shutdown flag for the lambda
    std::atomic<bool>& shuttingDown = m_shuttingDown;

    // Use QtConcurrent::mapped with custom thread pool
    // Each video is processed in a SEPARATE PROCESS for complete isolation
    // Note: collageProgress is emitted inside processVideoExternal when processing actually starts
    QFuture<CollageResult> future = QtConcurrent::mapped(
        &m_threadPool,
        indexedPaths,
        [&shuttingDown](const QPair<int, QUrl> &pair) -> CollageResult {
            return processVideoExternal(pair.first, pair.second, shuttingDown);
        }
    );

    m_watcher.setFuture(future);
}

QString Collage::createCollageSingle(const QUrl &path) {
    // This is called from CLI mode - does the actual collage work synchronously
    qDebug() << "Creating collage for:" << path.toString();

    auto meta = collectImages(path);
    auto collageImage = drawCollage(meta);

    if (collageImage.isNull()) {
        qDebug() << "Collage image is null for path:" << path.toString();
        return QString();
    }

    // Get the source file info
    QFileInfo sourceFile(path.toLocalFile());

    // Create output filename: originalname_collage.jpg
    QString outputFilename = sourceFile.completeBaseName() + "_collage.jpg";

    // Get the directory where the source file is located
    QString outputPath = sourceFile.dir().filePath(outputFilename);

    if (collageImage.save(outputPath, "JPEG", 100)) {
        qDebug() << "Collage saved to:" << outputPath;
        return outputPath;
    }

    qDebug() << "Failed to save collage to:" << outputPath;
    return QString();
}

CollageResult Collage::processVideoExternal(int index, const QUrl &path, std::atomic<bool>& shuttingDown) {
    // Check if we're shutting down before starting
    if (shuttingDown || s_shuttingDown) {
        return CollageResult{index, path.toLocalFile(), QString(), false};
    }

    // Spawn a separate process to create the collage
    // This completely isolates the heavy media processing from the main app
    qDebug() << "Spawning external process for video" << index << ":" << path.toString();

    QString appPath = QCoreApplication::applicationFilePath();
    QString inputPath = path.toLocalFile();

    // Use heap-allocated QProcess to ensure valid pointer lifetime while in s_runningProcesses
    auto process = new QProcess();
    process->setProgram(appPath);
    process->setArguments({"--collage", inputPath});

    // Set environment variable to indicate subprocess mode (suppresses spinner output)
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("GAV_SUBPROCESS", "1");
    process->setProcessEnvironment(env);

    // Register process for cleanup on shutdown
    {
        QMutexLocker locker(&s_processMutex);
        s_runningProcesses.append(process);
    }

    process->start();

    // Wait for process to finish
    bool finished = process->waitForFinished(PROCESS_TIMEOUT_MS);

    // Unregister process from tracking list
    {
        QMutexLocker locker(&s_processMutex);
        s_runningProcesses.removeOne(process);
    }

    // Check if we were killed due to shutdown
    if (shuttingDown || s_shuttingDown) {
        if (process->state() != QProcess::NotRunning) {
            process->kill();
            process->waitForFinished(1000);
        }
        delete process;
        return CollageResult{index, inputPath, QString(), false};
    }

    if (!finished) {
        qDebug() << "Process timeout for video" << index;
        process->kill();
        process->waitForFinished(1000);
        delete process;
        return CollageResult{index, inputPath, QString(), false};
    }

    int exitCode = process->exitCode();
    QString stdOut = QString::fromUtf8(process->readAllStandardOutput()).trimmed();
    QString stdErr = QString::fromUtf8(process->readAllStandardError()).trimmed();

    qDebug() << "Process finished for video" << index << "with exit code" << exitCode;
    qDebug() << "stdout:" << stdOut;
    if (!stdErr.isEmpty()) {
        qDebug() << "stderr:" << stdErr;
    }

    delete process;

    // Parse output - format is SUCCESS:<path> or FAILED:<path>
    if (exitCode == 0 && stdOut.startsWith("SUCCESS:")) {
        QString outputPath = stdOut.mid(8); // Remove "SUCCESS:" prefix
        return CollageResult{index, inputPath, outputPath, true};
    }

    return CollageResult{index, inputPath, QString(), false};
}

void Collage::onFutureFinished() {
    int successCount = 0;
    int failCount = 0;

    // Count results
    for (int i = 0; i < m_watcher.future().resultCount(); ++i) {
        if (m_watcher.resultAt(i).success) {
            successCount++;
        } else {
            failCount++;
        }
    }

    emit collageFinished(successCount, failCount);
}

ImageMeta Collage::collectImages(const QUrl &path) {
    qDebug() << "=== Starting collectImages for:" << path.toString();

    QMediaPlayer player;
    player.setSource(path);

    QEventLoop loop;
    QObject::connect(&player, &QMediaPlayer::mediaStatusChanged, &loop, [&](QMediaPlayer::MediaStatus status) {
        qDebug() << "Media status changed to:" << status;
        if (status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::InvalidMedia) {
            loop.quit();
        }
    });
    QObject::connect(&player, &QMediaPlayer::errorOccurred, &loop,
                     [&loop](QMediaPlayer::Error error, const QString &errorString) {
                         qDebug() << "Media player error occurred:" << error << errorString;
                         loop.quit();
                     });

    // Timeout after 2 seconds to prevent blocking indefinitely
    QTimer::singleShot(2000, &loop, &QEventLoop::quit);

    loop.exec();

    qDebug() << "Media loaded. Duration:" << player.duration() << "ms";

    auto data = player.metaData();
    QList<ImageTime> frames;

    // Calculate file size
    const QFileInfo fileInfo(path.toLocalFile());
    QString fileSize;
    if (fileInfo.exists()) {
        qint64 sizeInBytes = fileInfo.size();
        qDebug() << "File size in bytes:" << sizeInBytes;
        if (sizeInBytes < 1024) {
            fileSize = QString::number(sizeInBytes) + " B";
        } else if (sizeInBytes < 1024 * 1024) {
            fileSize = QString::number(sizeInBytes / 1024.0, 'f', 2) + " KB";
        } else if (sizeInBytes < 1024 * 1024 * 1024) {
            fileSize = QString::number(sizeInBytes / (1024.0 * 1024.0), 'f', 2) + " MB";
        } else {
            fileSize = QString::number(sizeInBytes / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB";
        }
        qDebug() << "Formatted file size:" << fileSize;
    } else {
        qDebug() << "File does not exist at:" << path.toLocalFile();
    }

    // Set up video sink for frame capture
    QVideoSink videoSink;
    player.setVideoSink(&videoSink);
    qDebug() << "Video sink set up";

    // Start playing to initialize the video pipeline
    player.play();
    qDebug() << "Player started playing";

    // Wait a bit for the player to start
    QEventLoop startLoop;
    QTimer::singleShot(300, &startLoop, &QEventLoop::quit);
    startLoop.exec();
    qDebug() << "Player initialization wait complete";

    // Seek over the video and capture frames
    qint64 duration = player.duration();
    if (duration > 0) {
        int numFrames = qMin(16, static_cast<int>(duration / 1000) + 1); // At least 1 frame, up to 10
        numFrames = qMax(1, numFrames); // Ensure at least one frame
        qDebug() << "Will attempt to capture" << numFrames << "frames";

        for (int i = 0; i < numFrames; ++i) {
            // Calculate position for this frame
            qint64 position = (duration * i) / numFrames;
            qDebug() << "Frame" << i << "- seeking to position:" << position << "ms";
            player.setPosition(position);

            // Brief wait for seek to complete
            QEventLoop seekLoop;
            QTimer::singleShot(100, &seekLoop, &QEventLoop::quit);
            seekLoop.exec();

            // Pause to render the frame at this position
            player.pause();

            // Wait for the frame to be available
            QEventLoop frameLoop;
            QTimer frameTimer;
            frameTimer.setSingleShot(true);

            bool frameReady = false;
            auto connection = QObject::connect(&videoSink, &QVideoSink::videoFrameChanged, &frameLoop,
                                               [&](const QVideoFrame &frame) {
                                                   if (frame.isValid()) {
                                                       if (const QImage image = frame.toImage(); !image.isNull()) {
                                                           frames.append({.image = image, .timestamp = position});
                                                           frameReady = true;
                                                           frameLoop.quit();
                                                       }
                                                   }
                                               });

            // Timeout after 800ms per frame
            QObject::connect(&frameTimer, &QTimer::timeout, &frameLoop, [&frameLoop]() {
                frameLoop.quit();
            });
            frameTimer.start(800);

            frameLoop.exec();

            QObject::disconnect(connection);

            // If we didn't get a frame via signal, try to get the current frame directly
            if (!frameReady) {
                qDebug() << "Frame" << i << "- signal did not provide frame, trying direct access";
                if (QVideoFrame frame = videoSink.videoFrame(); frame.isValid()) {
                    qDebug() << "Frame" << i << "- got valid frame directly";
                    if (QImage image = frame.toImage(); !image.isNull()) {
                        frames.append({.image = image, .timestamp = position});
                        frameReady = true;
                        qDebug() << "Frame" << i << "- successfully converted to image";
                    } else {
                        qDebug() << "Frame" << i << "- failed to convert to image";
                    }
                } else {
                    qDebug() << "Frame" << i << "- no valid frame available";
                }
            } else {
                qDebug() << "Frame" << i << "- captured successfully via signal";
            }

            // Resume playing for the next seek
            if (i < numFrames - 1) {
                player.play();
                QEventLoop resumeLoop;
                QTimer::singleShot(50, &resumeLoop, &QEventLoop::quit);
                resumeLoop.exec();
            }
        }
    } else {
        qDebug() << "Duration is 0 or invalid, cannot capture frames";
    }

    player.stop();
    qDebug() << "Total frames captured:" << frames.size();

    // If no frames were captured, try playing briefly then capturing
    if (frames.isEmpty()) {
        qDebug() << "No frames captured, trying fallback method";
        player.setPosition(0);
        player.play();

        QEventLoop playLoop;
        QTimer::singleShot(500, &playLoop, &QEventLoop::quit);
        playLoop.exec();

        player.pause();

        QEventLoop pauseLoop;
        QTimer::singleShot(200, &pauseLoop, &QEventLoop::quit);
        pauseLoop.exec();

        if (QVideoFrame frame = videoSink.videoFrame(); frame.isValid()) {
            qDebug() << "Fallback - got valid frame";
            if (QImage image = frame.toImage(); !image.isNull()) {
                frames.append({.image = image, .timestamp = player.position()});
                qDebug() << "Fallback - successfully captured frame";
            } else {
                qDebug() << "Fallback - failed to convert frame to image";
            }
        } else {
            qDebug() << "Fallback - no valid frame available";
        }

        player.stop();
    }

    auto image = ImageMeta{
        .image = frames,
        .name = player.source().fileName(),
        .duration = data.stringValue(QMediaMetaData::Duration),
        .audioCodec = data.stringValue(QMediaMetaData::AudioCodec),
        .videoCodec = data.stringValue(QMediaMetaData::VideoCodec),
        .size = fileSize,
        .resolution = data.stringValue(QMediaMetaData::Resolution)
    };

    qDebug() << "Collected" << frames.size() << "frames from" << path.toString() << "Name:" << image.name
            << "Duration:" << image.duration << "Audio Codec:" << image.audioCodec
            << "Video Codec:" << image.videoCodec << "Size:" << image.size
            << "Resolution:" << image.resolution;

    return image;
}

QImage Collage::drawCollage(const ImageMeta &meta) {
    auto images = meta.image;
    if (images.isEmpty()) {
        qDebug() << "No images to draw collage.";
        return {};
    }

    // Determine if video is portrait or landscape from first frame
    bool isPortrait = false;
    if (!images.isEmpty()) {
        const auto &firstImg = images.first().image;
        isPortrait = firstImg.height() > firstImg.width();
    }

    QList<QImage> processedImages;

    // Larger thumbnails for better quality
    constexpr int thumbSize = 320;

    for (const auto &img: images) {
        QImage resized = img.image.scaled(thumbSize, thumbSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        // convert img.timestamp to hh:mm:ss format
        const qint64 seconds = img.timestamp / 1000;
        const qint64 hh = seconds / 3600;
        const qint64 mm = (seconds % 3600) / 60;
        const qint64 ss = seconds % 60;
        QString timeText = QString("%1:%2:%3")
                .arg(hh, 2, 10, QChar('0'))
                .arg(mm, 2, 10, QChar('0'))
                .arg(ss, 2, 10, QChar('0'));

        QPainter painter(&resized);
        painter.setRenderHint(QPainter::Antialiasing);

        // Draw semi-transparent background for timestamp
        QFont font("Arial", 10, QFont::Bold);
        painter.setFont(font);
        QFontMetrics fm(font);
        QRect textRect = fm.boundingRect(timeText);
        int padding = 4;
        QRect bgRect(resized.width() - textRect.width() - padding * 2 - 2,
                     resized.height() - textRect.height() - padding * 2 - 2,
                     textRect.width() + padding * 2,
                     textRect.height() + padding * 2);
        painter.fillRect(bgRect, QColor(0, 0, 0, 160));

        // Draw timestamp text
        painter.setPen(Qt::white);
        painter.drawText(bgRect, Qt::AlignCenter, timeText);
        painter.end();

#ifdef QT_DEBUG
        // Save image to disk for debugging
        if (QString debugFilename = QString("debug_frame_%1ms.jpg").arg(img.timestamp); resized.save(debugFilename)) {
            qDebug() << "Saved debug image:" << debugFilename;
        } else {
            qDebug() << "Failed to save debug image:" << debugFilename;
        }
#endif
        processedImages.append(resized);
    }

    // Create description table at the top
    constexpr int tableMargin = 10;
    constexpr int tableRowHeight = 18;
    constexpr int tablePadding = 8;
    constexpr int tableRadius = 8;

    // Format duration as hh:mm:ss
    QString formattedDuration = meta.duration;
    bool ok;
    qint64 durationMs = meta.duration.toLongLong(&ok);
    if (ok && durationMs > 0) {
        qint64 totalSeconds = durationMs / 1000;
        qint64 hours = totalSeconds / 3600;
        qint64 minutes = (totalSeconds % 3600) / 60;
        qint64 secs = totalSeconds % 60;
        formattedDuration = QString("%1:%2:%3")
                .arg(hours, 2, 10, QChar('0'))
                .arg(minutes, 2, 10, QChar('0'))
                .arg(secs, 2, 10, QChar('0'));
    }

    // Prepare metadata text
    QStringList metaLines;
    metaLines << QString("Name: %1").arg(meta.name);
    metaLines << QString("Duration: %1").arg(formattedDuration);
    metaLines << QString("Resolution: %1").arg(meta.resolution);
    metaLines << QString("Size: %1").arg(meta.size);
    metaLines << QString("Video Codec: %1").arg(meta.videoCodec);
    metaLines << QString("Audio Codec: %1").arg(meta.audioCodec);

    const int tableHeight = metaLines.size() * tableRowHeight + 2 * tablePadding;

    // Draw collage using QPainter instead of QTableWidget
    // Auto-adjust columns: 3 for portrait videos, 4 for landscape
    const int cols = isPortrait ? 3 : 4;
    const int rows = (processedImages.size() + cols - 1) / cols;
    constexpr int paddingX = 5; // Gap between columns
    constexpr int paddingY = 5; // Gap between rows
    constexpr int marginTop = 5; // Top margin
    constexpr int marginBottom = 5; // Bottom margin
    constexpr int marginLeft = 5; // Left margin
    constexpr int marginRight = 5; // Right margin

    // Find the actual max dimensions from processed images
    int maxImgWidth = 0;
    int maxImgHeight = 0;
    for (const auto &img: processedImages) {
        maxImgWidth = qMax(maxImgWidth, img.width());
        maxImgHeight = qMax(maxImgHeight, img.height());
    }

    // Use actual image dimensions for cell size
    const int cellWidth = maxImgWidth;
    const int cellHeight = maxImgHeight;

    // Canvas width: left margin + all columns + gaps between columns + right margin
    const int canvasWidth = marginLeft + cols * cellWidth + (cols - 1) * paddingX + marginRight;
    // Canvas height: top margin + table + margin + all rows + gaps between rows + bottom margin
    const int canvasHeight = marginTop + tableHeight + tableMargin + rows * cellHeight + (rows - 1) * paddingY +
                             marginBottom;

    QImage collageImage(canvasWidth, canvasHeight, QImage::Format_ARGB32);
    collageImage.fill(Qt::white);

    QPainter collagePainter(&collageImage);

    // Draw description table background
    constexpr int tableX = marginLeft;
    constexpr int tableY = marginTop;
    const int tableWidth = canvasWidth - marginLeft - marginRight;

    collagePainter.setRenderHint(QPainter::Antialiasing);
    collagePainter.setPen(QPen(QColor(100, 100, 100), 1));
    collagePainter.setBrush(QColor(245, 245, 245));
    collagePainter.drawRoundedRect(tableX, tableY, tableWidth, tableHeight, tableRadius, tableRadius);

    // Draw table content
    collagePainter.setPen(Qt::black);
    collagePainter.setFont(QFont("Arial", 10));

    for (int i = 0; i < metaLines.size(); ++i) {
        constexpr int textX = tableX + tablePadding;
        const int textY = tableY + tablePadding + i * tableRowHeight + tableRowHeight / 2 + 5;
        collagePainter.drawText(textX, textY, metaLines[i]);
    }

    // Draw each image in a grid layout
    const int imagesStartY = marginTop + tableHeight + tableMargin;
    for (int i = 0; i < processedImages.size(); ++i) {
        const int row = i / cols;
        const int col = i % cols;
        // Position: margin + (cell size + gap) * index
        const int x = marginLeft + col * (cellWidth + paddingX);
        const int y = imagesStartY + row * (cellHeight + paddingY);

        // Draw the image centered in the cell
        const QImage &img = processedImages[i];
        const int imgX = x + (cellWidth - img.width()) / 2;
        const int imgY = y + (cellHeight - img.height()) / 2;

        collagePainter.drawImage(imgX, imgY, img);

        // Draw thin border around thumbnail
        collagePainter.setPen(QPen(QColor(180, 180, 180), 1));
        collagePainter.setBrush(Qt::NoBrush);
        collagePainter.drawRect(imgX, imgY, img.width(), img.height());
    }

    collagePainter.end();

#ifdef QT_DEBUG
    // Save collage image to disk for debugging
    if (collageImage.save("debug_collage.jpg")) {
        qDebug() << "Saved debug collage image: debug_collage.jpg";
    } else {
        qDebug() << "Failed to save debug collage image: debug_collage.jpg";
    }
#endif

    return collageImage;
}
