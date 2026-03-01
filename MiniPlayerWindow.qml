import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtMultimedia

import gavqml

Window {
    id: miniPlayerWindow

    property alias miniVideoOutput: miniVideoOutput
    required property var mediaPlayer

    signal closeRequested
    signal restoreRequested

    color: "black"
    flags: Qt.WindowStaysOnTopHint | Qt.FramelessWindowHint | Qt.Tool
    height: 200
    title: qsTr("GAV Mini Player")
    width: 320

    Material.theme: Material.Dark

    FontLoader {
        id: materialSymbolsOutlined

        source: "qrc:/assets/fonts/MaterialSymbolsOutlined.ttf"
    }

    VideoOutput {
        id: miniVideoOutput

        anchors.fill: parent
    }

    // Full-window drag area (beneath control overlays)
    MouseArea {
        id: dragArea

        property int startX: 0
        property int startY: 0

        anchors.fill: parent
        z: 1

        onPositionChanged: function (mouse) {
            if (pressed) {
                miniPlayerWindow.x += mouse.x - startX;
                miniPlayerWindow.y += mouse.y - startY;
            }
        }
        onPressed: function (mouse) {
            startX = mouse.x;
            startY = mouse.y;
        }
    }

    // Hover area to reveal play/pause overlay
    MouseArea {
        id: hoverArea

        anchors.fill: parent
        hoverEnabled: true
        z: 2

        acceptedButtons: Qt.NoButton
    }

    // Play / pause centre overlay (visible on hover)
    Rectangle {
        id: playPauseOverlay

        anchors.centerIn: parent
        color: Qt.rgba(0, 0, 0, 0.55)
        height: 48
        opacity: hoverArea.containsMouse ? 1.0 : 0.0
        radius: 24
        width: 48
        z: 10

        Behavior on opacity {
            NumberAnimation {
                duration: 200
            }
        }

        Text {
            anchors.centerIn: parent
            color: "white"
            font.family: materialSymbolsOutlined.name
            font.pixelSize: 28
            text: miniPlayerWindow.mediaPlayer
                  && miniPlayerWindow.mediaPlayer.playbackState === MediaPlayer.PlayingState ? "\ue034" : "\ue037"
        }

        MouseArea {
            anchors.fill: parent
            z: 20

            onClicked: {
                if (!miniPlayerWindow.mediaPlayer)
                    return;
                if (miniPlayerWindow.mediaPlayer.playbackState === MediaPlayer.PlayingState) {
                    miniPlayerWindow.mediaPlayer.pause();
                } else {
                    miniPlayerWindow.mediaPlayer.play();
                }
            }
        }
    }

    // Top control bar
    Rectangle {
        id: topBar

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        color: Qt.rgba(0, 0, 0, 0.65)
        height: 30
        z: 15

        Row {
            anchors.right: parent.right
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            // Restore to main window
            Button {
                id: restoreButton

                Accessible.description: qsTr("Restore to main window")
                Accessible.name: qsTr("Restore")
                Accessible.role: Accessible.Button
                Material.roundedScale: Material.NotRounded
                ToolTip.delay: AppConstants.tooltipDelay
                ToolTip.text: qsTr("Restore to main window")
                ToolTip.timeout: AppConstants.tooltipTimeout
                ToolTip.visible: hovered
                flat: true
                font.family: materialSymbolsOutlined.name
                font.pixelSize: 16
                height: 26
                padding: 0
                text: "\ue8c8"
                width: 26

                onClicked: miniPlayerWindow.restoreRequested()
            }

            // Minimize
            Button {
                id: minimizeButton

                Accessible.description: qsTr("Minimize mini player")
                Accessible.name: qsTr("Minimize")
                Accessible.role: Accessible.Button
                Material.roundedScale: Material.NotRounded
                ToolTip.delay: AppConstants.tooltipDelay
                ToolTip.text: qsTr("Minimize")
                ToolTip.timeout: AppConstants.tooltipTimeout
                ToolTip.visible: hovered
                flat: true
                font.family: materialSymbolsOutlined.name
                font.pixelSize: 16
                height: 26
                padding: 0
                text: "\uf849"
                width: 26

                onClicked: miniPlayerWindow.showMinimized()
            }

            // Close
            Button {
                id: closeButton

                Accessible.description: qsTr("Close mini player")
                Accessible.name: qsTr("Close")
                Accessible.role: Accessible.Button
                Material.roundedScale: Material.NotRounded
                ToolTip.delay: AppConstants.tooltipDelay
                ToolTip.text: qsTr("Close mini player")
                ToolTip.timeout: AppConstants.tooltipTimeout
                ToolTip.visible: hovered
                flat: true
                font.family: materialSymbolsOutlined.name
                font.pixelSize: 16
                height: 26
                padding: 0
                text: "\ue5cd"
                width: 26

                onClicked: miniPlayerWindow.closeRequested()
            }
        }
    }

    onClosing: function (close) {
        close.accepted = false;
        miniPlayerWindow.closeRequested();
    }
}
