import QtQuick
import QtMultimedia

Item {
    height: parent.height
    width: parent.width

    required property string path

    AudioOutput {
        id: audioOutput
        volume: 0.5
    }

    MediaPlayer {
        id: player
        source: path
        videoOutput: videoOutput
        audioOutput: audioOutput
    }

    VideoOutput {
        id: videoOutput
        anchors.fill: parent
    }

    Timer {
        interval: 3000
        running: false
        repeat: false
        onTriggered: {
            controlBar.visible = false
        }
    }

    MediaControls {
        id: controlBar
        player: player
        audioOutput: audioOutput
        videoOutput: videoOutput
        anchors.bottom: parent.bottom
    }
}
