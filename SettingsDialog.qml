import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import gavqml

Dialog {
    id: root

    required property var audioOutput
    required property var mediaPlayer

    modal: true
    title: qsTr("Settings")
    standardButtons: Dialog.Close
    width: 400

    ColumnLayout {
        width: parent.width
        spacing: 20

        // Volume section
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                color: "white"
                font.bold: true
                font.pixelSize: 14
                text: qsTr("Audio")
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Text {
                    color: "#aaa"
                    text: qsTr("Default Volume:")
                }
                Slider {
                    id: defaultVolumeSlider
                    Layout.fillWidth: true
                    from: 0
                    to: 1
                    value: audioOutput.volume
                    onValueChanged: {
                        audioOutput.volume = value;
                    }
                }
                Text {
                    color: "#aaa"
                    text: Math.round(defaultVolumeSlider.value * 100) + "%"
                    Layout.preferredWidth: 40
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: "#444"
        }

        // Playback section
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                color: "white"
                font.bold: true
                font.pixelSize: 14
                text: qsTr("Playback")
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Text {
                    color: "#aaa"
                    text: qsTr("Default Speed:")
                }
                ComboBox {
                    id: defaultSpeedCombo
                    Layout.fillWidth: true
                    model: AppConstants.playbackSpeeds.map(function(s) { return s + "x"; })
                    currentIndex: AppConstants.playbackSpeeds.indexOf(mediaPlayer.playbackRate) >= 0
                                  ? AppConstants.playbackSpeeds.indexOf(mediaPlayer.playbackRate)
                                  : 3
                    onCurrentIndexChanged: {
                        if (currentIndex >= 0) {
                            mediaPlayer.playbackRate = AppConstants.playbackSpeeds[currentIndex];
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: "#444"
        }

        // Interface section
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                color: "white"
                font.bold: true
                font.pixelSize: 14
                text: qsTr("Interface")
            }
            CheckBox {
                id: autoHideControlsCheck
                text: qsTr("Auto-hide controls during playback")
                checked: true

                contentItem: Text {
                    text: parent.text
                    color: "#aaa"
                    leftPadding: parent.indicator.width + parent.spacing
                    verticalAlignment: Text.AlignVCenter
                }
            }
            Text {
                color: "#666"
                font.pixelSize: 11
                text: qsTr("Controls hide after ") + (AppConstants.controlsHideDelay / 1000) + qsTr(" seconds of inactivity")
                leftPadding: 26
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: "#444"
        }

        // Keyboard shortcuts reference
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                color: "white"
                font.bold: true
                font.pixelSize: 14
                text: qsTr("Keyboard Shortcuts")
            }
            GridLayout {
                columns: 2
                columnSpacing: 20
                rowSpacing: 4

                Text { color: "#888"; text: "Space" }
                Text { color: "#aaa"; text: qsTr("Play/Pause") }

                Text { color: "#888"; text: "Left/Right" }
                Text { color: "#aaa"; text: qsTr("Seek 5 seconds") }

                Text { color: "#888"; text: "Scroll" }
                Text { color: "#aaa"; text: qsTr("Volume") }

                Text { color: "#888"; text: "Ctrl+Scroll" }
                Text { color: "#aaa"; text: qsTr("Zoom") }

                Text { color: "#888"; text: "Double-click" }
                Text { color: "#aaa"; text: qsTr("Fullscreen") }
            }
        }
    }
}
