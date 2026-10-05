import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtMultimedia

import gavqml

RowLayout {
    id: root

    required property var player
    required property bool mediaLoaded
    required property int repeatMode
    property alias rangeSlider: internalRangeSlider

    signal goToTimeRequested

    function markerNear(positionMs, toleranceMs) {
        var chapters = player.chapters;
        for (var i = 0; i < chapters.length; i++) {
            if (chapters[i].startMs > 0 && Math.abs(chapters[i].startMs - positionMs) <= toleranceMs)
                return chapters[i];
        }
        return null;
    }
    function chapterTitleAt(positionMs) {
        var chapters = player.chapters;
        var title = "";
        for (var i = 0; i < chapters.length; i++) {
            if (chapters[i].startMs <= positionMs)
                title = chapters[i].title;
        }
        return title;
    }

    spacing: 15

    Text {
        id: timeLabel

        color: Material.foreground
        text: player.isLive ? AppConstants.formatTime(player.position) : (repeatMode === 3
            ? AppConstants.formatTime(Math.round(rangeSlider.first.value)) + " \u2500 " + AppConstants.formatTime(Math.round(rangeSlider.second.value))
            : AppConstants.formatTime(player.position) + " / " + AppConstants.formatTime(player.duration))
        verticalAlignment: Text.AlignVCenter

        MouseArea {
            id: timeLabelArea

            Accessible.name: qsTr("Go to time")
            Accessible.role: Accessible.Button
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            enabled: mediaLoaded && !player.isLive
            hoverEnabled: true

            onClicked: root.goToTimeRequested()
        }
        ToolTip {
            delay: AppConstants.tooltipDelay
            text: qsTr("Go to time (Ctrl+T)")
            timeout: AppConstants.tooltipTimeout
            visible: timeLabelArea.containsMouse
        }
    }
    Rectangle {
        Accessible.name: qsTr("Live stream")
        Layout.preferredHeight: liveText.implicitHeight + 6
        Layout.preferredWidth: liveText.implicitWidth + 14
        color: Material.color(Material.Red)
        radius: 3
        visible: player.isLive

        Text {
            id: liveText

            anchors.centerIn: parent
            color: "white"
            font.bold: true
            font.pixelSize: 11
            text: qsTr("LIVE")
        }
    }
    Slider {
        id: seekSlider

        property string previewImageUrl: ""
        property int previewPosition: 0
        property int requestedPreviewPosition: -1
        property bool previewVisible: false
        property var hoveredMarker: null

        Layout.fillWidth: true
        Layout.preferredHeight: 10
        enabled: mediaLoaded && !player.isLive
        from: 0
        to: player.duration
        visible: repeatMode !== 3 || player.isLive

        onMoved: player.position = value

        Repeater {
            model: player.duration > 0 ? player.chapters : []

            Rectangle {
                required property var modelData

                color: seekSlider.hoveredMarker !== null && seekSlider.hoveredMarker.startMs === modelData.startMs ? Material.accent : Material.foreground
                height: 10
                opacity: 0.7
                visible: modelData.startMs > 0 && modelData.startMs < player.duration
                width: 2
                x: seekSlider.leftPadding + (modelData.startMs / player.duration) * seekSlider.availableWidth - width / 2
                y: seekSlider.topPadding + seekSlider.availableHeight / 2 - height / 2
            }
        }

        // Seek preview popup
        Rectangle {
            id: seekPreview

            property real hoverX: 0

            border.color: Material.dividerColor
            border.width: 1
            color: Qt.rgba(Material.background.r, Material.background.g, Material.background.b, 0.9)
            height: previewChapter.visible ? 132 : 115
            radius: 6
            visible: seekSlider.previewVisible && mediaLoaded && player.duration > 0 && !player.isLive
            width: 170
            x: Math.max(0, Math.min(hoverX - width / 2, seekSlider.width - width))
            y: -height - 10

            Column {
                anchors.centerIn: parent
                spacing: 4

                // Thumbnail container - fixed size, shows either image or loading
                Item {
                    height: 90
                    width: 160

                    // Thumbnail image
                    Image {
                        id: previewImage

                        anchors.fill: parent
                        fillMode: Image.PreserveAspectFit
                        source: seekSlider.previewImageUrl
                        visible: status === Image.Ready

                        Rectangle {
                            anchors.fill: parent
                            border.color: Material.dividerColor
                            border.width: 1
                            color: "transparent"
                            visible: previewImage.status === Image.Ready
                        }
                    }

                    // Loading indicator when no image yet
                    Rectangle {
                        anchors.fill: parent
                        color: Material.dividerColor
                        radius: 4
                        visible: previewImage.status !== Image.Ready
                        clip: true

                        Text {
                            anchors.centerIn: parent
                            anchors.verticalCenterOffset: -8
                            color: Material.foreground
                            opacity: 0.5
                            font.family: materialSymbolsOutlined.name
                            font.pixelSize: 32
                            text: "\ue04b"
                        }

                        // Animated loading bar at bottom
                        Rectangle {
                            id: loadingBar
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: 8
                            anchors.horizontalCenter: parent.horizontalCenter
                            height: 3
                            width: parent.width - 20
                            color: Qt.rgba(Material.foreground.r, Material.foreground.g, Material.foreground.b, 0.2)
                            radius: 1.5

                            Rectangle {
                                id: loadingProgress
                                height: parent.height
                                width: 40
                                radius: 1.5
                                color: Material.accent

                                SequentialAnimation on x {
                                    loops: Animation.Infinite
                                    running: previewImage.status !== Image.Ready && seekSlider.previewVisible

                                    NumberAnimation {
                                        from: 0
                                        to: loadingBar.width - loadingProgress.width
                                        duration: 800
                                        easing.type: Easing.InOutQuad
                                    }
                                    NumberAnimation {
                                        from: loadingBar.width - loadingProgress.width
                                        to: 0
                                        duration: 800
                                        easing.type: Easing.InOutQuad
                                    }
                                }
                            }
                        }
                    }
                }

                // Time label
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: Material.foreground
                    font.bold: true
                    font.pixelSize: 12
                    text: AppConstants.formatTime(seekSlider.previewPosition)
                }
                Text {
                    id: previewChapter

                    anchors.horizontalCenter: parent.horizontalCenter
                    color: Material.foreground
                    elide: Text.ElideRight
                    font.bold: seekSlider.hoveredMarker !== null
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                    opacity: seekSlider.hoveredMarker !== null ? 1.0 : 0.7
                    text: seekSlider.hoveredMarker !== null ? seekSlider.hoveredMarker.title : root.chapterTitleAt(seekSlider.previewPosition)
                    visible: text !== ""
                    width: 160
                }
            }
        }

        // Timer to update the slider position during playback
        Timer {
            interval: AppConstants.seekSliderUpdateInterval
            repeat: true
            running: player.playbackState === MediaPlayer.PlayingState

            onTriggered: {
                if (!seekSlider.pressed) {
                    seekSlider.value = player.position;
                }
            }
        }

        // Also sync on programmatic / paused seeks
        Connections {
            target: player
            function onPositionChanged() {
                if (!seekSlider.pressed) {
                    seekSlider.value = player.position;
                }
            }
        }

        // Preview hover detection and scroll wheel seeking
        MouseArea {
            id: previewMouseArea

            acceptedButtons: Qt.NoButton
            anchors.bottomMargin: -10
            anchors.fill: parent
            anchors.topMargin: -20
            hoverEnabled: true
            propagateComposedEvents: true

            onEntered: {
                seekSlider.previewVisible = true;
            }
            onExited: {
                seekSlider.previewVisible = false;
                seekSlider.previewImageUrl = "";
                seekSlider.hoveredMarker = null;
            }
            onPositionChanged: function (mouse) {
                if (mediaLoaded && player.duration > 0) {
                    var ratio = mouse.x / width;
                    ratio = Math.max(0, Math.min(1, ratio));
                    var pos = Math.floor(ratio * player.duration);
                    seekSlider.hoveredMarker = root.markerNear(pos, 5 * player.duration / Math.max(1, width));
                    seekSlider.previewPosition = seekSlider.hoveredMarker !== null ? seekSlider.hoveredMarker.startMs : pos;
                    seekPreview.hoverX = mouse.x;

                    // Request thumbnail from backend (throttled)
                    previewRequestTimer.restart();
                }
            }
            onWheel: function (wheel) {
                if (player.playbackState === MediaPlayer.PlayingState) {
                    const seekAmount = AppConstants.seekStepSmall;
                    if (wheel.angleDelta.y > 0) {
                        player.position = Math.min(player.position + seekAmount, player.duration);
                    } else if (wheel.angleDelta.y < 0) {
                        player.position = Math.max(player.position - seekAmount, 0);
                    }
                }
            }
        }

        // Throttle preview requests
        Timer {
            id: previewRequestTimer

            interval: AppConstants.previewRequestInterval

            onTriggered: {
                if (seekSlider.previewVisible) {
                    seekSlider.requestedPreviewPosition = seekSlider.previewPosition;
                    player.requestPreviewAt(seekSlider.previewPosition);
                }
            }
        }

        // Handle preview ready signal
        Connections {
            function onPreviewReady(position, imageDataUrl) {
                if (seekSlider.previewVisible
                    && imageDataUrl.length > 0
                    && position === seekSlider.requestedPreviewPosition) {
                    seekSlider.previewImageUrl = imageDataUrl;
                }
            }

            function onSourceChanged() {
                seekSlider.previewVisible = false;
                seekSlider.previewImageUrl = "";
                seekSlider.requestedPreviewPosition = -1;
            }

            target: player
        }
    }
    RangeSlider {
        id: internalRangeSlider

        Layout.fillWidth: true
        Layout.preferredHeight: 10
        enabled: mediaLoaded
        from: 0
        to: player.duration > 0 ? player.duration : 1
        visible: repeatMode === 3 && !player.isLive

        // Only the start handle seeks; the end handle just defines the loop boundary
        first.onMoved: player.position = first.value
    }

    function resetPreview() {
        seekSlider.value = 0;
        seekSlider.previewVisible = false;
        seekSlider.previewImageUrl = "";
    }
}
