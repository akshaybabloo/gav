import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

import gavqml

ItemDelegate {
    id: root

    required property bool collapsed
    property bool focused: false
    required property string group
    required property int groupCount
    readonly property string label: group !== "" ? group : qsTr("Ungrouped")

    Accessible.name: (collapsed ? qsTr("%1, %2 items, collapsed") : qsTr("%1, %2 items, expanded")).arg(label).arg(groupCount)
    bottomPadding: 0
    hoverEnabled: true
    implicitHeight: AppConstants.playlistGroupHeaderHeight
    leftPadding: 8
    rightPadding: 14
    topPadding: 0

    background: Rectangle {
        border.color: Material.foreground
        border.width: root.focused ? 2 : 0
        color: root.down ? Material.listHighlightColor : (root.hovered ? Material.dividerColor : Qt.rgba(Material.foreground.r, Material.foreground.g, Material.foreground.b, 0.06))
    }
    contentItem: RowLayout {
        spacing: 8

        Text {
            color: Material.foreground
            font.family: materialSymbolsOutlined.name
            font.pixelSize: 22
            text: root.collapsed ? "\ue5cc" : "\ue5cf"
        }
        Text {
            Layout.fillWidth: true
            color: Material.foreground
            elide: Text.ElideRight
            font.bold: true
            font.pixelSize: 13
            text: root.label
        }
        Text {
            color: Material.foreground
            font.pixelSize: 12
            opacity: 0.7
            text: root.groupCount
        }
    }

    Keys.onEnterPressed: clicked()
    Keys.onReturnPressed: clicked()
}
