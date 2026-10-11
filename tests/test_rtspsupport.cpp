#include <gtest/gtest.h>

#include <QByteArray>
#include <QSet>

extern "C" {
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
}

TEST(RtspSupportTest, TheLinkedFfmpegHasTheRtspDemuxer) { EXPECT_NE(av_find_input_format("rtsp"), nullptr); }

TEST(RtspSupportTest, TheLinkedFfmpegHasTheProtocolsRtspNeeds) {
    QSet<QByteArray> protocols;
    void *iterator = nullptr;
    while (const char *name = avio_enum_protocols(&iterator, 0)) {
        protocols.insert(name);
    }
    for (const char *needed : {"tcp", "udp", "rtp", "tls"}) {
        EXPECT_TRUE(protocols.contains(needed)) << needed;
    }
}
