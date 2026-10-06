import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

import gavqml

Item {
    id: root

    required property var collageTarget
    property bool narrow: false
    property bool overlay: false
    required property PlaylistModel playList
    required property PlaylistView playlistView
    property bool shuffleEnabled: false

    signal closeRequested
    signal itemSelected(string path, string name)
    signal playRequested
    signal saveRequested
    signal shuffleToggled(bool enabled)
    signal widthRequested(int requestedWidth)

    Rectangle {
        anchors.fill: parent
        color: Material.background
    }
    Dialog {
        id: clearConfirmDialog

        anchors.centerIn: parent
        modal: true
        parent: Overlay.overlay
        standardButtons: Dialog.Yes | Dialog.No
        title: qsTr("Clear Playlist")

        Text {
            color: Material.foreground
            text: qsTr("Are you sure you want to clear the playlist?") + "\n" + qsTr("This will remove all ") + root.playList.count + qsTr(" items.")
        }

        onAccepted: root.playList.clear()
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: headerColumn.implicitHeight + 20
            color: Material.dialogColor

            ColumnLayout {
                id: headerColumn

                anchors.fill: parent
                anchors.margins: 10
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 15

                    Text {
                        color: Material.foreground
                        font.bold: true
                        font.pixelSize: 16
                        text: qsTr("Playlist")
                    }
                    Text {
                        Layout.fillWidth: true
                        color: Material.foreground
                        elide: Text.ElideRight
                        font.pixelSize: 12
                        opacity: 0.7
                        text: root.playList.count + " " + (root.playList.count === 1 ? qsTr("item") : qsTr("items"))
                    }
                    Button {
                        id: shuffleButton

                        Accessible.description: qsTr("Play the playlist in random order")
                        Accessible.name: qsTr("Shuffle")
                        Accessible.role: Accessible.Button
                        Layout.preferredHeight: 30
                        Layout.preferredWidth: 25
                        Material.roundedScale: Material.NotRounded
                        checkable: true
                        checked: root.shuffleEnabled
                        font.family: materialSymbolsOutlined.name
                        font.weight: Font.Light
                        hoverEnabled: true
                        scale: 1.5
                        text: ""

                        contentItem: Text {
                            color: shuffleButton.checked ? Material.accent : Material.foreground
                            font: shuffleButton.font
                            horizontalAlignment: Text.AlignHCenter
                            opacity: shuffleButton.checked ? 1.0 : 0.5
                            text: shuffleButton.text
                            verticalAlignment: Text.AlignVCenter
                        }

                        onToggled: root.shuffleToggled(checked)

                        ToolTip {
                            delay: AppConstants.tooltipDelay
                            text: shuffleButton.checked ? qsTr("Shuffle: On") : qsTr("Shuffle: Off")
                            timeout: AppConstants.tooltipTimeout
                            visible: shuffleButton.hovered
                        }
                    }
                    Button {
                        id: saveButton

                        Accessible.description: qsTr("Save the playlist to a file")
                        Accessible.name: qsTr("Save playlist")
                        Accessible.role: Accessible.Button
                        Layout.preferredHeight: 30
                        Layout.preferredWidth: 25
                        Material.roundedScale: Material.NotRounded
                        enabled: root.playList.count > 0
                        font.family: materialSymbolsOutlined.name
                        font.weight: Font.Light
                        hoverEnabled: true
                        scale: 1.5
                        text: ""

                        onClicked: root.saveRequested()

                        ToolTip {
                            delay: AppConstants.tooltipDelay
                            text: qsTr("Save playlist")
                            timeout: AppConstants.tooltipTimeout
                            visible: saveButton.hovered
                        }
                    }
                    CollageButton {
                        collageTarget: root.collageTarget
                        sourceUrls: root.playList.count > 0 ? root.playList.locations() : []
                    }
                    Button {
                        id: moreButton

                        Accessible.description: qsTr("More playlist actions")
                        Accessible.name: qsTr("More")
                        Accessible.role: Accessible.Button
                        Layout.preferredHeight: 30
                        Layout.preferredWidth: 25
                        Material.roundedScale: Material.NotRounded
                        font.family: materialSymbolsOutlined.name
                        font.weight: Font.Light
                        hoverEnabled: true
                        scale: 1.5
                        text: ""

                        onClicked: moreMenu.popup(moreButton, 0, moreButton.height)

                        ToolTip {
                            delay: AppConstants.tooltipDelay
                            text: qsTr("More")
                            timeout: AppConstants.tooltipTimeout
                            visible: moreButton.hovered
                        }
                        Menu {
                            id: moreMenu

                            MenuItem {
                                enabled: root.playList.count > 0
                                text: qsTr("Clear playlist")

                                onTriggered: clearConfirmDialog.open()
                            }
                        }
                    }
                    Button {
                        id: closeButton

                        Accessible.description: qsTr("Hide the playlist")
                        Accessible.name: qsTr("Close playlist")
                        Accessible.role: Accessible.Button
                        Layout.preferredHeight: 30
                        Layout.preferredWidth: 25
                        Material.roundedScale: Material.NotRounded
                        font.family: materialSymbolsOutlined.name
                        font.weight: Font.Light
                        hoverEnabled: true
                        scale: 1.5
                        text: ""
                        visible: root.overlay && root.narrow

                        onClicked: root.closeRequested()

                        ToolTip {
                            delay: AppConstants.tooltipDelay
                            text: qsTr("Close playlist")
                            timeout: AppConstants.tooltipTimeout
                            visible: closeButton.hovered
                        }
                    }
                }
                TextField {
                    id: searchField

                    Accessible.name: qsTr("Search playlist")
                    Layout.fillWidth: true
                    placeholderText: qsTr("Search playlist...")
                    selectByMouse: true

                    Keys.onEscapePressed: {
                        if (text.length > 0) {
                            text = "";
                            return;
                        }
                        focus = false;
                        if (root.overlay)
                            root.closeRequested();
                    }

                    Button {
                        Accessible.name: qsTr("Clear search")
                        anchors.right: parent.right
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        bottomInset: 0
                        flat: true
                        font.family: materialSymbolsOutlined.name
                        font.pixelSize: 18
                        height: 28
                        hoverEnabled: true
                        leftInset: 0
                        padding: 0
                        rightInset: 0
                        text: ""
                        topInset: 0
                        visible: searchField.text.length > 0
                        width: 28

                        onClicked: {
                            searchField.text = "";
                            searchField.focus = false;
                        }
                    }
                }
            }
        }
        Item {
            Layout.fillHeight: true
            Layout.fillWidth: true

            PlayListComponent {
                anchors.fill: parent
                playList: root.playList
                playlistView: root.playlistView
                searchFilter: searchField.text
                visible: root.playList.count > 0

                onItemSelected: function (path, name) {
                    root.itemSelected(path, name);
                }
                onPlayRequested: root.playRequested()
            }
            ColumnLayout {
                anchors.centerIn: parent
                spacing: 10
                visible: root.playList.count === 0
                width: Math.min(parent.width - 40, 420)

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    color: Material.foreground
                    font.family: materialSymbolsOutlined.name
                    font.pixelSize: 64
                    font.weight: Font.ExtraLight
                    text: ""

                    SequentialAnimation on opacity {
                        loops: Animation.Infinite
                        running: root.visible && root.playList.count === 0

                        NumberAnimation {
                            duration: 1500
                            easing.type: Easing.InOutQuad
                            from: 1.0
                            to: 0.5
                        }
                        NumberAnimation {
                            duration: 1500
                            easing.type: Easing.InOutQuad
                            from: 0.5
                            to: 1.0
                        }
                    }
                }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    color: Material.foreground
                    font.pixelSize: 20
                    text: qsTr("No media files")
                }
                Repeater {
                    model: [qsTr("Drag files here or use File > Open"), qsTr("Open a playlist with File > Open Playlist"), qsTr("Play a web address with File > Open URL (Ctrl+N)")]

                    Text {
                        required property string modelData

                        Layout.fillWidth: true
                        color: Material.foreground
                        font.pixelSize: 14
                        horizontalAlignment: Text.AlignHCenter
                        opacity: 0.7
                        text: modelData
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.top: parent.top
        color: Material.dividerColor
        visible: root.overlay && !root.narrow
        width: 1
    }
    MouseArea {
        Accessible.name: qsTr("Resize playlist")
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.top: parent.top
        cursorShape: Qt.SizeHorCursor
        preventStealing: true
        visible: root.overlay && !root.narrow
        width: 6

        onPositionChanged: function (mouse) {
            if (pressed)
                root.widthRequested(Math.round(root.width - mouse.x));
        }
    }
}
