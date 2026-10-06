import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

import gavqml

Item {
    id: root

    required property bool available
    property bool canReorder: true
    readonly property string detail: {
        if (!available)
            return reason ? qsTr("Unavailable – %1").arg(reason) : qsTr("Unavailable");
        var parts = [kind === PlaylistModel.LocalAudio ? qsTr("Audio") : (kind === PlaylistModel.Stream ? qsTr("Stream") : qsTr("Video"))];
        if (kind === PlaylistModel.Stream && streamState === PlaylistModel.StreamLive)
            parts.push(qsTr("Live"));
        else if (durationMs > 0)
            parts.push(AppConstants.formatTime(durationMs));
        if (group && showGroup)
            parts.push(group);
        return parts.join(" · ");
    }
    required property real durationMs
    required property int entryId
    property bool focused: false
    required property string group
    readonly property bool hovered: hoverHandler.hovered
    required property int index
    required property bool isCurrent
    required property int kind
    required property url location
    readonly property string locationText: {
        var text = location.toString();
        if (!text.startsWith("file:"))
            return text;
        text = decodeURIComponent(text.replace(/^file:\/\//, ""));
        return /^\/[A-Za-z]:/.test(text) ? text.substring(1) : text;
    }
    required property url logo
    required property int queuePosition
    required property string reason
    required property bool selected
    property bool showGroup: true
    property bool showLogo: false
    required property int sourceRow
    required property int streamState
    required property string title

    signal activated(int modifiers)
    signal menuRequested
    signal playRequested
    signal removeRequested
    signal reorderCanceled
    signal reorderFinished
    signal reorderMoved(real position)
    signal reorderStarted

    Accessible.name: title + ", " + detail + (isCurrent ? ", " + qsTr("playing") : "") + (queuePosition > 0 ? ", " + qsTr("queued %1").arg(queuePosition) : "") + (selected ? ", " + qsTr("selected") : "")
    Accessible.role: Accessible.ListItem
    clip: true
    implicitHeight: AppConstants.playlistRowHeight

    HoverHandler {
        id: hoverHandler

    }
    Rectangle {
        anchors.fill: parent
        border.color: Material.foreground
        border.width: root.focused ? 2 : 0
        color: rowMouse.pressed ? Material.listHighlightColor : (root.isCurrent ? Qt.rgba(Material.accent.r, Material.accent.g, Material.accent.b, 0.3) : (root.selected ? Qt.rgba(Material.foreground.r, Material.foreground.g, Material.foreground.b, 0.14) : (root.hovered ? Material.dividerColor : "transparent")))
        radius: 4

        Rectangle {
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 6
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.topMargin: 6
            color: Material.foreground
            radius: 2
            visible: root.selected
            width: 4
        }
        MouseArea {
            id: rowMouse

            acceptedButtons: Qt.LeftButton | Qt.RightButton
            anchors.fill: parent

            onClicked: function (mouse) {
                if (mouse.button === Qt.RightButton)
                    root.menuRequested();
                else
                    root.activated(mouse.modifiers);
            }
            onDoubleClicked: function (mouse) {
                if (mouse.button === Qt.LeftButton)
                    root.playRequested();
            }
        }
    }
    RowLayout {
        anchors.bottomMargin: 7
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 8
        anchors.topMargin: 7
        spacing: 12

        Item {
            Layout.preferredHeight: root.showLogo ? 36 : 24
            Layout.preferredWidth: root.showLogo ? 36 : 24

            Loader {
                id: logoLoader

                readonly property bool ready: item !== null && item.status === Image.Ready && item.implicitWidth > 1

                active: root.showLogo && root.logo.toString() !== ""
                anchors.fill: parent
                visible: ready

                sourceComponent: Image {
                    asynchronous: true
                    fillMode: Image.PreserveAspectFit
                    opacity: root.available ? 1 : 0.7
                    source: "image://logo/" + encodeURIComponent(root.logo)
                }
            }
            Text {
                anchors.centerIn: parent
                color: Material.foreground
                font.family: materialSymbolsOutlined.name
                font.pixelSize: 24
                opacity: root.available ? 1 : 0.7
                text: root.kind === PlaylistModel.LocalAudio ? "\ue405" : (root.kind === PlaylistModel.Stream ? "\ue894" : "\ueb87")
                visible: !logoLoader.ready
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Text {
                Layout.fillWidth: true
                color: Material.foreground
                elide: Text.ElideRight
                font.bold: root.isCurrent
                font.pixelSize: 14
                opacity: root.available ? 1 : 0.7
                text: root.title
            }
            Text {
                Layout.fillWidth: true
                color: Material.foreground
                elide: Text.ElideMiddle
                font.pixelSize: 12
                opacity: 0.7
                text: root.locationText
            }
            Text {
                Layout.fillWidth: true
                color: Material.foreground
                elide: Text.ElideRight
                font.pixelSize: 12
                opacity: 0.7
                text: root.detail
            }
        }
        Rectangle {
            Layout.preferredHeight: 22
            Layout.preferredWidth: Math.max(22, queueText.implicitWidth + 10)
            color: Material.foreground
            radius: 11
            visible: root.queuePosition > 0

            Text {
                id: queueText

                anchors.centerIn: parent
                color: Material.background
                font.bold: true
                font.pixelSize: 12
                text: root.queuePosition
            }
        }
        Text {
            color: Material.foreground
            font.family: materialSymbolsOutlined.name
            font.pixelSize: 20
            text: "\ue037"
            visible: root.isCurrent
        }
        Item {
            Layout.preferredHeight: 28
            Layout.preferredWidth: 22
            opacity: root.hovered || root.focused || handleMouse.pressed ? (root.canReorder ? 1 : 0.4) : 0

            Behavior on opacity {
                NumberAnimation {
                    duration: 100
                }
            }

            Text {
                anchors.centerIn: parent
                color: Material.foreground
                font.family: materialSymbolsOutlined.name
                font.pixelSize: 20
                text: "\ue945"
            }
            MouseArea {
                id: handleMouse

                Accessible.name: qsTr("Drag to reorder")
                anchors.fill: parent
                cursorShape: root.canReorder ? (pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor) : Qt.ForbiddenCursor
                enabled: parent.opacity > 0
                hoverEnabled: true
                preventStealing: true

                onCanceled: root.reorderCanceled()
                onPositionChanged: function (mouse) {
                    if (pressed && root.canReorder)
                        root.reorderMoved(mapToItem(root, mouse.x, mouse.y).y);
                }
                onPressed: {
                    if (root.canReorder)
                        root.reorderStarted();
                }
                onReleased: {
                    if (root.canReorder)
                        root.reorderFinished();
                }

                ToolTip {
                    delay: AppConstants.tooltipDelay
                    text: root.canReorder ? qsTr("Drag to reorder") : qsTr("Reordering is available in playlist order with no search, filter or grouping")
                    timeout: AppConstants.tooltipTimeout
                    visible: handleMouse.containsMouse
                }
            }
        }
        Button {
            id: removeButton

            Accessible.name: qsTr("Remove from playlist")
            Layout.preferredHeight: 28
            Layout.preferredWidth: 28
            bottomInset: 0
            flat: true
            font.family: materialSymbolsOutlined.name
            font.pixelSize: 18
            hoverEnabled: true
            leftInset: 0
            enabled: opacity > 0
            opacity: root.hovered || root.focused || visualFocus ? 1 : 0
            padding: 0
            rightInset: 0
            topInset: 0
            text: "\ue5cd"

            Behavior on opacity {
                NumberAnimation {
                    duration: 100
                }
            }

            onClicked: root.removeRequested()

            ToolTip {
                delay: AppConstants.tooltipDelay
                text: qsTr("Remove from playlist")
                timeout: AppConstants.tooltipTimeout
                visible: removeButton.hovered
            }
        }
    }
}
