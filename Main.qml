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
    property bool isDarkTheme: true
    property bool playlistManualVisible: false

    // Dynamic theme switching - overrides qtquickcontrols2.conf at runtime
    Material.theme: isDarkTheme ? Material.Dark : Material.Light
    flags: Qt.Window | Qt.FramelessWindowHint

    function getMediaInfo(fileUrl) {
        var path = fileUrl.toString();
        // On Windows, fileUrl can start with 'file:///'
        if (path.startsWith('file:///')) {
            path = path.substring(8);
        }
        // Strip trailing slashes (e.g. directory URLs)
        while (path.endsWith('/'))
            path = path.substring(0, path.length - 1);
        var name = path.substring(path.lastIndexOf('/') + 1);
        // Strip query ('?') and fragment ('#') parts from the file name
        var queryIndex = name.indexOf("?");
        var fragmentIndex = name.indexOf("#");
        var cutIndex = name.length;
        if (queryIndex !== -1 && queryIndex < cutIndex)
            cutIndex = queryIndex;
        if (fragmentIndex !== -1 && fragmentIndex < cutIndex)
            cutIndex = fragmentIndex;
        name = name.substring(0, cutIndex);
        if (!name)
            return null;
        var extension = name.substring(name.lastIndexOf('.') + 1).toLowerCase();

        if (AppConstants.isAudioExtension(extension)) {
            return {
                "name": name,
                "path": fileUrl,
                "type": "audio",
                "icon": "\ue405"
            };
        }
        // Treat everything else (including unknown formats) as video;
        // the media player will report an error if it can't play it.
        return {
            "name": name,
            "path": fileUrl,
            "type": "video",
            "icon": "\ueb87"
        };
    }

    function exitMiniPlayer() {
        if (!miniPlayerWindow.visible)
            return;
        mediaComponent.mediaPlayer.videoOutput = mediaComponent.videoOutput;
        miniPlayerWindow.visible = false;
        mainWindow.show();
        mainWindow.showNormal();
        mainWindow.raise();
        mainWindow.requestActivate();
    }

    height: Screen.height * 0.75
    minimumHeight: 480
    minimumWidth: 640
    title: qsTr("GAV")
    visible: true
    width: Screen.width * 0.7

    Settings {
        id: appSettings

        property bool isDarkTheme: true
        property real volume: AppConstants.defaultVolume
        property real playbackRate: 1.0
        property bool checkUpdatesOnStartup: true
    }

    Component.onCompleted: {
        isDarkTheme = appSettings.isDarkTheme;
        if (mediaComponent.audioOutput)
            mediaComponent.audioOutput.volume = appSettings.volume;
        if (mediaComponent.mediaPlayer)
            mediaComponent.mediaPlayer.playbackRate = appSettings.playbackRate;
        if (appSettings.checkUpdatesOnStartup) {
            updateDialog.manualCheck = false;
            updates.checkUpdates();
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

    TitleBar {
        id: titleBar

        readonly property bool isFullScreen: mainWindow.visibility === Window.FullScreen

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        enabled: opacity > 0
        height: 40
        opacity: isFullScreen ? (controlsVisibleAlias ? 1 : 0) : 1
        targetWindow: mainWindow
        windowTitle: mainWindow.title
        z: 100

        Behavior on opacity {
            NumberAnimation {
                duration: 300
            }
        }

        onOpenFileRequested: fileDialog.open()
        onExitRequested: Qt.quit()
        onSettingsRequested: settingsDialog.open()
        onAboutRequested: aboutDialog.open()
        onCheckUpdatesRequested: {
            updateDialog.manualCheck = true;
            updates.checkUpdates();
        }
    }

    onSourceChanged: {
        const s = "" + source;
        if (!s) {
            console.log("No source provided");
            return;
        }

        const mediaInfo = getMediaInfo(source);
        if (!mediaInfo)
            return;
        playList.append(mediaInfo);
        mediaComponent.path = mediaInfo.path;
        mainWindow.title = "GAV - " + mediaInfo.name;
        playlistComponent.playListView.currentIndex = playList.count - 1;
        shouldAutoPlay = true;
    }

    // Resize grips (frameless window) — only active when windowed.
    // Edges
    MouseArea {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        cursorShape: Qt.SizeVerCursor
        enabled: mainWindow.visibility === Window.Windowed
        height: 4
        z: 102

        onPressed: mainWindow.startSystemResize(Qt.TopEdge)
    }
    MouseArea {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        cursorShape: Qt.SizeVerCursor
        enabled: mainWindow.visibility === Window.Windowed
        height: 4
        z: 100

        onPressed: mainWindow.startSystemResize(Qt.BottomEdge)
    }
    MouseArea {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.top: parent.top
        cursorShape: Qt.SizeHorCursor
        enabled: mainWindow.visibility === Window.Windowed
        width: 4
        z: 100

        onPressed: mainWindow.startSystemResize(Qt.LeftEdge)
    }
    MouseArea {
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.top: parent.top
        cursorShape: Qt.SizeHorCursor
        enabled: mainWindow.visibility === Window.Windowed
        width: 4
        z: 100

        onPressed: mainWindow.startSystemResize(Qt.RightEdge)
    }
    // Corners (drawn above edges)
    MouseArea {
        anchors.left: parent.left
        anchors.top: parent.top
        cursorShape: Qt.SizeFDiagCursor
        enabled: mainWindow.visibility === Window.Windowed
        height: 8
        width: 8
        z: 101

        onPressed: mainWindow.startSystemResize(Qt.LeftEdge | Qt.TopEdge)
    }
    MouseArea {
        anchors.right: parent.right
        anchors.top: parent.top
        cursorShape: Qt.SizeBDiagCursor
        enabled: mainWindow.visibility === Window.Windowed
        height: 8
        width: 8
        z: 101

        onPressed: mainWindow.startSystemResize(Qt.RightEdge | Qt.TopEdge)
    }
    MouseArea {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        cursorShape: Qt.SizeBDiagCursor
        enabled: mainWindow.visibility === Window.Windowed
        height: 8
        width: 8
        z: 101

        onPressed: mainWindow.startSystemResize(Qt.LeftEdge | Qt.BottomEdge)
    }
    MouseArea {
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        cursorShape: Qt.SizeFDiagCursor
        enabled: mainWindow.visibility === Window.Windowed
        height: 8
        width: 8
        z: 101

        onPressed: mainWindow.startSystemResize(Qt.RightEdge | Qt.BottomEdge)
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
        id: playbackErrorDialog

        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Ok
        title: "Playback Error"

        ColumnLayout {
            spacing: 10

            Text {
                color: Material.foreground
                text: "Unable to play this file. The format may not be supported."
                wrapMode: Text.WordWrap
                Layout.maximumWidth: 400
            }
            Text {
                color: Material.foreground
                opacity: 0.7
                font.pixelSize: 12
                text: "File: " + mediaComponent.path
                wrapMode: Text.WordWrap
                Layout.maximumWidth: 400
            }
            Text {
                color: Material.foreground
                opacity: 0.5
                font.pixelSize: 11
                visible: mediaComponent.mediaPlayer && mediaComponent.mediaPlayer.errorString !== ""
                text: "Error: " + (mediaComponent.mediaPlayer ? mediaComponent.mediaPlayer.errorString : "")
                wrapMode: Text.WordWrap
                Layout.maximumWidth: 400
            }
        }
    }
    Collage {
        id: collage
    }

    Updates {
        id: updates

        onUpdateAvailable: function(currentVersion, latestVersion, releaseUrl) {
            updateDialog.currentVersion = currentVersion;
            updateDialog.latestVersion = latestVersion;
            updateDialog.releaseUrl = releaseUrl;
            updateDialog.checkState = "available";
            updateDialog.open();
            updateDialog.manualCheck = false;
        }
        onUpToDate: function(currentVersion) {
            const wasManual = updateDialog.manualCheck;
            updateDialog.manualCheck = false;
            if (!wasManual)
                return;
            updateDialog.currentVersion = currentVersion;
            updateDialog.checkState = "upToDate";
            updateDialog.open();
        }
        onCheckFailed: function(errorMessage) {
            const wasManual = updateDialog.manualCheck;
            updateDialog.manualCheck = false;
            if (!wasManual)
                return;
            updateDialog.errorMessage = errorMessage;
            updateDialog.checkState = "failed";
            updateDialog.open();
        }
    }

    Dialog {
        id: updateDialog

        property string checkState: "upToDate"
        property string currentVersion: ""
        property string errorMessage: ""
        property string latestVersion: ""
        property bool manualCheck: false
        property url releaseUrl

        anchors.centerIn: parent
        modal: true
        standardButtons: checkState === "available" ? (Dialog.Ok | Dialog.Cancel) : Dialog.Ok
        title: {
            if (checkState === "available")
                return qsTr("Update Available");
            if (checkState === "upToDate")
                return qsTr("No Updates");
            return qsTr("Update Check Failed");
        }
        width: 400

        onAccepted: {
            if (checkState === "available")
                Qt.openUrlExternally(releaseUrl);
        }

        ColumnLayout {
            spacing: 10
            width: parent.width

            Text {
                Layout.fillWidth: true
                color: Material.foreground
                visible: updateDialog.checkState ==="available"
                wrapMode: Text.WordWrap
                text: qsTr("A new version of GAV is available.\n\nCurrent: %1\nLatest: %2\n\nOpen the release page?")
                        .arg(updateDialog.currentVersion)
                        .arg(updateDialog.latestVersion)
            }
            Text {
                Layout.fillWidth: true
                color: Material.foreground
                visible: updateDialog.checkState ==="upToDate"
                wrapMode: Text.WordWrap
                text: qsTr("You're on the latest version (%1).").arg(updateDialog.currentVersion)
            }
            Text {
                Layout.fillWidth: true
                color: Material.foreground
                visible: updateDialog.checkState ==="failed"
                wrapMode: Text.WordWrap
                text: qsTr("Could not check for updates:\n%1").arg(updateDialog.errorMessage)
            }
        }
    }

    SettingsDialog {
        id: settingsDialog

        audioOutput: mediaComponent.audioOutput
        checkUpdatesOnStartup: appSettings.checkUpdatesOnStartup
        mediaPlayer: mediaComponent.mediaPlayer
        isDarkTheme: mainWindow.isDarkTheme
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2

        onThemeToggled: function(isDark) {
            mainWindow.isDarkTheme = isDark;
            appSettings.isDarkTheme = isDark;
        }
        onDefaultSpeedChanged: function(speed) {
            appSettings.playbackRate = speed;
        }
        onCheckUpdatesOnStartupToggled: function(enabled) {
            appSettings.checkUpdatesOnStartup = enabled;
        }
    }
    Connections {
        target: mediaComponent.audioOutput

        function onVolumeChanged() {
            appSettings.volume = mediaComponent.audioOutput.volume;
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
                    if (!mediaInfo)
                        continue;
                    playList.append(mediaInfo);
                    if (!firstFileSet) {
                        mediaComponent.path = mediaInfo.path;
                        mainWindow.title = "GAV - " + mediaInfo.name;
                        playlistComponent.playListView.currentIndex = playList.count - 1;
                        firstFileSet = true;
                    }
                }
            }
        }
    }
    FileDialog {
        id: fileDialog

        currentFolder: StandardPaths.standardLocations(StandardPaths.DownloadLocation)[0]
        nameFilters: ["All files (*)"]

        onAccepted: {
            var mediaInfo = getMediaInfo(selectedFile);
            if (!mediaInfo)
                return;
            playList.append(mediaInfo);
            mediaComponent.path = mediaInfo.path;
            mainWindow.title = "GAV - " + mediaInfo.name;
            playlistComponent.playListView.currentIndex = playList.count - 1;
        }
    }
    MediaComponent {
        id: mediaComponent

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: mainWindow.visibility === Window.FullScreen ? parent.top : titleBar.bottom
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
            if (mediaLoaded) {
                mediaPlayer.playbackRate = appSettings.playbackRate;
                if (shouldAutoPlay) {
                    mediaPlayer.play();
                    shouldAutoPlay = false;
                }
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

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: mainWindow.visibility === Window.FullScreen ? parent.top : titleBar.bottom
        collageTarget: collage
        playList: playList
        visible: !mediaComponent.isVideoAndPlaying || mainWindow.playlistManualVisible

        onItemSelected: function(path, name) {
            mediaComponent.path = path;
            mainWindow.title = "GAV - " + name;
            mediaComponent.mediaPlayer.play();
        }
        onPlayRequested: {
            if (mediaComponent.path === "" && playlistComponent.playListView.currentIndex !== -1) {
                var item = playList.get(playlistComponent.playListView.currentIndex);
                mediaComponent.path = item.path;
                mainWindow.title = "GAV - " + item.name;
            }
            mediaComponent.mediaPlayer.play();
        }
    }
    Component {
        id: mediaControlsComponent

        MediaControlsComponent {
            id: controlBar

            audioOutput: mediaComponent.audioOutput
            collageTarget: collage
            implicitHeight: 90
            mediaLoaded: mediaComponent.mediaLoaded
            miniPlayerActive: miniPlayerWindow.visible
            player: mediaComponent.mediaPlayer
            playlistCount: playList.count
            playlistCurrentIndex: playlistComponent.playListView.currentIndex
            videoOutput: mediaComponent.videoOutput

            onContainsMouseChanged: mainWindow.mediaControlsContainsMouse = containsMouse
            onPlaylistToggleRequested: mainWindow.playlistManualVisible = !mainWindow.playlistManualVisible
            onMiniPlayerRequested: {
                var px = mainWindow.x + mainWindow.width - miniPlayerWindow.width - 20;
                var py = mainWindow.y + mainWindow.height - miniPlayerWindow.height - 60;
                miniPlayerWindow.x = Math.max(0, Math.min(px, Screen.width - miniPlayerWindow.width));
                miniPlayerWindow.y = Math.max(0, Math.min(py, Screen.height - miniPlayerWindow.height));
                miniPlayerWindow.visible = true;
                mediaComponent.mediaPlayer.videoOutput = miniPlayerWindow.miniVideoOutput;
                mainWindow.hide();
            }
            onNextTrack: playlistComponent.playListView.currentIndex++
            onPreviousTrack: playlistComponent.playListView.currentIndex--
        }
    }

    MiniPlayerWindow {
        id: miniPlayerWindow

        audioOutput: mediaComponent.audioOutput
        mediaPlayer: mediaComponent.mediaPlayer

        onCloseRequested: Qt.quit()
        onRestoreRequested: mainWindow.exitMiniPlayer()
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
