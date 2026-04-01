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

    spacing: 15

    Text {
        id: timeLabel

        color: Material.foreground
        text: repeatMode === 3
            ? AppConstants.formatTime(Math.round(rangeSlider.first.value)) + " \u2500 " + AppConstants.formatTime(Math.round(rangeSlider.second.value))
            : AppConstants.formatTime(player.position) + " / " + AppConstants.formatTime(player.duration)
        verticalAlignment: Text.AlignVCenter
    }
    Slider {
        id: seekSlider

        property string previewImageUrl: ""
        property int previewPosition: 0
        property bool previewVisible: false

        Layout.fillWidth: true
        Layout.preferredHeight: 10
        enabled: mediaLoaded
        from: 0
        to: player.duration
        visible: repeatMode !== 3

        onMoved: player.position = value

        // Seek preview popup
        Rectangle {
            id: seekPreview

            property real hoverX: 0

            border.color: Material.dividerColor
            border.width: 1
            color: Qt.rgba(Material.background.r, Material.background.g, Material.background.b, 0.9)
            height: 115
            radius: 6
            visible: seekSlider.previewVisible && mediaLoaded && player.duration > 0
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
            }
        }

        // Timer to update the slider position
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
            }
            onPositionChanged: function (mouse) {
                if (mediaLoaded && player.duration > 0) {
                    var ratio = mouse.x / width;
                    ratio = Math.max(0, Math.min(1, ratio));
                    var pos = Math.floor(ratio * player.duration);
                    seekSlider.previewPosition = pos;
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
                    player.requestPreviewAt(seekSlider.previewPosition);
                }
            }
        }

        // Handle preview ready signal
        Connections {
            function onPreviewReady(position, imageDataUrl) {
                if (seekSlider.previewVisible && imageDataUrl.length > 0) {
                    seekSlider.previewImageUrl = imageDataUrl;
                }
            }

            function onSourceChanged() {
                seekSlider.previewVisible = false;
                seekSlider.previewImageUrl = "";
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
        visible: repeatMode === 3

        // Only the start handle seeks; the end handle just defines the loop boundary
        first.onMoved: player.position = first.value
    }

    function resetPreview() {
        seekSlider.value = 0;
        seekSlider.previewVisible = false;
        seekSlider.previewImageUrl = "";
    }
}
