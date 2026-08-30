#include "buildinfo.h"

#ifdef GAV_HAS_FFVERSION_HEADER
extern "C" {
#include <libavutil/ffversion.h>
}
#endif

BuildInfo::BuildInfo(QObject *parent) : QObject(parent) {
}

QString BuildInfo::qtVersion() {
    return QString::fromLatin1(qVersion());
}

QString BuildInfo::ffmpegVersion() {
#ifdef FFMPEG_VERSION
    return QStringLiteral(FFMPEG_VERSION);
#else
    return {};
#endif
}

bool BuildInfo::isDebugBuild() {
#ifdef QT_DEBUG
    return true;
#else
    return false;
#endif
}
