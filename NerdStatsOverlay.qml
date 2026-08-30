import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import gavqml

Rectangle {
    id: root
    
    property bool hasVideo: false
    property var videoOutput: null
    
    width: 300
    height: contentLayout.height + 20
    color: Qt.rgba(0, 0, 0, 0.7)
    radius: 8
    border.color: Material.dividerColor
    border.width: 1
    
    visible: false
    
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
                text: "\ue5cd" // close
                font.family: "Material Symbols Outlined"
                flat: true
                padding: 0
                Layout.preferredWidth: 20
                Layout.preferredHeight: 20
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
