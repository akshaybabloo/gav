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
    property bool isDarkTheme: true
    property bool mediaControlsContainsMouse: false
    property bool playlistManualVisible: false
    property bool shouldAutoPlay: false
    property url source

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

    // Dynamic theme switching - overrides qtquickcontrols2.conf at runtime
    Material.theme: isDarkTheme ? Material.Dark : Material.Light
    flags: Qt.Window | Qt.FramelessWindowHint
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

    Settings {
        id: appSettings

        property bool checkUpdatesOnStartup: true
        property bool isDarkTheme: true
        property real playbackRate: 1.0
        property real volume: AppConstants.defaultVolume
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

        onAboutRequested: aboutDialog.open()
        onCheckUpdatesRequested: {
            updateDialog.manualCheck = true;
            updates.checkUpdates();
        }
        onExitRequested: Qt.quit()
        onOpenFileRequested: fileDialog.open()
        onSettingsRequested: settingsDialog.open()
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

        anchors.centerIn: parent
        bottomPadding: 20
        leftPadding: 28
        modal: true
        rightPadding: 28
        topPadding: 20
        width: 420

        background: Rectangle {
            border.color: Material.dividerColor
            border.width: 1
            color: Material.background
            radius: 10
        }
        footer: Item {
            implicitHeight: 60

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                color: Material.dividerColor
                height: 1
            }
            Button {
                id: aboutCloseButton

                Material.background: Material.accent
                Material.foreground: Material.theme === Material.Dark ? "#000000" : "#FFFFFF"
                Material.roundedScale: Material.SmallScale
                anchors.right: parent.right
                anchors.rightMargin: 20
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Close")

                onClicked: aboutDialog.close()
            }
        }
        header: Item {
            implicitHeight: 52

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 28
                anchors.right: parent.right
                anchors.rightMargin: 28
                anchors.verticalCenter: parent.verticalCenter
                color: Material.foreground
                font.pixelSize: 16
                font.weight: Font.DemiBold
                text: qsTr("About")
            }
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                color: Material.dividerColor
                height: 1
            }
        }

        ColumnLayout {
            spacing: 8
            width: parent.width

            Image {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredHeight: 72
                Layout.preferredWidth: 72
                Layout.topMargin: 4
                fillMode: Image.PreserveAspectFit
                smooth: true
                source: "qrc:/assets/images/logo-bw.png"
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 4
                color: Material.foreground
                font.pixelSize: 20
                font.weight: Font.DemiBold
                text: "GAV Media Player"
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                color: Material.foreground
                font.pixelSize: 12
                opacity: 0.6
                text: qsTr("Version %1").arg(Qt.application.version)
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                Layout.topMargin: 12
                color: Material.foreground
                font.pixelSize: 13
                horizontalAlignment: Text.AlignHCenter
                opacity: 0.85
                text: qsTr("A simple audio and video player")
                wrapMode: Text.WordWrap
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                color: Material.foreground
                font.pixelSize: 11
                opacity: 0.5
                text: qsTr("Built with Qt %1 and FFmpeg").arg(Qt.version)
            }
        }
    }

    // If an error occurs with the video/audio
    Dialog {
        id: playbackErrorDialog

        anchors.centerIn: parent
        bottomPadding: 20
        leftPadding: 24
        modal: true
        rightPadding: 24
        topPadding: 20
        width: 460

        background: Rectangle {
            border.color: Material.dividerColor
            border.width: 1
            color: Material.background
            radius: 10
        }
        footer: Item {
            implicitHeight: 60

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                color: Material.dividerColor
                height: 1
            }
            Button {
                id: playbackCloseButton

                Material.background: Material.accent
                Material.foreground: Material.theme === Material.Dark ? "#000000" : "#FFFFFF"
                Material.roundedScale: Material.SmallScale
                anchors.right: parent.right
                anchors.rightMargin: 20
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Close")

                onClicked: playbackErrorDialog.close()
            }
        }
        header: Item {
            implicitHeight: 52

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 24
                anchors.right: parent.right
                anchors.rightMargin: 24
                anchors.verticalCenter: parent.verticalCenter
                color: Material.foreground
                font.pixelSize: 16
                font.weight: Font.DemiBold
                text: qsTr("Playback Error")
            }
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                color: Material.dividerColor
                height: 1
            }
        }

        RowLayout {
            spacing: 16
            width: parent.width

            Text {
                Layout.alignment: Qt.AlignTop
                color: Material.color(Material.Red)
                font.family: materialSymbolsOutlined.name
                font.pixelSize: 36
                text: "\ue000"
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Text {
                    Layout.fillWidth: true
                    color: Material.foreground
                    font.pixelSize: 14
                    text: qsTr("Unable to play this file.")
                    wrapMode: Text.WordWrap
                }
                Text {
                    Layout.fillWidth: true
                    color: Material.foreground
                    font.pixelSize: 12
                    opacity: 0.7
                    text: qsTr("The format may not be supported.")
                    wrapMode: Text.WordWrap
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    Layout.topMargin: 6
                    color: Material.dividerColor
                    opacity: 0.5
                }
                Text {
                    Layout.fillWidth: true
                    Layout.topMargin: 2
                    color: Material.foreground
                    elide: Text.ElideMiddle
                    font.family: "monospace"
                    font.pixelSize: 11
                    opacity: 0.7
                    text: mediaComponent.path
                }
                Text {
                    Layout.fillWidth: true
                    color: Material.color(Material.Red)
                    font.pixelSize: 11
                    opacity: 0.85
                    text: (mediaComponent.mediaPlayer && mediaComponent.mediaPlayer.errorString) || ""
                    visible: mediaComponent.mediaPlayer && mediaComponent.mediaPlayer.errorString !== ""
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
    Collage {
        id: collage
    }
    Updates {
        id: updates

        onCheckFailed: function (errorMessage) {
            const wasManual = updateDialog.manualCheck;
            updateDialog.manualCheck = false;
            if (!wasManual)
                return;
            updateDialog.errorMessage = errorMessage;
            updateDialog.checkState = "failed";
            updateDialog.open();
        }
        onUpToDate: function (currentVersion) {
            const wasManual = updateDialog.manualCheck;
            updateDialog.manualCheck = false;
            if (!wasManual)
                return;
            updateDialog.currentVersion = currentVersion;
            updateDialog.checkState = "upToDate";
            updateDialog.open();
        }
        onUpdateAvailable: function (currentVersion, latestVersion, releaseUrl) {
            updateDialog.currentVersion = currentVersion;
            updateDialog.latestVersion = latestVersion;
            updateDialog.releaseUrl = releaseUrl;
            updateDialog.checkState = "available";
            updateDialog.open();
            updateDialog.manualCheck = false;
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
        bottomPadding: 20
        leftPadding: 24
        modal: true
        rightPadding: 24
        topPadding: 20
        width: 460

        background: Rectangle {
            border.color: Material.dividerColor
            border.width: 1
            color: Material.background
            radius: 10
        }
        footer: Item {
            implicitHeight: 60

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                color: Material.dividerColor
                height: 1
            }
            Row {
                anchors.right: parent.right
                anchors.rightMargin: 20
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                Button {
                    Material.roundedScale: Material.SmallScale
                    text: qsTr("Later")
                    visible: updateDialog.checkState === "available"

                    onClicked: updateDialog.reject()
                }
                Button {
                    id: updatePrimaryButton

                    Material.background: Material.accent
                    Material.foreground: Material.theme === Material.Dark ? "#000000" : "#FFFFFF"
                    Material.roundedScale: Material.SmallScale
                    text: updateDialog.checkState === "available" ? qsTr("View Release") : qsTr("Close")

                    onClicked: {
                        if (updateDialog.checkState === "available") {
                            updateDialog.accept();
                        } else {
                            updateDialog.close();
                        }
                    }
                }
            }
        }
        header: Item {
            implicitHeight: 52

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 24
                anchors.right: parent.right
                anchors.rightMargin: 24
                anchors.verticalCenter: parent.verticalCenter
                color: Material.foreground
                font.pixelSize: 16
                font.weight: Font.DemiBold
                text: {
                    if (updateDialog.checkState === "available")
                        return qsTr("Update Available");
                    if (updateDialog.checkState === "upToDate")
                        return qsTr("No Updates");
                    return qsTr("Update Check Failed");
                }
            }
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                color: Material.dividerColor
                height: 1
            }
        }

        onAccepted: {
            if (checkState === "available")
                Qt.openUrlExternally(releaseUrl);
        }

        RowLayout {
            spacing: 16
            width: parent.width

            Text {
                Layout.alignment: Qt.AlignTop
                color: {
                    if (updateDialog.checkState === "available")
                        return Material.accent;
                    if (updateDialog.checkState === "upToDate")
                        return Material.color(Material.Green);
                    return Material.color(Material.Red);
                }
                font.family: materialSymbolsOutlined.name
                font.pixelSize: 36
                text: {
                    if (updateDialog.checkState === "available")
                        return "\ue923"; // update
                    if (updateDialog.checkState === "upToDate")
                        return "\ue86c"; // check_circle
                    return "\ue000"; // error
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Text {
                    Layout.fillWidth: true
                    color: Material.foreground
                    font.pixelSize: 14
                    text: {
                        if (updateDialog.checkState === "available")
                            return qsTr("A new version of GAV is available.");
                        if (updateDialog.checkState === "upToDate")
                            return qsTr("You're on the latest version.");
                        return qsTr("Could not check for updates.");
                    }
                    wrapMode: Text.WordWrap
                }
                GridLayout {
                    Layout.topMargin: 6
                    columnSpacing: 16
                    columns: 2
                    rowSpacing: 4
                    visible: updateDialog.checkState === "available"

                    Text {
                        color: Material.foreground
                        font.pixelSize: 12
                        opacity: 0.6
                        text: qsTr("Current:")
                    }
                    Text {
                        color: Material.foreground
                        font.family: "monospace"
                        font.pixelSize: 12
                        text: updateDialog.currentVersion
                    }
                    Text {
                        color: Material.foreground
                        font.pixelSize: 12
                        opacity: 0.6
                        text: qsTr("Latest:")
                    }
                    Text {
                        color: Material.accent
                        font.family: "monospace"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        text: updateDialog.latestVersion
                    }
                }
                Text {
                    Layout.fillWidth: true
                    color: Material.foreground
                    font.pixelSize: 12
                    opacity: 0.6
                    text: qsTr("Current version: %1").arg(updateDialog.currentVersion)
                    visible: updateDialog.checkState === "upToDate"
                }
                Text {
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    color: Material.foreground
                    font.pixelSize: 12
                    opacity: 0.7
                    text: updateDialog.errorMessage
                    visible: updateDialog.checkState === "failed"
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
    SettingsDialog {
        id: settingsDialog

        audioOutput: mediaComponent.audioOutput
        checkUpdatesOnStartup: appSettings.checkUpdatesOnStartup
        isDarkTheme: mainWindow.isDarkTheme
        mediaPlayer: mediaComponent.mediaPlayer

        onCheckUpdatesOnStartupToggled: function (enabled) {
            appSettings.checkUpdatesOnStartup = enabled;
        }
        onDefaultSpeedChanged: function (speed) {
            appSettings.playbackRate = speed;
        }
        onThemeToggled: function (isDark) {
            mainWindow.isDarkTheme = isDark;
            appSettings.isDarkTheme = isDark;
        }
    }
    Connections {
        function onVolumeChanged() {
            appSettings.volume = mediaComponent.audioOutput.volume;
        }

        target: mediaComponent.audioOutput
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

        onFullscreenToggleRequested: {
            mainWindow.visibility = mainWindow.visibility === Window.FullScreen ? Window.Windowed : Window.FullScreen;
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

        onItemSelected: function (path, name) {
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
            onPlaylistToggleRequested: mainWindow.playlistManualVisible = !mainWindow.playlistManualVisible
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
