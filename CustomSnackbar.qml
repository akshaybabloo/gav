import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import gavqml

Popup {
    id: root

    property alias message: snackbarMessage.text

    // Public function to show
    function show() {
        open();
    }

    // Behavior
    closePolicy: Popup.NoAutoClose

    // Appearance
    padding: 10
    width: Math.min(parent.width * 0.9, 800)

    // Positioning
    x: (parent.width - width) / 2
    y: parent.height - height - 20

    background: Rectangle {
        color: "#333"
        radius: 4
    }
    contentItem: RowLayout {
        spacing: 10

        Label {
            id: snackbarMessage

            Layout.fillWidth: true
            color: "white"
            wrapMode: Text.WordWrap
        }
        Button {
            Layout.alignment: Qt.AlignVCenter
            text: "Dismiss"

            background: Rectangle {
                border.color: "white"
                border.width: 1
                color: "transparent"
                radius: 4
            }

            onClicked: root.close()
        }
    }

    onClosed: hideTimer.stop()
    onOpened: hideTimer.start()

    Timer {
        id: hideTimer

        interval: AppConstants.snackbarDuration

        onTriggered: root.close()
    }
}
