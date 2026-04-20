import QtQuick
import QtQuick.Effects
import QtMultimedia
import gavqml

Item {
    property alias audioOutput: audioOutput
    property bool controlsAreVisible: true
    property bool isPlaying: false
    property bool isVideo: customMediaPlayer.hasVideo
    property bool isVideoAndPlaying: isVideo && isPlaying
    property bool mediaLoaded: customMediaPlayer.mediaLoaded
    property alias mediaPlayer: customMediaPlayer
    required property string path
    property alias videoOutput: videoOutput

    signal stopped
    signal fullscreenToggleRequested

    height: parent.height
    width: parent.width

    CustomMediaPlayer {
        id: customMediaPlayer

        audioOutput: audioOutput
        source: path
        videoOutput: videoOutput

        onErrorOccurred: function (errorString) {
            console.log("MediaPlayer error:", errorString);
            playbackErrorDialog.open();
        }
        onPlaybackStateChanged: function (state) {
            if (state === MediaPlayer.PlayingState) {
                isPlaying = true;
                hideControlsTimer.start();
            } else if (state === MediaPlayer.PausedState) {
                isPlaying = true; // We still want to show the video when paused
            } else {
                // Stopped state - reset everything
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
            if (videoOutput.zoomLevel > 1.0) {
                isDragging = true;
                dragStartPos = Qt.point(mouseX, mouseY);
                startPanX = videoOutput.panX;
                startPanY = videoOutput.panY;
                cursorShape = Qt.ClosedHandCursor;
            }
        }
        onReleased: function (mouse) {
            isDragging = false;
            cursorShape = videoOutput.zoomLevel > 1.0 ? Qt.OpenHandCursor : Qt.ArrowCursor;
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
            } else if (wheel.angleDelta.y > 0 && videoOutput.visible && isActive) {
                audioOutput.volume = Math.min(audioOutput.volume + AppConstants.volumeStep, 1.0);
                volumeColumn.visible = true;
                volumeDisplayTimer.restart();
            } else if (wheel.angleDelta.y < 0 && videoOutput.visible && isActive) {
                audioOutput.volume = Math.max(audioOutput.volume - AppConstants.volumeStep, 0.0);
                volumeColumn.visible = true;
                volumeDisplayTimer.restart();
            }
        }
    }
    Timer {
        id: hideControlsTimer

        interval: AppConstants.controlsHideDelay
        repeat: false

        onTriggered: {
            if (!mainWindow.mediaControlsContainsMouse) {
                controlsAreVisible = false;
                mouseArea.lastPos = Qt.point(-1, -1); // Reset position detector
            }
        }
    }
}
