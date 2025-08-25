import QtQuick
import QtQuick.Controls
import QtMultimedia
import QtQuick.Layouts
import QtQuick.Controls.Material


Item {
    height: 60
    width: parent.width

    property bool containsMouse: controlMouseArea.containsMouse

    MouseArea {
        id: controlMouseArea
        anchors.fill: parent
        hoverEnabled: true
    }

    required property MediaPlayer player
    required property AudioOutput audioOutput
    required property VideoOutput videoOutput

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
        anchors.horizontalCenter: parent.horizontalCenter

        RowLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.margins: 10
            spacing: 15

            RowLayout {
                spacing: 20

                Button {
                    id: playPauseButton
                    text: player.playbackState === MediaPlayer.PlayingState ? "\ue034" : "\ue037"
                    font.family: materialSymbolsOutlined.name
                    scale: 1.5
                    onClicked: {
                        if (player.playbackState === MediaPlayer.PlayingState) {
                            player.pause()
                        } else {
                            player.play()
                        }
                    }
                    Material.roundedScale: Material.NotRounded
                    Layout.preferredWidth: 30
                }

                Button {
                    id: stopButton
                    text: "\ue047"
                    enabled: player.playbackState !== MediaPlayer.StoppedState
                    font.family: materialSymbolsOutlined.name
                    scale: 1.5
                    onClicked: {
                        player.stop()
                    }
                    Material.roundedScale: Material.NotRounded
                    Layout.preferredWidth: 30
                }
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

                ToolButton {
                    text: "\ue04d"
                    scale: 1.5
                    font.family: materialSymbolsOutlined.name
                    onClicked: {
                        audioOutput.muted = !audioOutput.muted
                        text = audioOutput.muted ? "\ue04e" : "\ue04d"
                        volumeSlider.enabled = !audioOutput.muted
                    }
                    Layout.preferredWidth: 15
                }

                Slider {
                    id: volumeSlider
                    from: 0
                    to: 1.0
                    value: audioOutput.volume
                    onValueChanged: audioOutput.volume = value
                    Layout.preferredWidth: 100
                }

                Button {
                    text: mainWindow.visibility === Window.FullScreen ? "\ue5d1" : "\ue5d0"
                    scale: 1.5
                    font.family: materialSymbolsOutlined.name
                    onClicked: {
                        mainWindow.visibility = mainWindow.visibility === Window.FullScreen ? Window.Windowed : Window.FullScreen
                    }
                    Layout.preferredWidth: 25
                    Material.roundedScale: Material.NotRounded
                    Material.background: "transparent"
                }
            }
        }
    }
}
