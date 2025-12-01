#include "collage.h"
#include <QMediaPlayer>
#include <QMediaMetaData>
#include <QEventLoop>
#include <QTimer>
#include <QVideoSink>
#include <QVideoFrame>
#include <QDebug>
#include <QPainter>
#include <QDir>

Collage::Collage()
= default;

void Collage::toCollage(const QList<QUrl> &paths) {
    for (const auto &path: paths) {
        auto meta = collectImages(path);
        if (auto collageImage = drawCollage(meta); !collageImage.isNull()) {
            // Get the source file info
            QFileInfo sourceFile(path.toLocalFile());

            // Create output filename: originalname_collage.jpg
            QString outputFilename = sourceFile.completeBaseName() + "_collage.jpg";

            // Get the directory where the source file is located
            QString outputPath = sourceFile.dir().filePath(outputFilename);

            if (collageImage.save(outputPath, "JPEG", 100)) {
                qDebug() << "Collage saved to:" << outputPath;
            } else {
                qDebug() << "Failed to save collage to:" << outputPath;
            }
        } else {
            qDebug() << "Collage image is null for path:" << path.toString();
        }
    }
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
        .videoCodes = data.stringValue(QMediaMetaData::VideoCodec),
        .size = fileSize,
        .resolution = data.stringValue(QMediaMetaData::Resolution)
    };

    qDebug() << "Collected" << frames.size() << "frames from" << path.toString() << "Name:" << image.name
            << "Duration:" << image.duration << "Audio Codec:" << image.audioCodec
            << "Video Codec:" << image.videoCodes << "Size:" << image.size
            << "Resolution:" << image.resolution;

    return image;
}

QImage Collage::drawCollage(const ImageMeta &meta) {
    auto images = meta.image;
    if (images.isEmpty()) {
        qDebug() << "No images to draw collage.";
        return {};
    }

    QList<QImage> processedImages;

    for (const auto &img: images) {
        QImage resized = img.image.scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation);

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
        painter.setPen(Qt::yellow);
        painter.setFont(QFont("Arial", 10, QFont::Bold));
        painter.drawText(resized.rect().adjusted(2, 2, -2, -2), Qt::AlignBottom | Qt::AlignRight, timeText);
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
    constexpr int tableMargin = -10;
    constexpr int tableRowHeight = 15;
    constexpr int tablePadding = 2;

    // Prepare metadata text
    QStringList metaLines;
    metaLines << QString("Name: %1").arg(meta.name);
    metaLines << QString("Duration: %1").arg(meta.duration);
    metaLines << QString("Resolution: %1").arg(meta.resolution);
    metaLines << QString("Size: %1").arg(meta.size);
    metaLines << QString("Video Codec: %1").arg(meta.videoCodes);
    metaLines << QString("Audio Codec: %1").arg(meta.audioCodec);

    const int tableHeight = metaLines.size() * tableRowHeight + 2 * tablePadding;

    // Draw collage using QPainter instead of QTableWidget
    constexpr int cols = 4;
    const int rows = (processedImages.size() + cols - 1) / cols;
    constexpr int cellWidth = 256;
    constexpr int cellHeight = 256;
    constexpr int paddingX = 5; // Gap between columns
    constexpr int paddingY = -100; // Gap between rows
    constexpr int marginTop = 5; // Top margin
    constexpr int marginBottom = 5; // Bottom margin
    constexpr int marginLeft = 5; // Left margin
    constexpr int marginRight = 5; // Right margin

    // Canvas width: left margin + all columns + gaps between columns + right margin
    constexpr int canvasWidth = marginLeft + cols * cellWidth + (cols - 1) * paddingX + marginRight;
    // Canvas height: top margin + table + margin + all rows + gaps between rows + bottom margin
    const int canvasHeight = marginTop + tableHeight + tableMargin + rows * cellHeight + (rows - 1) * paddingY +
                             marginBottom;

    QImage collageImage(canvasWidth, canvasHeight, QImage::Format_ARGB32);
    collageImage.fill(Qt::white);

    QPainter collagePainter(&collageImage);

    // Draw description table background
    constexpr int tableX = marginLeft;
    constexpr int tableY = marginTop;
    constexpr int tableWidth = canvasWidth - marginLeft - marginRight;

    collagePainter.setPen(QPen(Qt::black, 2));
    collagePainter.setBrush(QColor(240, 240, 240));
    collagePainter.drawRect(tableX, tableY, tableWidth, tableHeight);

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
