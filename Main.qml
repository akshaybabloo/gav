import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: mainWindow
    property bool mediaControlsContainsMouse: false
    width: 1024
    height: 768
    visible: true
    title: qsTr("GAV")

    property bool controlsVisibleAlias: mediaComponent.controlsAreVisible

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
        onLoaded: if (item)
                      item.anchors.fill = windowedMenuBarLoader

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
                            mediaComponent.path = mediaInfo.path
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
                mediaComponent.path = mediaInfo.path
                mainWindow.title = "GAV - " + mediaInfo.name
                playlistComponent.playListView.currentIndex = playList.count - 1
            } else {
                unsupportedFileDialog.open()
            }
        }
    }

    MediaComponent {
        id: mediaComponent
        anchors.fill: parent
        path: ""
    }

    ListModel {
        id: playList
    }

    PlayListComponent {
        id: playlistComponent
        anchors.fill: parent
        visible: !mediaComponent.isVideoAndPlaying
        playList: playList
    }

    Component {
        id: mediaControlsComponent
        MediaControlsComponent {
            id: controlBar
            onContainsMouseChanged: mainWindow.mediaControlsContainsMouse = containsMouse
            implicitHeight: 60
            player: mediaComponent.mediaPlayer
            audioOutput: mediaComponent.audioOutput
            videoOutput: mediaComponent.videoOutput
            mediaLoaded: mediaComponent.mediaLoaded
        }
    }

    footer: Loader {
        id: mediaControlsComponentLoader
        active: mainWindow.visibility !== Window.FullScreen
        sourceComponent: active ? mediaControlsComponent : null

        // The footer property handles positioning and width

        // Let the loaded MediaControls fill the Loader
        onLoaded: if (item)
                      item.anchors.fill = mediaControlsComponentLoader

        // Collapse space when inactive
        height: active && item ? item.implicitHeight : 0
    }

    // --- Loader for FULLSCREEN mode ---
    Loader {
        id: fullscreenMediaControlsComponentLoader
        active: mainWindow.visibility === Window.FullScreen
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        z: 100
        opacity: controlsVisibleAlias ? 1 : 0
        enabled: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 300 } }

        sourceComponent: mediaControlsComponent
        onLoaded: if (item) item.anchors.fill = fullscreenMediaControlsComponentLoader
        height: item ? item.implicitHeight : 0
    }

    FontLoader {
        id: materialSymbolsOutlined
        source: "qrc:/assets/fonts/MaterialSymbolsOutlined.ttf"
    }
}
