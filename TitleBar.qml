import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

import gavqml

Rectangle {
    id: root

    readonly property bool isMac: Qt.platform.os === "osx"
    readonly property bool isMaximized: targetWindow.visibility === Window.Maximized
    required property Window targetWindow
    property string windowTitle: ""

    signal aboutRequested
    signal checkUpdatesRequested
    signal exitRequested
    signal openFileRequested
    signal settingsRequested

    function toggleMaximize() {
        if (targetWindow.visibility === Window.Maximized) {
            targetWindow.visibility = Window.Windowed;
        } else {
            targetWindow.visibility = Window.Maximized;
        }
    }

    color: Material.background.darker(1.2)
    implicitHeight: 32

    FontLoader {
        id: materialSymbolsOutlined

        source: "qrc:/assets/fonts/MaterialSymbolsOutlined.ttf"
    }
    DragHandler {
        target: null

        onActiveChanged: if (active)
            root.targetWindow.startSystemMove()
    }
    TapHandler {
        onDoubleTapped: root.toggleMaximize()
    }
    RowLayout {
        anchors.fill: parent
        spacing: 8

        // macOS: traffic lights on the left
        Row {
            Layout.alignment: Qt.AlignVCenter
            Layout.leftMargin: 10
            spacing: 8
            visible: root.isMac

            Rectangle {
                color: "#ff5f57"
                height: 12
                radius: 6
                width: 12

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true

                    onClicked: root.exitRequested()
                }
            }
            Rectangle {
                color: "#febc2e"
                height: 12
                radius: 6
                width: 12

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true

                    onClicked: root.targetWindow.showMinimized()
                }
            }
            Rectangle {
                color: "#28c840"
                height: 12
                radius: 6
                width: 12

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true

                    onClicked: root.toggleMaximize()
                }
            }
        }

        // Windows/Linux: title text on the left
        Text {
            Layout.alignment: Qt.AlignVCenter
            Layout.leftMargin: 10
            Layout.maximumWidth: 280
            color: Material.foreground
            elide: Text.ElideRight
            text: root.windowTitle
            visible: !root.isMac
        }

        // Inline menu (File / View / Help) for all platforms
        Row {
            Layout.alignment: Qt.AlignVCenter
            Layout.leftMargin: 8

            Button {
                id: fileMenuButton

                Material.roundedScale: Material.NotRounded
                flat: true
                font.pixelSize: 12
                padding: 6
                text: qsTr("File")

                onClicked: fileMenu.open()

                Menu {
                    id: fileMenu

                    y: fileMenuButton.height

                    Action {
                        text: qsTr("Open")

                        onTriggered: root.openFileRequested()
                    }
                    MenuSeparator {
                    }
                    Action {
                        text: qsTr("Exit")

                        onTriggered: root.exitRequested()
                    }
                }
            }
            Button {
                id: viewMenuButton

                Material.roundedScale: Material.NotRounded
                flat: true
                font.pixelSize: 12
                padding: 6
                text: qsTr("View")

                onClicked: viewMenu.open()

                Menu {
                    id: viewMenu

                    y: viewMenuButton.height

                    Action {
                        text: qsTr("Settings")

                        onTriggered: root.settingsRequested()
                    }
                }
            }
            Button {
                id: helpMenuButton

                Material.roundedScale: Material.NotRounded
                flat: true
                font.pixelSize: 12
                padding: 6
                text: qsTr("Help")

                onClicked: helpMenu.open()

                Menu {
                    id: helpMenu

                    y: helpMenuButton.height

                    Action {
                        text: qsTr("Check for Updates")

                        onTriggered: root.checkUpdatesRequested()
                    }
                    Action {
                        text: qsTr("About")

                        onTriggered: root.aboutRequested()
                    }
                }
            }
        }
        Item {
            Layout.fillWidth: true
        }

        // macOS: title on the right (after the spacer)
        Text {
            Layout.alignment: Qt.AlignVCenter
            Layout.rightMargin: 10
            color: Material.foreground
            elide: Text.ElideRight
            text: root.windowTitle
            visible: root.isMac
        }

        // Windows/Linux: window buttons on the right
        Row {
            Layout.alignment: Qt.AlignVCenter
            spacing: 0
            visible: !root.isMac

            ToolButton {
                id: minimizeButton

                ToolTip.delay: AppConstants.tooltipDelay
                ToolTip.text: qsTr("Minimize")
                ToolTip.timeout: AppConstants.tooltipTimeout
                ToolTip.visible: hovered
                font.family: materialSymbolsOutlined.name
                font.pixelSize: 16
                height: 30
                hoverEnabled: true
                padding: 0
                text: "\ue15b"
                width: 46

                onClicked: root.targetWindow.showMinimized()
            }
            ToolButton {
                id: maximizeButton

                ToolTip.delay: AppConstants.tooltipDelay
                ToolTip.text: root.isMaximized ? qsTr("Restore") : qsTr("Maximize")
                ToolTip.timeout: AppConstants.tooltipTimeout
                ToolTip.visible: hovered
                font.family: materialSymbolsOutlined.name
                font.pixelSize: 16
                height: 30
                hoverEnabled: true
                padding: 0
                text: root.isMaximized ? "\ue3bb" : "\ue3c6"
                width: 46

                onClicked: root.toggleMaximize()
            }
            ToolButton {
                id: closeButton

                ToolTip.delay: AppConstants.tooltipDelay
                ToolTip.text: qsTr("Close")
                ToolTip.timeout: AppConstants.tooltipTimeout
                ToolTip.visible: hovered
                font.family: materialSymbolsOutlined.name
                font.pixelSize: 16
                height: 30
                hoverEnabled: true
                padding: 0
                text: "\ue5cd"
                width: 46

                background: Rectangle {
                    color: closeButton.hovered ? "#e81123" : "transparent"
                }
                contentItem: Text {
                    color: closeButton.hovered ? "white" : Material.foreground
                    font: closeButton.font
                    horizontalAlignment: Text.AlignHCenter
                    text: closeButton.text
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: root.exitRequested()
            }
        }
    }
}
