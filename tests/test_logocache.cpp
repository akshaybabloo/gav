#include <gtest/gtest.h>
#include "../logoprovider.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QRandomGenerator>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>

namespace {

QImage noise(int side, quint32 seed) {
    QRandomGenerator random(seed);
    QImage image(side, side, QImage::Format_ARGB32);
    for (int y = 0; y < side; ++y) {
        for (int x = 0; x < side; ++x) {
            image.setPixel(x, y, random.generate() | 0xff000000);
        }
    }
    return image;
}

QByteArray encoded(const QImage &image, const char *format) {
    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    EXPECT_TRUE(image.save(&buffer, format));
    return data;
}

class LogoServer : public QTcpServer {
public:
    LogoServer() {
        EXPECT_TRUE(listen(QHostAddress::LocalHost));
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket *socket = nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, this, [this, socket] { handle(socket); });
            }
        });
    }

    QUrl url(const QString &path) const { return QUrl(QStringLiteral("http://127.0.0.1:%1%2").arg(serverPort()).arg(path)); }

    void release() {
        const QList<QTcpSocket *> sockets = held;
        held.clear();
        for (QTcpSocket *socket : sockets) {
            respond(socket, "200 OK", "image/png", png);
        }
    }

    QByteArray png = encoded(noise(200, 7), "PNG");
    QList<QTcpSocket *> held;
    int requests = 0;
    int mostHeld = 0;

private:
    void respond(QTcpSocket *socket, const QByteArray &status, const QByteArray &type, const QByteArray &body, const QByteArray &headers = {}) {
        socket->write("HTTP/1.1 " + status + "\r\nContent-Type: " + type + "\r\nContent-Length: " + QByteArray::number(body.size()) +
                      "\r\nConnection: close\r\n" + headers + "\r\n" + body);
        socket->disconnectFromHost();
    }

    void handle(QTcpSocket *socket) {
        const QByteArray received = socket->property("received").toByteArray() + socket->readAll();
        socket->setProperty("received", received);
        if (!received.contains("\r\n\r\n") || socket->property("handled").toBool()) {
            return;
        }
        socket->setProperty("handled", true);
        ++requests;
        const QByteArray path = received.split(' ').value(1);
        if (path.startsWith("/slow")) {
            held.append(socket);
            mostHeld = qMax(mostHeld, int(held.size()));
        } else if (path == "/ok.png") {
            respond(socket, "200 OK", "image/png", png);
        } else if (path == "/text") {
            respond(socket, "200 OK", "text/html", "<html><body>Not a logo</body></html>");
        } else if (path == "/big.png") {
            respond(socket, "200 OK", "image/png", QByteArray(600 * 1024, 'x'));
        } else if (path == "/big-stream") {
            socket->write("HTTP/1.1 200 OK\r\nContent-Type: image/png\r\nConnection: close\r\n\r\n" + QByteArray(600 * 1024, 'x'));
            socket->disconnectFromHost();
        } else if (path == "/to-file") {
            respond(socket, "302 Found", "text/plain", "", "Location: file:///etc/passwd\r\n");
        } else {
            respond(socket, "404 Not Found", "text/plain", "Not found");
        }
    }
};

class LogoProviderTest : public ::testing::Test {
protected:
    QString directory() const { return QDir(tempDir.path()).filePath("logos"); }

    bool waitFor(const QSignalSpy &spy, int count) {
        for (int i = 0; i < 100 && spy.count() < count; ++i) {
            QTest::qWait(50);
        }
        return spy.count() >= count;
    }

    QTemporaryDir tempDir;
    LogoServer server;
};

class LogoCacheTest : public ::testing::Test {
protected:
    QString directory() const { return QDir(tempDir.path()).filePath("logos"); }

    QStringList files() const { return QDir(directory()).entryList(QDir::Files, QDir::Name); }

    void setAge(const QUrl &address, int secondsAgo) {
        QFile file(QDir(directory()).filePath(LogoCache::fileName(address)));
        ASSERT_TRUE(file.open(QIODevice::ReadWrite));
        ASSERT_TRUE(file.setFileTime(QDateTime::currentDateTimeUtc().addSecs(-secondsAgo), QFileDevice::FileModificationTime));
    }

