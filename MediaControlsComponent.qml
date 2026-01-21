import QtQuick
import QtQuick.Controls
import QtMultimedia
import QtQuick.Layouts
import QtQuick.Controls.Material

import gavqml

Item {
    required property var audioOutput
    property bool containsMouse: controlMouseArea.containsMouse
    property real fastForwardRate: 1.0
    property bool isFastForwarding: false
    property bool isFastRewinding: false
    required property bool mediaLoaded
    required property var player
    required property int playlistCount
    required property int playlistCurrentIndex
    property real previousVolume: AppConstants.defaultVolume
    property int rewindMultiplier: 1
    required property var videoOutput
    property int currentSpeedIndex: 3  // Index of 1.0x in playbackSpeeds array

    signal nextTrack
    signal previousTrack

    function formatTime(ms) {
        var totalSecs = Math.floor(ms / 1000);
        var h = Math.floor(totalSecs / 3600);
        var m = Math.floor((totalSecs % 3600) / 60);
        var s = totalSecs % 60;
        var pad = function(num) { return String(num).padStart(2, '0'); };
        return h > 0 ? h + ":" + pad(m) + ":" + pad(s) : pad(m) + ":" + pad(s);
    }
    function stopFastForwarding() {
        if (isFastForwarding) {
            player.playbackRate = 1.0;
            isFastForwarding = false;
            fastForwardRate = 1.0;
            return true;
        }
        return false;
    }
    function stopFastRewinding() {
        if (isFastRewinding) {
            rewindSeekTimer.stop();
            isFastRewinding = false;
            rewindMultiplier = 1;
            return true;
        }
        return false;
    }
    function updateVolumeIcon() {
        if (audioOutput.muted || audioOutput.volume === 0) {
            volumeButton.text = "\ue04e";
        } else if (audioOutput.volume < 0.5) {
            volumeButton.text = "\ue04d";
        } else if (audioOutput.volume < 1.0) {
            volumeButton.text = "\ue050";
        } else {
            // volume is 1.0
            volumeButton.text = "\ue98e";
        }
    }

    height: 60
    width: parent.width

    Timer {
        id: forwardHoldTimer

        interval: 200
        repeat: true

        onTriggered: player.position = Math.min(player.position + 1000, player.duration)
    }
    Timer {
        id: rewindHoldTimer

        interval: 200
        repeat: true

        onTriggered: player.position = Math.max(player.position - 1000, 0)
    }
    Timer {
        id: rewindSeekTimer

        interval: 100
        repeat: true

        onTriggered: {
            var nextPos = player.position - (100 * rewindMultiplier);
            if (nextPos < 0) {
                player.position = 0;
                isFastRewinding = false;
                rewindSeekTimer.stop();
            } else {
                player.position = nextPos;
            }
        }
    }
    MouseArea {
        id: controlMouseArea

        anchors.fill: parent
        hoverEnabled: true
    }
    Rectangle {
        id: controlBar

        anchors.horizontalCenter: parent.horizontalCenter
        color: "#80000000"
        height: parent.height
        width: parent.width

        ColumnLayout {
            anchors.left: parent.left
            anchors.margins: {
                left: 10;
                right: 10;
            }
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: 15

            // Seek row
            RowLayout {
                spacing: 15

                Text {
                    id: timeLabel

                    color: "white"
                    text: formatTime(player.position) + " / " + formatTime(player.duration)
                    verticalAlignment: Text.AlignVCenter
                }
                Slider {
                    id: seekSlider

                    property string previewImageUrl: ""
                    property qint64 previewPosition: 0
                    property bool previewVisible: false

                    Layout.fillWidth: true
                    Layout.preferredHeight: 10
                    enabled: mediaLoaded
                    from: 0
                    to: player.duration

                    onMoved: player.position = value

                    // Seek preview popup
                    Rectangle {
                        id: seekPreview

                        property real hoverX: 0

                        visible: seekSlider.previewVisible && mediaLoaded && player.duration > 0
                        x: Math.max(0, Math.min(hoverX - width / 2, seekSlider.width - width))
                        y: -height - 10
                        width: 170
                        height: previewImage.status === Image.Ready ? 115 : 40
                        color: "#e0222222"
                        radius: 6
                        border.color: "#444"
                        border.width: 1

                        Behavior on height {
                            NumberAnimation { duration: 100 }
                        }

                        Column {
                            anchors.centerIn: parent
                            spacing: 4

                            // Thumbnail image
                            Image {
                                id: previewImage
                                width: 160
                                height: 90
                                visible: status === Image.Ready
                                source: seekSlider.previewImageUrl
                                fillMode: Image.PreserveAspectFit

                                Rectangle {
                                    anchors.fill: parent
                                    color: "transparent"
                                    border.color: "#333"
                                    border.width: 1
                                    visible: previewImage.status === Image.Ready
                                }
                            }

                            // Loading indicator when no image yet
                            Rectangle {
                                width: 160
                                height: 90
                                color: "#333"
                                visible: previewImage.status !== Image.Ready && seekSlider.previewVisible
                                radius: 4

                                Text {
                                    anchors.centerIn: parent
                                    color: "#666"
                                    font.family: materialSymbolsOutlined.name
                                    font.pixelSize: 32
                                    text: "\ue04b"
                                }
                            }

                            // Time label
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                color: "white"
                                font.pixelSize: 12
                                font.bold: true
                                text: formatTime(seekSlider.previewPosition)
                            }
                        }
                    }

                    // Timer to update the slider position
                    Timer {
                        interval: 500
                        repeat: true
                        running: player.playbackState === MediaPlayer.PlayingState

                        onTriggered: {
                            if (!seekSlider.pressed) {
                                // Do not update while user is seeking
                                seekSlider.value = player.position;
                            }
                        }
                    }

                    // Preview hover detection
                    MouseArea {
                        id: previewMouseArea
                        anchors.fill: parent
                        anchors.topMargin: -20
                        anchors.bottomMargin: -10
                        acceptedButtons: Qt.NoButton
                        hoverEnabled: true
                        propagateComposedEvents: true

                        onPositionChanged: function(mouse) {
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
                        onEntered: {
                            seekSlider.previewVisible = true;
                        }
                        onExited: {
                            seekSlider.previewVisible = false;
                            seekSlider.previewImageUrl = "";
                        }
                    }

                    // Throttle preview requests
                    Timer {
                        id: previewRequestTimer
                        interval: 200
                        onTriggered: {
                            if (seekSlider.previewVisible) {
                                player.requestPreviewAt(seekSlider.previewPosition);
                            }
                        }
                    }

                    // Handle preview ready signal
                    Connections {
                        target: player
                        function onPreviewReady(position, imageDataUrl) {
                            if (seekSlider.previewVisible && imageDataUrl.length > 0) {
                                seekSlider.previewImageUrl = imageDataUrl;
                            }
                        }
                    }

                    // Scroll wheel seeking
                    MouseArea {
                        acceptedButtons: Qt.NoButton
                        anchors.fill: parent
                        propagateComposedEvents: true

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
                }
            }
            RowLayout {
                // Play/pause buttons
                RowLayout {
                    spacing: 15

                    Button {
                        id: playPauseButton

                        Layout.preferredHeight: 30
                        Layout.preferredWidth: 25
                        Material.roundedScale: Material.NotRounded
                        ToolTip.delay: AppConstants.tooltipDelay
                        ToolTip.text: player.playbackState === MediaPlayer.PlayingState ? qsTr("Pause") : qsTr("Play")
                        ToolTip.timeout: AppConstants.tooltipTimeout
                        ToolTip.visible: hovered
                        enabled: mediaLoaded
                        font.family: materialSymbolsOutlined.name
                        hoverEnabled: true
                        scale: 1.5
                        text: player.playbackState === MediaPlayer.PlayingState ? "\ue034" : "\ue037"

                        Accessible.name: player.playbackState === MediaPlayer.PlayingState ? qsTr("Pause") : qsTr("Play")
                        Accessible.description: qsTr("Play or pause the media")
                        Accessible.role: Accessible.Button

                        onClicked: {
                            if (player.playbackState === MediaPlayer.PlayingState) {
                                player.pause();
                                stopFastForwarding();
                                stopFastRewinding();
                            } else {
                                player.play();
                            }
                        }
                    }
                    Button {
                        id: fastRewindButton

                        Layout.preferredHeight: 30
                        Layout.preferredWidth: 25
                        Material.roundedScale: Material.NotRounded
                        ToolTip.delay: AppConstants.tooltipDelay
                        ToolTip.text: isFastRewinding ? qsTr("Fast rewind (10x)") : qsTr("Fast rewind")
                        ToolTip.timeout: AppConstants.tooltipTimeout
                        ToolTip.visible: hovered
                        enabled: player.playbackState !== MediaPlayer.StoppedState
                        font.family: materialSymbolsOutlined.name
                        font.weight: Font.Light
                        hoverEnabled: true
                        scale: 1.5
                        text: "\ue020"

                        Accessible.name: qsTr("Fast rewind")
                        Accessible.description: qsTr("Rewind the video. Double-click for 10x speed.")
                        Accessible.role: Accessible.Button

                        contentItem: Item {
                            Text {
                                anchors.centerIn: parent
                                color: fastRewindButton.enabled ? "white" : "#a0a0a0"
                                font: fastRewindButton.font
                                text: "\ue020"
                            }
                            Rectangle {
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.rightMargin: -2
                                anchors.bottomMargin: 2
                                width: 14
                                height: 10
                                radius: 2
                                color: "#e53935"
                                visible: isFastRewinding

                                Text {
                                    anchors.centerIn: parent
                                    color: "white"
                                    font.pixelSize: 8
                                    font.bold: true
                                    text: "10x"
                                }
                            }
                        }

                        onClicked: {
                            stopFastForwarding();
                            if (!stopFastRewinding()) {
                                rewSingleClickTimer.start();
                            }
                        }
                        onDoubleClicked: {
                            stopFastForwarding();
                            rewSingleClickTimer.stop();
                            isFastRewinding = !isFastRewinding;
                            if (isFastRewinding) {
                                rewindMultiplier = 10;
                                rewindSeekTimer.start();
                            } else {
                                rewindMultiplier = 1;
                                rewindSeekTimer.stop();
                            }
                        }
                        onPressAndHold: rewindHoldTimer.start()
                        onReleased: rewindHoldTimer.stop()

                        Timer {
                            id: rewSingleClickTimer

                            interval: 250

                            onTriggered: player.position = Math.max(player.position - AppConstants.seekStepSmall, 0)
                        }
                    }
                    Button {
                        id: stopButton

                        Layout.preferredHeight: 30
                        Layout.preferredWidth: 25
                        Material.roundedScale: Material.NotRounded
                        ToolTip.delay: 1000
                        ToolTip.text: qsTr("Stop")
                        ToolTip.timeout: 5000
                        ToolTip.visible: hovered
                        enabled: player.playbackState !== MediaPlayer.StoppedState
                        font.family: materialSymbolsOutlined.name
                        font.weight: Font.Light
                        hoverEnabled: true
                        scale: 1.5
                        text: "\ue047"

                        onClicked: {
                            player.stop();
                            seekSlider.value = 0;
                            stopFastForwarding();
                            stopFastRewinding();
                        }
                    }
                    Button {
                        id: fastForwardButton

                        Layout.preferredHeight: 30
                        Layout.preferredWidth: 25
                        Material.roundedScale: Material.NotRounded
                        ToolTip.delay: AppConstants.tooltipDelay
                        ToolTip.text: isFastForwarding ? qsTr("Fast forward (10x)") : qsTr("Fast forward")
                        ToolTip.timeout: AppConstants.tooltipTimeout
                        ToolTip.visible: hovered
                        enabled: player.playbackState !== MediaPlayer.StoppedState
                        font.family: materialSymbolsOutlined.name
                        font.weight: Font.Light
                        hoverEnabled: true
                        scale: 1.5
                        text: "\ue01f"

                        Accessible.name: qsTr("Fast forward")
                        Accessible.description: qsTr("Fast forward the video. Double-click for 10x speed.")
                        Accessible.role: Accessible.Button

                        contentItem: Item {
                            Text {
                                anchors.centerIn: parent
                                color: fastForwardButton.enabled ? "white" : "#a0a0a0"
                                font: fastForwardButton.font
                                text: "\ue01f"
                            }
                            Rectangle {
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.rightMargin: -2
                                anchors.bottomMargin: 2
                                width: 14
                                height: 10
                                radius: 2
                                color: "#4caf50"
                                visible: isFastForwarding

                                Text {
                                    anchors.centerIn: parent
                                    color: "white"
                                    font.pixelSize: 8
                                    font.bold: true
                                    text: "10x"
                                }
                            }
                        }

                        onClicked: {
                            stopFastRewinding();
                            if (!stopFastForwarding()) {
                                ffwSingleClickTimer.start();
                            }
                        }
                        onDoubleClicked: {
                            stopFastRewinding();
                            ffwSingleClickTimer.stop();
                            isFastForwarding = !isFastForwarding;
                            if (isFastForwarding) {
                                fastForwardRate = 10.0;
                                player.playbackRate = fastForwardRate;
                            } else {
                                fastForwardRate = 1.0;
                                player.playbackRate = 1.0;
                            }
                        }
                        onPressAndHold: forwardHoldTimer.start()
                        onReleased: forwardHoldTimer.stop()

                        Timer {
                            id: ffwSingleClickTimer

                            interval: 250

                            onTriggered: player.position = Math.min(player.position + AppConstants.seekStepSmall, player.duration)
                        }
                    }
                    Button {
                        id: playListButton

                        Layout.preferredHeight: 30
                        Layout.preferredWidth: 25
                        Material.roundedScale: Material.NotRounded
                        enabled: true
                        font.family: materialSymbolsOutlined.name
                        font.weight: Font.Light
                        hoverEnabled: true
                        scale: 1.5
                        text: "\ue3c7"

                        Accessible.name: qsTr("Toggle playlist")
                        Accessible.description: qsTr("Show or hide the playlist")
                        Accessible.role: Accessible.Button

                        onClicked: {
                            playlistComponent.visible = !playlistComponent.visible;
                        }

                        ToolTip {
                            delay: AppConstants.tooltipDelay
                            text: qsTr("Toggle playlist")
                            timeout: AppConstants.tooltipTimeout
                            visible: playListButton.hovered
                        }
                    }
                    Rectangle {
                        Layout.preferredHeight: parent.height
                        Layout.preferredWidth: 2
                        color: "#a0a0a0"
                        visible: true
                    }

                    // Playback speed selector
                    Button {
                        id: speedButton

                        Layout.preferredHeight: 30
                        Layout.preferredWidth: 40
                        Material.roundedScale: Material.NotRounded
                        enabled: mediaLoaded
                        hoverEnabled: true

                        Accessible.name: qsTr("Playback speed")
                        Accessible.description: qsTr("Change playback speed")
                        Accessible.role: Accessible.Button

                        contentItem: Text {
                            color: speedButton.enabled ? "white" : "#a0a0a0"
                            font.pixelSize: 11
                            font.bold: player.playbackRate !== 1.0
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            text: player.playbackRate.toFixed(2).replace(/\.?0+$/, '') + "x"
                        }

                        onClicked: speedMenu.open()

                        ToolTip {
                            delay: AppConstants.tooltipDelay
                            text: qsTr("Playback speed: ") + player.playbackRate + "x"
                            timeout: AppConstants.tooltipTimeout
                            visible: speedButton.hovered
                        }

                        Menu {
                            id: speedMenu
                            y: -height

                            Repeater {
                                model: AppConstants.playbackSpeeds

                                MenuItem {
                                    text: modelData + "x"
                                    checkable: true
                                    checked: Math.abs(player.playbackRate - modelData) < 0.01

                                    onTriggered: {
                                        player.playbackRate = modelData;
                                        stopFastForwarding();
                                        stopFastRewinding();
                                        currentSpeedIndex = index;
                                    }
                                }
                            }
                        }
                    }
                    Rectangle {
                        Layout.preferredHeight: parent.height
                        Layout.preferredWidth: 2
                        color: "#a0a0a0"
                        visible: true
                    }
                    Button {
                        id: captureButton

                        Layout.preferredHeight: 30
                        Layout.preferredWidth: 25
                        Material.roundedScale: Material.NotRounded
                        enabled: player.hasVideo
                        font.family: materialSymbolsOutlined.name
                        font.weight: Font.Light
                        hoverEnabled: true
                        scale: 1.5
                        text: "\ue412"

                        onClicked: {
                            player.captureFrame();
                        }

                        ToolTip {
                            delay: 1000
                            text: qsTr("Capture a frame")
                            timeout: 5000
                            visible: captureButton.hovered
                        }
                    }
                    Button {
                        id: collageButton2

                        property bool isLoading: false

                        Layout.preferredHeight: 30
                        Layout.preferredWidth: 25
                        Material.roundedScale: Material.NotRounded
                        enabled: player.hasVideo && !collageButton2.isLoading
                        font.family: materialSymbolsOutlined.name
                        font.weight: Font.Light
                        hoverEnabled: true
                        scale: 1.5
                        text: collageButton2.isLoading ? "" : "\uefb2"

                        onClicked: {
                            collageButton2.isLoading = true;
                            collage.toCollage([player.source]);
                        }

                        // Loading spinner
                        BusyIndicator {
                            anchors.centerIn: parent
                            height: parent.height * 0.8
                            running: collageButton2.isLoading
                            visible: collageButton2.isLoading
                            width: parent.width * 0.8
                        }
                        ToolTip {
                            delay: 1000
                            text: collageButton2.isLoading ? qsTr("Creating collage...") : qsTr("Create collage")
                            timeout: 5000
                            visible: collageButton2.hovered
                        }
                        Connections {
                            function onCollageCompleted(index, outputPath, success) {
                                if (success) {
                                    console.log("Collage", index, "saved to:", outputPath);
                                } else {
                                    console.log("Collage", index, "failed");
                                }
                            }
                            function onCollageFinished(successCount, failCount) {
                                collageButton2.isLoading = false;
                                console.log("Collage creation finished:", successCount, "success,", failCount, "failed");
                            }
                            function onCollageProgress(index, inputPath) {
                                console.log("Processing collage", index, ":", inputPath);
                            }
                            function onCollageStarted(total) {
                                console.log("Starting collage creation for", total, "file(s)");
                            }

                            target: collage
                        }
                    }
                    Rectangle {
                        Layout.preferredHeight: parent.height
                        Layout.preferredWidth: 2
                        color: "#a0a0a0"
                        visible: true
                    }
                    Button {
                        id: backTrackButton

                        Layout.preferredHeight: 30
                        Layout.preferredWidth: 25
                        Material.roundedScale: Material.NotRounded
                        enabled: playlistCurrentIndex > 0
                        font.family: materialSymbolsOutlined.name
                        font.weight: Font.Light
                        hoverEnabled: true
                        scale: 1.5
                        text: "\ue045"

                        onClicked: previousTrack()

                        ToolTip {
                            delay: 1000
                            text: qsTr("Skip back")
                            timeout: 5000
                            visible: backTrackButton.hovered
                        }
                    }
                    Button {
                        id: nextTrackButton

                        Layout.preferredHeight: 30
                        Layout.preferredWidth: 25
                        Material.roundedScale: Material.NotRounded
                        enabled: playlistCurrentIndex < playlistCount - 1
                        font.family: materialSymbolsOutlined.name
                        font.weight: Font.Light
                        hoverEnabled: true
                        scale: 1.5
                        text: "\ue044"

                        onClicked: nextTrack()

                        ToolTip {
                            delay: 1000
                            text: qsTr("Skip next")
                            timeout: 5000
                            visible: nextTrackButton.hovered
                        }
                    }
                }
                Item {
                    Layout.fillWidth: true
                }

                // Volume seek and mute
                RowLayout {
                    Layout.alignment: Qt.AlignRight
                    spacing: 5

                    ToolButton {
                        id: volumeButton

                        Layout.preferredHeight: 25
                        Layout.preferredWidth: 15
                        ToolTip.delay: 1000
                        ToolTip.text: qsTr("Volume")
                        ToolTip.timeout: 5000
                        ToolTip.visible: hovered
                        font.family: materialSymbolsOutlined.name
                        font.weight: Font.Light
                        hoverEnabled: true
                        scale: 1.5
                        text: "\ue04d"

                        onClicked: {
                            audioOutput.muted = !audioOutput.muted;
                            if (audioOutput.muted) {
                                previousVolume = audioOutput.volume;
                                audioOutput.volume = 0;
                            } else {
                                audioOutput.volume = previousVolume;
                            }
                            updateVolumeIcon();
                        }
                    }
                    Slider {
                        id: volumeSlider

                        Layout.preferredHeight: 10
                        Layout.preferredWidth: 100
                        from: 0
                        to: 1.0
                        value: audioOutput.volume

                        onValueChanged: {
                            audioOutput.volume = value;
                            if (value > 0) {
                                audioOutput.muted = false;
                            }
                            updateVolumeIcon();
                        }

                        MouseArea {
                            acceptedButtons: Qt.NoButton
                            anchors.fill: parent
                            propagateComposedEvents: true

                            onWheel: function (wheel) {
                                if (wheel.angleDelta.y > 0) {
                                    volumeSlider.value = Math.min(volumeSlider.value + AppConstants.volumeStep, 1.0);
                                } else if (wheel.angleDelta.y < 0) {
                                    volumeSlider.value = Math.max(volumeSlider.value - AppConstants.volumeStep, 0.0);
                                }
                            }
                        }
                    }
                    Button {
                        id: fullscreenButton

                        Layout.preferredHeight: 30
                        Layout.preferredWidth: 25
                        Material.background: "transparent"
                        Material.roundedScale: Material.NotRounded
                        ToolTip.delay: AppConstants.tooltipDelay
                        ToolTip.text: qsTr("Toggle fullscreen")
                        ToolTip.timeout: AppConstants.tooltipTimeout
                        ToolTip.visible: hovered
                        font.family: materialSymbolsOutlined.name
                        hoverEnabled: true
                        scale: 1.5
                        text: mainWindow.visibility === Window.FullScreen ? "\ue5d1" : "\ue5d0"

                        Accessible.name: qsTr("Toggle fullscreen")
                        Accessible.description: qsTr("Enter or exit fullscreen mode")
                        Accessible.role: Accessible.Button

                        onClicked: {
                            mainWindow.visibility = mainWindow.visibility === Window.FullScreen ? Window.Windowed : Window.FullScreen;
                        }
                    }
                }
            }
        }
    }
}
