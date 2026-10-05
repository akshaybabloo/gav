import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

import gavqml

Dialog {
    id: root

    property bool invalid: false

    signal urlAccepted(url url)

    function submit() {
        var text = urlField.text.trim();
        if (!/^https?:\/\/[^\s\/]+/i.test(text)) {
            invalid = true;
            return;
        }
        urlAccepted(text);
        accept();
    }

    anchors.centerIn: parent
    modal: true
    title: qsTr("Open URL")
    width: parent ? Math.min(460, parent.width - 48) : 460

    footer: DialogButtonBox {
        Button {
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            flat: true
            text: qsTr("Cancel")
        }
        Button {
            highlighted: true
            text: qsTr("Open")

            onClicked: root.submit()
        }
    }

    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 10

        Text {
            Layout.fillWidth: true
            color: Material.foreground
            text: qsTr("Enter the web address of a video or audio file, a stream (.m3u8) or a playlist (.m3u).")
            wrapMode: Text.WordWrap
        }
        TextField {
            id: urlField

            Accessible.name: qsTr("Stream address")
            Layout.fillWidth: true
            inputMethodHints: Qt.ImhUrlCharactersOnly
            placeholderText: qsTr("https://")
            selectByMouse: true

            Keys.onEnterPressed: root.submit()
            Keys.onReturnPressed: root.submit()
            onTextEdited: root.invalid = false
        }
        Text {
            Layout.fillWidth: true
            color: Material.color(Material.Red)
            font.pixelSize: 12
            text: qsTr("Enter an address starting with http:// or https://")
            visible: root.invalid
            wrapMode: Text.WordWrap
        }
    }

    onAboutToShow: {
        invalid = false;
        urlField.text = "";
    }
    onOpened: urlField.forceActiveFocus()
}
