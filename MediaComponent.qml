import QtQuick
import QtMultimedia
import gavqml

Item {
    height: parent.height
    width: parent.width

    required property string path

    property alias mediaPlayer: customMediaPlayer
    property alias audioOutput: audioOutput
    property alias videoOutput: videoOutput

    property bool controlsAreVisible: true
    property bool mediaLoaded: customMediaPlayer.mediaLoaded
    property bool isVideoAndPlaying: isVideo && isPlaying

    property bool isVideo: customMediaPlayer.hasVideo
    property bool isPlaying: false

    signal stopped()

    CustomMediaPlayer {
        id: customMediaPlayer
        source: path
        videoOutput: videoOutput
        audioOutput: audioOutput

        onVideoVisibilityChanged: function (visible) {
            videoOutput.visible = visible
        }

        onPlaybackStateChanged: function (state) {
            if (state === MediaPlayer.PlayingState) {
                isPlaying = true
                hideControlsTimer.start()
            } else if (state === MediaPlayer.PausedState) {
                isPlaying = true // We still want to show the video when paused
            } else {
                // Stopped state - reset everything
                controlsAreVisible = true
                isPlaying = false
                hideControlsTimer.stop()

                // Reset zoom and pan
                videoOutput.zoomLevel = 1.0
                videoOutput.panX = 0
                videoOutput.panY = 0
                videoOutput.zoomOriginX = videoOutput.width / 2
                videoOutput.zoomOriginY = videoOutput.height / 2

                // Notify parent to reset path and title
                stopped()
            }
        }

        onErrorOccurred: function (errorString) {
            console.log("MediaPlayer error:", errorString)
            unsupportedFileDialog.open()
        }
    }

    // Vertical volume display
    Column {
        id: volumeColumn
        anchors.right: parent.right
        anchors.rightMargin: 20
        anchors.verticalCenter: parent.verticalCenter
        spacing: 10
        visible: false
        z: 100
        opacity: 0.7

        Text {
            id: volumeText
            text: Math.round(audioOutput.volume * 100) + "%"
            color: "white"
            font.pixelSize: 25
            font.bold: true
            width: 50
            horizontalAlignment: Text.AlignHCenter
            style: Text.Outline
            styleColor: "black"
        }

        Rectangle {
            id: volumeViz
            width: 50
            height: 250
            color: "#2a2a2a"
            border.color: "white"
            border.width: 2
            radius: 5
            clip: true

            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: parent.height * audioOutput.volume
                color: "#4CAF50"
                radius: 5
            }
        }

        Timer {
            id: volumeDisplayTimer
            interval: 2000
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
        width: zoomText.width + 30
        height: zoomText.height + 20
        color: "#2a2a2a"
        border.color: "white"
        border.width: 2
        radius: 8
        visible: videoOutput.zoomLevel > 1.0
        z: 100
        opacity: 0.8

        Text {
            id: zoomText
            anchors.centerIn: parent
            text: "Zoom: " + Math.round(videoOutput.zoomLevel * 100) + "%"
            color: "white"
            font.pixelSize: 18
            font.bold: true
        }

        // Auto-hide after 2 seconds of no zoom changes
        Timer {
            id: zoomDisplayTimer
            interval: 2000
            repeat: false
            running: videoOutput.zoomLevel > 1.0
            onTriggered: {
                if (videoOutput.zoomLevel <= 1.0) {
                    zoomIndicator.visible = false
                }
            }
        }

        Behavior on opacity {
            NumberAnimation { duration: 200 }
        }
    }

    AudioOutput {
        id: audioOutput
        volume: 0.5
    }

    VideoOutput {
        id: videoOutput
        anchors.fill: parent
        visible: false

        // Zoom and pan properties
        property real zoomLevel: 1.0
        property real panX: 0
        property real panY: 0

        // Zoom origin point (where the mouse was when zooming)
        property real zoomOriginX: width / 2
        property real zoomOriginY: height / 2

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
            NumberAnimation { duration: 50; easing.type: Easing.OutQuad }
        }

        Behavior on panX {
            NumberAnimation { duration: 100; easing.type: Easing.OutQuad }
        }

        Behavior on panY {
            NumberAnimation { duration: 100; easing.type: Easing.OutQuad }
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton

        property point lastPos: Qt.point(mouseX, mouseY)
        property point dragStartPos: Qt.point(0, 0)
        property bool isDragging: false
        property real startPanX: 0
        property real startPanY: 0

        onPositionChanged: {
            if (isDragging && videoOutput.zoomLevel > 1.0) {
                // Calculate drag delta
                var deltaX = mouseX - dragStartPos.x
                var deltaY = mouseY - dragStartPos.y

                // Update pan position with constraints
                var maxPanX = videoOutput.width * (videoOutput.zoomLevel - 1) / 2
                var maxPanY = videoOutput.height * (videoOutput.zoomLevel - 1) / 2

                videoOutput.panX = Math.max(-maxPanX, Math.min(maxPanX, startPanX + deltaX))
                videoOutput.panY = Math.max(-maxPanY, Math.min(maxPanY, startPanY + deltaY))
            } else if (mouseX !== lastPos.x || mouseY !== lastPos.y) {
                controlsAreVisible = true
                if (customMediaPlayer.playbackState === MediaPlayer.PlayingState) {
                    hideControlsTimer.restart()
                }
                lastPos = Qt.point(mouseX, mouseY)
            }
        }

        onPressed: function (mouse) {
            if (videoOutput.zoomLevel > 1.0) {
                isDragging = true
                dragStartPos = Qt.point(mouseX, mouseY)
                startPanX = videoOutput.panX
                startPanY = videoOutput.panY
                cursorShape = Qt.ClosedHandCursor
            }
        }

        onReleased: function (mouse) {
            isDragging = false
            cursorShape = videoOutput.zoomLevel > 1.0 ? Qt.OpenHandCursor : Qt.ArrowCursor
        }

        onDoubleClicked: function (mouse) {
            mainWindow.visibility = mainWindow.visibility
                    === Window.FullScreen ? Window.Windowed : Window.FullScreen
        }

        cursorShape: {
            if (isDragging) {
                return Qt.ClosedHandCursor
            } else if (videoOutput.zoomLevel > 1.0) {
                return Qt.OpenHandCursor
            } else {
                return Qt.ArrowCursor
            }
        }

        onWheel: function (wheel) {
            // Only handle scroll when video is playing or paused (not stopped)
            var isActive = customMediaPlayer.playbackState !== MediaPlayer.StoppedState

            // Ctrl + Scroll = Zoom (Chrome-like multiplicative scaling)
            if (wheel.modifiers & Qt.ControlModifier && videoOutput.visible && isActive) {
                // Each scroll step multiplies/divides by ~1.25 (Chrome uses similar factor)
                var zoomFactor = wheel.angleDelta.y > 0 ? 1.25 : 0.8
                var newZoom = Math.max(1.0, Math.min(5.0, videoOutput.zoomLevel * zoomFactor))

                // Reset when zooming back to 1.0
                if (newZoom === 1.0) {
                    videoOutput.panX = 0
                    videoOutput.panY = 0
                    videoOutput.zoomOriginX = videoOutput.width / 2
                    videoOutput.zoomOriginY = videoOutput.height / 2
                } else {
                    // Set zoom origin to mouse position for zoom-to-cursor effect
                    videoOutput.zoomOriginX = mouseX
                    videoOutput.zoomOriginY = mouseY
                }

                videoOutput.zoomLevel = newZoom

            // Regular Scroll = Volume
            } else if (wheel.angleDelta.y > 0 && videoOutput.visible && isActive) {
                audioOutput.volume = Math.min(audioOutput.volume + 0.05, 1.0)
                volumeColumn.visible = true
                volumeDisplayTimer.restart()
            } else if (wheel.angleDelta.y < 0 && videoOutput.visible && isActive) {
                audioOutput.volume = Math.max(audioOutput.volume - 0.05, 0.0)
                volumeColumn.visible = true
                volumeDisplayTimer.restart()
            }
        }
    }

    Timer {
        id: hideControlsTimer
        interval: 3000
        repeat: false
        onTriggered: {
            if (!mainWindow.mediaControlsContainsMouse) {
                controlsAreVisible = false
                mouseArea.lastPos = Qt.point(-1, -1) // Reset position detector
            }
        }
    }
}
