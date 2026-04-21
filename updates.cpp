#include "updates.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QVersionNumber>

#ifndef APP_VERSION
#define APP_VERSION "0.0.0"
#endif

namespace {
constexpr auto kGitHubReleasesUrl = "https://api.github.com/repos/akshaybabloo/gav/releases/latest";

QString stripVersionPrefix(QString tag) {
    if (tag.startsWith('v') || tag.startsWith('V'))
        tag.remove(0, 1);
    return tag;
}
} // namespace

Updates::Updates(QObject *parent) : QObject(parent) {}

void Updates::checkUpdates() {
    if (m_checking)
        return;
    m_checking = true;

    QNetworkRequest request{QUrl(kGitHubReleasesUrl)};
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", "gav-update-checker");
    request.setTransferTimeout(15000);

    QNetworkReply *reply = m_manager.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        m_checking = false;

        if (reply->error() != QNetworkReply::NoError) {
            emit checkFailed(reply->errorString());
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject()) {
            emit checkFailed(tr("Invalid response from update server"));
            return;
        }

        const QJsonObject obj = doc.object();
        const QString tag = obj.value("tag_name").toString();
        const QString htmlUrl = obj.value("html_url").toString();
        if (tag.isEmpty()) {
            emit checkFailed(tr("No release tag found"));
            return;
        }

        const QString currentStr = stripVersionPrefix(QString::fromLatin1(APP_VERSION));
        const QString latestStr = stripVersionPrefix(tag);
        const QVersionNumber current = QVersionNumber::fromString(currentStr);
        const QVersionNumber latest = QVersionNumber::fromString(latestStr);

        if (current.isNull() || latest.isNull()) {
            emit checkFailed(tr("Could not parse version: current=\"%1\", latest=\"%2\"")
                                 .arg(currentStr, latestStr));
            return;
        }

        if (latest > current) {
            emit updateAvailable(currentStr, latestStr, QUrl(htmlUrl));
        } else {
            emit upToDate(currentStr);
        }
    });
}
