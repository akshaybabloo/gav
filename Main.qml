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

    // --- Reusable MenuBar definition ---
    Component {
        id: menuBarComponent
        MenuBar {
            Menu {
                title: qsTr("File")
                Action {
                    text: qsTr("Open")
                    onTriggered: fileDialog.open()
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
    }

    // --- Loader for WINDOWED mode ---
    menuBar: Loader {
        active: mainWindow.visibility !== Window.FullScreen
        sourceComponent: menuBarComponent
    }

    // --- Loader for FULLSCREEN mode ---
    Loader {
        id: fullscreenMenuBarLoader
        active: mainWindow.visibility === Window.FullScreen
        y: 0
        width: parent.width
        height: item ? item.implicitHeight : 0
        z: 100
        opacity: !controlsVisibleAlias ? 0 : 1
        enabled: opacity > 0

        Behavior on opacity {
            NumberAnimation {
                duration: 300
            }
        }

        sourceComponent: menuBarComponent
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
