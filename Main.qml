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
                MenuSeparator {}
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
        id: windowedMenuBarLoader
        active: mainWindow.visibility !== Window.FullScreen
        sourceComponent: active ? menuBarComponent : null

        // Make the Loader span the window width
        anchors.left: parent.left
        anchors.right: parent.right

        // Let the loaded MenuBar fill the Loader
        onLoaded: if (item) item.anchors.fill = windowedMenuBarLoader

        // Collapse space when inactive
        height: active && item ? item.implicitHeight : 0
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

    // About dialog
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

    // If an error occurs with the video/audio
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

    // TODO: Maybe use backend to verify
    function getMediaInfo(fileUrl) {
        var path = fileUrl.toString()
        // On Windows, fileUrl can start with 'file:///'
        if (path.startsWith('file:///')) {
            path = path.substring(8)
        }
        var name = path.substring(path.lastIndexOf('/') + 1)
        var extension = name.substring(name.lastIndexOf('.') + 1).toLowerCase()
        var videoExtensions = ["mp4", "avi", "mkv", "mov", "wmv", "flv", "webm"]
        var audioExtensions = ["mp3", "wav", "ogg", "flac", "aac", "wma"]

        if (videoExtensions.indexOf(extension) !== -1) {
            return {
                "name": name,
                "path": fileUrl,
                "type": "video",
                "icon": "\ueb87"
            }
        } else if (audioExtensions.indexOf(extension) !== -1) {
            return {
                "name": name,
                "path": fileUrl,
                "type": "audio",
                "icon": "\ue405"
            }
        } else {
            return null
        }
    }

    DropArea {
        anchors.fill: parent
        onDropped: function (drop) {
            if (drop.urls && drop.urls.length > 0) {
                var firstFileSet = false
                for (var i = 0; i < drop.urls.length; i++) {
                    var mediaInfo = getMediaInfo(drop.urls[i])
                    if (mediaInfo) {
                        playList.append(mediaInfo)
                        if (!firstFileSet) {
                            mediaScreen.path = mediaInfo.path
                            mainWindow.title = "GAV - " + mediaInfo.name
                            playlistComponent.playListView.currentIndex = playList.count - 1
                            firstFileSet = true
                        }
                    } else {
                        unsupportedFileDialog.open()
                    }
                }
            }
        }
    }

    FileDialog {
        id: fileDialog
        currentFolder: StandardPaths.standardLocations(
                           StandardPaths.VideosLocation)[0]
        nameFilters: ["Video Files (*.mp4 *.avi *.mkv *.mov *.wmv)", "Audio Files (*.mp3 *.wav *.ogg)", "All files (*)"]
        onAccepted: {
            var mediaInfo = getMediaInfo(selectedFile)
            if (mediaInfo) {
                playList.append(mediaInfo)
                mediaScreen.path = mediaInfo.path
                mainWindow.title = "GAV - " + mediaInfo.name
                playlistComponent.playListView.currentIndex = playList.count - 1
            } else {
                unsupportedFileDialog.open()
            }
        }
    }

    MediaScreen {
        id: mediaScreen
        anchors.fill: parent
        path: ""
    }

    ListModel {
        id: playList
    }

    PlayListComponent {
        id: playlistComponent
        anchors.fill: parent
        visible: !mediaScreen.isVideoAndPlaying
        playList: playList
    }

    footer: MediaControls {
        id: controlBar
        width: parent.width
        anchors.bottom: parent.bottom
        anchors.bottomMargin: mediaScreen.controlsAreVisible ? 0 : -height // No gap
        opacity: mediaScreen.controlsAreVisible ? 1 : 0

        mediaScreen: mediaScreen

        Behavior on anchors.bottomMargin {
            NumberAnimation {
                duration: 300
                easing.type: Easing.OutCubic
            }
        }
        Behavior on opacity {
            NumberAnimation {
                duration: 300
            }
        }
    }

    FontLoader {
        id: materialSymbolsOutlined
        source: "qrc:/assets/fonts/MaterialSymbolsOutlined.ttf"
    }
}
