import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

import gavqml

Item {
    id: root

    required property PlaylistModel playList
    required property PlaylistView playlistView
    property string searchFilter: ""

    signal itemSelected(string path, string name)
    signal playRequested

    function matchesFilter(name) {
        if (!searchFilter || searchFilter.length === 0)
            return true;
        return name.toLowerCase().indexOf(searchFilter.toLowerCase()) !== -1;
    }
    function removeItem(sourceRow) {
        playList.remove([playList.idAt(sourceRow)]);
    }
    function showCurrent() {
        var row = playlistView.currentViewRow;
        if (row >= 0 && !playListView.moving && !playListView.dragging)
            playListView.positionViewAtIndex(row, ListView.Contain);
    }

    onVisibleChanged: {
        if (visible)
            Qt.callLater(showCurrent);
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
            root.showCurrent();
        }

        target: root.playlistView
    }
    ListView {
        id: playListView

        anchors.fill: parent
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        currentIndex: root.playlistView.currentViewRow
        model: root.playlistView
        topMargin: 5

        ScrollBar.vertical: ScrollBar {
        }
        delegate: ItemDelegate {
            Accessible.name: model.isCurrent ? qsTr("%1, playing").arg(model.title) : model.title
            clip: true
            height: matchesFilter(model.title) ? 40 : 0
            padding: 8
            visible: matchesFilter(model.title)
            width: parent?.width

            Behavior on height {
                NumberAnimation {
                    duration: 150
                }
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
                    text: model.kind === PlaylistModel.LocalAudio ? "" : (model.kind === PlaylistModel.Stream ? "" : "")
                }
                Text {
                    Layout.fillWidth: true
                    color: Material.foreground
                    elide: Text.ElideRight
                    font.bold: model.isCurrent
                    font.pixelSize: 14
                    text: model.title
                }
                Text {
                    color: Material.accent
                    font.family: materialSymbolsOutlined.name
                    font.pixelSize: 20
                    text: ""
                    visible: model.isCurrent
                }
                Button {
                    Accessible.name: qsTr("Remove from playlist")
                    Layout.preferredHeight: 24
                    Layout.preferredWidth: 24
                    flat: true
                    font.family: materialSymbolsOutlined.name
                    font.pixelSize: 18
                    opacity: parent.parent.hovered ? 1 : 0
                    text: ""
                    visible: opacity > 0

                    Behavior on opacity {
                        NumberAnimation {
                            duration: 100
                        }
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
    }
}
