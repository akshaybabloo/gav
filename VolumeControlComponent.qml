import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

import gavqml

RowLayout {
    id: root

    required property var audioOutput
    property real previousVolume: AppConstants.defaultVolume

    Layout.alignment: Qt.AlignRight
    spacing: 5

    function updateVolumeIcon() {
        if (audioOutput.muted || audioOutput.volume === 0) {
            volumeButton.text = "\ue04e";
        } else if (audioOutput.volume < 0.5) {
            volumeButton.text = "\ue04d";
        } else if (audioOutput.volume < 1.0) {
            volumeButton.text = "\ue050";
        } else {
            volumeButton.text = "\ue98e";
        }
    }

    function applyVolume(newValue) {
        audioOutput.volume = newValue;
        audioOutput.muted = newValue <= 0;
        updateVolumeIcon();
    }

    ToolButton {
        id: volumeButton

        Layout.preferredHeight: 25
        Layout.preferredWidth: 15
        ToolTip.delay: AppConstants.tooltipDelay
        ToolTip.text: qsTr("Volume")
        ToolTip.timeout: AppConstants.tooltipTimeout
        ToolTip.visible: hovered
        font.family: materialSymbolsOutlined.name
        font.weight: Font.Light
        hoverEnabled: true
        scale: 1.5
        text: "\ue04d"

        onClicked: {
            audioOutput.muted = !audioOutput.muted;
            if (audioOutput.muted) {
                previousVolume = audioOutput.volume;
                audioOutput.volume = 0;
            } else {
                audioOutput.volume = previousVolume;
            }
            updateVolumeIcon();
        }
    }
    Slider {
        id: volumeSlider

        Layout.preferredHeight: 10
        Layout.preferredWidth: 100
        from: 0
        to: 1.0
        value: audioOutput.volume

        onMoved: applyVolume(value)

        // Sync icon when volume or mute changes from external sources (settings, shortcuts, etc.)
        Connections {
            target: audioOutput
            function onVolumeChanged() {
                volumeSlider.value = audioOutput.volume;
                updateVolumeIcon();
            }
            function onMutedChanged() {
                updateVolumeIcon();
            }
        }

        MouseArea {
            acceptedButtons: Qt.NoButton
            anchors.fill: parent
            propagateComposedEvents: true

            onWheel: function (wheel) {
                var newValue = volumeSlider.value;
                if (wheel.angleDelta.y > 0) {
                    newValue = Math.min(volumeSlider.value + AppConstants.volumeStep, 1.0);
                } else if (wheel.angleDelta.y < 0) {
                    newValue = Math.max(volumeSlider.value - AppConstants.volumeStep, 0.0);
                } else {
                    return;
                }
                volumeSlider.value = newValue;
                applyVolume(newValue);
            }
        }
    }
}
