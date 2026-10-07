import QtQml.Models
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

import gavqml

Item {
    id: root

    property real dragPosition: 0
    property var draggedIds: []
    property int dropRow: -1
    readonly property bool menuOpen: rowMenu.visible
    required property PlaylistModel playList
    required property PlaylistView playlistView
    property bool showLogos: false

    signal entriesRemoved(int count)
    signal itemSelected(string path, string name)
    signal playEntryRequested(int sourceRow)
    signal searchRequested
    signal urlsDropped(var urls, int sourceRow)

    function activateRow(viewRow) {
        var sourceRow = playlistView.sourceRowFor(viewRow);
        if (sourceRow >= 0)
            playEntryRequested(sourceRow);
        else if (isHeader(viewRow) && playListView.itemAtIndex(viewRow))
            playlistView.toggleGroup(playListView.itemAtIndex(viewRow).group);
    }
    function finishReorder(apply) {
        if (apply && dropRow >= 0 && draggedIds.length > 0)
            playList.move(draggedIds, dropRow);
        draggedIds = [];
        dropRow = -1;
    }
    function focusList() {
        var fresh = playListView.currentIndex < 0;
        playListView.forceActiveFocus();
        if (fresh && playListView.count > 0)
            focusRow(0, Qt.NoModifier);
    }
    function focusRow(viewRow, modifiers) {
        if (playListView.count === 0)
            return;
        var row = Math.max(0, Math.min(playListView.count - 1, viewRow));
        playListView.currentIndex = row;
        playListView.positionViewAtIndex(row, ListView.Contain);
        playlistView.select(row, modifiers);
    }
    function isHeader(viewRow) {
        return viewRow >= 0 && viewRow < playListView.count && playlistView.sourceRowFor(viewRow) < 0;
    }
    function moveTargets(delta) {
        var ids = targetIds();
        if (!playlistView.canReorder || ids.length === 0)
            return;
        var first = playList.count;
        var last = -1;
        for (var i = 0; i < ids.length; i++) {
            var row = playList.rowForId(ids[i]);
            first = Math.min(first, row);
            last = Math.max(last, row);
        }
        var focused = playList.idAt(playlistView.sourceRowFor(playListView.currentIndex));
        if (playList.move(ids, delta < 0 ? first - 1 : last + 2) && focused >= 0) {
            playListView.currentIndex = playlistView.viewRowFor(playList.rowForId(focused));
            playListView.positionViewAtIndex(playListView.currentIndex, ListView.Contain);
        }
    }
    function openMenu(viewRow, item) {
        var sourceRow = playlistView.sourceRowFor(viewRow);
        if (sourceRow < 0)
            return;
        var id = playList.idAt(sourceRow);
        var ids = playlistView.selectedIds();
        if (ids.indexOf(id) < 0) {
            playListView.currentIndex = viewRow;
            playlistView.select(viewRow, Qt.NoModifier);
            ids = [id];
        }
        var entry = playList.entryAt(sourceRow);
        rowMenu.entryId = id;
        rowMenu.ids = ids;
        rowMenu.location = entry.path;
        rowMenu.local = entry.kind !== PlaylistModel.Stream;
        if (item)
            rowMenu.popup(item, 48, item.height - 8);
        else
            rowMenu.popup();
    }
    function queueIds(ids) {
        for (var i = 0; i < ids.length; i++)
            playList.playNext(ids[i]);
    }
    function removeIds(ids) {
        var count = playList.remove(ids);
        if (count > 0)
            entriesRemoved(count);
    }
    function showCurrent() {
        var row = playlistView.currentViewRow;
        if (row >= 0 && !playListView.moving && !playListView.dragging)
            playListView.positionViewAtIndex(row, ListView.Contain);
    }
    function showStart() {
        playListView.forceLayout();
        playListView.positionViewAtBeginning();
    }
    function targetIds() {
        var ids = playlistView.selectedIds();
        if (ids.length === 0) {
            var sourceRow = playlistView.sourceRowFor(playListView.currentIndex);
            if (sourceRow >= 0)
                ids = [playList.idAt(sourceRow)];
        }
        return ids;
    }
    function updateDrop(position) {
        dragPosition = position;
        var contentPosition = position + playListView.contentY;
        var row = playListView.indexAt(10, contentPosition);
        if (row < 0) {
            row = contentPosition < playListView.originY ? 0 : playListView.count;
        } else {
            var item = playListView.itemAtIndex(row);
            if (item && contentPosition > item.y + item.height / 2)
                row += 1;
        }
        dropRow = row;
        var target = playListView.itemAtIndex(Math.min(row, playListView.count - 1));
        if (target)
            dropLine.y = (row < playListView.count ? target.y : target.y + target.height) - 1;
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
    Timer {
        id: scrollTimer

        interval: 30
        repeat: true
        running: root.draggedIds.length > 0 && (root.dragPosition < 28 || root.dragPosition > playListView.height - 28)

        onTriggered: {
            var top = playListView.originY - playListView.topMargin;
            var bottom = Math.max(top, playListView.originY + playListView.contentHeight - playListView.height);
            playListView.contentY = Math.max(top, Math.min(bottom, playListView.contentY + (root.dragPosition < 28 ? -16 : 16)));
            root.updateDrop(root.dragPosition);
        }
    }
    Menu {
        id: rowMenu

        property int entryId: -1
        property var ids: []
        property bool local: true
        property string location: ""
        readonly property bool single: ids.length <= 1

        MenuItem {
            height: visible ? implicitHeight : 0
            text: qsTr("Play")
            visible: rowMenu.single

            onTriggered: root.playEntryRequested(root.playList.rowForId(rowMenu.entryId))
        }
        MenuItem {
            text: qsTr("Play next")

            onTriggered: root.queueIds(rowMenu.ids)
        }
        MenuItem {
            text: qsTr("Remove")

            onTriggered: root.removeIds(rowMenu.ids)
        }
        MenuItem {
            height: visible ? implicitHeight : 0
            text: qsTr("Show in file manager")
            visible: rowMenu.single && rowMenu.local

            onTriggered: PlaybackUtils.revealInFileManager(rowMenu.location)
        }
        MenuItem {
            height: visible ? implicitHeight : 0
            text: qsTr("Copy address")
            visible: rowMenu.single && !rowMenu.local

            onTriggered: SystemStats.copyToClipboard(rowMenu.location)
        }
    }
    ListView {
        id: playListView

        Accessible.name: qsTr("Playlist entries")
        Accessible.role: Accessible.List
        activeFocusOnTab: true
        anchors.fill: parent
        boundsBehavior: Flickable.StopAtBounds
        cacheBuffer: AppConstants.playlistRowHeight * 3
        clip: true
        currentIndex: -1
        highlightFollowsCurrentItem: false
        keyNavigationEnabled: false
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
                    required property int index

                    focused: ListView.isCurrentItem && ListView.view.activeFocus
                    width: ListView.view.width

                    onClicked: {
                        ListView.view.currentIndex = index;
                        ListView.view.forceActiveFocus();
                        root.playlistView.toggleGroup(group);
                    }
                }
            }
            DelegateChoice {
                roleValue: false

                PlaylistRow {
                    id: row

                    canReorder: root.playlistView.canReorder
                    focused: ListView.isCurrentItem && ListView.view.activeFocus
                    showGroup: !(root.playlistView.grouped && root.playlistView.hasGroups)
                    showLogo: root.showLogos
                    width: ListView.view.width

                    onActivated: function (modifiers) {
                        ListView.view.currentIndex = index;
                        ListView.view.forceActiveFocus();
                        root.playlistView.select(index, modifiers);
                    }
                    onMenuRequested: {
                        ListView.view.forceActiveFocus();
                        root.openMenu(index, null);
                    }
                    onPlayRequested: root.playEntryRequested(sourceRow)
                    onRemoveRequested: root.removeIds([entryId])
                    onReorderCanceled: root.finishReorder(false)
                    onReorderFinished: root.finishReorder(true)
                    onReorderMoved: function (position) {
                        root.updateDrop(row.mapToItem(playListView, 0, position).y);
                    }
                    onReorderStarted: {
                        ListView.view.currentIndex = index;
                        root.draggedIds = selected ? root.playlistView.selectedIds() : [entryId];
                    }
                }
            }
        }

        onActiveFocusChanged: {
            if (activeFocus && currentIndex < 0 && count > 0)
                currentIndex = Math.max(0, root.playlistView.currentViewRow);
        }

        Keys.onPressed: function (event) {
            var row = playListView.currentIndex;
            var shift = event.modifiers & Qt.ShiftModifier;
            var control = event.modifiers & Qt.ControlModifier;
            var page = Math.max(1, Math.floor(playListView.height / AppConstants.playlistRowHeight) - 1);
            switch (event.key) {
            case Qt.Key_Up:
            case Qt.Key_Down:
                var step = event.key === Qt.Key_Up ? -1 : 1;
                if (event.modifiers & Qt.AltModifier)
                    root.moveTargets(step);
                else
                    root.focusRow(row < 0 ? 0 : row + step, shift ? Qt.ShiftModifier : Qt.NoModifier);
                break;
            case Qt.Key_PageUp:
                root.focusRow(row - page, shift ? Qt.ShiftModifier : Qt.NoModifier);
                break;
            case Qt.Key_PageDown:
                root.focusRow(row + page, shift ? Qt.ShiftModifier : Qt.NoModifier);
                break;
            case Qt.Key_Home:
                root.focusRow(0, shift ? Qt.ShiftModifier : Qt.NoModifier);
                break;
            case Qt.Key_End:
                root.focusRow(playListView.count - 1, shift ? Qt.ShiftModifier : Qt.NoModifier);
                break;
            case Qt.Key_Return:
            case Qt.Key_Enter:
                root.activateRow(row);
                break;
            case Qt.Key_Space:
                if (!control)
                    return;
                root.playlistView.select(row, Qt.ControlModifier);
                break;
            case Qt.Key_A:
                if (!control)
                    return;
                root.playlistView.selectAll();
                break;
            case Qt.Key_Z:
                if (!control)
                    return;
                root.playList.undoRemove();
                break;
            case Qt.Key_Delete:
                root.removeIds(root.targetIds());
                break;
            case Qt.Key_Q:
                var queued = root.playlistView.sourceRowFor(row);
                if (queued >= 0)
                    root.playList.playNext(root.playList.idAt(queued));
                break;
            case Qt.Key_Menu:
                root.openMenu(row, playListView.itemAtIndex(row));
                break;
            case Qt.Key_F10:
                if (!shift)
                    return;
                root.openMenu(row, playListView.itemAtIndex(row));
                break;
            case Qt.Key_Left:
            case Qt.Key_Right:
                if (!root.isHeader(row))
                    return;
                var item = playListView.itemAtIndex(row);
                if (item && item.collapsed !== (event.key === Qt.Key_Left))
                    root.playlistView.toggleGroup(item.group);
                break;
            case Qt.Key_Slash:
                root.searchRequested();
                break;
            default:
                return;
            }
            event.accepted = true;
        }
        Keys.onShortcutOverride: function (event) {
            if ((event.key === Qt.Key_Left || event.key === Qt.Key_Right) && root.isHeader(playListView.currentIndex))
                event.accepted = true;
        }

        Rectangle {
            id: dropLine

            color: Material.accent
            height: 3
            parent: playListView.contentItem
            radius: 1
            visible: root.dropRow >= 0
            width: playListView.width
            z: 5
        }
        DropArea {
            anchors.fill: parent

            onDropped: function (drop) {
                var row = root.dropRow;
                root.dropRow = -1;
                if (!drop.urls || drop.urls.length === 0)
                    return;
                root.urlsDropped(drop.urls, row >= 0 && row < playListView.count ? root.playlistView.sourceRowFor(row) : -1);
                drop.accept();
            }
            onExited: root.dropRow = -1
            onPositionChanged: function (drag) {
                root.updateDrop(drag.y);
            }
        }
    }
}
