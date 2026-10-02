import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

import gavqml

Dialog {
    id: root

    property int positionMs: 0

    signal resumeChosen
    signal startOverChosen

    anchors.centerIn: parent
    closePolicy: Popup.NoAutoClose
    modal: true
    title: qsTr("Resume from %1?").arg(AppConstants.formatTime(positionMs))

    footer: DialogButtonBox {
        Button {
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            flat: true
            text: qsTr("Start over")
        }
        Button {
            id: resumeButton

            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            highlighted: true
            text: qsTr("Resume")

            Keys.onEnterPressed: root.accept()
            Keys.onEscapePressed: root.reject()
            Keys.onReturnPressed: root.accept()
        }
    }

    Text {
        color: Material.foreground
        opacity: 0.7
        text: qsTr("You stopped partway through this file last time.")
    }

    onAccepted: resumeChosen()
    onOpened: resumeButton.forceActiveFocus()
    onRejected: startOverChosen()
}
