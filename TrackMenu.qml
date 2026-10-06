import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Layouts

import gavqml

Button {
    id: root

    required property var player
    readonly property var subtitles: player.subtitles
    readonly property var audioTracks: player.audioTracks

    Accessible.description: qsTr("Choose subtitle and audio tracks")
    Accessible.name: qsTr("Subtitles and audio")
    Accessible.role: Accessible.Button
    Layout.preferredHeight: 30
    Layout.preferredWidth: 25
    Material.roundedScale: Material.NotRounded
    enabled: player.mediaLoaded
    font.family: materialSymbolsOutlined.name
    font.weight: Font.Light
    hoverEnabled: true
    scale: 1.5
    text: "\ue048"
    visible: player.hasVideo || audioTracks.length > 1

    onClicked: popup.open()

    ToolTip {
        delay: AppConstants.tooltipDelay
        text: qsTr("Subtitles: ") + subtitles.activeTrackName
        timeout: AppConstants.tooltipTimeout
        visible: root.hovered
    }
    FileDialog {
        id: subtitleFileDialog

        nameFilters: [AppConstants.getSubtitleExtensionsFilter(), qsTr("All files (*)")]
        title: qsTr("Load subtitle file")

        onAccepted: subtitles.loadFile(selectedFile)
    }
    Popup {
        id: popup

        margins: 8
        padding: 12
        width: 300
        x: root.width / 2 - width / (2 * root.scale)
        y: -(height + 10) / root.scale

        background: Rectangle {
            border.color: Material.dividerColor
            border.width: 1
            color: Material.background
            radius: 8
        }

        ColumnLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: 4

            Text {
                color: Material.foreground
                font.bold: true
                text: qsTr("Subtitles")
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(subtitleColumn.implicitHeight, 200)
                clip: true

                ColumnLayout {
                    id: subtitleColumn

                    spacing: 0
                    width: parent.width

                    Repeater {
                        model: subtitles.tracks

                        RadioButton {
                            required property var modelData

                            Layout.fillWidth: true
                            checked: modelData.id === subtitles.activeTrackId
                            enabled: !(modelData.loadState === "failed" && modelData.origin === "external")
                            text: modelData.loadState !== "failed" || modelData.origin === "embedded" ? modelData.displayName : modelData.displayName + (modelData.origin === "stream" ? qsTr(" (failed, select to retry)") : qsTr(" (unreadable)"))

                            onClicked: subtitles.selectTrack(modelData.id)
                        }
                    }
                    RadioButton {
                        Layout.fillWidth: true
                        checked: subtitles.activeTrackId === ""
                        text: qsTr("Off")
                        visible: subtitles.tracks.length > 0

                        onClicked: subtitles.selectTrack("")
                    }
                    Text {
                        color: Material.foreground
                        opacity: 0.5
                        text: qsTr("No subtitles")
                        visible: subtitles.tracks.length === 0
                    }
                }
            }
            Button {
                Layout.fillWidth: true
                flat: true
                text: qsTr("Load subtitle file…")

                onClicked: {
                    popup.close();
                    subtitleFileDialog.open();
                }
            }
            RowLayout {
                Layout.fillWidth: true
                enabled: subtitles.activeTrackId !== ""

                Text {
                    Layout.fillWidth: true
                    color: Material.foreground
                    opacity: parent.enabled ? 0.7 : 0.4
                    text: qsTr("Delay")
                }
                ToolButton {
                    Accessible.name: qsTr("Show subtitles earlier")
                    text: "−"

                    onClicked: subtitles.adjustDelay(-AppConstants.subtitleDelayStep)
                }
                Text {
                    Layout.preferredWidth: 70
                    color: Material.foreground
                    horizontalAlignment: Text.AlignHCenter
                    text: AppConstants.formatDelay(subtitles.delay)
                }
                ToolButton {
                    Accessible.name: qsTr("Show subtitles later")
                    text: "+"

                    onClicked: subtitles.adjustDelay(AppConstants.subtitleDelayStep)
                }
            }
            RowLayout {
                Layout.fillWidth: true

                Text {
                    color: Material.foreground
                    opacity: 0.7
                    text: qsTr("Size")
                }
                Slider {
                    Accessible.name: qsTr("Subtitle size")
                    Layout.fillWidth: true
                    from: AppConstants.subtitleScaleMin
                    stepSize: AppConstants.subtitleScaleStep
                    to: AppConstants.subtitleScaleMax
                    value: subtitles.scale

                    onMoved: subtitles.scale = value
                }
                Text {
                    Layout.preferredWidth: 40
                    color: Material.foreground
                    text: Math.round(subtitles.scale * 100) + "%"
                }
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Material.dividerColor
            }
            Text {
                color: Material.foreground
                font.bold: true
                text: qsTr("Audio")
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(audioColumn.implicitHeight, 160)
                clip: true
                visible: audioTracks.length > 1

                ColumnLayout {
                    id: audioColumn

                    spacing: 0
                    width: parent.width

                    Repeater {
                        model: audioTracks.length > 1 ? audioTracks : []

                        RadioButton {
                            required property var modelData

                            Layout.fillWidth: true
                            checked: modelData.index === player.activeAudioTrack
                            text: modelData.displayName

                            onClicked: player.selectAudioTrack(modelData.index)
                        }
                    }
                }
            }
            Text {
                color: Material.foreground
                opacity: 0.5
                text: audioTracks.length === 1 ? qsTr("Single audio track") : qsTr("No audio")
                visible: audioTracks.length < 2
            }
        }
    }
}
