import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

import gavqml

ApplicationWindow {
    id: mainWindow

    property url source
    property bool shouldAutoPlay: false

    property bool controlsVisibleAlias: mediaComponent.controlsAreVisible
    property bool mediaControlsContainsMouse: false

    // TODO: Maybe use backend to verify
    function getMediaInfo(fileUrl) {
        var path = fileUrl.toString()
        // On Windows, fileUrl can start with 'file:///'
        if (path.startsWith('file:///')) {
            path = path.substring(8)
        }
        var name = path.substring(path.lastIndexOf('/') + 1)
        var extension = name.substring(name.lastIndexOf('.') + 1).toLowerCase()
        var videoExtensions = ["mp4", "avi", "mkv", "mov", "wmv", "flv", "webm", "m4v", "mpg"]
        var audioExtensions = ["mp3", "wav", "ogg", "flac", "aac", "wma", "m4a"]

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

    height: 768
    title: qsTr("GAV")
    visible: true
    width: 1024

    onSourceChanged: {
        const s = "" + source
        if (!s) {
            console.log("No source provided")
            return
        }

        const mediaInfo = getMediaInfo(source)
        if (mediaInfo) {
            playList.append(mediaInfo)
            mediaComponent.path = mediaInfo.path
            mainWindow.title = "GAV - " + mediaInfo.name
            playlistComponent.playListView.currentIndex = playList.count - 1
            shouldAutoPlay = true
        } else {
            unsupportedFileDialog.open()
        }
    }

    footer: Loader {
        id: mediaControlsComponentLoader

        active: mainWindow.visibility !== Window.FullScreen

        // Collapse space when inactive
        height: active && item ? item.implicitHeight : 0
        sourceComponent: active ? mediaControlsComponent : null

        // The footer property handles positioning and width

        // Let the loaded MediaControls fill the Loader
        onLoaded: if (item)
                      item.anchors.fill = mediaControlsComponentLoader
    }

    // --- Loader for WINDOWED mode ---
    menuBar: Loader {
        id: windowedMenuBarLoader

        active: mainWindow.visibility !== Window.FullScreen

        // Make the Loader span the window width
        anchors.left: parent.left
        anchors.right: parent.right

        // Collapse space when inactive
        height: active && item ? item.implicitHeight : 0
        sourceComponent: active ? menuBarComponent : null

        // Let the loaded MenuBar fill the Loader
        onLoaded: if (item)
                      item.anchors.fill = windowedMenuBarLoader
    }

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

    // --- Loader for FULLSCREEN mode ---
    Loader {
        id: fullscreenMenuBarLoader

        active: mainWindow.visibility === Window.FullScreen
        enabled: opacity > 0
        height: item ? item.implicitHeight : 0
        opacity: !controlsVisibleAlias ? 0 : 1
        sourceComponent: menuBarComponent
        width: parent.width
        y: 0
        z: 100

        Behavior on opacity {
            NumberAnimation {
                duration: 300
            }
        }
    }

    CustomSnackbar {
        id: captureSnackbar
    }

    CustomSnackbar {
        id: collageSnackbar
    }

    Connections {
        target: mediaComponent.mediaPlayer
        function onFrameCaptured(success, path) {
            if (success) {
                captureSnackbar.message = "Frame captured: " + path
            } else {
                captureSnackbar.message = "Error: " + path
            }
            captureSnackbar.show()
        }
    }

    Connections {
        target: collage

        function onCollageFinished(successCount, failCount) {
            if (successCount > 0) {
                collageSnackbar.message = successCount + " collage(s) created successfully"
            } else {
                collageSnackbar.message = "Collage creation failed"
            }
            collageSnackbar.show()
        }
    }

    // About dialog
    Dialog {
        id: aboutDialog

        modal: true
        standardButtons: Dialog.Ok
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2

        ColumnLayout {
            spacing: 5
            Image {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredHeight: 200
                Layout.preferredWidth: 200
                fillMode: Image.PreserveAspectFit
                smooth: true
                source: "qrc:/assets/images/logo-bw.png"
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                color: "white"
                text: "v" + Qt.application.version
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                color: "white"
                text: "GAV - A simple media player built with Qt and FFmpeg"
            }
        }
    }

    // If an error occurs with the video/audio
    Dialog {
        id: unsupportedFileDialog

        modal: true
        standardButtons: Dialog.Ok
        title: "Unsupported File"
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2

        Text {
            color: "white"
            text: "The dropped file is not a supported video format."
        }
    }

    Collage {
        id: collage
    }

    DropArea {
        anchors.fill: parent

        onDropped: function (drop) {
            if (drop.urls && drop.urls.length > 0) {
                var firstFileSet = false
                for (var i = 0; i < drop.urls.length; i++) {
                    var mediaInfo = getMediaInfo(drop.urls[i])
                    console.debug("Media info for dropped file:", mediaInfo)
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
                           StandardPaths.DownloadLocation)[0]
        nameFilters: ["Video Files (*.mp4 *.avi *.mkv *.mov *.wmv *.flv *.webm *.m4v *.mpg)", "Audio Files (*.mp3 *.wav *.ogg *.flac *.aac *.wma *m4a)", "All files (*)"]

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
        focus: true

        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_Left) {
                // Seek backward 5 seconds
                mediaPlayer.position = Math.max(0, mediaPlayer.position - 5000)
                event.accepted = true
            } else if (event.key === Qt.Key_Right) {
                // Seek forward 5 seconds
                mediaPlayer.position = Math.min(mediaPlayer.duration, mediaPlayer.position + 5000)
                event.accepted = true
            } else if (event.key === Qt.Key_Space) {
                // Play/Pause
                if (mediaPlayer.playbackState === MediaPlayer.PlayingState) {
                    mediaPlayer.pause()
                } else {
                    mediaPlayer.play()
                }
                event.accepted = true
            }
        }

        onMediaLoadedChanged: {
            if (mediaLoaded && shouldAutoPlay) {
                mediaPlayer.play()
                shouldAutoPlay = false
            }
        }
    }
    ListModel {
        id: playList
    }
    PlayListComponent {
        id: playlistComponent

        anchors.fill: parent
        playList: playList
        visible: !mediaComponent.isVideoAndPlaying
    }
    Component {
        id: mediaControlsComponent

        MediaControlsComponent {
            id: controlBar

            audioOutput: mediaComponent.audioOutput
            implicitHeight: 60
            mediaLoaded: mediaComponent.mediaLoaded
            player: mediaComponent.mediaPlayer
            videoOutput: mediaComponent.videoOutput
            playlistCount: playList.count
            playlistCurrentIndex: playlistComponent.playListView.currentIndex

            onNextTrack: playlistComponent.playListView.currentIndex++
            onPreviousTrack: playlistComponent.playListView.currentIndex--
            onContainsMouseChanged: mainWindow.mediaControlsContainsMouse = containsMouse
        }
    }

    // --- Loader for FULLSCREEN mode ---
    Loader {
        id: fullscreenMediaControlsComponentLoader

        active: mainWindow.visibility === Window.FullScreen
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        enabled: opacity > 0
        height: item ? item.implicitHeight : 0
        opacity: controlsVisibleAlias ? 1 : 0
        sourceComponent: mediaControlsComponent
        z: 100

        Behavior on opacity {
            NumberAnimation {
                duration: 300
            }
        }

        onLoaded: if (item)
                      item.anchors.fill = fullscreenMediaControlsComponentLoader
    }
    FontLoader {
        id: materialSymbolsOutlined

        source: "qrc:/assets/fonts/MaterialSymbolsOutlined.ttf"
    }
}
