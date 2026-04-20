import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

import gavqml

Button {
    id: root

    required property var player
    required property bool mediaLoaded

    signal speedChanged(real speed)

    Accessible.description: qsTr("Change playback speed")
    Accessible.name: qsTr("Playback speed")
    Accessible.role: Accessible.Button
    Layout.preferredHeight: 30
    Layout.preferredWidth: 40
    Material.roundedScale: Material.NotRounded
    enabled: mediaLoaded
    hoverEnabled: true

    contentItem: Text {
        color: Material.foreground
        opacity: root.enabled ? 1.0 : 0.5
        font.bold: player.playbackRate !== 1.0
        font.pixelSize: 11
        horizontalAlignment: Text.AlignHCenter
        text: player.playbackRate.toFixed(2).replace(/\.?0+$/, '') + "x"
        verticalAlignment: Text.AlignVCenter
    }

    onClicked: speedMenu.open()

    ToolTip {
        delay: AppConstants.tooltipDelay
        text: qsTr("Playback speed: ") + player.playbackRate + "x"
        timeout: AppConstants.tooltipTimeout
        visible: root.hovered
    }
    Menu {
        id: speedMenu

        y: -height

        Repeater {
            model: AppConstants.playbackSpeeds

            MenuItem {
                checkable: true
                checked: Math.abs(player.playbackRate - modelData) < 0.01
                text: modelData + "x"

                onTriggered: {
                    player.playbackRate = modelData;
                    speedChanged(modelData);
                }
            }
        }
    }
}
