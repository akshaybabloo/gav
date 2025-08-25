import QtQuick
import QtQuick.Controls

Rectangle {
    property string borderColor: "red"

    anchors.fill: parent
    border.width: 5
    border.color: color
    color: "transparent"
}
