import QtQuick
import QtQuick.Controls
import QtMultimedia
import QtQuick.Layouts
import QtQuick.Controls.Material

Item {
    height: 60
    width: parent.width

    property real previousVolume: 0.5
    property bool containsMouse: controlMouseArea.containsMouse

    MouseArea {
        id: controlMouseArea
        anchors.fill: parent
        hoverEnabled: true
    }

    required property var player
    required property var audioOutput
    required property var videoOutput
    required property bool mediaLoaded

    function formatTime(ms) {
        var seconds = Math.floor(ms / 1000)
        var minutes = Math.floor(seconds / 60)
        var hours = Math.floor(minutes / 60)
        seconds = seconds % 60
        return Qt.formatTime(new Date(0, 0, hours, 0, minutes, seconds),
                             "hh:mm:ss")
    }

    function updateVolumeIcon() {
        if (audioOutput.muted || audioOutput.volume === 0) {
            volumeButton.text = "\ue04e"
        } else if (audioOutput.volume < 0.5) {
            volumeButton.text = "\ue04d"
        } else if (audioOutput.volume < 1.0) {
            volumeButton.text = "\ue050"
        } else {
            // volume is 1.0
            volumeButton.text = "\ue98e"
        }
    }

    Rectangle {
        id: controlBar
        height: parent.height
        width: parent.width
        color: "#80000000"
        anchors.horizontalCenter: parent.horizontalCenter

        ColumnLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.margins: {
                left: 10
                right: 10
            }

            spacing: 5

            // Seek row
            RowLayout {
                spacing: 15
                Text {
                    id: timeLabel
                    text: formatTime(player.position) + " / " + formatTime(
                              player.duration)
                    color: "white"
                    verticalAlignment: Text.AlignVCenter
                }

                Slider {
                    id: seekSlider
                    from: 0
                    to: player.duration
                    enabled: mediaLoaded

                    onMoved: player.position = value

                    Layout.fillWidth: true
                    Layout.preferredHeight: 10

                    // Timer to update the slider position
                    Timer {
                        interval: 500
                        running: player.playbackState === MediaPlayer.PlayingState
                        repeat: true
                        onTriggered: {
                            if (!seekSlider.pressed) {
                                // Do not update while user is seeking
                                seekSlider.value = player.position
                            }
                        }
                    }
                }
            }

            RowLayout {
                // Play/pause buttons
                RowLayout {
                    spacing: 15

                    Button {
                        id: playPauseButton
                        enabled: mediaLoaded
                        text: player.playbackState
                              === MediaPlayer.PlayingState ? "\ue034" : "\ue037"
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
                        Layout.preferredWidth: 25
                        Layout.preferredHeight: 30

                        hoverEnabled: true

                        ToolTip.text: player.playbackState
                                      === MediaPlayer.PlayingState ? qsTr("Pause") : qsTr(
                                                                         "Play")
                        ToolTip.delay: 1000
                        ToolTip.timeout: 5000
                        ToolTip.visible: hovered
                    }

                    Button {
                        id: fastRewindButton
                        text: "\ue020"
                        font.family: materialSymbolsOutlined.name
                        enabled: player.playbackState !== MediaPlayer.StoppedState
                        scale: 1.5
                        Material.roundedScale: Material.NotRounded
                        Layout.preferredWidth: 25
                        Layout.preferredHeight: 30
                        font.weight: Font.Light
                        hoverEnabled: true

                        ToolTip.text: qsTr("Fast rewind")
                        ToolTip.delay: 1000
                        ToolTip.timeout: 5000
                        ToolTip.visible: hovered

                        onClicked: {
                            // TODO: One second forward

                        }

                        onPressAndHold: {
                            // TODO: One second forward
                        }

                        onReleased: {
                            // TODO: Stop going forward on release
                        }

                        onDoubleClicked: {
                            // TODO: Continue going forward 10x, 20x and 30x and single click to cancel. If end is reached cancel and play
                        }
                    }

                    Button {
                        id: stopButton
                        text: "\ue047"
                        enabled: player.playbackState !== MediaPlayer.StoppedState
                        font.family: materialSymbolsOutlined.name
                        scale: 1.5
                        onClicked: {
                            player.stop()
                            seekSlider.value = 0
                        }
                        Material.roundedScale: Material.NotRounded
                        Layout.preferredWidth: 25
                        Layout.preferredHeight: 30
                        font.weight: Font.Light
                        hoverEnabled: true

                        ToolTip.text: qsTr("Stop")
                        ToolTip.delay: 1000
                        ToolTip.timeout: 5000
                        ToolTip.visible: hovered
                    }

                    Button {
                        id: fastForwardButton
                        text: "\ue01f"
                        font.family: materialSymbolsOutlined.name
                        enabled: player.playbackState !== MediaPlayer.StoppedState
                        scale: 1.5
                        Material.roundedScale: Material.NotRounded
                        Layout.preferredWidth: 25
                        Layout.preferredHeight: 30
                        font.weight: Font.Light

                        hoverEnabled: true

                        ToolTip.text: qsTr("Fast forward")
                        ToolTip.delay: 1000
                        ToolTip.timeout: 5000
                        ToolTip.visible: hovered

                        onClicked: {
                            // TODO: One second rewind

                        }

                        onPressAndHold: {
                            // TODO: One second rewind
                        }

                        onReleased: {
                            // TODO: Stop going rewind on release
                        }

                        onDoubleClicked: {
                            // TODO: Continue going rewind 10x, 20x and 30x and single click to cancel. If end is reached cancel and play
                        }
                    }

                    Button {
                        id: playListButton
                        text: "\ue3c7"
                        enabled: true
                        font.family: materialSymbolsOutlined.name
                        scale: 1.5
                        onClicked: {
                            playlistComponent.visible = !playlistComponent.visible
                        }
                        Material.roundedScale: Material.NotRounded
                        Layout.preferredWidth: 25
                        Layout.preferredHeight: 30
                        font.weight: Font.Light
                        hoverEnabled: true

                        ToolTip {
                            text: qsTr("Toggle playlist")
                            delay: 1000
                            timeout: 5000
                            visible: playListButton.hovered
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                }

                // Volume seek and mute
                RowLayout {
                    spacing: 5
                    Layout.alignment: Qt.AlignRight

                    ToolButton {
                        id: volumeButton
                        text: "\ue04d"
                        scale: 1.5
                        font.family: materialSymbolsOutlined.name
                        onClicked: {
                            audioOutput.muted = !audioOutput.muted
                            if (audioOutput.muted) {
                                previousVolume = audioOutput.volume
                                audioOutput.volume = 0
                            } else {
                                audioOutput.volume = previousVolume
                            }
                            updateVolumeIcon()
                        }
                        Layout.preferredWidth: 15
                        Layout.preferredHeight: 25
                        font.weight: Font.Light

                        hoverEnabled: true

                        ToolTip.text: qsTr("Volume")
                        ToolTip.delay: 1000
                        ToolTip.timeout: 5000
                        ToolTip.visible: hovered
                    }

                    Slider {
                        id: volumeSlider
                        from: 0
                        to: 1.0
                        value: audioOutput.volume
                        onValueChanged: {
                            audioOutput.volume = value
                            if (value > 0) {
                                audioOutput.muted = false
                            }
                            updateVolumeIcon()
                        }

                        Layout.preferredWidth: 100
                        Layout.preferredHeight: 10
                    }

                    Button {
                        text: mainWindow.visibility === Window.FullScreen ? "\ue5d1" : "\ue5d0"
                        scale: 1.5
                        font.family: materialSymbolsOutlined.name
                        onClicked: {
                            mainWindow.visibility = mainWindow.visibility
                                    === Window.FullScreen ? Window.Windowed : Window.FullScreen
                        }
                        Layout.preferredWidth: 25
                        Layout.preferredHeight: 30
                        Material.roundedScale: Material.NotRounded
                        Material.background: "transparent"

                        hoverEnabled: true

                        ToolTip.text: qsTr("Toggle fullscreen")
                        ToolTip.delay: 1000
                        ToolTip.timeout: 5000
                        ToolTip.visible: hovered
                    }
                }
            }
        }
    }
}
