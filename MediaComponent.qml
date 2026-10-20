import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Effects
import QtMultimedia
import gavqml

Item {
    id: root

    property alias audioOutput: audioOutput
    readonly property rect cell: {
        const width = parent ? parent.width : 0;
        const height = parent ? parent.height : 0;
        if (!grid)
            return Qt.rect(0, 0, width, height);
        return grid.revision >= 0 ? grid.cellFor(slot, width, height) : Qt.rect(0, 0, 0, 0);
    }
    property bool controlsAreVisible: true
    property int entryId: -1
    property string errorPath: ""
    property string failure: ""
    property TileGrid grid: null
    property bool hasError: errorPath !== "" && errorPath === path
    property var host: null
    property bool isPlaying: false
    property bool isVideo: customMediaPlayer.hasVideo
    property bool isVideoAndPlaying: isVideo && isPlaying
    property bool mediaLoaded: customMediaPlayer.mediaLoaded
    readonly property bool openingStream: customMediaPlayer.mediaStatus === MediaPlayer.LoadingMedia && PlaybackUtils.isStreamAddress(path)
    property alias mediaPlayer: customMediaPlayer
    required property string path
    readonly property int position: grid && grid.revision >= 0 ? grid.positionOf(slot) : 1
    property real rangeEnd: 0
    property real rangeStart: 0
    property int repeatMode: 0
    readonly property bool selected: !grid || grid.selectedSlot === slot
    property int slot: 0
    readonly property bool splitView: grid !== null && grid.splitView
    property string title: ""
    property alias videoOutput: videoOutput

    signal stopped
    signal fullscreenToggleRequested
    signal endedWithoutRepeat

    function stop() {
        if (host)
            host.stopTile(root);
        else
            customMediaPlayer.stop();
    }
    function startRange() {
        rangeStart = 0;
        rangeEnd = customMediaPlayer.duration;
    }
    function showVolume() {
        volumeColumn.visible = true;
        volumeDisplayTimer.restart();
    }
    function showOsd(text) {
        osdText.text = text;
        osdIndicator.visible = true;
        osdTimer.restart();
    }

    onPathChanged: {
        failure = "";
        if (path === "") {
            errorPath = "";
            repeatMode = 0;
            rangeStart = 0;
            rangeEnd = 0;
        }
    }

    Accessible.name: splitView ? qsTr("Video %1: %2").arg(position).arg(title) : title
    Accessible.role: Accessible.Pane
    Accessible.selected: splitView && selected
    height: cell.height
    visible: cell.width > 0
    width: cell.width
    x: cell.x
    y: cell.y

    onMediaLoadedChanged: {
        if (mediaLoaded && host)
            host.tileLoaded(root);
    }
    onStopped: {
        if (host)
            host.tileStopped(root);
    }
    onEndedWithoutRepeat: {
        if (host)
            host.tileEnded(root);
    }
    onFullscreenToggleRequested: {
        if (host)
            host.tileDoubleClicked(root);
    }

    Binding {
        property: "volume"
        target: audioOutput
        value: root.host ? root.host.volume : AppConstants.defaultVolume
        when: root.host !== null
    }
    Binding {
        property: "muted"
        target: audioOutput
        value: !root.selected || (root.host !== null && root.host.muted)
        when: root.host !== null
    }
    Connections {
        function onMutedChanged() {
            if (root.selected && root.host && root.host.muted !== audioOutput.muted)
                root.host.muted = audioOutput.muted;
        }
        function onVolumeChanged() {
            if (root.selected && root.host && Math.abs(root.host.volume - audioOutput.volume) > 0.001)
                root.host.volume = audioOutput.volume;
        }

        target: audioOutput
    }
    Connections {
        function onDurationChanged() {
            if (root.repeatMode === 3 && customMediaPlayer.duration > 0)
                root.startRange();
            if (root.host)
                root.host.reportLoadedEntry(root);
        }
        function onLiveChanged() {
            if (root.host)
                root.host.reportLoadedEntry(root);
        }
        function onMediaStatusChanged(status) {
            if (status !== MediaPlayer.EndOfMedia)
                return;
            if (root.repeatMode === 1) {
                customMediaPlayer.position = 0;
                customMediaPlayer.play();
                root.repeatMode = 0;
            } else if (root.repeatMode === 2) {
                customMediaPlayer.position = 0;
                customMediaPlayer.play();
            } else if (root.repeatMode === 3) {
                customMediaPlayer.position = root.rangeStart;
                customMediaPlayer.play();
            } else {
                root.endedWithoutRepeat();
            }
        }
        function onPositionChanged() {
            if (root.repeatMode === 3 && root.rangeEnd > root.rangeStart && customMediaPlayer.position >= root.rangeEnd)
                customMediaPlayer.position = root.rangeStart;
        }
        function onPositionCheckpoint(source, position, duration) {
            if (root.host)
                root.host.tileCheckpoint(source, position, duration);
        }
        function onRtspFailed(kind, message, address) {
            if (root.host)
                root.host.tileRtspFailed(root, kind, message, address);
        }

        target: customMediaPlayer
    }
    CustomMediaPlayer {
        id: customMediaPlayer

        audioOutput: audioOutput
        rtspTransport: root.host ? root.host.rtspTransport : "auto"
        source: path
        stopAtEnd: !root.splitView
        videoOutput: videoOutput

        onErrorOccurred: function (errorString) {
            errorPath = path;
            if (root.host)
                root.host.tileFailed(root, errorString);
        }
        onPlaybackStateChanged: function (state) {
            if (state === MediaPlayer.PlayingState) {
                isPlaying = true;
                hideControlsTimer.start();
            } else if (state === MediaPlayer.PausedState) {
                isPlaying = true; // We still want to show the video when paused
            }
        }
        // Only a real teardown clears the source; a repeat loop passes through
        // StoppedState without one, and loading a new file sets a non-empty one.
        onSourceChanged: {
            if (customMediaPlayer.source.toString() !== "") {
                return;
            }

            controlsAreVisible = true;
            isPlaying = false;
            hideControlsTimer.stop();

            // Reset zoom and pan
            videoOutput.zoomLevel = 1.0;
            videoOutput.panX = 0;
            videoOutput.panY = 0;
            videoOutput.zoomOriginX = videoOutput.width / 2;
            videoOutput.zoomOriginY = videoOutput.height / 2;

            // Reset brightness and contrast
            videoOutput.brightnessLevel = AppConstants.defaultBrightness;
            videoOutput.contrastLevel = AppConstants.defaultContrast;

            // Notify parent to reset path and title
            stopped();
        }
        onVideoVisibilityChanged: function (visible) {
            videoOutput.visible = visible;
        }
    }

    // Vertical volume display
    Column {
        id: volumeColumn

        anchors.right: parent.right
        anchors.rightMargin: 20
        anchors.verticalCenter: parent.verticalCenter
        opacity: 0.7
        spacing: 10
        visible: false
        z: 100

        Text {
            id: volumeText

            color: AppConstants.overlayTextColor
            font.bold: true
            font.pixelSize: 25
            horizontalAlignment: Text.AlignHCenter
            style: Text.Outline
            styleColor: AppConstants.overlayOutlineColor
            text: Math.round(audioOutput.volume * 100) + "%"
            width: 50
        }
        Rectangle {
            id: volumeViz

            border.color: AppConstants.overlayTextColor
            border.width: 2
            clip: true
            color: AppConstants.overlayBackgroundColor
            height: 250
            radius: 5
            width: 50

            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                color: AppConstants.volumeBarColor
                height: parent.height * audioOutput.volume
                radius: 5
            }
        }
        Timer {
            id: volumeDisplayTimer

            interval: AppConstants.volumeDisplayDuration
            repeat: false

            onTriggered: volumeColumn.visible = false
        }
    }

    // Zoom level indicator
    Rectangle {
        id: zoomIndicator

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 20
        border.color: AppConstants.overlayTextColor
        border.width: 2
        color: AppConstants.overlayBackgroundColor
        height: zoomText.height + 20
        opacity: 0.8
        radius: 8
        visible: videoOutput.zoomLevel > 1.0
        width: zoomText.width + 30
        z: 100

        Behavior on opacity {
            NumberAnimation {
                duration: 200
            }
        }

        Text {
            id: zoomText

            anchors.centerIn: parent
            color: AppConstants.overlayTextColor
            font.bold: true
            font.pixelSize: 18
            text: "Zoom: " + Math.round(videoOutput.zoomLevel * 100) + "%"
        }

        // Auto-hide after zoom changes
        Timer {
            id: zoomDisplayTimer

            interval: AppConstants.zoomDisplayDuration
            repeat: false
            running: videoOutput.zoomLevel > 1.0

            onTriggered: {
                if (videoOutput.zoomLevel <= 1.0) {
                    zoomIndicator.visible = false;
                }
            }
        }
    }
    Rectangle {
        id: osdIndicator

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: zoomIndicator.visible ? zoomIndicator.height + 30 : 20
        border.color: AppConstants.overlayTextColor
        border.width: 2
        color: AppConstants.overlayBackgroundColor
        height: osdText.height + 20
        opacity: 0.8
        radius: 8
        visible: false
        width: osdText.width + 30
        z: 100

        Text {
            id: osdText

            anchors.centerIn: parent
            color: AppConstants.overlayTextColor
            font.bold: true
            font.pixelSize: 18
        }
        Timer {
            id: osdTimer

            interval: AppConstants.volumeDisplayDuration
            repeat: false

            onTriggered: osdIndicator.visible = false
        }
    }
    AudioOutput {
        id: audioOutput

        volume: AppConstants.defaultVolume
    }
    VideoOutput {
        id: videoOutput

        // Brightness and contrast properties
        property real brightnessLevel: AppConstants.defaultBrightness
        property real contrastLevel: AppConstants.defaultContrast

        property real panX: 0
        property real panY: 0

        // Zoom and pan properties
        property real zoomLevel: 1.0

        // Zoom origin point (where the mouse was when zooming)
        property real zoomOriginX: width / 2
        property real zoomOriginY: height / 2

        anchors.fill: parent
        visible: false

        // Apply brightness/contrast effect via layer
        layer.enabled: brightnessLevel !== AppConstants.defaultBrightness || contrastLevel !== AppConstants.defaultContrast
        layer.effect: MultiEffect {
            brightness: videoOutput.brightnessLevel
            contrast: videoOutput.contrastLevel
        }

        Behavior on panX {
            NumberAnimation {
                duration: 100
                easing.type: Easing.OutQuad
            }
        }
        Behavior on panY {
            NumberAnimation {
                duration: 100
                easing.type: Easing.OutQuad
            }
        }
        transform: [
            Scale {
                id: videoScale

                origin.x: videoOutput.zoomOriginX
                origin.y: videoOutput.zoomOriginY
                xScale: videoOutput.zoomLevel
                yScale: videoOutput.zoomLevel
            },
            Translate {
                id: videoTranslate

                x: videoOutput.panX
                y: videoOutput.panY
            }
        ]
        Behavior on zoomLevel {
            NumberAnimation {
                duration: 50
                easing.type: Easing.OutQuad
            }
        }
    }
    SubtitleOverlay {
        anchors.fill: videoOutput
        contentRect: videoOutput.contentRect
        delayMs: customMediaPlayer.subtitles.delay
        renderer: customMediaPlayer.subtitles.renderer
        videoSink: videoOutput.videoSink
        visible: videoOutput.visible
    }
    Rectangle {
        id: bufferingIndicator

        Accessible.name: bufferingText.text
        anchors.centerIn: parent
        color: Qt.rgba(0, 0, 0, 0.6)
        height: bufferingRow.implicitHeight + 20
        radius: 10
        visible: customMediaPlayer.buffering || openingStream
        width: bufferingRow.implicitWidth + 32
        z: 90

        Row {
            id: bufferingRow

            anchors.centerIn: parent
            spacing: 12

            BusyIndicator {
                anchors.verticalCenter: parent.verticalCenter
                height: 32
                running: bufferingIndicator.visible
                width: 32
            }
            Text {
                id: bufferingText

                anchors.verticalCenter: parent.verticalCenter
                color: AppConstants.overlayTextColor
                font.pixelSize: 16
                text: customMediaPlayer.reconnecting ? qsTr("Reconnecting…") : (openingStream ? qsTr("Opening stream…") : qsTr("Buffering…"))
            }
        }
    }
    Rectangle {
        anchors.centerIn: parent
        color: Qt.rgba(0, 0, 0, 0.6)
        height: failureText.implicitHeight + 20
        radius: 10
        visible: root.splitView && root.failure !== ""
        width: Math.min(parent.width - 24, failureText.implicitWidth + 32)
        z: 90

        Text {
            id: failureText

            anchors.centerIn: parent
            color: AppConstants.overlayTextColor
            font.pixelSize: 14
            horizontalAlignment: Text.AlignHCenter
            text: root.failure
            width: Math.min(implicitWidth, root.width - 56)
            wrapMode: Text.WordWrap
        }
    }
    Rectangle {
        anchors.fill: parent
        border.color: Material.accent
        border.width: 2
        color: "transparent"
        visible: root.splitView && root.selected && !(root.grid && root.grid.focused)
        z: 95
    }
    MouseArea {
        id: mouseArea

        property point dragStartPos: Qt.point(0, 0)
        property bool isDragging: false
        property point lastPos: Qt.point(mouseX, mouseY)
        property real startPanX: 0
        property real startPanY: 0

        acceptedButtons: Qt.LeftButton | Qt.RightButton
        anchors.fill: parent
        cursorShape: {
            if (isDragging) {
                return Qt.ClosedHandCursor;
            } else if (!controlsAreVisible && !volumeColumn.visible && mainWindow.visibility === Window.FullScreen) {
                return Qt.BlankCursor;
            } else if (videoOutput.zoomLevel > 1.0) {
                return Qt.OpenHandCursor;
            } else {
                return Qt.ArrowCursor;
            }
        }
        hoverEnabled: true

        onDoubleClicked: function (mouse) {
            // Only toggle fullscreen when video is playing or paused
            if (customMediaPlayer.playbackState !== MediaPlayer.StoppedState) {
                fullscreenToggleRequested();
            }
        }
        onPositionChanged: {
            if (isDragging && videoOutput.zoomLevel > 1.0) {
                // Calculate drag delta
                var deltaX = mouseX - dragStartPos.x;
                var deltaY = mouseY - dragStartPos.y;

                // Update pan position with constraints
                var maxPanX = videoOutput.width * (videoOutput.zoomLevel - 1) / 2;
                var maxPanY = videoOutput.height * (videoOutput.zoomLevel - 1) / 2;

                videoOutput.panX = Math.max(-maxPanX, Math.min(maxPanX, startPanX + deltaX));
                videoOutput.panY = Math.max(-maxPanY, Math.min(maxPanY, startPanY + deltaY));
            } else if (mouseX !== lastPos.x || mouseY !== lastPos.y) {
                controlsAreVisible = true;
                if (customMediaPlayer.playbackState === MediaPlayer.PlayingState) {
                    hideControlsTimer.restart();
                }
                lastPos = Qt.point(mouseX, mouseY);
            }
        }
        onPressed: function (mouse) {
            if (root.host)
                root.host.tileClicked(root);
            if (videoOutput.zoomLevel > 1.0) {
                isDragging = true;
                dragStartPos = Qt.point(mouseX, mouseY);
                startPanX = videoOutput.panX;
                startPanY = videoOutput.panY;
            }
        }
        onReleased: function (mouse) {
            isDragging = false;
        }
        onWheel: function (wheel) {
            // Only handle scroll when video is playing or paused (not stopped)
            var isActive = customMediaPlayer.playbackState !== MediaPlayer.StoppedState;

            // Ctrl + Scroll = Zoom (Chrome-like multiplicative scaling)
            if (wheel.modifiers & Qt.ControlModifier && videoOutput.visible && isActive) {
                // Each scroll step multiplies/divides by zoom factor
                var zoomFactor = wheel.angleDelta.y > 0 ? AppConstants.zoomFactor : (1 / AppConstants.zoomFactor);
                var newZoom = Math.max(AppConstants.zoomMin, Math.min(AppConstants.zoomMax, videoOutput.zoomLevel * zoomFactor));

                // Reset when zooming back to minimum
                if (newZoom === AppConstants.zoomMin) {
                    videoOutput.panX = 0;
                    videoOutput.panY = 0;
                    videoOutput.zoomOriginX = videoOutput.width / 2;
                    videoOutput.zoomOriginY = videoOutput.height / 2;
                } else {
                    // Set zoom origin to mouse position for zoom-to-cursor effect
                    videoOutput.zoomOriginX = mouseX;
                    videoOutput.zoomOriginY = mouseY;
                }

                videoOutput.zoomLevel = newZoom;

                // Regular Scroll = Volume
            } else if (wheel.angleDelta.y !== 0 && videoOutput.visible && isActive) {
                const volume = Math.max(0.0, Math.min(1.0, audioOutput.volume + (wheel.angleDelta.y > 0 ? AppConstants.volumeStep : -AppConstants.volumeStep)));
                if (root.host)
                    root.host.volume = volume;
                else
                    audioOutput.volume = volume;
                showVolume();
            }
        }
    }
    Connections {
        function onPlaylistOverlayOpenChanged() {
            if (!mainWindow.playlistOverlayOpen && customMediaPlayer.playbackState === MediaPlayer.PlayingState)
                hideControlsTimer.restart();
        }

        target: mainWindow
    }
    Timer {
        id: hideControlsTimer

        interval: AppConstants.controlsHideDelay
        repeat: false

        onTriggered: {
            if (!mainWindow.mediaControlsContainsMouse && !mainWindow.playlistOverlayOpen) {
                controlsAreVisible = false;
                mouseArea.lastPos = Qt.point(-1, -1); // Reset position detector
            }
        }
    }
}
