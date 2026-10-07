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
    readonly property bool popupOpen: clearConfirmDialog.visible || moreMenu.visible || filterMenu.visible || sortMenu.visible || entryList.menuOpen
    property bool showLogos: false
    property bool shuffleEnabled: false

    signal closeRequested
    signal entriesRemoved(int count)
    signal itemSelected(string path, string name)
    signal playEntryRequested(int sourceRow)
    signal removeDuplicatesRequested
    signal saveRequested
    signal showLogosToggled(bool enabled)
    signal shuffleToggled(bool enabled)
    signal urlsDropped(var urls, int sourceRow)
    signal widthRequested(int requestedWidth)

    component FocusRing: Rectangle {
        anchors.fill: parent
        border.color: Material.foreground
        border.width: 2
        color: "transparent"
        radius: 3
        visible: parent.visualFocus
    }
    component PanelButton: Button {
        property bool active: false
        property string tip: ""

        Accessible.role: Accessible.Button
        Layout.preferredHeight: 30
        Layout.preferredWidth: 25
        Material.roundedScale: Material.NotRounded
        font.family: materialSymbolsOutlined.name
        font.weight: Font.Light
        hoverEnabled: true
        scale: 1.5

        FocusRing {
        }
        Rectangle {
            anchors.right: parent.right
            anchors.rightMargin: 2
            anchors.top: parent.top
            anchors.topMargin: 5
            color: Material.foreground
            height: 5
            radius: 2.5
            visible: parent.active
            width: 5
        }
        ToolTip {
            delay: AppConstants.tooltipDelay
            text: parent.tip
            timeout: AppConstants.tooltipTimeout
            visible: parent.hovered && parent.tip !== ""
        }
    }

    function focusList() {
        entryList.focusList();
    }
    function focusSearch() {
        searchField.forceActiveFocus();
        searchField.selectAll();
    }

    Connections {
        function onSearchTextChanged() {
            if (searchField.text !== root.playlistView.searchText)
                searchField.text = root.playlistView.searchText;
        }

        target: root.playlistView
    }
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
                        text: root.playlistView.searchText.trim() !== "" || root.playlistView.filter !== PlaylistView.All ? qsTr("%1 of %2").arg(root.playlistView.matchCount).arg(root.playList.count) : root.playList.count + " " + (root.playList.count === 1 ? qsTr("item") : qsTr("items"))
                    }
                    PanelButton {
                        id: shuffleButton

                        Accessible.description: qsTr("Play the playlist in random order")
                        Accessible.name: checked ? qsTr("Shuffle: On") : qsTr("Shuffle: Off")
                        active: checked
                        checkable: true
                        checked: root.shuffleEnabled
                        text: "\ue043"
                        tip: checked ? qsTr("Shuffle: On") : qsTr("Shuffle: Off")

                        contentItem: Text {
                            color: shuffleButton.checked ? Material.accent : Material.foreground
                            font: shuffleButton.font
                            horizontalAlignment: Text.AlignHCenter
                            opacity: shuffleButton.checked ? 1.0 : 0.5
                            text: shuffleButton.text
                            verticalAlignment: Text.AlignVCenter
                        }

                        onToggled: root.shuffleToggled(checked)
                    }
                    PanelButton {
                        id: saveButton

                        Accessible.description: qsTr("Save the playlist to a file")
                        Accessible.name: qsTr("Save playlist")
                        enabled: root.playList.count > 0
                        text: "\ue161"
                        tip: qsTr("Save playlist")

                        onClicked: root.saveRequested()
                    }
                    CollageButton {
                        collageTarget: root.collageTarget
                        sourceUrls: root.playList.count > 0 ? root.playList.locations() : []

                        FocusRing {
                        }
                    }
                    PanelButton {
                        id: moreButton

                        Accessible.description: qsTr("More playlist actions")
                        Accessible.name: qsTr("More")
                        text: "\ue5d4"
                        tip: qsTr("More")

                        onClicked: moreMenu.popup(moreButton, 0, moreButton.height)
                        Menu {
                            id: moreMenu

                            MenuItem {
                                enabled: root.playList.count > 0
                                text: qsTr("Clear playlist")

                                onTriggered: clearConfirmDialog.open()
                            }
                            MenuItem {
                                enabled: root.playList.count > 1
                                text: qsTr("Remove duplicates")

                                onTriggered: root.removeDuplicatesRequested()
                            }
                            MenuItem {
                                checkable: true
                                checked: root.showLogos
                                text: qsTr("Show channel logos")

                                onTriggered: root.showLogosToggled(checked)
                            }
                        }
                    }
                    PanelButton {
                        id: closeButton

                        Accessible.description: qsTr("Hide the playlist")
                        Accessible.name: qsTr("Close playlist")
                        text: "\ue5cd"
                        tip: qsTr("Close playlist")
                        visible: root.overlay && root.narrow

                        onClicked: root.closeRequested()
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 15

                TextField {
                    id: searchField

                    Accessible.name: qsTr("Search playlist")
                    Layout.fillWidth: true
                    placeholderText: qsTr("Search playlist...")
                    placeholderTextColor: Qt.rgba(Material.foreground.r, Material.foreground.g, Material.foreground.b, 0.7)
                    selectByMouse: true

                    onTextChanged: root.playlistView.searchText = text

                    Keys.onDownPressed: root.focusList()
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
                        text: "\ue5cd"
                        topInset: 0
                        visible: searchField.text.length > 0
                        width: 28

                        FocusRing {
                            radius: 14
                        }

                        onClicked: {
                            searchField.text = "";
                            searchField.focus = false;
                        }
                    }
                }
                PanelButton {
                    id: filterButton

                    readonly property string current: root.playlistView.filter === PlaylistView.LocalFiles ? qsTr("Local files") : (root.playlistView.filter === PlaylistView.Streams ? qsTr("Streams") : qsTr("All"))

                    Accessible.description: qsTr("Choose which kinds of entry are shown")
                    Accessible.name: qsTr("Filter: %1").arg(current)
                    Material.foreground: root.playlistView.filter !== PlaylistView.All ? Material.accent : undefined
                    active: root.playlistView.filter !== PlaylistView.All
                    text: "\ue152"
                    tip: qsTr("Filter: %1").arg(current)

                    onClicked: filterMenu.popup(filterButton, 0, filterButton.height)

                    Menu {
                        id: filterMenu

                        ActionGroup {
                            id: filterGroup

                        }
                        MenuItem {
                            action: Action {
                                ActionGroup.group: filterGroup
                                checkable: true
                                checked: root.playlistView.filter === PlaylistView.All
                                text: qsTr("All")

                                onTriggered: root.playlistView.filter = PlaylistView.All
                            }
                        }
                        MenuItem {
                            action: Action {
                                ActionGroup.group: filterGroup
                                checkable: true
                                checked: root.playlistView.filter === PlaylistView.LocalFiles
                                text: qsTr("Local files")

                                onTriggered: root.playlistView.filter = PlaylistView.LocalFiles
                            }
                        }
                        MenuItem {
                            action: Action {
                                ActionGroup.group: filterGroup
                                checkable: true
                                checked: root.playlistView.filter === PlaylistView.Streams
                                text: qsTr("Streams")

                                onTriggered: root.playlistView.filter = PlaylistView.Streams
                            }
                        }
                    }
                }
                PanelButton {
                    id: sortButton

                    readonly property string current: root.playlistView.sortOrder === PlaylistView.Title ? qsTr("Title") : (root.playlistView.sortOrder === PlaylistView.Duration ? qsTr("Duration") : qsTr("Playlist order"))

                    Accessible.description: qsTr("Choose the order entries are shown in")
                    Accessible.name: qsTr("Sort: %1").arg(current)
                    Material.foreground: root.playlistView.sortOrder !== PlaylistView.PlaylistOrder ? Material.accent : undefined
                    active: root.playlistView.sortOrder !== PlaylistView.PlaylistOrder
                    text: "\ue8d5"
                    tip: qsTr("Sort: %1").arg(current)

                    onClicked: sortMenu.popup(sortButton, 0, sortButton.height)

                    Menu {
                        id: sortMenu

                        ActionGroup {
                            id: sortGroup

                        }
                        MenuItem {
                            action: Action {
                                ActionGroup.group: sortGroup
                                checkable: true
                                checked: root.playlistView.sortOrder === PlaylistView.PlaylistOrder
                                text: qsTr("Playlist order")

                                onTriggered: root.playlistView.sortOrder = PlaylistView.PlaylistOrder
                            }
                        }
                        MenuItem {
                            action: Action {
                                ActionGroup.group: sortGroup
                                checkable: true
                                checked: root.playlistView.sortOrder === PlaylistView.Title
                                text: qsTr("Title")

                                onTriggered: root.playlistView.sortOrder = PlaylistView.Title
                            }
                        }
                        MenuItem {
                            action: Action {
                                ActionGroup.group: sortGroup
                                checkable: true
                                checked: root.playlistView.sortOrder === PlaylistView.Duration
                                text: qsTr("Duration")

                                onTriggered: root.playlistView.sortOrder = PlaylistView.Duration
                            }
                        }
                    }
                }
                PanelButton {
                    id: groupButton

                    Accessible.description: qsTr("Show entries under their group headings")
                    Accessible.name: checked ? qsTr("Grouping: On") : qsTr("Grouping: Off")
                    Material.foreground: checked ? Material.accent : undefined
                    active: checked
                    checkable: true
                    checked: root.playlistView.grouped
                    text: "\ue574"
                    tip: checked ? qsTr("Grouping: On") : qsTr("Grouping: Off")
                    visible: root.playlistView.hasGroups

                    onToggled: root.playlistView.grouped = checked
                }
                }
            }
        }
        Item {
            Layout.fillHeight: true
            Layout.fillWidth: true

            PlayListComponent {
                id: entryList

                anchors.fill: parent
                playList: root.playList
                playlistView: root.playlistView
                showLogos: root.showLogos
                visible: root.playlistView.matchCount > 0

                onEntriesRemoved: function (count) {
                    root.entriesRemoved(count);
                }
                onItemSelected: function (path, name) {
                    root.itemSelected(path, name);
                }
                onPlayEntryRequested: function (sourceRow) {
                    root.playEntryRequested(sourceRow);
                }
                onSearchRequested: root.focusSearch()
                onUrlsDropped: function (urls, sourceRow) {
                    root.urlsDropped(urls, sourceRow);
                }
            }
            ColumnLayout {
                anchors.centerIn: parent
                spacing: 10
                visible: root.playList.count > 0 && root.playlistView.matchCount === 0
                width: Math.min(parent.width - 40, 420)

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    color: Material.foreground
                    font.family: materialSymbolsOutlined.name
                    font.pixelSize: 48
                    font.weight: Font.ExtraLight
                    text: "\uea76"
                }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    color: Material.foreground
                    font.pixelSize: 18
                    text: qsTr("Nothing matches")
                }
                Button {
                    Layout.alignment: Qt.AlignHCenter
                    flat: true
                    text: qsTr("Clear search and filter")

                    onClicked: root.playlistView.clearSearchAndFilter()

                    FocusRing {
                    }
                }
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
                    text: "\uf523"

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
        Accessible.role: Accessible.Grip
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
