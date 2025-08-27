import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

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
                MenuSeparator {
                }
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
            text: "GAV - A simple media player built with Qt and FFmpeg"
            color: "white"
        }
    }

    Dialog {
        id: unsupportedFileDialog
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2
        title: "Unsupported File"
        modal: true
        standardButtons: Dialog.Ok
        Text {
            text: "The dropped file is not a supported video format."
            color: "white"
        }
    }

    DropArea {
        anchors.fill: parent // Or define specific size/position
        onDropped: function (drop) {
            if (drop.urls && drop.urls.length > 0) {
                console.log("Dropped files:", drop.urls);
                mediaScreen.path = drop.urls[0];
                mainWindow.title = "GAV - " + drop.urls[0].toString().split('/').pop();
            } else if (drop.text) {
                console.log("Dropped text:", drop.text);
            }
        }
    }

    FileDialog {
        id: fileDialog
        currentFolder: StandardPaths.standardLocations(StandardPaths.VideosLocation)[0]
        nameFilters: ["All files (*)"]
        onAccepted: {
            mediaScreen.path = selectedFile;
            mainWindow.title = "GAV - " + selectedFile.toString().split('/').pop();
        }
    }

    ListModel {
        id: playList
        ListElement {
            name: "Some name"
            path: "path://something"
            type: "audio"
            icon: "\ue405"
        }
        ListElement {
            name: "Some name"
            path: "path://something"
            type: "video"
            icon: "\ueb87"
        }
    }

    SplitView {
        id: splitView
        anchors.fill: parent

        handle: Rectangle {
            id: handleSeparator
            width: 1
            color: "#3a3a3e"
            implicitWidth: 1
        }

        MediaScreen {
            id: mediaScreen
            path: ""
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 400
        }

        Pane {
            id: playlistPane
            Layout.fillHeight: true
            Layout.minimumWidth: 180
            Layout.preferredWidth: 280
            Layout.maximumWidth: 500
            background: Rectangle {
                color: "#1e1e1e"
            }

            ListView {
                id: playListView
                anchors.fill: parent
                clip: true

                model: playList
                delegate: ItemDelegate {
                    width: parent.width
                    height: 48
                    padding: 8

                    contentItem: Row {
                        spacing: 12
                        anchors.verticalCenter: parent.verticalCenter

                        Text {
                            text: model.icon
                            font.family: materialSymbolsOutlined.name
                            font.pixelSize: 24
                            color: "white"
                        }
                        Text {
                            text: model.name
                            color: "white"
                            font.pixelSize: 14
                            elide: Text.ElideRight
                        }
                    }

                    background: Rectangle {
                        color: parent.down ? "#4a4a4e" : (parent.hovered ? "#2a2a2e" : "transparent")
                        radius: 4
                    }
                }
            }
        }
    }

    FontLoader {
        id: materialSymbolsOutlined
        source: "qrc:/assets/fonts/MaterialSymbolsOutlined.ttf"
    }
}
