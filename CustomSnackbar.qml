import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: root

    property alias message: snackbarMessage.text

    // Positioning
    x: (parent.width - width) / 2
    y: parent.height - height - 20
    width: Math.min(parent.width * 0.9, 800)

    // Appearance
    padding: 10
    background: Rectangle {
        color: "#333"
        radius: 4
    }

    contentItem: RowLayout {
        spacing: 10

        Label {
            id: snackbarMessage
            color: "white"
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        Button {
            text: "Dismiss"
            onClicked: root.close()
            Layout.alignment: Qt.AlignVCenter
            background: Rectangle {
                color: "transparent"
                border.color: "white"
                border.width: 1
                radius: 4
            }
        }
    }

    // Behavior
    closePolicy: Popup.NoAutoClose
    Timer {
        id: hideTimer
        interval: 3000
        onTriggered: root.close()
    }
    onOpened: hideTimer.start()
    onClosed: hideTimer.stop()

    // Public function to show
    function show() {
        open()
    }
}