    QTemporaryDir tempDir;
    const QUrl news{"https://logos.example.org/channels/news.png?size=large"};
    const QUrl sport{"http://logos.example.org/channels/sport.png"};
    const QUrl films{"https://logos.example.org/channels/films.png"};
    const QUrl music{"https://logos.example.org/channels/music.png"};
};

}

TEST_F(LogoCacheTest, AcceptsOnlyWebAddresses) {
    EXPECT_TRUE(LogoCache::accepts(QUrl("http://example.org/a.png")));
    EXPECT_TRUE(LogoCache::accepts(QUrl("HTTPS://example.org/a.png")));
    EXPECT_FALSE(LogoCache::accepts(QUrl("file:///etc/passwd")));
    EXPECT_FALSE(LogoCache::accepts(QUrl("ftp://example.org/a.png")));
    EXPECT_FALSE(LogoCache::accepts(QUrl("data:image/png;base64,AAAA")));
    EXPECT_FALSE(LogoCache::accepts(QUrl("qrc:/assets/images/icon.png")));
    EXPECT_FALSE(LogoCache::accepts(QUrl("logos/a.png")));
    EXPECT_FALSE(LogoCache::accepts(QUrl("https:///a.png")));
    EXPECT_FALSE(LogoCache::accepts(QUrl()));
}

TEST_F(LogoCacheTest, FileNameIsTheSha256OfTheAddress) {
    const QString name = LogoCache::fileName(news);
    const QByteArray hash = QCryptographicHash::hash(news.toString(QUrl::FullyEncoded).toUtf8(), QCryptographicHash::Sha256).toHex();
    EXPECT_EQ(name, QString::fromLatin1(hash) + ".png");
    EXPECT_EQ(name.size(), 68);
    EXPECT_NE(name, LogoCache::fileName(sport));
    EXPECT_TRUE(LogoCache::fileName(QUrl("file:///etc/passwd")).isEmpty());
}

TEST_F(LogoCacheTest, StoresAndFindsAnImage) {
    LogoCache cache(directory());
    EXPECT_TRUE(cache.lookup(news).isNull());

    const QImage image = noise(16, 1);
    ASSERT_TRUE(cache.store(news, image));
    EXPECT_EQ(files(), QStringList{LogoCache::fileName(news)});

    const QImage found = cache.lookup(news);
    ASSERT_FALSE(found.isNull());
    EXPECT_EQ(found.size(), image.size());
    EXPECT_EQ(found.pixel(3, 5), image.pixel(3, 5));
    EXPECT_TRUE(cache.lookup(sport).isNull());
}

TEST_F(LogoCacheTest, RefusesAddressesAndImagesItCannotKeep) {
    LogoCache cache(directory());
    EXPECT_FALSE(cache.store(QUrl("file:///etc/passwd"), noise(16, 1)));
    EXPECT_FALSE(cache.store(news, QImage()));
    EXPECT_TRUE(cache.lookup(QUrl("file:///etc/passwd")).isNull());
    EXPECT_TRUE(files().isEmpty());
}

TEST_F(LogoCacheTest, RemovesTheOldestFilesPastTheLimit) {
    const qint64 one = encoded(noise(64, 1), "PNG").size();
    LogoCache cache(directory(), one * 5 / 2);

    ASSERT_TRUE(cache.store(news, noise(64, 1)));
    setAge(news, 300);
    ASSERT_TRUE(cache.store(sport, noise(64, 2)));
    setAge(sport, 200);
    EXPECT_EQ(files().size(), 2);

    ASSERT_TRUE(cache.store(films, noise(64, 3)));
    EXPECT_TRUE(cache.lookup(news).isNull());
    EXPECT_FALSE(cache.lookup(sport).isNull());
    EXPECT_FALSE(cache.lookup(films).isNull());
    EXPECT_LE(cache.totalBytes(), one * 5 / 2);

    setAge(films, 100);
    ASSERT_TRUE(cache.store(music, noise(64, 4)));
    EXPECT_TRUE(cache.lookup(sport).isNull());
    EXPECT_FALSE(cache.lookup(films).isNull());
    EXPECT_FALSE(cache.lookup(music).isNull());
}

