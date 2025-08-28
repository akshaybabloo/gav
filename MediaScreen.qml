import QtQuick
import QtMultimedia

Item {
    height: parent.height
    width: parent.width

    required property string path

    property alias mediaPlayer: mediaPlayer
    property alias audioOutput: audioOutput
    property alias videoOutput: videoOutput

    property bool controlsAreVisible: true
    property bool mediaLoaded: false
    property bool isVideoAndPlaying: isVideo && isPlaying

    property bool isVideo: false
    property bool isPlaying: false

    AudioOutput {
        id: audioOutput
        volume: 0.5
    }

    MediaPlayer {
        id: mediaPlayer
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

        onErrorOccurred: function (error, errorString) {
            if (error !== MediaPlayer.NoError) {
                console.log(error, errorString)
                unsupportedFileDialog.open()
            }
        }

        onMediaStatusChanged: {
            if (mediaPlayer.mediaStatus === MediaPlayer.LoadedMedia) {
                mediaLoaded = true
                videoOutput.visible = mediaPlayer.videoTracks.length > 0
                console.log("Media loaded")
                if (mediaPlayer.videoTracks.length > 0) {
                    isVideo = true
                }
                if (mediaPlayer.audioTracks.length === 0
                        && mediaPlayer.videoTracks.length > 0) {
                    isVideo = true
                }
                if (mediaPlayer.videoTracks.length === 0) {
                    isVideo = false
                }
            } else if (mediaPlayer.mediaStatus === MediaPlayer.NoMedia
                       || mediaPlayer.mediaStatus === MediaPlayer.InvalidMedia) {
                videoOutput.visible = false
                mediaLoaded = false
                isVideo = false
            }
        }
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
                if (mediaPlayer.playbackState === MediaPlayer.PlayingState) {
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

