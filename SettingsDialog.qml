import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

import gavqml

Dialog {
    id: root

    required property var audioOutput
    required property var mediaPlayer
    required property bool isDarkTheme
    required property bool checkUpdatesOnStartup

    signal themeToggled(bool isDark)
    signal defaultSpeedChanged(real speed)
    signal checkUpdatesOnStartupToggled(bool enabled)

    anchors.centerIn: parent
    bottomPadding: 20
    leftPadding: 24
    modal: true
    rightPadding: 24
    topPadding: 20
    width: 440

    background: Rectangle {
        border.color: Material.dividerColor
        border.width: 1
        color: Material.background
        radius: 10
    }

    header: Item {
        implicitHeight: 52

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 24
            anchors.right: parent.right
            anchors.rightMargin: 24
            anchors.verticalCenter: parent.verticalCenter
            color: Material.foreground
            font.pixelSize: 16
            font.weight: Font.DemiBold
            text: qsTr("Settings")
        }
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            color: Material.dividerColor
            height: 1
        }
    }

    footer: Item {
        implicitHeight: 60

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            color: Material.dividerColor
            height: 1
        }
        Button {
            id: settingsCloseButton

            anchors.right: parent.right
            anchors.rightMargin: 20
            anchors.verticalCenter: parent.verticalCenter
            Material.background: Material.accent
            Material.foreground: Material.theme === Material.Dark ? "#000000" : "#FFFFFF"
            Material.roundedScale: Material.SmallScale
            text: qsTr("Close")

            onClicked: root.close()
        }
    }

    ColumnLayout {
        spacing: 18
        width: parent.width

        // Appearance section
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                color: Material.foreground
                font.bold: true
                font.pixelSize: 14
                text: qsTr("Appearance")
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Text {
                    color: Material.foreground
                    opacity: 0.7
                    text: qsTr("Theme:")
                }
                Item { Layout.fillWidth: true }
                Text {
                    color: Material.foreground
                    opacity: 0.7
                    text: qsTr("Light")
                }
                Switch {
                    id: themeSwitch
                    checked: isDarkTheme
                    onCheckedChanged: {
                        themeToggled(checked);
                    }
                }
                Text {
                    color: Material.foreground
                    opacity: 0.7
                    text: qsTr("Dark")
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Material.dividerColor
        }

        // Volume section
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                color: Material.foreground
                font.bold: true
                font.pixelSize: 14
                text: qsTr("Audio")
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Text {
                    color: Material.foreground
                    opacity: 0.7
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
                    color: Material.foreground
                    opacity: 0.7
                    text: Math.round(defaultVolumeSlider.value * 100) + "%"
                    Layout.preferredWidth: 40
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Material.dividerColor
        }

        // Playback section
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                color: Material.foreground
                font.bold: true
                font.pixelSize: 14
                text: qsTr("Playback")
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Text {
                    color: Material.foreground
                    opacity: 0.7
                    text: qsTr("Default Speed:")
                }
                ComboBox {
                    id: defaultSpeedCombo
                    Layout.fillWidth: true
                    model: AppConstants.playbackSpeeds.map(function(s) { return s + "x"; })
                    currentIndex: AppConstants.playbackSpeeds.indexOf(mediaPlayer.playbackRate) >= 0
                                  ? AppConstants.playbackSpeeds.indexOf(mediaPlayer.playbackRate)
                                  : 3
                    onActivated: function(index) {
                        mediaPlayer.playbackRate = AppConstants.playbackSpeeds[index];
                        defaultSpeedChanged(AppConstants.playbackSpeeds[index]);
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Material.dividerColor
        }

        // Updates section
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                color: Material.foreground
                font.bold: true
                font.pixelSize: 14
                text: qsTr("Updates")
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Text {
                    color: Material.foreground
                    opacity: 0.7
                    text: qsTr("Check for updates on startup")
                }
                Item { Layout.fillWidth: true }
                Switch {
                    id: checkUpdatesSwitch
                    checked: root.checkUpdatesOnStartup
                    onCheckedChanged: root.checkUpdatesOnStartupToggled(checked)
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Material.dividerColor
        }

        // Keyboard shortcuts reference
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                color: Material.foreground
                font.bold: true
                font.pixelSize: 14
                text: qsTr("Keyboard Shortcuts")
            }
            GridLayout {
                columns: 2
                columnSpacing: 20
                rowSpacing: 4

                Text { color: Material.foreground; opacity: 0.5; text: "Space" }
                Text { color: Material.foreground; opacity: 0.7; text: qsTr("Play/Pause") }

                Text { color: Material.foreground; opacity: 0.5; text: "Left/Right" }
                Text { color: Material.foreground; opacity: 0.7; text: qsTr("Seek 5 seconds") }

                Text { color: Material.foreground; opacity: 0.5; text: "Scroll" }
                Text { color: Material.foreground; opacity: 0.7; text: qsTr("Volume") }

                Text { color: Material.foreground; opacity: 0.5; text: "Ctrl+Scroll" }
                Text { color: Material.foreground; opacity: 0.7; text: qsTr("Zoom") }

                Text { color: Material.foreground; opacity: 0.5; text: "Double-click" }
                Text { color: Material.foreground; opacity: 0.7; text: qsTr("Fullscreen") }
            }
        }
    }
}
