import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

import gavqml

Button {
    id: root

    required property var player
    readonly property var qualities: player.qualities
    readonly property var audioFormats: player.audioFormats
    readonly property string activeLabel: player.activeQuality >= 0 && player.activeQuality < qualities.length ? qualities[player.activeQuality] : ""
    readonly property string autoLabel: activeLabel !== "" ? qsTr("Auto (%1)").arg(activeLabel) : qsTr("Auto")

    Accessible.description: qsTr("Choose the stream quality")
    Accessible.name: qsTr("Quality")
    Accessible.role: Accessible.Button
    Layout.preferredHeight: 30
    Layout.preferredWidth: 25
    Material.roundedScale: Material.NotRounded
    enabled: player.mediaLoaded
    font.family: materialSymbolsOutlined.name
    font.weight: Font.Light
    hoverEnabled: true
    scale: 1.5
    text: "\ue024"
    visible: qualities.length > 0

    onClicked: popup.open()

    ToolTip {
        delay: AppConstants.tooltipDelay
        text: qsTr("Quality: ") + (player.autoQuality ? root.autoLabel : root.activeLabel)
        timeout: AppConstants.tooltipTimeout
        visible: root.hovered
    }
    Popup {
        id: popup

        margins: 8
        padding: 12
        width: 240
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
                text: qsTr("Quality")
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(qualityColumn.implicitHeight, root.audioFormats.length > 1 ? 200 : 320)
                clip: true

                ColumnLayout {
                    id: qualityColumn

                    spacing: 0
                    width: parent.width

                    Repeater {
                        model: [root.autoLabel].concat(root.qualities)

                        RadioButton {
                            required property int index
                            required property string modelData

                            Layout.fillWidth: true
                            checked: index === 0 ? root.player.autoQuality : !root.player.autoQuality && index - 1 === root.player.activeQuality
                            text: modelData

                            onClicked: {
                                popup.close();
                                root.player.selectQuality(index - 1);
                            }
                        }
                    }
                }
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Material.dividerColor
                visible: root.audioFormats.length > 1
            }
            Text {
                color: Material.foreground
                font.bold: true
                text: qsTr("Audio format")
                visible: root.audioFormats.length > 1
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(formatColumn.implicitHeight, 160)
                clip: true
                visible: root.audioFormats.length > 1

                ColumnLayout {
                    id: formatColumn

                    spacing: 0
                    width: parent.width

                    Repeater {
                        model: root.audioFormats.length > 1 ? root.audioFormats : []

                        RadioButton {
                            required property int index
                            required property string modelData

                            Layout.fillWidth: true
                            checked: index === root.player.activeAudioFormat
                            text: modelData

                            onClicked: {
                                popup.close();
                                root.player.selectAudioFormat(index);
                            }
                        }
                    }
                }
            }
        }
    }
}
