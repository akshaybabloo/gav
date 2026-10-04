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

    readonly property string appTitle: qsTr("GAV Player")
    property bool controlsVisibleAlias: mediaComponent.controlsAreVisible
    property bool isDarkTheme: true
    property bool mediaControlsContainsMouse: false
    property var pendingStartupUrls: []
    property var pendingSubtitleUrls: []
    readonly property bool hasNextItem: appSettings.shuffle ? shuffleOrder.remaining > 0 : playlistComponent.playListView.currentIndex >= 0 && playlistComponent.playListView.currentIndex < playList.count - 1
    readonly property bool hasPreviousItem: appSettings.shuffle ? shuffleOrder.canGoBack : playlistComponent.playListView.currentIndex > 0
    property bool restoringSession: false
    readonly property url sessionPlaylistUrl: StandardPaths.writableLocation(StandardPaths.AppDataLocation) + "/session.m3u8"
    property bool playlistManualVisible: false
    property int repeatMode: 0
    readonly property bool dialogOpen: aboutDialog.opened || playbackErrorDialog.opened || updateDialog.opened || settingsDialog.opened || resumeDialog.opened || goToTimeDialog.opened
    readonly property bool shortcutsEnabled: !textInputFocused && !dialogOpen
    property bool shouldAutoPlay: false
    readonly property bool textInputFocused: activeFocusItem instanceof TextInput || activeFocusItem instanceof TextEdit

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
    function showOsd(text) {
        if (miniPlayerWindow.visible)
            miniPlayerWindow.showOsd(text);
        else
            mediaComponent.showOsd(text);
    }
    function toggleFullScreen() {
        mainWindow.visibility = mainWindow.visibility === Window.FullScreen ? Window.Windowed : Window.FullScreen;
    }
    function changeVolume(delta) {
        var output = mediaComponent.audioOutput;
        output.volume = Math.max(0, Math.min(1, output.volume + delta));
        if (miniPlayerWindow.visible)
            miniPlayerWindow.showOsd(qsTr("Volume: %1%").arg(Math.round(output.volume * 100)));
        else
            mediaComponent.showVolume();
    }
    function changeSpeed(direction) {
        var speeds = AppConstants.playbackSpeeds;
        var player = mediaComponent.mediaPlayer;
        var index = 0;
        for (var i = 1; i < speeds.length; i++) {
            if (Math.abs(speeds[i] - player.playbackRate) < Math.abs(speeds[index] - player.playbackRate))
                index = i;
        }
        var target = direction === 0 ? 1.0 : speeds[Math.max(0, Math.min(speeds.length - 1, index + direction))];
        player.playbackRate = target;
        showOsd(qsTr("Speed: %1x").arg(target));
    }
    function isPlaylistUrl(url) {
        var name = url.toString();
        return AppConstants.isPlaylistExtension(name.substring(name.lastIndexOf('.') + 1));
    }
    function isStreamUrl(url) {
        var name = url.toString();
        return name.startsWith("http://") || name.startsWith("https://");
    }
    function supportedMediaExtensions() {
        return AppConstants.videoExtensions.concat(AppConstants.audioExtensions);
    }
    function playlistItems() {
        var items = [];
        for (var i = 0; i < playList.count; i++) {
            var item = playList.get(i);
            items.push({
                "name": item.name,
                "path": item.path
            });
        }
        return items;
    }
    function selectPlaylistItem(index) {
        if (index < 0 || index >= playList.count)
            return;
        if (playlistComponent.playListView.currentIndex === index) {
            var item = playList.get(index);
            mediaComponent.path = item.path;
            mainWindow.title = appTitle + " - " + item.name;
            mediaComponent.mediaPlayer.play();
        } else {
            playlistComponent.playListView.currentIndex = index;
        }
    }
    function nextItem() {
        if (appSettings.shuffle) {
            selectPlaylistItem(shuffleOrder.next(false));
        } else if (hasNextItem) {
            selectPlaylistItem(playlistComponent.playListView.currentIndex + 1);
        }
    }
    function previousItem() {
        if (appSettings.shuffle) {
            selectPlaylistItem(shuffleOrder.previous());
        } else if (hasPreviousItem) {
            selectPlaylistItem(playlistComponent.playListView.currentIndex - 1);
        }
    }
    function applyLoadedPlaylist(tag, result) {
        if (!result.ok) {
            if (tag !== "session") {
                captureSnackbar.message = qsTr("Could not open playlist: ") + result.error;
                captureSnackbar.show();
            }
            return {
                "first": -1,
                "positions": []
            };
        }
        if (tag === "replace")
            playList.clear();
        var firstIndex = playList.count;
        var positions = [];
        for (var i = 0; i < result.entries.length; i++) {
            var mediaInfo = getMediaInfo(result.entries[i].path);
            if (mediaInfo) {
                positions.push(playList.count);
                playList.append(mediaInfo);
            } else {
                positions.push(-1);
            }
        }
        var added = playList.count - firstIndex;
        var skipped = result.skippedMissing + result.skippedUnsupported + (result.entries.length - added);
        if (skipped > 0) {
            captureSnackbar.message = qsTr("Loaded %1 items, skipped %2 (missing or unsupported)").arg(added).arg(skipped);
            captureSnackbar.show();
        }
        return {
            "first": added > 0 ? firstIndex : -1,
            "positions": positions
        };
    }
    function promptResumeIfSaved() {
        var player = mediaComponent.mediaPlayer;
        var source = player.source.toString();
        if (!appSettings.rememberPositions || source === "" || isStreamUrl(source))
            return false;
        var saved = PlaybackHistory.savedPosition(source);
        if (saved < 0)
            return false;
        if (saved >= player.duration) {
            PlaybackHistory.removePosition(source);
            return false;
        }
        player.pause();
        player.position = saved;
        resumeDialog.positionMs = saved;
        resumeDialog.open();
        return true;
    }
    function saveSession() {
        if (appSettings.restoreLastPlaylist && playList.count > 0)
            PlaylistFiles.save(sessionPlaylistUrl, playlistItems(), playlistComponent.playListView.currentIndex, "session");
        else
            PlaylistFiles.remove(sessionPlaylistUrl);
    }
    function isSubtitleUrl(url) {
        var name = url.toString();
        return AppConstants.isSubtitleExtension(name.substring(name.lastIndexOf('.') + 1));
    }
    function loadSubtitleUrl(url) {
        if (!isSubtitleUrl(url))
            return false;
        if (!mediaComponent.mediaPlayer.mediaLoaded || !mediaComponent.mediaPlayer.hasVideo) {
            captureSnackbar.message = qsTr("Open a video before loading subtitles");
            captureSnackbar.show();
            return true;
        }
        if (!mediaComponent.mediaPlayer.subtitles.loadFile(url)) {
            captureSnackbar.message = qsTr("Could not load subtitle file");
            captureSnackbar.show();
        }
        return true;
    }
    function openUrls(urls) {
        var startPlayback = mediaComponent.path === "";
        for (var i = 0; i < urls.length; i++) {
            if (isPlaylistUrl(urls[i])) {
                PlaylistFiles.load(urls[i], supportedMediaExtensions(), "append");
                continue;
            }
            var mediaInfo = getMediaInfo(urls[i]);
            if (!mediaInfo)
                continue;
            playList.append(mediaInfo);
            if (startPlayback) {
                mediaComponent.path = mediaInfo.path;
                mainWindow.title = appTitle + " - " + mediaInfo.name;
                playlistComponent.playListView.currentIndex = playList.count - 1;
                shouldAutoPlay = true;
                startPlayback = false;
            }
        }
    }

    // Dynamic theme switching - overrides qtquickcontrols2.conf at runtime
    Material.theme: isDarkTheme ? Material.Dark : Material.Light
    flags: Qt.Window | Qt.FramelessWindowHint
    height: Screen.height * 0.75
    minimumHeight: 480
    minimumWidth: 640
    title: appTitle
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
        if (mediaComponent.mediaPlayer) {
            mediaComponent.mediaPlayer.playbackRate = appSettings.playbackRate;
            mediaComponent.mediaPlayer.preferredAudioLanguage = appSettings.preferredAudioLanguage;
            mediaComponent.mediaPlayer.subtitles.preferredLanguage = appSettings.preferredSubtitleLanguage;
            mediaComponent.mediaPlayer.subtitles.scale = appSettings.subtitleScale;
        }
        if (appSettings.checkUpdatesOnStartup && !BuildInfo.isDebugBuild) {
            updateDialog.manualCheck = false;
            updates.checkUpdates();
        }
        if (appSettings.restoreLastPlaylist) {
            pendingStartupUrls = InstanceManager.takePendingUrls();
            PlaylistFiles.load(sessionPlaylistUrl, supportedMediaExtensions(), "session");
        } else {
            openUrls(InstanceManager.takePendingUrls());
        }
    }

    Settings {
        id: appSettings

        property bool checkUpdatesOnStartup: true
        property bool isDarkTheme: true
        property real playbackRate: 1.0
        property string preferredAudioLanguage: ""
        property string preferredSubtitleLanguage: ""
        property bool rememberPositions: false
        property bool rememberRecentFiles: false
        property bool restoreLastPlaylist: false
        property bool shuffle: false
        property real subtitleScale: 1.0
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
        onOpenPlaylistRequested: openPlaylistDialog.open()
        onOpenRecentRequested: function (url) {
            if (mainWindow.isPlaylistUrl(url)) {
                PlaylistFiles.load(url, mainWindow.supportedMediaExtensions(), "append");
                return;
            }
            var mediaInfo = mainWindow.getMediaInfo(url);
            if (!mediaInfo)
                return;
            playList.append(mediaInfo);
            mainWindow.selectPlaylistItem(playList.count - 1);
        }
        onRecentFileMissing: function (path) {
            recentSnackbar.missingPath = path;
            recentSnackbar.message = qsTr("File not found: ") + path;
            recentSnackbar.show();
        }
        onSavePlaylistRequested: savePlaylistDialog.open()
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
    CustomSnackbar {
        id: subtitleSnackbar
    }
    CustomSnackbar {
        id: recentSnackbar

        property string missingPath: ""

        actionText: qsTr("Remove from list")

        onActionTriggered: PlaybackHistory.removeRecent(missingPath)
    }
    GoToTimeDialog {
        id: goToTimeDialog

        parent: miniPlayerWindow.visible ? miniPlayerWindow.contentItem : mainWindow.contentItem
        player: mediaComponent.mediaPlayer
    }
    ResumeDialog {
        id: resumeDialog

        parent: miniPlayerWindow.visible ? miniPlayerWindow.contentItem : mainWindow.contentItem

        onResumeChosen: mediaComponent.mediaPlayer.play()
        onStartOverChosen: {
            PlaybackHistory.removePosition(mediaComponent.mediaPlayer.source.toString());
            mediaComponent.mediaPlayer.position = 0;
            mediaComponent.mediaPlayer.play();
        }
    }
    ShuffleOrder {
        id: shuffleOrder

        enabled: appSettings.shuffle

        onEnabledChanged: {
            if (enabled)
                reset(playList.count, playlistComponent.playListView.currentIndex);
        }
    }
    Connections {
        function onModelReset() {
            shuffleOrder.reset(playList.count, playlistComponent.playListView.currentIndex);
        }
        function onRowsInserted(parent, first, last) {
            for (var i = first; i <= last; i++)
                shuffleOrder.itemInserted(i);
        }
        function onRowsMoved() {
            shuffleOrder.reset(playList.count, playlistComponent.playListView.currentIndex);
        }
        function onRowsRemoved(parent, first, last) {
            for (var i = last; i >= first; i--)
                shuffleOrder.itemRemoved(i);
        }

        target: playList
    }
    Connections {
        function onPathChanged() {
            if (resumeDialog.opened)
                resumeDialog.close();
        }

        target: mediaComponent
    }
    Connections {
        function onPositionCheckpoint(source, position, duration) {
            if (appSettings.rememberPositions)
                PlaybackHistory.recordPosition(source.toString(), position, duration);
        }

        target: mediaComponent.mediaPlayer
    }
    Connections {
        function onLoaded(tag, result) {
            if (tag === "session") {
                mainWindow.restoringSession = true;
                var restored = mainWindow.applyLoadedPlaylist(tag, result);
                var restoredCurrent = result.currentIndex >= 0 && result.currentIndex < restored.positions.length ? restored.positions[result.currentIndex] : -1;
                if (restoredCurrent >= 0)
                    playlistComponent.playListView.currentIndex = restoredCurrent;
                mainWindow.restoringSession = false;
                var startupUrls = mainWindow.pendingStartupUrls;
                mainWindow.pendingStartupUrls = [];
                var firstPending = playList.count;
                mainWindow.openUrls(startupUrls);
                if (playList.count > firstPending)
                    mainWindow.selectPlaylistItem(firstPending);
                return;
            }
            var loaded = mainWindow.applyLoadedPlaylist(tag, result);
            if (loaded.first < 0)
                return;
            var loadedCurrent = result.currentIndex >= 0 && result.currentIndex < loaded.positions.length ? loaded.positions[result.currentIndex] : -1;
            if (tag === "replace")
                mainWindow.selectPlaylistItem(loadedCurrent >= 0 ? loadedCurrent : loaded.first);
            else if (mediaComponent.path === "")
                mainWindow.selectPlaylistItem(loaded.first);
        }
        function onSaved(tag, ok) {
            if (tag === "save") {
                captureSnackbar.message = ok ? qsTr("Playlist saved") : qsTr("Could not save playlist");
                captureSnackbar.show();
            }
        }

        target: PlaylistFiles
    }
    Connections {
        function onAboutToQuit() {
            mediaComponent.mediaPlayer.checkpoint();
            mainWindow.saveSession();
        }

        target: Qt.application
    }
    Connections {
        function onErrorOccurred(message) {
            subtitleSnackbar.message = message;
            subtitleSnackbar.show();
        }
        function onScaleChanged() {
            appSettings.subtitleScale = mediaComponent.mediaPlayer.subtitles.scale;
        }

        target: mediaComponent.mediaPlayer.subtitles
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
        function onUrlsPending() {
            mainWindow.openUrls(InstanceManager.takePendingUrls());
            if (!miniPlayerWindow.visible) {
                mainWindow.raise();
                mainWindow.requestActivate();
            }
        }

        target: InstanceManager
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
                text: appTitle
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
                text: BuildInfo.ffmpegVersion
                    ? qsTr("Built with Qt %1 and FFmpeg %2").arg(BuildInfo.qtVersion).arg(BuildInfo.ffmpegVersion)
                    : qsTr("Built with Qt %1").arg(BuildInfo.qtVersion)
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
                            return qsTr("A new version of %1 is available.").arg(appTitle);
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
        rememberPositions: appSettings.rememberPositions
        rememberRecentFiles: appSettings.rememberRecentFiles
        restoreLastPlaylist: appSettings.restoreLastPlaylist

        onCheckUpdatesOnStartupToggled: function (enabled) {
            appSettings.checkUpdatesOnStartup = enabled;
        }
        onDefaultSpeedChanged: function (speed) {
            appSettings.playbackRate = speed;
        }
        onClearHistoryRequested: {
            PlaybackHistory.clear();
            PlaylistFiles.remove(mainWindow.sessionPlaylistUrl);
            captureSnackbar.message = qsTr("History cleared");
            captureSnackbar.show();
        }
        onRememberPositionsToggled: function (enabled) {
            appSettings.rememberPositions = enabled;
        }
        onRememberRecentFilesToggled: function (enabled) {
            appSettings.rememberRecentFiles = enabled;
        }
        onRestoreLastPlaylistToggled: function (enabled) {
            appSettings.restoreLastPlaylist = enabled;
        }
        onPreferredAudioLanguageEdited: function (language) {
            appSettings.preferredAudioLanguage = language;
            mediaComponent.mediaPlayer.preferredAudioLanguage = language;
        }
        onPreferredSubtitleLanguageEdited: function (language) {
            appSettings.preferredSubtitleLanguage = language;
            mediaComponent.mediaPlayer.subtitles.preferredLanguage = language;
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
                var subtitleUrls = [];
                for (var i = 0; i < drop.urls.length; i++) {
                    if (isSubtitleUrl(drop.urls[i])) {
                        subtitleUrls.push(drop.urls[i]);
                        continue;
                    }
                    if (isPlaylistUrl(drop.urls[i])) {
                        PlaylistFiles.load(drop.urls[i], supportedMediaExtensions(), "append");
                        continue;
                    }
                    var mediaInfo = getMediaInfo(drop.urls[i]);
                    console.debug("Media info for dropped file:", JSON.stringify(mediaInfo));
                    if (!mediaInfo)
                        continue;
                    playList.append(mediaInfo);
                    if (!firstFileSet) {
                        mediaComponent.path = mediaInfo.path;
                        mainWindow.title = appTitle + " - " + mediaInfo.name;
                        playlistComponent.playListView.currentIndex = playList.count - 1;
                        firstFileSet = true;
                    }
                }
                if (firstFileSet) {
                    mainWindow.pendingSubtitleUrls = subtitleUrls;
                } else {
                    for (var j = 0; j < subtitleUrls.length; j++)
                        loadSubtitleUrl(subtitleUrls[j]);
                }
            }
        }
    }
    FileDialog {
        id: openPlaylistDialog

        nameFilters: [AppConstants.getPlaylistExtensionsFilter(), qsTr("All files (*)")]
        title: qsTr("Open Playlist")

        onAccepted: PlaylistFiles.load(selectedFile, mainWindow.supportedMediaExtensions(), "replace")
    }
    FileDialog {
        id: savePlaylistDialog

        defaultSuffix: "m3u8"
        fileMode: FileDialog.SaveFile
        nameFilters: [AppConstants.getPlaylistExtensionsFilter()]
        title: qsTr("Save Playlist")

        onAccepted: PlaylistFiles.save(selectedFile, mainWindow.playlistItems(), playlistComponent.playListView.currentIndex, "save")
    }
    FileDialog {
        id: fileDialog

        currentFolder: StandardPaths.standardLocations(StandardPaths.DownloadLocation)[0]
        nameFilters: ["All files (*)"]

        onAccepted: {
            if (loadSubtitleUrl(selectedFile))
                return;
            var mediaInfo = getMediaInfo(selectedFile);
            if (!mediaInfo)
                return;
            playList.append(mediaInfo);
            mediaComponent.path = mediaInfo.path;
            mainWindow.title = appTitle + " - " + mediaInfo.name;
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

        onFullscreenToggleRequested: mainWindow.toggleFullScreen()
        onMediaLoadedChanged: {
            if (mediaLoaded) {
                mediaPlayer.playbackRate = appSettings.playbackRate;
                if (appSettings.rememberRecentFiles)
                    PlaybackHistory.recordOpened(mediaPlayer.source.toString());
                if (mainWindow.promptResumeIfSaved())
                    shouldAutoPlay = false;
                var subtitleUrls = mainWindow.pendingSubtitleUrls;
                mainWindow.pendingSubtitleUrls = [];
                for (var i = 0; i < subtitleUrls.length; i++)
                    mainWindow.loadSubtitleUrl(subtitleUrls[i]);
                if (shouldAutoPlay) {
                    mediaPlayer.play();
                    shouldAutoPlay = false;
                }
            }
        }
        onStopped: {
            mainWindow.pendingSubtitleUrls = [];
            mediaComponent.path = "";
            mainWindow.title = appTitle;
        }

        Shortcut {
            context: Qt.ApplicationShortcut
            enabled: mainWindow.shortcutsEnabled
            sequence: "Space"

            onActivated: {
                // Play/Pause
                if (mediaComponent.path === "" && playlistComponent.playListView.currentIndex >= 0) {
                    mainWindow.selectPlaylistItem(playlistComponent.playListView.currentIndex);
                } else if (mediaComponent.mediaPlayer.playbackState === MediaPlayer.PlayingState) {
                    mediaComponent.mediaPlayer.pause();
                } else {
                    mediaComponent.mediaPlayer.play();
                }
            }
        }
        Shortcut {
            context: Qt.ApplicationShortcut
            enabled: mainWindow.shortcutsEnabled
            sequence: "Left"

            onActivated: {
                // Seek backward
                mediaComponent.mediaPlayer.position = Math.max(0, mediaComponent.mediaPlayer.position - AppConstants.seekStep);
            }
        }
        Shortcut {
            context: Qt.ApplicationShortcut
            enabled: mainWindow.shortcutsEnabled
            sequence: "Right"

            onActivated: {
                // Seek forward
                mediaComponent.mediaPlayer.position = Math.min(mediaComponent.mediaPlayer.duration, mediaComponent.mediaPlayer.position + AppConstants.seekStep);
            }
        }
        Shortcut {
            context: Qt.ApplicationShortcut
            enabled: mainWindow.shortcutsEnabled && (mediaComponent.isVideo || nerdStats.visible)
            sequence: "I"

            onActivated: nerdStats.visible = !nerdStats.visible
        }
        Shortcut {
            context: Qt.ApplicationShortcut
            enabled: mainWindow.shortcutsEnabled && mediaComponent.isVideo && mediaComponent.mediaPlayer.subtitles.tracks.length > 0
            sequence: "V"

            onActivated: mainWindow.showOsd(qsTr("Subtitles: ") + mediaComponent.mediaPlayer.subtitles.cycleTrack())
        }
        Shortcut {
            context: Qt.ApplicationShortcut
            enabled: mainWindow.shortcutsEnabled && mediaComponent.mediaPlayer.audioTracks.length > 1
            sequence: "B"

            onActivated: mainWindow.showOsd(qsTr("Audio: ") + mediaComponent.mediaPlayer.cycleAudioTrack())
        }
        Shortcut {
            context: Qt.ApplicationShortcut
            enabled: mainWindow.shortcutsEnabled && mediaComponent.mediaPlayer.subtitles.activeTrackId !== ""
            sequence: "G"

            onActivated: mainWindow.showOsd(qsTr("Subtitle delay: ") + AppConstants.formatDelay(mediaComponent.mediaPlayer.subtitles.adjustDelay(-AppConstants.subtitleDelayStep)))
        }
        Shortcut {
            context: Qt.ApplicationShortcut
            enabled: mainWindow.shortcutsEnabled && mediaComponent.mediaPlayer.subtitles.activeTrackId !== ""
            sequence: "H"

            onActivated: mainWindow.showOsd(qsTr("Subtitle delay: ") + AppConstants.formatDelay(mediaComponent.mediaPlayer.subtitles.adjustDelay(AppConstants.subtitleDelayStep)))
        }
        Shortcut {
            context: Qt.ApplicationShortcut
            enabled: mainWindow.shortcutsEnabled
            sequence: "Ctrl+Up"

            onActivated: mainWindow.changeVolume(AppConstants.volumeStep)
        }
        Shortcut {
            context: Qt.ApplicationShortcut
            enabled: mainWindow.shortcutsEnabled
            sequence: "Ctrl+Down"

            onActivated: mainWindow.changeVolume(-AppConstants.volumeStep)
        }
        Shortcut {
            context: Qt.ApplicationShortcut
            enabled: mainWindow.shortcutsEnabled
            sequence: "M"

            onActivated: {
                mediaComponent.audioOutput.muted = !mediaComponent.audioOutput.muted;
                mainWindow.showOsd(mediaComponent.audioOutput.muted ? qsTr("Muted") : qsTr("Unmuted"));
            }
        }
        Shortcut {
            context: Qt.ApplicationShortcut
            enabled: mainWindow.shortcutsEnabled && mediaComponent.isVideo && !miniPlayerWindow.visible
            sequence: "F"

            onActivated: mainWindow.toggleFullScreen()
        }
        Shortcut {
            context: Qt.ApplicationShortcut
            enabled: mainWindow.shortcutsEnabled && mainWindow.visibility === Window.FullScreen
            sequence: "Esc"

            onActivated: mainWindow.visibility = Window.Windowed
        }
        Shortcut {
            context: Qt.ApplicationShortcut
            enabled: mainWindow.shortcutsEnabled && mediaComponent.mediaLoaded
            sequence: "["

            onActivated: mainWindow.changeSpeed(-1)
        }
        Shortcut {
            context: Qt.ApplicationShortcut
            enabled: mainWindow.shortcutsEnabled && mediaComponent.mediaLoaded
            sequence: "]"

            onActivated: mainWindow.changeSpeed(1)
        }
        Shortcut {
            context: Qt.ApplicationShortcut
            enabled: mainWindow.shortcutsEnabled && mediaComponent.mediaLoaded
            sequence: "="

            onActivated: mainWindow.changeSpeed(0)
        }
        Shortcut {
            context: Qt.ApplicationShortcut
            enabled: mainWindow.shortcutsEnabled && mediaComponent.isVideo && mediaComponent.mediaLoaded
            sequence: "E"

            onActivated: mediaComponent.mediaPlayer.stepFrame(1)
        }
        Shortcut {
            context: Qt.ApplicationShortcut
            enabled: mainWindow.shortcutsEnabled && mediaComponent.isVideo && mediaComponent.mediaLoaded
            sequence: "Shift+E"

            onActivated: mediaComponent.mediaPlayer.stepFrame(-1)
        }
        Shortcut {
            context: Qt.ApplicationShortcut
            enabled: mainWindow.shortcutsEnabled && mediaComponent.mediaPlayer.chapters.length > 0
            sequence: "Shift+N"

            onActivated: {
                var title = mediaComponent.mediaPlayer.nextChapter();
                if (title !== "")
                    mainWindow.showOsd(qsTr("Chapter: ") + title);
            }
        }
        Shortcut {
            context: Qt.ApplicationShortcut
            enabled: mainWindow.shortcutsEnabled && mediaComponent.mediaPlayer.chapters.length > 0
            sequence: "Shift+P"

            onActivated: {
                var title = mediaComponent.mediaPlayer.previousChapter();
                if (title !== "")
                    mainWindow.showOsd(qsTr("Chapter: ") + title);
            }
        }
        Shortcut {
            context: Qt.ApplicationShortcut
            enabled: mainWindow.shortcutsEnabled && mainWindow.hasNextItem
            sequence: "N"

            onActivated: mainWindow.nextItem()
        }
        Shortcut {
            context: Qt.ApplicationShortcut
            enabled: mainWindow.shortcutsEnabled && mainWindow.hasPreviousItem
            sequence: "P"

            onActivated: mainWindow.previousItem()
        }
        Shortcut {
            context: Qt.ApplicationShortcut
            enabled: !mainWindow.dialogOpen && mediaComponent.mediaLoaded
            sequence: "Ctrl+T"

            onActivated: goToTimeDialog.open()
        }
        Shortcut {
            context: Qt.ApplicationShortcut
            enabled: !mainWindow.dialogOpen && !miniPlayerWindow.visible
            sequence: "Ctrl+O"

            onActivated: fileDialog.open()
        }
    }
    ListModel {
        id: playList
    }
    Rectangle {
        id: loadingScreen

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: mainWindow.visibility === Window.FullScreen ? parent.top : titleBar.bottom
        color: Material.background
        visible: mediaComponent.path !== "" && !mediaComponent.mediaLoaded && !mediaComponent.hasError
        z: 50

        ColumnLayout {
            anchors.centerIn: parent
            spacing: 16

            BusyIndicator {
                Layout.alignment: Qt.AlignHCenter
                running: loadingScreen.visible
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                color: Material.foreground
                font.pixelSize: 16
                text: qsTr("Loading...")
            }
        }
    }
    PlayListComponent {
        id: playlistComponent

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: mainWindow.visibility === Window.FullScreen ? parent.top : titleBar.bottom
        collageTarget: collage
        playList: playList
        shuffleEnabled: appSettings.shuffle
        visible: (!mediaComponent.isVideoAndPlaying && !loadingScreen.visible) || mainWindow.playlistManualVisible

        onItemSelected: function (path, name) {
            if (appSettings.shuffle)
                shuffleOrder.setCurrent(playListView.currentIndex);
            if (mainWindow.restoringSession) {
                if (!mainWindow.isStreamUrl(path)) {
                    mediaComponent.path = path;
                    mainWindow.title = appTitle + " - " + name;
                }
                return;
            }
            mediaComponent.path = path;
            mainWindow.title = appTitle + " - " + name;
            mediaComponent.mediaPlayer.play();
        }
        onShuffleToggled: function (enabled) {
            appSettings.shuffle = enabled;
        }
        onPlayRequested: {
            if (mediaComponent.path === "" && playlistComponent.playListView.currentIndex !== -1) {
                var item = playList.get(playlistComponent.playListView.currentIndex);
                mediaComponent.path = item.path;
                mainWindow.title = appTitle + " - " + item.name;
            }
            mediaComponent.mediaPlayer.play();
        }
    }
    
    NerdStatsOverlay {
        id: nerdStats
        
        anchors.top: mainWindow.visibility === Window.FullScreen ? parent.top : titleBar.bottom
        anchors.left: parent.left
        anchors.margins: 20
        z: 90
        
        audioOutput: mediaComponent.audioOutput
        hasVideo: mediaComponent.isVideo
        maximumHeight: mediaComponent.height - 40
        player: mediaComponent.mediaPlayer
        videoOutput: mediaComponent.videoOutput
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
            nerdStatsActive: nerdStats.visible
            player: mediaComponent.mediaPlayer
            hasNextTrack: mainWindow.hasNextItem
            hasPreviousTrack: mainWindow.hasPreviousItem
            playlistCount: playList.count
            playlistCurrentIndex: playlistComponent.playListView.currentIndex
            repeatMode: mainWindow.repeatMode
            videoOutput: mediaComponent.videoOutput

            onContainsMouseChanged: mainWindow.mediaControlsContainsMouse = containsMouse
            onRepeatModeChanged: mainWindow.repeatMode = repeatMode
            onNerdStatsToggleRequested: nerdStats.visible = !nerdStats.visible
            onMiniPlayerRequested: {
                var px = mainWindow.x + mainWindow.width - miniPlayerWindow.width - 20;
                var py = mainWindow.y + mainWindow.height - miniPlayerWindow.height - 60;
                miniPlayerWindow.x = Math.max(0, Math.min(px, Screen.width - miniPlayerWindow.width));
                miniPlayerWindow.y = Math.max(0, Math.min(py, Screen.height - miniPlayerWindow.height));
                miniPlayerWindow.visible = true;
                mediaComponent.mediaPlayer.videoOutput = miniPlayerWindow.miniVideoOutput;
                mainWindow.hide();
            }
            onChapterJumped: function (title) {
                if (title !== "")
                    mainWindow.showOsd(qsTr("Chapter: ") + title);
            }
            onGoToTimeRequested: goToTimeDialog.open()
            onNextTrack: mainWindow.nextItem()
            onPlaylistToggleRequested: mainWindow.playlistManualVisible = !mainWindow.playlistManualVisible
            onPreviousTrack: mainWindow.previousItem()
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
