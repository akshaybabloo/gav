import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

import gavqml

Button {
    id: root

    required property var videoOutput
    required property bool hasVideo

    Accessible.description: qsTr("Adjust brightness and contrast")
    Accessible.name: qsTr("Brightness/Contrast")
    Accessible.role: Accessible.Button
    Layout.preferredHeight: 30
    Layout.preferredWidth: 25
    Material.roundedScale: Material.NotRounded
    enabled: hasVideo
    font.family: materialSymbolsOutlined.name
    font.weight: Font.Light
    hoverEnabled: true
    scale: 1.5
    text: "\ue1ac"

    onClicked: popup.open()

    ToolTip {
        delay: AppConstants.tooltipDelay
        text: qsTr("Brightness / Contrast")
        timeout: AppConstants.tooltipTimeout
        visible: root.hovered
    }

    Popup {
        id: popup

        padding: 12
        x: -width / 2 + parent.width / 2
        y: -height - 10

        background: Rectangle {
            border.color: Material.dividerColor
            border.width: 1
            color: Material.background
            radius: 8
        }

        ColumnLayout {
            spacing: 8

            // Brightness row
            RowLayout {
                spacing: 8

                Text {
                    color: Material.foreground
                    font.family: materialSymbolsOutlined.name
                    font.pixelSize: 18
                    text: "\ue518"

                    MouseArea {
                        id: brightnessLabel
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.NoButton
                    }
                    ToolTip {
                        delay: AppConstants.tooltipDelay
                        text: qsTr("Brightness")
                        timeout: AppConstants.tooltipTimeout
                        visible: brightnessLabel.containsMouse
                    }
                }
                Slider {
                    id: brightnessSlider

                    Accessible.name: qsTr("Brightness")
                    Accessible.description: qsTr("Brightness slider, adjusts video brightness")
                    Layout.preferredWidth: 150
                    from: AppConstants.brightnessMin
                    to: AppConstants.brightnessMax
                    stepSize: AppConstants.brightnessContrastStep
                    value: videoOutput.brightnessLevel

                    onMoved: videoOutput.brightnessLevel = value
                }
                Text {
                    Layout.preferredWidth: 35
                    color: Material.foreground
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignRight
                    text: brightnessSlider.value.toFixed(2)
                }
            }

            // Contrast row
            RowLayout {
                spacing: 8

                Text {
                    color: Material.foreground
                    font.family: materialSymbolsOutlined.name
                    font.pixelSize: 18
                    text: "\ue3a5"

                    MouseArea {
                        id: contrastLabel
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.NoButton
                    }
                    ToolTip {
                        delay: AppConstants.tooltipDelay
                        text: qsTr("Contrast")
                        timeout: AppConstants.tooltipTimeout
                        visible: contrastLabel.containsMouse
                    }
                }
                Slider {
                    id: contrastSlider

                    Accessible.name: qsTr("Contrast")
                    Accessible.description: qsTr("Contrast slider, adjusts video contrast")
                    Layout.preferredWidth: 150
                    from: AppConstants.contrastMin
                    to: AppConstants.contrastMax
                    stepSize: AppConstants.brightnessContrastStep
                    value: videoOutput.contrastLevel

                    onMoved: videoOutput.contrastLevel = value
                }
                Text {
                    Layout.preferredWidth: 35
                    color: Material.foreground
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignRight
                    text: contrastSlider.value.toFixed(2)
                }
            }

            // Reset button
            Button {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredHeight: 25
                Material.roundedScale: Material.SmallScale
                font.pixelSize: 11
                text: qsTr("Reset")

                onClicked: {
                    videoOutput.brightnessLevel = AppConstants.defaultBrightness;
                    videoOutput.contrastLevel = AppConstants.defaultContrast;
                    brightnessSlider.value = AppConstants.defaultBrightness;
                    contrastSlider.value = AppConstants.defaultContrast;
                }
            }
        }
    }
}
