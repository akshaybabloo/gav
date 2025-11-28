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
                controlsAreVisible = true
                isPlaying = false
                hideControlsTimer.stop()
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

    AudioOutput {
        id: audioOutput
        volume: 0.5
    }

    VideoOutput {
        id: videoOutput
        anchors.fill: parent
        visible: false
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        property point lastPos: Qt.point(mouseX, mouseY)

        onPositionChanged: {
            if (mouseX !== lastPos.x || mouseY !== lastPos.y) {
                controlsAreVisible = true
                if (customMediaPlayer.playbackState === MediaPlayer.PlayingState) {
                    hideControlsTimer.restart()
                }
                lastPos = Qt.point(mouseX, mouseY)
            }
        }

        onWheel: function (wheel) {
            if (wheel.angleDelta.y > 0 && videoOutput.visible) {
                audioOutput.volume = Math.min(audioOutput.volume + 0.05, 1.0)
                volumeColumn.visible = true
                volumeDisplayTimer.restart()
            } else if (wheel.angleDelta.y < 0 && videoOutput.visible) {
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
