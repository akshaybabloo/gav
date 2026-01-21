import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

import gavqml

Item {
    required property ListModel playList
    property alias playListView: playListView
    property string searchFilter: ""

    // Signals for decoupling from parent components
    signal itemSelected(string path, string name)
    signal playRequested()

    // Full background
    Rectangle {
        anchors.fill: parent
        color: Material.background
    }

    function toPathList(model) {
        var dataArray = [];
        for (var i = 0; i < model.count; ++i) {
            dataArray.push(model.get(i).path);
        }
        return dataArray;
    }

    function matchesFilter(name) {
        if (!searchFilter || searchFilter.length === 0) return true;
        return name.toLowerCase().indexOf(searchFilter.toLowerCase()) !== -1;
    }

    function removeItem(index) {
        if (index >= 0 && index < playList.count) {
            playList.remove(index);
            if (playListView.currentIndex >= playList.count) {
                playListView.currentIndex = playList.count - 1;
            }
        }
    }

    // Clear confirmation dialog
    Dialog {
        id: clearConfirmDialog

        anchors.centerIn: parent
        modal: true
        title: qsTr("Clear Playlist")
        standardButtons: Dialog.Yes | Dialog.No

        Text {
            color: "white"
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
        model: playList
        visible: playList.count > 0
        headerPositioning: ListView.OverlayHeader
        topMargin: 5

        ScrollBar.vertical: ScrollBar {
        }
        delegate: ItemDelegate {
            height: matchesFilter(model.name) ? 40 : 0
            padding: 8
            width: parent?.width
            visible: matchesFilter(model.name)
            clip: true

            Behavior on height {
                NumberAnimation { duration: 150 }
            }

            background: Rectangle {
                color: parent.down ? Material.listHighlightColor : (parent.hovered ? Material.dividerColor : (parent.ListView.isCurrentItem ? Material.highlightedButtonColor : "transparent"))
                radius: 4
            }
            contentItem: RowLayout {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 12

                Text {
                    color: Material.foreground
                    font.family: materialSymbolsOutlined.name
                    font.pixelSize: 24
                    text: model.icon
                }
                Text {
                    Layout.fillWidth: true
                    color: Material.foreground
                    elide: Text.ElideRight
                    font.pixelSize: 14
                    text: model.name
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
                        removeItem(index);
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
                playListView.currentIndex = index;
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
                    Button {
                        id: collageButton

                        property bool isLoading: false

                        Layout.preferredHeight: 30
                        Layout.preferredWidth: 25
                        Material.roundedScale: Material.NotRounded
                        enabled: !collageButton.isLoading && playList.count > 0
                        font.family: materialSymbolsOutlined.name
                        font.weight: Font.Light
                        hoverEnabled: true
                        scale: 1.5
                        text: collageButton.isLoading ? "" : "\uefb2"

                        Accessible.name: qsTr("Create collages")
                        Accessible.description: qsTr("Create collages for all videos in the playlist")
                        Accessible.role: Accessible.Button

                        onClicked: {
                            collageButton.isLoading = true;
                            collage.toCollage(toPathList(playList));
                        }

                        BusyIndicator {
                            anchors.centerIn: parent
                            height: parent.height * 0.8
                            running: collageButton.isLoading
                            visible: collageButton.isLoading
                            width: parent.width * 0.8
                        }
                        ToolTip {
                            delay: AppConstants.tooltipDelay
                            text: collageButton.isLoading ? qsTr("Creating collages...") : qsTr("Create collages for all videos")
                            timeout: AppConstants.tooltipTimeout
                            visible: collageButton.hovered
                        }
                        Connections {
                            function onCollageFinished(successCount, failCount) {
                                collageButton.isLoading = false;
                            }

                            target: collage
                        }
                    }
                }
            }
        }

        onCurrentIndexChanged: {
            if (currentIndex !== -1) {
                var item = playList.get(currentIndex);
                itemSelected(item.path, item.name);
            }
        }
    }
}
