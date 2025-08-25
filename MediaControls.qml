import QtQuick
import QtQuick.Controls
import QtMultimedia
import QtQuick.Layouts

Item {
    height: 60
    width: parent.width

    required property MediaPlayer player

    function formatTime(ms) {
        var seconds = Math.floor(ms / 1000);
        var minutes = Math.floor(seconds / 60);
        var hours = Math.floor(minutes / 60);
        seconds = seconds % 60;
        return Qt.formatTime(new Date(0, 0, hours, 0, minutes, seconds), "hh:mm:ss");
    }

    Rectangle {
        id: controlBar
        height: parent.height
        width: parent.width
        color: "#80000000"
        anchors.bottom: parent.bottom

        RowLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 15

            Button {
                id: playPauseButton
                text: player.playbackState === MediaPlayer.PlayingState ? "Pause" : "Play"
                onClicked: {
                    if (player.playbackState === MediaPlayer.PlayingState) {
                        player.pause()
                    } else {
                        player.play()
                    }
                }
                Layout.preferredWidth: 80
            }

            Text {
                id: timeLabel
                text: formatTime(player.position) + " / " + formatTime(player.duration)
                color: "white"
                verticalAlignment: Text.AlignVCenter
            }

            Slider {
                id: seekSlider
                from: 0
                to: player.duration
                value: player.position
                Layout.fillWidth: true

                onMoved: player.position = value

                // Timer to update the slider position
                Timer {
                    interval: 500
                    running: player.playbackState === MediaPlayer.PlayingState
                    repeat: true
                    onTriggered: {
                        if (!seekSlider.pressed) { // Do not update while user is seeking
                            seekSlider.value = player.position
                        }
                    }
                }
            }

            RowLayout {
                spacing: 5
                Layout.alignment: Qt.AlignRight

                // You might want to use an icon for the volume
                Text {
                    text: "Volume:"
                    color: "white"
                    verticalAlignment: Text.AlignVCenter
                }

                Slider {
                    id: volumeSlider
                    from: 0
                    to: 1.0
                    value: player.audioOutput.volume
                    onValueChanged: player.audioOutput.volume = value
                    Layout.preferredWidth: 100
                }
            }
        }
    }
}