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

        onMoved: {
            audioOutput.volume = value;
            if (value > 0) {
                audioOutput.muted = false;
            }
            updateVolumeIcon();
        }

        // Sync icon when volume changes from external sources (e.g. scroll wheel, settings)
        Connections {
            target: audioOutput
            function onVolumeChanged() {
                volumeSlider.value = audioOutput.volume;
                updateVolumeIcon();
            }
        }

        MouseArea {
            acceptedButtons: Qt.NoButton
            anchors.fill: parent
            propagateComposedEvents: true

            onWheel: function (wheel) {
                if (wheel.angleDelta.y > 0) {
                    volumeSlider.value = Math.min(volumeSlider.value + AppConstants.volumeStep, 1.0);
                } else if (wheel.angleDelta.y < 0) {
                    volumeSlider.value = Math.max(volumeSlider.value - AppConstants.volumeStep, 0.0);
                }
            }
        }
    }
}
