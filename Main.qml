import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: mainWindow
    width: 640
    height: 480
    visible: true
    title: qsTr("Hello World")

    MediaScreen {
        height: parent.height
        width: parent.width
        path: "file://home/akshay/Downloads/Roux-design-system-demo-and-discussion.mp4"

        Debug {}
    }

    FontLoader {
        id: materialSymbolsOutlined
        source: "qrc:/assets/fonts/MaterialSymbolsOutlined.ttf"
    }
}
