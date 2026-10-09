#ifndef STREAMADDRESS_H
#define STREAMADDRESS_H

#include <QString>
#include <QStringList>
#include <QUrl>

namespace StreamAddress {

QStringList schemes();
bool isStream(const QUrl &url);
bool isRtsp(const QUrl &url);
bool isSecureRtsp(const QUrl &url);
bool hasPassword(const QUrl &url);
QUrl displayAddress(const QUrl &url);
QString displayText(const QString &pathOrUrl);
QString sourceKey(const QUrl &url);
QString withoutCredentials(const QString &text);

} // namespace StreamAddress

#endif // STREAMADDRESS_H
