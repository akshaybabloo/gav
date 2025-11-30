#include "collage.h"
#include <QMediaPlayer>
#include <QMediaMetaData>
#include <QEventLoop>
#include <QTimer>
#include <QVideoSink>
#include <QVideoFrame>
#include <QFileInfo>
#include <QDebug>
#include <iostream>

Collage::Collage()
= default;

void Collage::toCollage(const QList<QUrl> &paths) {
    for (const auto &path: paths) {
        collectImages(path);
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
        int numFrames = qMin(10, static_cast<int>(duration / 1000) + 1); // At least 1 frame, up to 10
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
