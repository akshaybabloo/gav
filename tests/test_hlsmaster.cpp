#include <gtest/gtest.h>
#include "../hlsmaster.h"

namespace {

const QUrl base("https://example.org/live/master.m3u8");

const QByteArray muxedMaster = "#EXTM3U\n"
                               "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=288000,RESOLUTION=256x144\n"
                               "low/144p.m3u8\n"
                               "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=5400000,CODECS=\"avc1.640028,mp4a.40.2\",RESOLUTION=1920x1080\n"
                               "fullhd/1080p.m3u8\n"
                               "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=1366000,RESOLUTION=854x480\n"
                               "https://cdn.example.org/480p.m3u8\n"
                               "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=2742000,CODECS=\"avc1.4d001f,mp4a.40.2\",RESOLUTION=1280x720\n"
                               "hdready/720p.m3u8\n"
                               "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=10768000,RESOLUTION=3840x2160\n"
                               "4k/2160p.m3u8\n";

QStringList labels(const QList<HlsVariant> &variants) {
    QStringList result;
    for (const HlsVariant &variant : variants) {
        result.append(variant.label);
    }
    return result;
}

}

TEST(HlsMasterTest, ListsVariantsBestFirstWithResolvedUrls) {
    const HlsMaster master = Hls::parseMaster(muxedMaster, base);
    EXPECT_FALSE(master.separateAudio);
    EXPECT_EQ(labels(master.variants), QStringList({"2160p", "1080p", "720p", "480p", "144p"}));
    EXPECT_EQ(master.variants[1].url, QUrl("https://example.org/live/fullhd/1080p.m3u8"));
    EXPECT_EQ(master.variants[1].bandwidth, 5400000);
    EXPECT_EQ(master.variants[3].url, QUrl("https://cdn.example.org/480p.m3u8"));
}

TEST(HlsMasterTest, MediaPlaylistHasNoVariants) {
    const QByteArray media = "#EXTM3U\n#EXT-X-TARGETDURATION:10\n#EXTINF:9.6,\nsegment0.ts\n#EXTINF:9.6,\nsegment1.ts\n";
    EXPECT_TRUE(Hls::parseMaster(media, base).variants.isEmpty());
    EXPECT_EQ(Hls::firstSegment(media, base), QUrl("https://example.org/live/segment0.ts"));
    EXPECT_TRUE(Hls::firstSegment("#EXTM3U\n#EXT-X-ENDLIST\n", base).isEmpty());
}

TEST(HlsMasterTest, DetectsSeparateAudioRenditions) {
    const QByteArray data = "#EXTM3U\r\n"
                            "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"aud1\",NAME=\"English, stereo\",URI=\"a1/prog_index.m3u8\"\r\n"
                            "#EXT-X-MEDIA:TYPE=CLOSED-CAPTIONS,GROUP-ID=\"cc1\",INSTREAM-ID=\"CC1\"\r\n"
                            "#EXT-X-STREAM-INF:BANDWIDTH=2177116,RESOLUTION=960x540,AUDIO=\"aud1\"\r\n"
                            "v5/prog_index.m3u8\r\n"
                            "#EXT-X-STREAM-INF:BANDWIDTH=8001098,RESOLUTION=1920x1080,AUDIO=\"aud1\"\r\n"
                            "v9/prog_index.m3u8\r\n";
    const HlsMaster master = Hls::parseMaster(data, base);
    EXPECT_TRUE(master.separateAudio);
    EXPECT_EQ(master.variants.size(), 2);

    const QByteArray muxedGroup = "#EXTM3U\n"
                                  "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"aud1\",NAME=\"English\"\n"
                                  "#EXT-X-STREAM-INF:BANDWIDTH=2177116,RESOLUTION=960x540,AUDIO=\"aud1\"\n"
                                  "v5/prog_index.m3u8\n";
    EXPECT_FALSE(Hls::parseMaster(muxedGroup, base).separateAudio);
}

