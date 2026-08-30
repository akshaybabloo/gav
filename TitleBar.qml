import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

import gavqml

Rectangle {
    id: root

    readonly property bool isMac: Qt.platform.os === "osx"
    readonly property bool isWindows: Qt.platform.os === "windows"
    readonly property bool isLinux: Qt.platform.os === "linux"
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

    // Window title
    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        color: Material.foreground
        elide: Text.ElideRight
        horizontalAlignment: Text.AlignHCenter
        text: root.windowTitle
        width: Math.min(implicitWidth, parent.width - 200)
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

        // Windows/Linux: window buttons on the right
        Row {
            id: windowButtons

            readonly property int buttonHeight: root.isLinux ? 24 : 30
            readonly property int buttonWidth: root.isLinux ? 28 : 46
            readonly property int buttonRadius: root.isLinux ? buttonHeight / 2 : 0
            readonly property int glyphSize: root.isLinux ? 14 : 16

            Layout.alignment: Qt.AlignVCenter
            Layout.rightMargin: root.isLinux ? 8 : 0
            spacing: root.isLinux ? 6 : 0
            visible: !root.isMac

            ToolButton {
                id: pinButton

                checkable: true
                checked: (targetWindow.flags & Qt.WindowStaysOnTopHint) !== 0

                ToolTip.delay: AppConstants.tooltipDelay
                ToolTip.text: checked ? qsTr("Unpin Window") : qsTr("Pin Window")
                ToolTip.timeout: AppConstants.tooltipTimeout
                ToolTip.visible: hovered
                font.family: materialSymbolsOutlined.name
                font.pixelSize: windowButtons.glyphSize
                height: windowButtons.buttonHeight
                hoverEnabled: true
                padding: 0
                text: checked ? "\ue6aa" : "\ue6f9"
                width: windowButtons.buttonWidth

                background: Rectangle {
                    color: (pinButton.hovered || pinButton.checked) ? Qt.rgba(Material.foreground.r, Material.foreground.g, Material.foreground.b, 0.12) : "transparent"
                    radius: windowButtons.buttonRadius
                }

                onCheckedChanged: {
                    if (checked) {
                        targetWindow.flags |= Qt.WindowStaysOnTopHint
                    } else {
                        targetWindow.flags &= ~Qt.WindowStaysOnTopHint
                    }
                }
            }


            ToolButton {
                id: minimizeButton

                ToolTip.delay: AppConstants.tooltipDelay
                ToolTip.text: qsTr("Minimize")
                ToolTip.timeout: AppConstants.tooltipTimeout
                ToolTip.visible: hovered
                font.family: materialSymbolsOutlined.name
                font.pixelSize: windowButtons.glyphSize
                height: windowButtons.buttonHeight
                hoverEnabled: true
                padding: 0
                text: "\ue15b"
                width: windowButtons.buttonWidth

                background: Rectangle {
                    color: minimizeButton.hovered ? Qt.rgba(Material.foreground.r, Material.foreground.g, Material.foreground.b, 0.12) : "transparent"
                    radius: windowButtons.buttonRadius
                }

                onClicked: root.targetWindow.showMinimized()
            }
            ToolButton {
                id: maximizeButton

                ToolTip.delay: AppConstants.tooltipDelay
                ToolTip.text: root.isMaximized ? qsTr("Restore") : qsTr("Maximize")
                ToolTip.timeout: AppConstants.tooltipTimeout
                ToolTip.visible: hovered
                font.family: materialSymbolsOutlined.name
                font.pixelSize: windowButtons.glyphSize
                height: windowButtons.buttonHeight
                hoverEnabled: true
                padding: 0
                text: root.isMaximized ? "\ue3e0" : "\ue3c6"
                width: windowButtons.buttonWidth

                background: Rectangle {
                    color: maximizeButton.hovered ? Qt.rgba(Material.foreground.r, Material.foreground.g, Material.foreground.b, 0.12) : "transparent"
                    radius: windowButtons.buttonRadius
                }

                onClicked: root.toggleMaximize()
            }
            ToolButton {
                id: closeButton

                ToolTip.delay: AppConstants.tooltipDelay
                ToolTip.text: qsTr("Close")
                ToolTip.timeout: AppConstants.tooltipTimeout
                ToolTip.visible: hovered
                font.family: materialSymbolsOutlined.name
                font.pixelSize: windowButtons.glyphSize
                height: windowButtons.buttonHeight
                hoverEnabled: true
                padding: 0
                text: "\ue5cd"
                width: windowButtons.buttonWidth

                background: Rectangle {
                    color: {
                        if (!closeButton.hovered)
                            return "transparent";
                        if (root.isWindows)
                            return "#e81123";
                        return Qt.rgba(0.91, 0.13, 0.13, 0.18);
                    }
                    radius: windowButtons.buttonRadius
                }
                contentItem: Text {
                    color: (closeButton.hovered && root.isWindows) ? "white" : Material.foreground
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
