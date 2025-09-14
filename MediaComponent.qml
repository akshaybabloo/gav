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
    property bool mediaLoaded: false
    property bool isVideoAndPlaying: isVideo && isPlaying

    property bool isVideo: customMediaPlayer.hasVideo
    property bool isPlaying: false

    CustomMediaPlayer {
        id: customMediaPlayer
        source: path
        videoOutput: videoOutput
        audioOutput: audioOutput

        onPlaybackStateChanged: {
            if (playbackState === MediaPlayer.PlayingState) {
                isPlaying = true
                hideControlsTimer.start()
            } else if (playbackState === MediaPlayer.PausedState) {
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

        onMediaStatusChanged: {
            if (mediaStatus === MediaPlayer.LoadedMedia) {
                mediaLoaded = true
                videoOutput.visible = customMediaPlayer.hasVideo
                console.log("Media loaded")
            } else if (mediaStatus === MediaPlayer.NoMedia
                       || mediaStatus === MediaPlayer.InvalidMedia) {
                videoOutput.visible = false
                mediaLoaded = false
            }
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

