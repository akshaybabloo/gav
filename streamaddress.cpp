#include "streamaddress.h"

#include <QRegularExpression>

namespace {

const QLatin1String rtspScheme("rtsp");
const QLatin1String secureRtspScheme("rtsps");
constexpr int rtspPort = 554;
constexpr int secureRtspPort = 322;

} // namespace

QStringList StreamAddress::schemes() { return {QStringLiteral("http"), QStringLiteral("https"), rtspScheme, secureRtspScheme}; }

bool StreamAddress::isStream(const QUrl &url) { return url.isValid() && !url.host().isEmpty() && schemes().contains(url.scheme().toLower()); }

bool StreamAddress::isRtsp(const QUrl &url) {
    const QString scheme = url.scheme().toLower();
    return scheme == rtspScheme || scheme == secureRtspScheme;
}

bool StreamAddress::isSecureRtsp(const QUrl &url) { return url.scheme().toLower() == secureRtspScheme; }

bool StreamAddress::hasPassword(const QUrl &url) { return !url.password().isEmpty(); }

QUrl StreamAddress::displayAddress(const QUrl &url) { return url.userInfo().isEmpty() ? url : url.adjusted(QUrl::RemoveUserInfo); }

QString StreamAddress::displayText(const QString &pathOrUrl) {
    const QUrl url(pathOrUrl);
    return url.userInfo().isEmpty() || url.host().isEmpty() ? pathOrUrl : url.adjusted(QUrl::RemoveUserInfo).toString();
}

QString StreamAddress::sourceKey(const QUrl &url) {
    const int port = url.port(isSecureRtsp(url) ? secureRtspPort : rtspPort);
    return url.host().toLower() + QLatin1Char(':') + QString::number(port);
}

QString StreamAddress::withoutCredentials(const QString &text) {
    static const QRegularExpression credentials(QStringLiteral(R"(([A-Za-z][A-Za-z0-9+.\-]*://)[^/?#\s]*@)"));
    QString result = text;
    return result.replace(credentials, QStringLiteral("\\1"));
}
