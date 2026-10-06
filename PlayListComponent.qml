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
    property bool showLogos: false

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
        delegate: PlaylistRow {
            readonly property bool matches: root.matchesFilter(title)

            height: matches ? implicitHeight : 0
            showLogo: root.showLogos
            visible: matches
            width: ListView.view.width

            Behavior on height {
                NumberAnimation {
                    duration: 150
                }
            }

            onClicked: root.playList.currentRow = sourceRow
            onDoubleClicked: root.playRequested()
            onRemoveRequested: root.removeItem(sourceRow)
        }
    }
}