TEST(HlsMasterTest, KeepsEveryVideoRenditionAndTellsSameResolutionsApart) {
    const QByteArray data = "#EXTM3U\n"
                            "#EXT-X-STREAM-INF:BANDWIDTH=6312875,RESOLUTION=1920x1080\n"
                            "v8.m3u8\n"
                            "#EXT-X-STREAM-INF:BANDWIDTH=8001098,RESOLUTION=1920x1080\n"
                            "v9.m3u8\n"
                            "#EXT-X-STREAM-INF:BANDWIDTH=128000,CODECS=\"mp4a.40.2\"\n"
                            "audio.m3u8\n"
                            "#EXT-X-I-FRAME-STREAM-INF:BANDWIDTH=90000,RESOLUTION=1920x1080,URI=\"iframes.m3u8\"\n"
                            "#EXT-X-STREAM-INF:BANDWIDTH=2177116,RESOLUTION=960x540\n"
                            "v5.m3u8\n";
    const HlsMaster master = Hls::parseMaster(data, base);
    EXPECT_EQ(labels(master.variants), QStringList({QString::fromUtf8("1080p · 8.0 Mbps"), QString::fromUtf8("1080p · 6.3 Mbps"), "540p", "128 kbps"}));
    EXPECT_EQ(master.variants[0].url, QUrl("https://example.org/live/v9.m3u8"));
    EXPECT_TRUE(master.audioFormats.isEmpty());
    EXPECT_EQ(Hls::mediumIndex(master.variants), 1);
    EXPECT_EQ(Hls::indexForBitrate(master.variants, 100000, 0), 2);
    EXPECT_EQ(Hls::playback(master, 3, -1).url, QUrl("https://example.org/live/audio.m3u8"));
    EXPECT_TRUE(Hls::playback(master, 3, -1).manifest.isEmpty());
    EXPECT_TRUE(Hls::playback(master, 9, -1).url.isEmpty());
}

TEST(HlsMasterTest, MediumIsTheMiddleVideoVariant) {
    const QList<HlsVariant> variants = Hls::parseMaster(muxedMaster, base).variants;
    EXPECT_EQ(variants[Hls::mediumIndex(variants)].label, "720p");
    EXPECT_EQ(Hls::mediumIndex({}), -1);
}

TEST(HlsMasterTest, PicksTheBestVariantTheConnectionAndScreenAllow) {
    const QList<HlsVariant> variants = Hls::parseMaster(muxedMaster, base).variants;
    const auto pick = [&variants](qint64 bitsPerSecond, int maxHeight) { return variants[Hls::indexForBitrate(variants, bitsPerSecond, maxHeight)].label; };
    EXPECT_EQ(pick(50000000, 0), "2160p");
    EXPECT_EQ(pick(50000000, 1080), "1080p");
    EXPECT_EQ(pick(50000000, 100), "144p");
    EXPECT_EQ(pick(8100000, 0), "1080p");
    EXPECT_EQ(pick(8000000, 0), "720p");
    EXPECT_EQ(pick(2100000, 0), "480p");
    EXPECT_EQ(pick(100000, 0), "144p");
    EXPECT_EQ(pick(0, 0), "720p");
}

TEST(HlsMasterTest, ListsSubtitleRenditionsWithHttpAddresses) {
    const QByteArray data = "#EXTM3U\n"
                            "#EXT-X-MEDIA:TYPE=SUBTITLES,GROUP-ID=\"sub1\",LANGUAGE=\"en\",NAME=\"English\",AUTOSELECT=YES,DEFAULT=YES,FORCED=NO,URI=\"s1/en/prog_index.m3u8\"\n"
                            "#EXT-X-MEDIA:TYPE=SUBTITLES,GROUP-ID=\"sub2\",LANGUAGE=\"en\",NAME=\"English\",URI=\"s1/en/prog_index.m3u8\"\n"
                            "#EXT-X-MEDIA:TYPE=SUBTITLES,GROUP-ID=\"sub1\",LANGUAGE=\"fr\",NAME=\"Français, forcé\",FORCED=YES,URI=\"https://cdn.example.org/fr.m3u8\"\n"
                            "#EXT-X-MEDIA:TYPE=SUBTITLES,GROUP-ID=\"sub1\",LANGUAGE=\"de\",NAME=\"Local\",URI=\"file:///etc/passwd\"\n"
                            "#EXT-X-MEDIA:TYPE=CLOSED-CAPTIONS,GROUP-ID=\"cc1\",LANGUAGE=\"en\",NAME=\"English\",INSTREAM-ID=\"CC1\"\n"
                            "#EXT-X-STREAM-INF:BANDWIDTH=2177116,RESOLUTION=960x540,SUBTITLES=\"sub1\"\n"
                            "v5/prog_index.m3u8\n"
                            "#EXT-X-STREAM-INF:BANDWIDTH=900000,RESOLUTION=640x360\n"
                            "file:///home/user/video.m3u8\n";
    const HlsMaster master = Hls::parseMaster(data, base);
    ASSERT_EQ(master.subtitles.size(), 2);
    EXPECT_EQ(master.subtitles[0].url, QUrl("https://example.org/live/s1/en/prog_index.m3u8"));
    EXPECT_EQ(master.subtitles[0].language, "en");
    EXPECT_EQ(master.subtitles[0].name, "English");
    EXPECT_TRUE(master.subtitles[0].isDefault);
    EXPECT_FALSE(master.subtitles[0].forced);
    EXPECT_EQ(master.subtitles[1].url, QUrl("https://cdn.example.org/fr.m3u8"));
    EXPECT_EQ(master.subtitles[1].name, QString::fromUtf8("Français, forcé"));
    EXPECT_TRUE(master.subtitles[1].forced);
    ASSERT_EQ(master.variants.size(), 1);
    EXPECT_EQ(master.variants[0].label, "540p");
}

