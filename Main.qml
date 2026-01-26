import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Layouts
import QtMultimedia

import gavqml

ApplicationWindow {
    id: mainWindow

    property bool controlsVisibleAlias: mediaComponent.controlsAreVisible
    property bool mediaControlsContainsMouse: false
    property bool shouldAutoPlay: false
    property url source
    property string lastUnsupportedFile: ""
    property bool isDarkTheme: true

    // Dynamic theme switching - overrides qtquickcontrols2.conf at runtime
    Material.theme: isDarkTheme ? Material.Dark : Material.Light

    function getMediaInfo(fileUrl) {
        var path = fileUrl.toString();
        // On Windows, fileUrl can start with 'file:///'
        if (path.startsWith('file:///')) {
            path = path.substring(8);
        }
        var name = path.substring(path.lastIndexOf('/') + 1);
        var extension = name.substring(name.lastIndexOf('.') + 1).toLowerCase();

        if (AppConstants.isVideoExtension(extension)) {
            return {
                "name": name,
                "path": fileUrl,
                "type": "video",
                "icon": "\ueb87"
            };
        } else if (AppConstants.isAudioExtension(extension)) {
            return {
                "name": name,
                "path": fileUrl,
                "type": "audio",
                "icon": "\ue405"
            };
        } else {
            lastUnsupportedFile = name;
            return null;
        }
    }

    height: Screen.height * 0.75
    minimumHeight: 480
    minimumWidth: 640
    title: qsTr("GAV")
    visible: true
    width: Screen.width * 0.7

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

    onSourceChanged: {
        const s = "" + source;
        if (!s) {
            console.log("No source provided");
            return;
        }

        const mediaInfo = getMediaInfo(source);
        if (mediaInfo) {
            playList.append(mediaInfo);
            mediaComponent.path = mediaInfo.path;
            mainWindow.title = "GAV - " + mediaInfo.name;
            playlistComponent.playListView.currentIndex = playList.count - 1;
            shouldAutoPlay = true;
        } else {
            unsupportedFileDialog.open();
        }
    }

    // --- Reusable MenuBar definition ---
    Component {
        id: menuBarComponent

        MenuBar {
            background: Rectangle {
                color: Material.background.darker(1.2)
            }

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
                title: qsTr("View")

                Action {
                    text: qsTr("Settings")
                    onTriggered: settingsDialog.open()
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
        function onFrameCaptured(success, path) {
            if (success) {
                captureSnackbar.message = "Frame captured: " + path;
            } else {
                captureSnackbar.message = "Error: " + path;
            }
            captureSnackbar.show();
        }

        target: mediaComponent.mediaPlayer
    }
    Connections {
        function onCollageFinished(successCount, failCount) {
            if (successCount > 0) {
                collageSnackbar.message = successCount + " collage(s) created successfully";
            } else {
                collageSnackbar.message = "Collage creation failed";
            }
            collageSnackbar.show();
        }

        target: collage
    }

    // About dialog
    Dialog {
        id: aboutDialog

        modal: true
        standardButtons: Dialog.Ok
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2

        ColumnLayout {
            spacing: 10

            Image {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredHeight: 150
                Layout.preferredWidth: 150
                fillMode: Image.PreserveAspectFit
                smooth: true
                source: "qrc:/assets/images/logo-bw.png"
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                color: Material.foreground
                font.pixelSize: 18
                font.bold: true
                text: "GAV Media Player"
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                color: Material.foreground
                opacity: 0.7
                text: "Version " + Qt.application.version
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Material.dividerColor
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                color: Material.foreground
                text: "A simple audio and video player"
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                color: Material.foreground
                opacity: 0.5
                font.pixelSize: 12
                text: "Built with Qt " + Qt.version + " and FFmpeg"
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Material.dividerColor
            }
            ColumnLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 4

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    color: Material.foreground
                    opacity: 0.5
                    font.pixelSize: 11
                    text: "Keyboard Shortcuts:"
                }
                Text {
                    color: Material.foreground
                    opacity: 0.7
                    font.pixelSize: 11
                    text: "Space - Play/Pause"
                }
                Text {
                    color: Material.foreground
                    opacity: 0.7
                    font.pixelSize: 11
                    text: "Left/Right - Seek 5 seconds"
                }
                Text {
                    color: Material.foreground
                    opacity: 0.7
                    font.pixelSize: 11
                    text: "Scroll - Volume"
                }
                Text {
                    color: Material.foreground
                    opacity: 0.7
                    font.pixelSize: 11
                    text: "Ctrl+Scroll - Zoom"
                }
                Text {
                    color: Material.foreground
                    opacity: 0.7
                    font.pixelSize: 11
                    text: "Double-click - Fullscreen"
                }
            }
        }
    }

    // If an error occurs with the video/audio
    Dialog {
        id: unsupportedFileDialog

        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Ok
        title: "Unsupported File"

        ColumnLayout {
            spacing: 10

            Text {
                color: Material.foreground
                text: lastUnsupportedFile ? "'" + lastUnsupportedFile + "' is not a supported format." : "The file is not a supported format."
                wrapMode: Text.WordWrap
                Layout.maximumWidth: 400
            }
            Text {
                color: Material.foreground
                opacity: 0.5
                font.pixelSize: 12
                text: "Supported formats:"
                Layout.topMargin: 5
            }
            Text {
                color: Material.foreground
                opacity: 0.7
                font.pixelSize: 11
                text: AppConstants.getSupportedFormatsString()
                wrapMode: Text.WordWrap
                Layout.maximumWidth: 400
            }
        }
    }
    Collage {
        id: collage

    }
    SettingsDialog {
        id: settingsDialog

        audioOutput: mediaComponent.audioOutput
        mediaPlayer: mediaComponent.mediaPlayer
        isDarkTheme: mainWindow.isDarkTheme
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2

        onThemeToggled: function(isDark) {
            mainWindow.isDarkTheme = isDark;
        }
    }
    DropArea {
        anchors.fill: parent

        onDropped: function (drop) {
            if (drop.urls && drop.urls.length > 0) {
                var firstFileSet = false;
                for (var i = 0; i < drop.urls.length; i++) {
                    var mediaInfo = getMediaInfo(drop.urls[i]);
                    console.debug("Media info for dropped file:", JSON.stringify(mediaInfo));
                    if (mediaInfo) {
                        playList.append(mediaInfo);
                        if (!firstFileSet) {
                            mediaComponent.path = mediaInfo.path;
                            mainWindow.title = "GAV - " + mediaInfo.name;
                            playlistComponent.playListView.currentIndex = playList.count - 1;
                            firstFileSet = true;
                        }
                    } else {
                        unsupportedFileDialog.open();
                    }
                }
            }
        }
    }
    FileDialog {
        id: fileDialog

        currentFolder: StandardPaths.standardLocations(StandardPaths.DownloadLocation)[0]
        nameFilters: [AppConstants.getVideoExtensionsFilter(), AppConstants.getAudioExtensionsFilter(), "All files (*)"]

        onAccepted: {
            var mediaInfo = getMediaInfo(selectedFile);
            if (mediaInfo) {
                playList.append(mediaInfo);
                mediaComponent.path = mediaInfo.path;
                mainWindow.title = "GAV - " + mediaInfo.name;
                playlistComponent.playListView.currentIndex = playList.count - 1;
            } else {
                unsupportedFileDialog.open();
            }
        }
    }
    MediaComponent {
        id: mediaComponent

        anchors.fill: parent
        focus: true
        path: ""

        Shortcut {
            sequence: "Space"
            onActivated: {
                // Play/Pause
                if (mediaComponent.mediaPlayer.playbackState === MediaPlayer.PlayingState) {
                    mediaComponent.mediaPlayer.pause();
                } else {
                    mediaComponent.mediaPlayer.play();
                }
            }
        }
        Shortcut {
            sequence: "Left"
            onActivated: {
                // Seek backward
                mediaComponent.mediaPlayer.position = Math.max(0, mediaComponent.mediaPlayer.position - AppConstants.seekStep);
            }
        }
        Shortcut {
            sequence: "Right"
            onActivated: {
                // Seek forward
                mediaComponent.mediaPlayer.position = Math.min(mediaComponent.mediaPlayer.duration, mediaComponent.mediaPlayer.position + AppConstants.seekStep);
            }
        }
        onMediaLoadedChanged: {
            if (mediaLoaded && shouldAutoPlay) {
                mediaPlayer.play();
                shouldAutoPlay = false;
            }
        }
        onStopped: {
            mediaComponent.path = "";
            mainWindow.title = qsTr("GAV");
        }
        onFullscreenToggleRequested: {
            mainWindow.visibility = mainWindow.visibility === Window.FullScreen ? Window.Windowed : Window.FullScreen;
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

        onItemSelected: function(path, name) {
            mediaComponent.path = path;
            mainWindow.title = "GAV - " + name;
            mediaComponent.mediaPlayer.play();
        }
        onPlayRequested: {
            mediaComponent.mediaPlayer.play();
        }
    }
    Component {
        id: mediaControlsComponent

        MediaControlsComponent {
            id: controlBar

            audioOutput: mediaComponent.audioOutput
            implicitHeight: 60
            mediaLoaded: mediaComponent.mediaLoaded
            player: mediaComponent.mediaPlayer
            playlistCount: playList.count
            playlistCurrentIndex: playlistComponent.playListView.currentIndex
            videoOutput: mediaComponent.videoOutput

            onContainsMouseChanged: mainWindow.mediaControlsContainsMouse = containsMouse
            onNextTrack: playlistComponent.playListView.currentIndex++
            onPreviousTrack: playlistComponent.playListView.currentIndex--
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
