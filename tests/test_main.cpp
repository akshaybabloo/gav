#include <gtest/gtest.h>
#include <QCoreApplication>

int main(int argc, char **argv) {
    // Qt requires QCoreApplication for event loop and multimedia
    QCoreApplication app(argc, argv);
    
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
