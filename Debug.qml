import QtQuick
import QtQuick.Controls

Rectangle {
    property string borderColor: "red"

    anchors.fill: parent
    border.color: color
    border.width: 5
    color: "transparent"
}
