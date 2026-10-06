import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

import gavqml

ItemDelegate {
    id: root

    required property bool available
    readonly property string detail: {
        if (!available)
            return reason ? qsTr("Unavailable – %1").arg(reason) : qsTr("Unavailable");
        var parts = [kind === PlaylistModel.LocalAudio ? qsTr("Audio") : (kind === PlaylistModel.Stream ? qsTr("Stream") : qsTr("Video"))];
        if (kind === PlaylistModel.Stream && streamState === PlaylistModel.StreamLive)
            parts.push(qsTr("Live"));
        else if (durationMs > 0)
            parts.push(AppConstants.formatTime(durationMs));
        if (group)
            parts.push(group);
        return parts.join(" · ");
    }
    required property real durationMs
    required property string group
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
    required property string reason
    property bool showLogo: false
    required property int sourceRow
    required property int streamState
    required property string title

    signal removeRequested

    Accessible.name: title + ", " + detail + (isCurrent ? ", " + qsTr("playing") : "")
    bottomPadding: 7
    clip: true
    hoverEnabled: true
    implicitHeight: AppConstants.playlistRowHeight
    leftPadding: 10
    rightPadding: 8
    topPadding: 7

    background: Rectangle {
        color: root.down ? Material.listHighlightColor : (root.hovered ? Material.dividerColor : (root.isCurrent ? Qt.rgba(Material.accent.r, Material.accent.g, Material.accent.b, 0.3) : "transparent"))
        radius: 4
    }
    contentItem: RowLayout {
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
        Text {
            color: Material.foreground
            font.family: materialSymbolsOutlined.name
            font.pixelSize: 20
            text: "\ue037"
            visible: root.isCurrent
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
            opacity: root.hovered || visualFocus ? 1 : 0
            padding: 0
            rightInset: 0
            topInset: 0
            text: "\ue5cd"
            visible: opacity > 0

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