TEST(HlsMasterTest, AlternateAudioDoesNotCountAsSeparateWhenTheDefaultIsMuxed) {
    const QByteArray data = "#EXTM3U\n"
                            "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"aud\",NAME=\"Main\",DEFAULT=YES\n"
                            "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"aud\",NAME=\"Alternate\",DEFAULT=NO,URI=\"alt/prog_index.m3u8\"\n"
                            "#EXT-X-STREAM-INF:BANDWIDTH=263851,RESOLUTION=416x234,AUDIO=\"aud\"\n"
                            "gear1/prog_index.m3u8\n"
                            "#EXT-X-STREAM-INF:BANDWIDTH=577610,RESOLUTION=640x360,AUDIO=\"aud\"\n"
                            "gear2/prog_index.m3u8\n";
    EXPECT_FALSE(Hls::parseMaster(data, base).separateAudio);

    QByteArray defaultSeparate = data;
    defaultSeparate.replace("NAME=\"Main\",DEFAULT=YES", "NAME=\"Main\",DEFAULT=YES,URI=\"main/prog_index.m3u8\"");
    EXPECT_TRUE(Hls::parseMaster(defaultSeparate, base).separateAudio);
}

TEST(HlsMasterTest, SeparateAudioPlaysThroughAOneQualityPlaylist) {
    const QByteArray data = "#EXTM3U\n"
                            "#EXT-X-STREAM-INF:BANDWIDTH=8001098,CODECS=\"avc1.64002a,mp4a.40.2\",RESOLUTION=1920x1080,AUDIO=\"aac\",SUBTITLES=\"subs\"\n"
                            "v9/prog_index.m3u8\n"
                            "#EXT-X-STREAM-INF:BANDWIDTH=2177116,CODECS=\"avc1.640020,mp4a.40.2\",RESOLUTION=960x540,AUDIO=\"aac\"\n"
                            "v5/prog_index.m3u8?token=a b\n"
                            "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"aac\",NAME=\"Deutsch\",LANGUAGE=\"de\",DEFAULT=NO,URI=\"https://cdn.example.org/de.m3u8\"\n"
                            "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"aac\",NAME=\"English\",LANGUAGE=\"en\",DEFAULT=YES,URI=\"a1/prog_index.m3u8\"\n"
                            "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"aac\",NAME=\"Local\",DEFAULT=NO,URI=\"file:///etc/passwd\"\n"
                            "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"ec3\",NAME=\"English\",DEFAULT=YES,URI=\"a3/prog_index.m3u8\"\n";
    const HlsMaster master = Hls::parseMaster(data, base);
    ASSERT_EQ(master.variants.size(), 2);
    EXPECT_TRUE(master.separateAudio);
    ASSERT_EQ(master.audioFormats.size(), 1);
    EXPECT_EQ(master.audioFormats[0].label, "AAC");

    const HlsPlayback low = Hls::playback(master, 1, 0);
    EXPECT_EQ(low.bandwidth, 2177116);
    EXPECT_EQ(low.manifest,
              QByteArray("#EXTM3U\n"
                         "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"aac\",NAME=\"English\",LANGUAGE=\"en\",DEFAULT=YES,URI=\"https://example.org/live/a1/prog_index.m3u8\"\n"
                         "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"aac\",NAME=\"Deutsch\",LANGUAGE=\"de\",DEFAULT=NO,URI=\"https://cdn.example.org/de.m3u8\"\n"
                         "#EXT-X-STREAM-INF:BANDWIDTH=2177116,CODECS=\"avc1.640020,mp4a.40.2\",RESOLUTION=960x540,AUDIO=\"aac\"\n"
                         "https://example.org/live/v5/prog_index.m3u8?token=a%20b\n"));
    const HlsPlayback high = Hls::playback(master, 0, 0);
    EXPECT_TRUE(high.manifest.contains("https://example.org/live/v9/prog_index.m3u8\n"));
    EXPECT_FALSE(high.manifest.contains("ec3"));

    const HlsMaster muxed = Hls::parseMaster(muxedMaster, base);
    EXPECT_TRUE(Hls::playback(muxed, 0, -1).manifest.isEmpty());
    EXPECT_EQ(Hls::playback(muxed, 0, -1).url, muxed.variants[0].url);
}

