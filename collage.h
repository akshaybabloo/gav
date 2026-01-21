#ifndef COLLAGE_H
#define COLLAGE_H

#include <QQuickItem>
#include <QImage>
#include <QFutureWatcher>
#include <QThreadPool>
#include <QProcess>
#include <atomic>

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

    /**
     * Process a video file via an external subprocess for complete isolation.
     *
     * This method spawns the application as a subprocess with --collage argument
     * to create a collage for a single video. This isolation prevents heavy media
     * processing from affecting the main application's stability.
     *
     * Protocol:
     *   - Sets GAV_SUBPROCESS=1 environment variable to suppress spinner output
     *   - Expects subprocess stdout: "SUCCESS:<output_path>" or "FAILED:<input_path>"
     *   - Times out after PROCESS_TIMEOUT_MS (default 5 minutes)
     *
     * @param index The index of this video in the batch (for progress tracking)
     * @param path The URL of the video file to process
     * @return CollageResult containing success status and output path
     */
    CollageResult processVideoExternal(int index, const QUrl& path);

    void onFutureFinished();

    QThreadPool m_threadPool;
    QFutureWatcher<CollageResult> m_watcher;
    QList<QUrl> m_currentPaths;
    std::atomic<bool> m_shuttingDown{false};

signals:
    void collageStarted(int total);
    void collageProgress(int index, const QString& inputPath);
    void collageCompleted(int index, const QString& outputPath, bool success);
    void collageFinished(int successCount, int failCount);
};

#endif // COLLAGE_H
