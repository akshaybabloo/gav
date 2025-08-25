import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs

ApplicationWindow {
    id: mainWindow
    width: 1024
    height: 768
    visible: true
    title: qsTr("GAV")

    property bool controlsVisibleAlias: mediaScreen.controlsAreVisible

    menuBar: MenuBar {
        id: menuBar
        opacity: (mainWindow.visibility === Window.FullScreen && !controlsVisibleAlias) ? 0 : 1
        enabled: opacity > 0

        Behavior on opacity {
            enabled: mainWindow.visibility === Window.FullScreen
            NumberAnimation {
                duration: 300
            }
        }

        Menu {
            title: qsTr("File")
            Action {
                text: qsTr("Open")
                onTriggered: {
                    fileDialog.open()
                }
            }
            MenuSeparator { }
            Action {
                text: qsTr("Exit")
                onTriggered: Qt.quit()
            }
        }
        Menu {
            title: qsTr("Help")
            Action {
                text: qsTr("About")
                onTriggered: aboutDialog.open()
            }
        }
    }

    Dialog {
        id: aboutDialog
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2
        title: qsTr("GAV")
        modal: true
        standardButtons: Dialog.Ok
        Text {
            text: "GAV - A simple media player built with Qt\n\n"
        }
    }

    FileDialog {
        id: fileDialog
        currentFolder: StandardPaths.standardLocations(
                           StandardPaths.VideosLocation)[0]
        nameFilters: ["Video files (*.mp4 *.avi *.mkv)", "All files (*)"]
        onAccepted: {
            mediaScreen.path = selectedFile
            mainWindow.title = "GAV - " + selectedFile.toString().split('/').pop()
        }
    }

    MediaScreen {
        id: mediaScreen
        height: parent.height
        width: parent.width
        path: ""
    }

    FontLoader {
        id: materialSymbolsOutlined
        source: "qrc:/assets/fonts/MaterialSymbolsOutlined.ttf"
    }
}