TEST(HlsMasterTest, OffersEveryAudioFormatForEachVideoRendition) {
    QByteArray data = "#EXTM3U\n"
                      "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"ec3-48-768\",NAME=\"English (DVS)\",DEFAULT=NO,CHANNELS=\"16/JOC\",URI=\"atmos-dvs.m3u8\"\n"
                      "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"ec3-48-768\",NAME=\"English\",DEFAULT=YES,CHANNELS=\"16/JOC\",URI=\"atmos.m3u8\"\n"
                      "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"ac3-48-384\",NAME=\"English\",DEFAULT=YES,CHANNELS=\"6\",URI=\"dd.m3u8\"\n"
                      "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"aaclc-48-160\",NAME=\"English\",DEFAULT=YES,CHANNELS=\"2\",URI=\"aac.m3u8\"\n";
    const QList<QPair<QByteArray, QByteArray>> audio{{"ec-3", "ec3-48-768"}, {"ac-3", "ac3-48-384"}, {"mp4a.40.2", "aaclc-48-160"}};
    const QList<QPair<QByteArray, int>> video{{"1920x1080", 10000000}, {"1280x720", 3000000}};
    for (const auto &[resolution, bitrate] : video) {
        int extra = 300000;
        for (const auto &[codec, group] : audio) {
            data += "#EXT-X-STREAM-INF:BANDWIDTH=" + QByteArray::number(bitrate + extra) + ",CODECS=\"av01.0.08M.10," + codec + "\",RESOLUTION=" + resolution +
                    ",AUDIO=\"" + group + "\"\nvideo-" + resolution + ".m3u8\n";
            extra -= 100000;
        }
    }
    const HlsMaster master = Hls::parseMaster(data, base);
    EXPECT_EQ(labels(master.variants), QStringList({"1080p", "720p"}));
    EXPECT_EQ(master.variants[0].bandwidth, 10100000);
    QStringList formats;
    for (const HlsAudioFormat &format : master.audioFormats) {
        formats.append(format.label);
    }
    EXPECT_EQ(formats, QStringList({"AAC stereo", "Dolby Digital 5.1", "Dolby Atmos"}));

    const HlsPlayback atmos = Hls::playback(master, 1, 2);
    EXPECT_EQ(atmos.bandwidth, 3300000);
    EXPECT_EQ(atmos.url, QUrl("https://example.org/live/video-1280x720.m3u8"));
    EXPECT_TRUE(atmos.manifest.contains("AUDIO=\"ec3-48-768\"\nhttps://example.org/live/video-1280x720.m3u8\n"));
    EXPECT_LT(atmos.manifest.indexOf("atmos.m3u8"), atmos.manifest.indexOf("atmos-dvs.m3u8"));
    EXPECT_FALSE(atmos.manifest.contains("aac.m3u8"));
    EXPECT_TRUE(Hls::playback(master, 1, 0).manifest.contains("https://example.org/live/aac.m3u8"));
}

TEST(HlsMasterTest, NamesTheCodecWhenAResolutionComesInSeveral) {
    const QByteArray data = "#EXTM3U\n"
                            "#EXT-X-STREAM-INF:BANDWIDTH=11297325,CODECS=\"av01.0.08M.10,ec-3\",RESOLUTION=1920x1080\n"
                            "av1-ec3.m3u8\n"
                            "#EXT-X-STREAM-INF:BANDWIDTH=4612515,CODECS=\"av01.0.08M.10,mp4a.40.2\",RESOLUTION=1920x1080\n"
                            "av1-aac-low.m3u8\n"
                            "#EXT-X-STREAM-INF:BANDWIDTH=6000000,CODECS=\"hvc1.2.4.L123.B0,mp4a.40.2\",RESOLUTION=1280x720\n"
                            "hevc.m3u8\n"
                            "#EXT-X-STREAM-INF:BANDWIDTH=5000000,CODECS=\"mp4a.40.2,avc1.64001f\",RESOLUTION=1280x720\n"
                            "avc.m3u8\n"
                            "#EXT-X-STREAM-INF:BANDWIDTH=900000,CODECS=\"mp4a.40.2,avc1.64001f\",RESOLUTION=640x360\n"
                            "avc-low.m3u8\n";
    const HlsMaster master = Hls::parseMaster(data, base);
    EXPECT_EQ(labels(master.variants),
              QStringList({QString::fromUtf8("1080p AV1 · 11.3 Mbps"), QString::fromUtf8("1080p AV1 · 4.6 Mbps"), "720p H.264", "720p HEVC", "360p"}));
}
