import QtQml.Models
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

import gavqml

Item {
    id: root

    required property PlaylistModel playList
    required property PlaylistView playlistView
    property bool showLogos: false

    signal itemSelected(string path, string name)
    signal playRequested

    function removeItem(sourceRow) {
        playList.remove([playList.idAt(sourceRow)]);
    }
    function showStart() {
        playListView.forceLayout();
        playListView.positionViewAtBeginning();
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
            Qt.callLater(root.showCurrent);
        }

        target: root.playList
    }
    Connections {
        function onModelReset() {
            Qt.callLater(root.showStart);
        }

        target: root.playlistView
    }
    ListView {
        id: playListView

        anchors.fill: parent
        boundsBehavior: Flickable.StopAtBounds
        cacheBuffer: AppConstants.playlistRowHeight * 3
        clip: true
        highlightFollowsCurrentItem: false
        model: root.playlistView
        reuseItems: true
        topMargin: 5

        ScrollBar.vertical: ScrollBar {
        }
        delegate: DelegateChooser {
            role: "isHeader"

            DelegateChoice {
                roleValue: true

                PlaylistGroupHeader {
                    width: ListView.view.width

                    onClicked: root.playlistView.toggleGroup(group)
                }
            }
            DelegateChoice {
                roleValue: false

                PlaylistRow {
                    showGroup: !(root.playlistView.grouped && root.playlistView.hasGroups)
                    showLogo: root.showLogos
                    width: ListView.view.width

                    onClicked: root.playList.currentRow = sourceRow
                    onDoubleClicked: root.playRequested()
                    onRemoveRequested: root.removeItem(sourceRow)
                }
            }
        }
    }
}
