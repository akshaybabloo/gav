import QtQuick
import QtMultimedia

Item {
    height: parent.height
    width: parent.width

    required property string path

    MediaPlayer {
        id: player
        source: path
        videoOutput: videoOutput
        audioOutput: AudioOutput {}
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
        anchors.bottom: parent.bottom
    }
}
