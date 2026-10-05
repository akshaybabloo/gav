import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

import gavqml

Item {
    id: root

    required property var collageTarget
    required property PlaylistModel playList
    required property PlaylistView playlistView
    property string searchFilter: ""
    property bool shuffleEnabled: false

    // Signals for decoupling from parent components
    signal itemSelected(string path, string name)
    signal playRequested()
    signal shuffleToggled(bool enabled)

    // Full background
    Rectangle {
        anchors.fill: parent
        color: Material.background
    }

    function matchesFilter(name) {
        if (!searchFilter || searchFilter.length === 0) return true;
        return name.toLowerCase().indexOf(searchFilter.toLowerCase()) !== -1;
    }

    function removeItem(sourceRow) {
        playList.remove([playList.idAt(sourceRow)]);
    }

    Connections {
        function onCurrentChanged() {
            if (root.playList.currentRow < 0)
                return;
            var item = root.playList.entryAt(root.playList.currentRow);
            root.itemSelected(item.path, item.title);
        }

        target: root.playList
    }
    Connections {
        function onCurrentViewRowChanged() {
            playListView.currentIndex = root.playlistView.currentViewRow;
        }

        target: root.playlistView
    }

    // Clear confirmation dialog
    Dialog {
        id: clearConfirmDialog

        anchors.centerIn: parent
        modal: true
        title: qsTr("Clear Playlist")
        standardButtons: Dialog.Yes | Dialog.No

        Text {
            color: Material.foreground
            text: qsTr("Are you sure you want to clear the playlist?") + "\n" +
                  qsTr("This will remove all ") + playList.count + qsTr(" items.")
        }

        onAccepted: {
            playList.clear();
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 10
        visible: playList.count === 0

        Text {
            Layout.alignment: Qt.AlignHCenter
            color: Material.foreground
            font.family: materialSymbolsOutlined.name
            font.pixelSize: 64
            font.weight: Font.ExtraLight
            text: "\uf523"

            SequentialAnimation on opacity {
                loops: Animation.Infinite
                running: playList.count === 0

                NumberAnimation {
                    from: 1.0
                    to: 0.5
                    duration: 1500
                    easing.type: Easing.InOutQuad
                }
                NumberAnimation {
                    from: 0.5
                    to: 1.0
                    duration: 1500
                    easing.type: Easing.InOutQuad
                }
            }
        }
        Text {
            Layout.alignment: Qt.AlignHCenter
            color: Material.foreground
            font.pixelSize: 20
            text: qsTr("No media files")
        }
        Text {
            Layout.alignment: Qt.AlignHCenter
            color: Material.foreground
            opacity: 0.5
            font.pixelSize: 14
            text: qsTr("Drag files here or use File > Open")
        }
    }
    ListView {
        id: playListView

        anchors.fill: parent
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        currentIndex: root.playlistView.currentViewRow
        model: root.playlistView
        visible: playList.count > 0
        headerPositioning: ListView.OverlayHeader
        topMargin: 5

        ScrollBar.vertical: ScrollBar {
        }
        delegate: ItemDelegate {
            height: matchesFilter(model.title) ? 40 : 0
            padding: 8
            width: parent?.width
            visible: matchesFilter(model.title)
            clip: true

            Behavior on height {
                NumberAnimation { duration: 150 }
            }

            background: Rectangle {
                color: parent.down ? Material.listHighlightColor : (parent.hovered ? Material.dividerColor : (model.isCurrent ? Qt.rgba(Material.accent.r, Material.accent.g, Material.accent.b, 0.3) : "transparent"))
                radius: 4
            }
            contentItem: RowLayout {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 12

                Text {
                    color: Material.foreground
                    font.family: materialSymbolsOutlined.name
                    font.pixelSize: 24
                    text: model.kind === PlaylistModel.LocalAudio ? "\ue405" : (model.kind === PlaylistModel.Stream ? "\ue894" : "\ueb87")
                }
                Text {
                    Layout.fillWidth: true
                    color: Material.foreground
                    elide: Text.ElideRight
                    font.pixelSize: 14
                    text: model.title
                }
                Button {
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                    flat: true
                    font.family: materialSymbolsOutlined.name
                    font.pixelSize: 18
                    text: "\ue5cd"
                    opacity: parent.parent.hovered ? 1 : 0
                    visible: opacity > 0

                    Behavior on opacity {
                        NumberAnimation { duration: 100 }
                    }

                    onClicked: {
                        removeItem(model.sourceRow);
                    }

                    ToolTip {
                        delay: AppConstants.tooltipDelay
                        text: qsTr("Remove from playlist")
                        timeout: AppConstants.tooltipTimeout
                        visible: parent.hovered
                    }
                }
            }

            onClicked: {
                root.playList.currentRow = model.sourceRow;
            }
            onDoubleClicked: {
                playRequested();
            }
        }
        header: Rectangle {
            color: Material.dialogColor
            height: 100
            width: parent.width
            z: 2

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8

                // Search row
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    TextField {
                        id: searchField
                        Layout.fillWidth: true
                        placeholderText: qsTr("Search playlist...")
                        selectByMouse: true

                        onTextChanged: {
                            searchFilter = text;
                        }

                        // Clear search button
                        Button {
                            anchors.right: parent.right
                            anchors.rightMargin: 4
                            anchors.verticalCenter: parent.verticalCenter
                            width: 20
                            height: 20
                            visible: searchField.text.length > 0
                            flat: true
                            font.family: materialSymbolsOutlined.name
                            text: "\ue5cd"

                            onClicked: {
                                searchField.text = "";
                                searchField.focus = false;
                            }
                        }
                    }
                }

                // Info and actions row
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Text {
                        color: Material.foreground
                        opacity: 0.7
                        font.pixelSize: 12
                        text: playList.count + " " + (playList.count === 1 ? qsTr("item") : qsTr("items"))
                    }
                    Item {
                        Layout.fillWidth: true
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
                        text: "\ue043"

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
                        id: clearButton

                        Layout.preferredHeight: 30
                        Layout.preferredWidth: 25
                        Material.roundedScale: Material.NotRounded
                        font.family: materialSymbolsOutlined.name
                        font.weight: Font.Light
                        hoverEnabled: true
                        enabled: playList.count > 0
                        scale: 1.5
                        text: "\ue12d"

                        Accessible.name: qsTr("Clear playlist")
                        Accessible.description: qsTr("Remove all items from the playlist")
                        Accessible.role: Accessible.Button

                        onClicked: {
                            clearConfirmDialog.open();
                        }

                        ToolTip {
                            delay: AppConstants.tooltipDelay
                            text: qsTr("Clear playlist")
                            timeout: AppConstants.tooltipTimeout
                            visible: clearButton.hovered
                        }
                    }
                    CollageButton {
                        collageTarget: root.collageTarget
                        sourceUrls: playList.count > 0 ? playList.locations() : []
                    }
                }
            }
        }
    }
}