TEST_F(LogoCacheTest, ClearEmptiesTheDirectory) {
    LogoCache cache(directory());
    ASSERT_TRUE(cache.store(news, noise(16, 1)));
    ASSERT_TRUE(cache.store(sport, noise(16, 2)));
    EXPECT_EQ(files().size(), 2);
    EXPECT_GT(cache.totalBytes(), 0);

    cache.clear();
    EXPECT_TRUE(files().isEmpty());
    EXPECT_EQ(cache.totalBytes(), 0);
    EXPECT_TRUE(cache.lookup(news).isNull());
    ASSERT_TRUE(cache.store(news, noise(16, 1)));
    EXPECT_EQ(files().size(), 1);
}

TEST_F(LogoCacheTest, StoredFilesDoNotNameTheAddress) {
    LogoCache cache(directory());
    QImage image = noise(16, 1);
    image.setText("Source", news.toString());
    image.setText("Description", "logos.example.org");
    ASSERT_TRUE(cache.store(news, image));

    const QStringList names = files();
    ASSERT_EQ(names.size(), 1);
    EXPECT_FALSE(names[0].contains("example"));
    EXPECT_FALSE(names[0].contains("news"));

    QFile file(QDir(directory()).filePath(names[0]));
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    const QByteArray content = file.readAll();
    EXPECT_FALSE(content.contains("example.org"));
    EXPECT_FALSE(content.contains("news.png"));
    EXPECT_TRUE(cache.lookup(news).textKeys().isEmpty());
}

TEST(LogoDecodeTest, AcceptsCommonStillImagesAndScalesThemDown) {
    const QImage png = LogoCache::decode(encoded(noise(300, 1), "PNG"));
    ASSERT_FALSE(png.isNull());
    EXPECT_EQ(png.size(), QSize(LogoCache::storedSide, LogoCache::storedSide));

    QImage wide(400, 100, QImage::Format_RGB32);
    wide.fill(Qt::red);
    const QImage jpeg = LogoCache::decode(encoded(wide, "JPG"));
    ASSERT_FALSE(jpeg.isNull());
    EXPECT_EQ(jpeg.size(), QSize(LogoCache::storedSide, LogoCache::storedSide / 4));

    const QImage small = LogoCache::decode(encoded(noise(20, 2), "PNG"));
    ASSERT_FALSE(small.isNull());
    EXPECT_EQ(small.size(), QSize(20, 20));
}

TEST(LogoDecodeTest, RejectsOtherFormatsLargeImagesAndGarbage) {
    EXPECT_TRUE(LogoCache::decode(encoded(noise(32, 1), "BMP")).isNull());
    EXPECT_TRUE(LogoCache::decode("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"10\" height=\"10\"/>").isNull());
    EXPECT_TRUE(LogoCache::decode("GIF89a\x01\x00\x01\x00\x00\x00\x00;").isNull());
    EXPECT_TRUE(LogoCache::decode("<html><body>Not found</body></html>").isNull());
    EXPECT_TRUE(LogoCache::decode(QByteArray()).isNull());

    QImage tall(8, LogoCache::maxSourceSide + 1, QImage::Format_RGB32);
    tall.fill(Qt::blue);
    EXPECT_TRUE(LogoCache::decode(encoded(tall, "PNG")).isNull());

    QImage largest(LogoCache::maxSourceSide, 8, QImage::Format_RGB32);
    largest.fill(Qt::blue);
    EXPECT_FALSE(LogoCache::decode(encoded(largest, "PNG")).isNull());

    const QByteArray truncated = encoded(noise(64, 1), "PNG").left(200);
    EXPECT_TRUE(LogoCache::decode(truncated).isNull());
}

TEST_F(LogoProviderTest, DownloadsDecodesAndCachesALogo) {
    LogoProvider provider(directory());
    QSignalSpy finished(&provider, &LogoProvider::finished);
    const QUrl address = server.url("/ok.png");

    provider.request(1, address);
    provider.request(2, address);
    ASSERT_TRUE(waitFor(finished, 2));
    EXPECT_EQ(server.requests, 1);
    for (const QList<QVariant> &arguments : finished) {
        const QImage image = arguments[1].value<QImage>();
        EXPECT_EQ(image.size(), QSize(LogoCache::storedSide, LogoCache::storedSide));
    }
    EXPECT_FALSE(provider.cache()->lookup(address).isNull());
    EXPECT_EQ(provider.pendingCount(), 0);
}

