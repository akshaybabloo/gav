import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

Dialog {
    id: root

    property var urls: []

    signal replaceChosen(var urls)
    signal splitViewChosen(var urls)

    anchors.centerIn: parent
    modal: true
    title: qsTr("Play in split view?")

    footer: DialogButtonBox {
        Button {
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            flat: true
            text: qsTr("Cancel")
        }
        Button {
            DialogButtonBox.buttonRole: DialogButtonBox.ApplyRole
            flat: true
            text: qsTr("Replace")
        }
        Button {
            id: splitViewButton

            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            highlighted: true
            text: qsTr("Play in split view")

            Keys.onEnterPressed: root.accept()
            Keys.onEscapePressed: root.reject()
            Keys.onReturnPressed: root.accept()
        }
    }

    Text {
        color: Material.foreground
        opacity: 0.7
        text: qsTr("Play the dropped video beside the one playing, or replace it?")
    }

    onAccepted: splitViewChosen(urls)
    onApplied: {
        close();
        replaceChosen(urls);
    }
    onOpened: splitViewButton.forceActiveFocus()
}
