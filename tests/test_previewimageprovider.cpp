#include <gtest/gtest.h>
#include "../previewimageprovider.h"

namespace {

QImage filled(Qt::GlobalColor color) {
    QImage image(4, 4, QImage::Format_ARGB32_Premultiplied);
    image.fill(color);
    return image;
}

QImage request(PreviewImageProvider &provider, const QString &id) {
    QSize size;
    return provider.requestImage(id, &size, {});
}

} // namespace

TEST(PreviewImageProviderTest, OwnersStoringInTheSameMillisecondDoNotCollide) {
    PreviewImageProvider provider;
    const QString first = PreviewImageProvider::imageId("p1", 500);
    const QString second = PreviewImageProvider::imageId("p2", 500);
    EXPECT_NE(first, second);
    provider.storeImage(first, filled(Qt::red));
    provider.storeImage(second, filled(Qt::blue));
    EXPECT_EQ(request(provider, first).pixelColor(0, 0), QColor(Qt::red));
    EXPECT_EQ(request(provider, second).pixelColor(0, 0), QColor(Qt::blue));
}

TEST(PreviewImageProviderTest, ClearingOneOwnerKeepsTheOthers) {
    PreviewImageProvider provider;
    provider.storeImage(PreviewImageProvider::imageId("p1", 1), filled(Qt::red));
    provider.storeImage(PreviewImageProvider::imageId("p10", 1), filled(Qt::green));
    provider.storeImage(PreviewImageProvider::imageId("p2", 1), filled(Qt::blue));
    provider.clearImages("p1");
    EXPECT_EQ(request(provider, PreviewImageProvider::imageId("p1", 1)).size(), QSize(1, 1));
    EXPECT_EQ(request(provider, PreviewImageProvider::imageId("p10", 1)).size(), QSize(4, 4));
    EXPECT_EQ(request(provider, PreviewImageProvider::imageId("p2", 1)).size(), QSize(4, 4));
}

TEST(PreviewImageProviderTest, TheSizeLimitAppliesToOneOwnerAtATime) {
    PreviewImageProvider provider;
    provider.storeImage(PreviewImageProvider::imageId("p2", 1), filled(Qt::blue));
    for (int stamp = 1; stamp <= 21; ++stamp) {
        provider.storeImage(PreviewImageProvider::imageId("p1", stamp), filled(Qt::red));
    }
    EXPECT_EQ(request(provider, PreviewImageProvider::imageId("p1", 21)).size(), QSize(4, 4));
    EXPECT_EQ(request(provider, PreviewImageProvider::imageId("p1", 1)).size(), QSize(1, 1));
    EXPECT_EQ(request(provider, PreviewImageProvider::imageId("p2", 1)).size(), QSize(4, 4));
}