TEST_F(LogoProviderTest, FailuresGiveAnEmptyImageAndCacheNothing) {
    LogoProvider provider(directory());
    QSignalSpy finished(&provider, &LogoProvider::finished);
    const QStringList paths{"/missing.png", "/text", "/big.png", "/big-stream", "/to-file"};
    for (qsizetype i = 0; i < paths.size(); ++i) {
        provider.request(quint64(i + 1), server.url(paths[i]));
    }
    provider.request(99, QUrl("file:///etc/passwd"));
    ASSERT_TRUE(waitFor(finished, int(paths.size()) + 1));
    for (const QList<QVariant> &arguments : finished) {
        EXPECT_TRUE(arguments[1].value<QImage>().isNull()) << arguments[0].toULongLong();
    }
    EXPECT_EQ(server.requests, paths.size());
    EXPECT_EQ(provider.cache()->totalBytes(), 0);
    EXPECT_EQ(provider.pendingCount(), 0);
}

TEST_F(LogoProviderTest, KeepsAtMostFourDownloadsInFlight) {
    LogoProvider provider(directory());
    QSignalSpy finished(&provider, &LogoProvider::finished);
    for (int i = 0; i < 10; ++i) {
        provider.request(quint64(i + 1), server.url(QStringLiteral("/slow/%1.png").arg(i)));
    }
    QTest::qWait(300);
    EXPECT_EQ(server.held.size(), LogoProvider::maxInFlight);
    EXPECT_EQ(finished.count(), 0);

    for (int round = 0; round < 20 && finished.count() < 10; ++round) {
        server.release();
        QTest::qWait(100);
    }
    EXPECT_EQ(finished.count(), 10);
    EXPECT_EQ(server.requests, 10);
    EXPECT_LE(server.mostHeld, LogoProvider::maxInFlight);
}

TEST_F(LogoProviderTest, CancelPendingStopsQueuedAndRunningDownloads) {
    LogoProvider provider(directory());
    QSignalSpy finished(&provider, &LogoProvider::finished);
    for (int i = 0; i < 7; ++i) {
        provider.request(quint64(i + 1), server.url(QStringLiteral("/slow/%1.png").arg(i)));
    }
    QTest::qWait(300);
    ASSERT_EQ(server.requests, LogoProvider::maxInFlight);

    provider.cancelPending();
    EXPECT_EQ(finished.count(), 7);
    for (const QList<QVariant> &arguments : finished) {
        EXPECT_TRUE(arguments[1].value<QImage>().isNull());
    }
    EXPECT_EQ(provider.pendingCount(), 0);

    server.release();
    QTest::qWait(300);
    EXPECT_EQ(server.requests, LogoProvider::maxInFlight);
    EXPECT_EQ(finished.count(), 7);
    EXPECT_EQ(provider.cache()->totalBytes(), 0);
}

TEST_F(LogoProviderTest, CancellingARowDropsItsDownload) {
    LogoProvider provider(directory());
    QSignalSpy finished(&provider, &LogoProvider::finished);
    for (int i = 0; i < 5; ++i) {
        provider.request(quint64(i + 1), server.url(QStringLiteral("/slow/%1.png").arg(i)));
    }
    QTest::qWait(300);
    ASSERT_EQ(server.requests, LogoProvider::maxInFlight);

    provider.cancel(5);
    provider.cancel(1);
    QTest::qWait(300);
    EXPECT_EQ(provider.pendingCount(), 3);
    EXPECT_EQ(server.requests, LogoProvider::maxInFlight);

    server.release();
    ASSERT_TRUE(waitFor(finished, 3));
    QTest::qWait(200);
    EXPECT_EQ(finished.count(), 3);
    EXPECT_EQ(server.requests, LogoProvider::maxInFlight);
    EXPECT_EQ(provider.pendingCount(), 0);
}
