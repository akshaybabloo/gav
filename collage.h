#ifndef COLLAGE_H
#define COLLAGE_H

#include <QQuickItem>
#include <QImage>
#include <QFutureWatcher>
#include <QThreadPool>
#include <QProcess>

struct ImageTime {
    QImage image;
    qint64 timestamp;
};

struct ImageMeta
{
    QList<ImageTime> image;
    QString name;
    QString duration;
    QString audioCodec;
    QString videoCodec;
    QString size;
    QString resolution;
};

// Result of processing a single video
struct CollageResult {
    int index;
    QString inputPath;
    QString outputPath;
    bool success;
};

class Collage : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
public:
    explicit Collage();
    ~Collage() override;

    Q_INVOKABLE void toCollage(const QList<QUrl>& paths);

    // Static method for CLI mode - processes single file synchronously
    static QString createCollageSingle(const QUrl& path);

    // Maximum concurrent collage operations (separate processes)
    static constexpr int MAX_CONCURRENT = 4;

private:
    static ImageMeta collectImages(const QUrl& path);
    static QImage drawCollage(const ImageMeta& meta);

    // Process video via external process
    static CollageResult processVideoExternal(int index, const QUrl& path);

    void onFutureFinished();

    QThreadPool m_threadPool;
    QFutureWatcher<CollageResult> m_watcher;
    QList<QUrl> m_currentPaths;

signals:
    void collageStarted(int total);
    void collageProgress(int index, const QString& inputPath);
    void collageCompleted(int index, const QString& outputPath, bool success);
    void collageFinished(int successCount, int failCount);
};

#endif // COLLAGE_H
