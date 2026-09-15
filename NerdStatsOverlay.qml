import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import gavqml

Rectangle {
    id: root
    
    property bool hasVideo: false
    property var videoOutput: null
    
    width: 320
    height: contentLayout.height + 20
    color: Qt.rgba(0, 0, 0, 0.7)
    radius: 8
    border.color: Material.dividerColor
    border.width: 1
    
    visible: false

    Binding {
        target: SystemStats
        property: "active"
        value: root.visible
    }

    ColumnLayout {
        id: contentLayout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 10
        spacing: 5
        
        RowLayout {
            Layout.fillWidth: true
            Text {
                text: qsTr("Stats for nerds")
                color: Material.foreground
                font.bold: true
                font.pixelSize: 14
                Layout.fillWidth: true
            }
            Button {
                id: closeButton
                text: "\ue5cd"
                font.family: materialSymbolsOutlined.name
                font.pixelSize: 16
                flat: true
                hoverEnabled: true
                padding: 0
                Layout.preferredWidth: 24
                Layout.preferredHeight: 24

                ToolTip.delay: AppConstants.tooltipDelay
                ToolTip.text: qsTr("Close")
                ToolTip.timeout: AppConstants.tooltipTimeout
                ToolTip.visible: hovered

                contentItem: Text {
                    text: closeButton.text
                    font: closeButton.font
                    color: closeButton.hovered ? (Material.theme === Material.Dark ? "#ff6b6b" : "#e02424") : Material.foreground
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    color: closeButton.hovered ? Qt.rgba(1, 1, 1, 0.12) : "transparent"
                    radius: 4
                }

                onClicked: root.visible = false
            }
        }
        
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Material.dividerColor
        }
        
        GridLayout {
            columns: 2
            rowSpacing: 4
            columnSpacing: 10
            Layout.fillWidth: true
            
            Text { text: "FPS:"; color: Material.foreground; font.pixelSize: 12; opacity: 0.8 }
            Text { text: (root.hasVideo && mediaComponent.mediaPlayer.fps > 0) ? mediaComponent.mediaPlayer.fps.toFixed(0) : "N/A"; color: Material.foreground; font.pixelSize: 12; font.family: "monospace" }
            
            Text { text: "CPU Usage:"; color: Material.foreground; font.pixelSize: 12; opacity: 0.8 }
            Text { text: SystemStats.cpuUsage; color: Material.foreground; font.pixelSize: 12; font.family: "monospace" }
            
            Text { text: "GPU Usage:"; color: Material.foreground; font.pixelSize: 12; opacity: 0.8 }
            Text { text: SystemStats.gpuUsage; color: Material.foreground; font.pixelSize: 12; font.family: "monospace" }
            
            Text { text: "RAM Usage:"; color: Material.foreground; font.pixelSize: 12; opacity: 0.8 }
            Text { text: SystemStats.ramUsage; color: Material.foreground; font.pixelSize: 12; font.family: "monospace" }
            
            Text { text: "IO Usage:"; color: Material.foreground; font.pixelSize: 12; opacity: 0.8 }
            Text { text: SystemStats.ioUsage; color: Material.foreground; font.pixelSize: 12; font.family: "monospace" }
            
            Text { text: "Resolution:"; color: Material.foreground; font.pixelSize: 12; opacity: 0.8; visible: root.hasVideo }
            Text { text: root.videoOutput ? root.videoOutput.sourceRect.width + "x" + root.videoOutput.sourceRect.height : "N/A"; color: Material.foreground; font.pixelSize: 12; font.family: "monospace"; visible: root.hasVideo }
        }
    }
}
