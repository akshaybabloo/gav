import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

import gavqml

Button {
    id: root

    property bool isLoading: false
    required property var collageTarget
    required property var sourceUrls

    Accessible.name: qsTr("Create collages")
    Accessible.description: qsTr("Create collages for videos")
    Accessible.role: Accessible.Button
    Layout.preferredHeight: 30
    Layout.preferredWidth: 25
    Material.roundedScale: Material.NotRounded
    enabled: !isLoading && sourceUrls.length > 0
    font.family: materialSymbolsOutlined.name
    font.weight: Font.Light
    hoverEnabled: true
    scale: 1.5
    text: isLoading ? "" : "\uefb2"

    onClicked: {
        isLoading = true;
        collageTarget.toCollage(sourceUrls);
    }

    BusyIndicator {
        anchors.centerIn: parent
        height: parent.height * 0.8
        running: root.isLoading
        visible: root.isLoading
        width: parent.width * 0.8
    }
    ToolTip {
        delay: AppConstants.tooltipDelay
        text: root.isLoading ? qsTr("Creating collage...") : qsTr("Create collage")
        timeout: AppConstants.tooltipTimeout
        visible: root.hovered
    }
    Connections {
        function onCollageFinished(successCount, failCount) {
            root.isLoading = false;
        }

        target: collageTarget
    }
}
