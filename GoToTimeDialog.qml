import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

import gavqml

Dialog {
    id: root

    required property var player
    property string errorCode: ""

    function submit() {
        var parsed = PlaybackUtils.parseTime(timeField.text, player.duration);
        if (!parsed.ok) {
            errorCode = parsed.error;
            return;
        }
        player.position = parsed.ms;
        accept();
    }

    anchors.centerIn: parent
    modal: true
    title: qsTr("Go to time")
    width: 300

    footer: DialogButtonBox {
        Button {
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            flat: true
            text: qsTr("Cancel")
        }
        Button {
            highlighted: true
            text: qsTr("Go")

            onClicked: root.submit()
        }
    }

    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 6

        TextField {
            id: timeField

            Accessible.name: qsTr("Time")
            Layout.fillWidth: true
            placeholderText: qsTr("h:mm:ss, m:ss or seconds")
            selectByMouse: true

            Keys.onEnterPressed: root.submit()
            Keys.onReturnPressed: root.submit()
            onTextEdited: root.errorCode = ""
        }
        Text {
            Layout.fillWidth: true
            color: Material.color(Material.Red)
            font.pixelSize: 12
            text: root.errorCode === "outOfRange" ? qsTr("Beyond the end of the media") : qsTr("Use h:mm:ss, m:ss or seconds")
            visible: root.errorCode !== ""
            wrapMode: Text.WordWrap
        }
        Text {
            Layout.fillWidth: true
            color: Material.foreground
            font.pixelSize: 12
            opacity: 0.5
            text: qsTr("Length: ") + AppConstants.formatTime(root.player.duration)
            visible: root.player.duration > 0
        }
    }

    onAboutToShow: {
        errorCode = "";
        timeField.text = AppConstants.formatTime(player.position);
    }
    onOpened: {
        timeField.forceActiveFocus();
        timeField.selectAll();
    }
}
