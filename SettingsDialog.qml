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
    signal preferredAudioLanguageEdited(string language)
    signal preferredSubtitleLanguageEdited(string language)

    component SectionTitle: Text {
        Layout.topMargin: 6
        color: Material.foreground
        font.bold: true
        font.pixelSize: 14
    }
    component SettingLabel: Text {
        Layout.fillWidth: true
        color: Material.foreground
        elide: Text.ElideRight
        opacity: 0.7
    }
    component Divider: Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 1
        color: Material.dividerColor
    }
    component SettingsPage: ScrollView {
        id: page

        default property alias content: pageColumn.data

        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            id: pageColumn

            spacing: 12
            width: page.availableWidth
        }
    }

    anchors.centerIn: parent
    bottomPadding: 0
    height: parent ? Math.min(520, parent.height - 48) : 520
    leftPadding: 0
    modal: true
    rightPadding: 0
    topPadding: 0
    width: parent ? Math.min(560, parent.width - 48) : 560

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
        anchors.fill: parent
        spacing: 0

        TabBar {
            id: tabs

            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            clip: true

            background: Rectangle {
                color: "transparent"

                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    color: Material.dividerColor
                    height: 1
                }
            }

            TabButton {
                text: qsTr("General")
            }
            TabButton {
                text: qsTr("Playback")
            }
            TabButton {
                text: qsTr("Languages")
            }
            TabButton {
                text: qsTr("Shortcuts")
            }
        }
        StackLayout {
            Layout.bottomMargin: 12
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 12
            Layout.topMargin: 12
            currentIndex: tabs.currentIndex

            SettingsPage {
                SectionTitle {
                    text: qsTr("Appearance")
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.rightMargin: 12
                    spacing: 10

                    SettingLabel {
                        text: qsTr("Theme")
                    }
                    Text {
                        color: Material.foreground
                        opacity: 0.7
                        text: qsTr("Light")
                    }
                    Switch {
                        id: themeSwitch

                        Accessible.name: qsTr("Dark theme")
                        checked: root.isDarkTheme

                        onCheckedChanged: root.themeToggled(checked)
                    }
                    Text {
                        color: Material.foreground
                        opacity: 0.7
                        text: qsTr("Dark")
                    }
                }
                Divider {
                    Layout.rightMargin: 12
                }
                SectionTitle {
                    text: qsTr("Updates")
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.rightMargin: 12
                    spacing: 10

                    SettingLabel {
                        text: qsTr("Check for updates on startup")
                    }
                    Switch {
                        id: checkUpdatesSwitch

                        Accessible.name: qsTr("Check for updates on startup")
                        checked: root.checkUpdatesOnStartup

                        onCheckedChanged: root.checkUpdatesOnStartupToggled(checked)
                    }
                }
            }
            SettingsPage {
                SectionTitle {
                    text: qsTr("Audio")
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.rightMargin: 12
                    spacing: 10

                    SettingLabel {
                        Layout.fillWidth: false
                        text: qsTr("Default volume")
                    }
                    Slider {
                        id: defaultVolumeSlider

                        Accessible.name: qsTr("Default volume")
                        Layout.fillWidth: true
                        from: 0
                        to: 1
                        value: root.audioOutput.volume

                        onValueChanged: root.audioOutput.volume = value
                    }
                    Text {
                        Layout.preferredWidth: 40
                        color: Material.foreground
                        horizontalAlignment: Text.AlignRight
                        opacity: 0.7
                        text: Math.round(defaultVolumeSlider.value * 100) + "%"
                    }
                }
                Divider {
                    Layout.rightMargin: 12
                }
                SectionTitle {
                    text: qsTr("Playback")
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.rightMargin: 12
                    spacing: 10

                    SettingLabel {
                        text: qsTr("Default speed")
                    }
                    ComboBox {
                        id: defaultSpeedCombo

                        Accessible.name: qsTr("Default speed")
                        Layout.preferredWidth: 120
                        currentIndex: AppConstants.playbackSpeeds.indexOf(root.mediaPlayer.playbackRate) >= 0 ? AppConstants.playbackSpeeds.indexOf(root.mediaPlayer.playbackRate) : 3
                        model: AppConstants.playbackSpeeds.map(function (s) {
                            return s + "x";
                        })

                        onActivated: function (index) {
                            root.mediaPlayer.playbackRate = AppConstants.playbackSpeeds[index];
                            root.defaultSpeedChanged(AppConstants.playbackSpeeds[index]);
                        }
                    }
                }
            }
            SettingsPage {
                SectionTitle {
                    text: qsTr("Preferred languages")
                }
                Text {
                    Layout.fillWidth: true
                    Layout.rightMargin: 12
                    color: Material.foreground
                    opacity: 0.5
                    text: qsTr("Two- or three-letter language codes, such as en, fra or ja. Matching tracks are chosen automatically when a file opens.")
                    wrapMode: Text.WordWrap
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.rightMargin: 12
                    spacing: 10

                    SettingLabel {
                        text: qsTr("Subtitles")
                    }
                    TextField {
                        Accessible.name: qsTr("Preferred subtitle language")
                        Layout.preferredWidth: 120
                        placeholderText: qsTr("e.g. en")
                        text: root.mediaPlayer.subtitles.preferredLanguage

                        onEditingFinished: root.preferredSubtitleLanguageEdited(text.trim())
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.rightMargin: 12
                    spacing: 10

                    SettingLabel {
                        text: qsTr("Audio")
                    }
                    TextField {
                        Accessible.name: qsTr("Preferred audio language")
                        Layout.preferredWidth: 120
                        placeholderText: qsTr("e.g. ja")
                        text: root.mediaPlayer.preferredAudioLanguage

                        onEditingFinished: root.preferredAudioLanguageEdited(text.trim())
                    }
                }
            }
            SettingsPage {
                Repeater {
                    model: AppConstants.shortcutReference

                    RowLayout {
                        required property var modelData

                        Layout.fillWidth: true
                        Layout.rightMargin: 12
                        spacing: 16

                        Rectangle {
                            Layout.preferredHeight: keyText.implicitHeight + 8
                            Layout.preferredWidth: 130
                            border.color: Material.dividerColor
                            border.width: 1
                            color: "transparent"
                            radius: 4

                            Text {
                                id: keyText

                                anchors.centerIn: parent
                                color: Material.foreground
                                font.family: "monospace"
                                font.pixelSize: 12
                                text: modelData.keys
                            }
                        }
                        SettingLabel {
                            text: modelData.action
                        }
                    }
                }
            }
        }
    }
}
