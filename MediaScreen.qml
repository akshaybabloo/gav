import QtQuick
import QtMultimedia

Item {
    height: parent.height
    width: parent.width

    required property string path
    property alias player: mediaPlayer
    property alias audioOutput: audioOutput
    property alias videoOutput: videoOutput
    property bool controlsAreVisible: true

    AudioOutput {
        id: audioOutput
        volume: 0.5
    }

    MediaPlayer {
        id: mediaPlayer
        source: path
        videoOutput: mediaPlayer.videoAvailable ? videoOutput : null
        audioOutput: audioOutput

        onPlaybackStateChanged: {
            if (playbackState === MediaPlayer.PlayingState) {
                hideControlsTimer.start()
            } else {
                controlsAreVisible = true
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
                videoOutput.visible = mediaPlayer.videoTracks.length > 0;
                console.log("Media loaded")
                if (mediaPlayer.videoTracks.length > 0) {
                    console.log("This media contains video.")
                }
                if (mediaPlayer.audioTracks.length > 0) {
                    console.log("This media contains audio.")
                }
            } else if (mediaPlayer.mediaStatus === MediaPlayer.NoMedia || mediaPlayer.mediaStatus === MediaPlayer.InvalidMedia) {
                videoOutput.visible = false;
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
            if (!controlBar.containsMouse) {
                controlsAreVisible = false
                mouseArea.lastPos = Qt.point(-1, -1) // Reset position detector
            }
        }
    }

    // MediaControls {
    //     id: controlBar
    //     player: mediaPlayer
    //     audioOutput: audioOutput
    //     videoOutput: videoOutput
    //     width: parent.width // Ensure full width
    //     anchors.bottom: parent.bottom
    //     anchors.bottomMargin: controlsAreVisible ? 0 : -height // No gap
    //     opacity: controlsAreVisible ? 1 : 0

    //     Behavior on anchors.bottomMargin {
    //         NumberAnimation {
    //             duration: 300
    //             easing.type: Easing.OutCubic
    //         }
    //     }
    //     Behavior on opacity {
    //         NumberAnimation {
    //             duration: 300
    //         }
    //     }
    // }
}

