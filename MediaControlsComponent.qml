import QtQuick
import QtQuick.Controls
import QtMultimedia
import QtQuick.Layouts
import QtQuick.Controls.Material

import gavqml

Item {
    required property var audioOutput
    property bool containsMouse: controlMouseArea.containsMouse
    property int currentSpeedIndex: 3  // Index of 1.0x in playbackSpeeds array

    property real fastForwardRate: 1.0
    property bool isFastForwarding: false
    property bool isFastRewinding: false
    required property bool mediaLoaded
    required property var player
    required property int playlistCount
    required property int playlistCurrentIndex
    property real previousVolume: AppConstants.defaultVolume
    property int rewindMultiplier: 1
    property int repeatMode: 0  // 0=none, 1=once, 2=loop, 3=range
    required property var videoOutput

    signal nextTrack
    signal previousTrack

    function formatTime(ms) {
        var totalSecs = Math.floor(ms / 1000);
        var h = Math.floor(totalSecs / 3600);
        var m = Math.floor((totalSecs % 3600) / 60);
        var s = totalSecs % 60;
        var pad = function (num) {
            return String(num).padStart(2, '0');
        };
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
    Connections {
        target: player

        function onPositionChanged() {
            if (repeatMode === 3 && rangeSlider.second.value > rangeSlider.first.value && player.position >= rangeSlider.second.value) {
                player.position = rangeSlider.first.value;
            }
        }
        function onMediaStatusChanged(status) {
            if (status === MediaPlayer.EndOfMedia) {
                if (repeatMode === 1) {
                    player.position = 0;
                    player.play();
                    repeatMode = 0;
                } else if (repeatMode === 2) {
                    player.position = 0;
                    player.play();
                } else if (repeatMode === 3) {
                    player.position = rangeSlider.first.value;
                    player.play();
                }
            }
        }
        function onDurationChanged() {
            if (repeatMode === 3 && player.duration > 0) {
                rangeSlider.setValues(0, player.duration);
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
        color: Material.background.darker(1.2)
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

                    color: Material.foreground
                    text: repeatMode === 3
                        ? formatTime(Math.round(rangeSlider.first.value)) + " \u2500 " + formatTime(Math.round(rangeSlider.second.value))
                        : formatTime(player.position) + " / " + formatTime(player.duration)
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
                    }

                    // Throttle preview requests
                    Timer {
                        id: previewRequestTimer

                        interval: 50

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
                            // Clear preview when source changes
                            seekSlider.previewVisible = false;
                            seekSlider.previewImageUrl = "";
                        }

                        target: player
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
                RangeSlider {
                    id: rangeSlider

                    Layout.fillWidth: true
                    Layout.preferredHeight: 10
                    enabled: mediaLoaded
                    from: 0
                    to: player.duration > 0 ? player.duration : 1
                    visible: repeatMode === 3

                    first.onMoved: player.position = first.value
                }
            }
            RowLayout {
                // Play/pause buttons
                RowLayout {
                    spacing: 15

                    Button {
                        id: playPauseButton

                        Accessible.description: qsTr("Play or pause the media")
                        Accessible.name: player.playbackState === MediaPlayer.PlayingState ? qsTr("Pause") : qsTr("Play")
                        Accessible.role: Accessible.Button
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

                        Accessible.description: qsTr("Rewind the video. Double-click for 10x speed.")
                        Accessible.name: qsTr("Fast rewind")
                        Accessible.role: Accessible.Button
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

                        contentItem: Item {
                            Text {
                                anchors.centerIn: parent
                                color: Material.foreground
                                opacity: fastRewindButton.enabled ? 1.0 : 0.5
                                font: fastRewindButton.font
                                text: "\ue020"
                            }
                            Rectangle {
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 2
                                anchors.right: parent.right
                                anchors.rightMargin: -2
                                color: Material.color(Material.Red)
                                height: 10
                                radius: 2
                                visible: isFastRewinding
                                width: 14

                                Text {
                                    anchors.centerIn: parent
                                    color: Material.foreground
                                    font.bold: true
                                    font.pixelSize: 8
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
                            seekSlider.previewVisible = false;
                            seekSlider.previewImageUrl = "";
                            stopFastForwarding();
                            stopFastRewinding();
                            repeatMode = 0;
                        }
                    }
                    Button {
                        id: fastForwardButton

                        Accessible.description: qsTr("Fast forward the video. Double-click for 10x speed.")
                        Accessible.name: qsTr("Fast forward")
                        Accessible.role: Accessible.Button
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

                        contentItem: Item {
                            Text {
                                anchors.centerIn: parent
                                color: Material.foreground
                                opacity: fastForwardButton.enabled ? 1.0 : 0.5
                                font: fastForwardButton.font
                                text: "\ue01f"
                            }
                            Rectangle {
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 2
                                anchors.right: parent.right
                                anchors.rightMargin: -2
                                color: Material.color(Material.Green)
                                height: 10
                                radius: 2
                                visible: isFastForwarding
                                width: 14

                                Text {
                                    anchors.centerIn: parent
                                    color: Material.foreground
                                    font.bold: true
                                    font.pixelSize: 8
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

                        Accessible.description: qsTr("Show or hide the playlist")
                        Accessible.name: qsTr("Toggle playlist")
                        Accessible.role: Accessible.Button
                        Layout.preferredHeight: 30
                        Layout.preferredWidth: 25
                        Material.roundedScale: Material.NotRounded
                        enabled: true
                        font.family: materialSymbolsOutlined.name
                        font.weight: Font.Light
                        hoverEnabled: true
                        scale: 1.5
                        text: "\ue3c7"

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
                    Button {
                        id: repeatButton

                        Accessible.description: qsTr("Toggle repeat mode")
                        Accessible.name: qsTr("Repeat")
                        Accessible.role: Accessible.Button
                        Layout.preferredHeight: 30
                        Layout.preferredWidth: 25
                        Material.roundedScale: Material.NotRounded
                        enabled: mediaLoaded
                        font.family: materialSymbolsOutlined.name
                        font.weight: Font.Light
                        hoverEnabled: true
                        scale: 1.5
                        text: repeatMode === 1 ? "\ue9d7" : (repeatMode >= 2 ? "\ue9d6" : "\ue040")

                        contentItem: Item {
                            Text {
                                anchors.centerIn: parent
                                color: repeatMode > 0 ? Material.accent : Material.foreground
                                opacity: repeatMode === 0 ? 0.5 : 1.0
                                font: repeatButton.font
                                text: repeatMode === 1 ? "\ue9d7" : (repeatMode >= 2 ? "\ue9d6" : "\ue040")
                            }
                            Rectangle {
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 2
                                anchors.right: parent.right
                                anchors.rightMargin: -2
                                color: Material.accent
                                height: 10
                                radius: 2
                                visible: repeatMode === 3
                                width: 20

                                Text {
                                    anchors.centerIn: parent
                                    color: Material.foreground
                                    font.bold: true
                                    font.pixelSize: 8
                                    text: "RNG"
                                }
                            }
                        }

                        onClicked: {
                            repeatMode = (repeatMode + 1) % 4;
                            if (repeatMode === 3 && player.duration > 0) {
                                rangeSlider.setValues(0, player.duration);
                            }
                        }

                        ToolTip {
                            delay: AppConstants.tooltipDelay
                            text: {
                                if (repeatMode === 0) return qsTr("Repeat: Off");
                                if (repeatMode === 1) return qsTr("Repeat: Once");
                                if (repeatMode === 2) return qsTr("Repeat: Loop");
                                return qsTr("Repeat: Range");
                            }
                            timeout: AppConstants.tooltipTimeout
                            visible: repeatButton.hovered
                        }
                    }
                    Rectangle {
                        Layout.preferredHeight: parent.height
                        Layout.preferredWidth: 2
                        color: Material.dividerColor
                        visible: true
                    }

                    // Playback speed selector
                    Button {
                        id: speedButton

                        Accessible.description: qsTr("Change playback speed")
                        Accessible.name: qsTr("Playback speed")
                        Accessible.role: Accessible.Button
                        Layout.preferredHeight: 30
                        Layout.preferredWidth: 40
                        Material.roundedScale: Material.NotRounded
                        enabled: mediaLoaded
                        hoverEnabled: true

                        contentItem: Text {
                            color: Material.foreground
                            opacity: speedButton.enabled ? 1.0 : 0.5
                            font.bold: player.playbackRate !== 1.0
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignHCenter
                            text: player.playbackRate.toFixed(2).replace(/\.?0+$/, '') + "x"
                            verticalAlignment: Text.AlignVCenter
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
                                    checkable: true
                                    checked: Math.abs(player.playbackRate - modelData) < 0.01
                                    text: modelData + "x"

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
                        color: Material.dividerColor
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
                    Button {
                        id: brightnessContrastButton

                        Accessible.description: qsTr("Adjust brightness and contrast")
                        Accessible.name: qsTr("Brightness/Contrast")
                        Accessible.role: Accessible.Button
                        Layout.preferredHeight: 30
                        Layout.preferredWidth: 25
                        Material.roundedScale: Material.NotRounded
                        enabled: player.hasVideo
                        font.family: materialSymbolsOutlined.name
                        font.weight: Font.Light
                        hoverEnabled: true
                        scale: 1.5
                        text: "\ue1ac"

                        onClicked: brightnessContrastPopup.open()

                        ToolTip {
                            delay: AppConstants.tooltipDelay
                            text: qsTr("Brightness / Contrast")
                            timeout: AppConstants.tooltipTimeout
                            visible: brightnessContrastButton.hovered
                        }

                        Popup {
                            id: brightnessContrastPopup

                            padding: 12
                            x: -width / 2 + parent.width / 2
                            y: -height - 10

                            background: Rectangle {
                                border.color: Material.dividerColor
                                border.width: 1
                                color: Material.background
                                radius: 8
                            }

                            ColumnLayout {
                                spacing: 8

                                // Brightness row
                                RowLayout {
                                    spacing: 8

                                    Text {
                                        color: Material.foreground
                                        font.family: materialSymbolsOutlined.name
                                        font.pixelSize: 18
                                        text: "\ue518"

                                        MouseArea {
                                            id: brightnessLabel
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            acceptedButtons: Qt.NoButton
                                        }
                                        ToolTip {
                                            delay: AppConstants.tooltipDelay
                                            text: qsTr("Brightness")
                                            timeout: AppConstants.tooltipTimeout
                                            visible: brightnessLabel.containsMouse
                                        }
                                    }
                                    Slider {
                                        id: brightnessSlider

                                        Layout.preferredWidth: 150
                                        from: AppConstants.brightnessMin
                                        to: AppConstants.brightnessMax
                                        stepSize: AppConstants.brightnessContrastStep
                                        value: videoOutput.brightnessLevel

                                        onMoved: videoOutput.brightnessLevel = value
                                    }
                                    Text {
                                        Layout.preferredWidth: 35
                                        color: Material.foreground
                                        font.pixelSize: 11
                                        horizontalAlignment: Text.AlignRight
                                        text: brightnessSlider.value.toFixed(2)
                                    }
                                }

                                // Contrast row
                                RowLayout {
                                    spacing: 8

                                    Text {
                                        color: Material.foreground
                                        font.family: materialSymbolsOutlined.name
                                        font.pixelSize: 18
                                        text: "\ue3a5"

                                        MouseArea {
                                            id: contrastLabel
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            acceptedButtons: Qt.NoButton
                                        }
                                        ToolTip {
                                            delay: AppConstants.tooltipDelay
                                            text: qsTr("Contrast")
                                            timeout: AppConstants.tooltipTimeout
                                            visible: contrastLabel.containsMouse
                                        }
                                    }
                                    Slider {
                                        id: contrastSlider

                                        Layout.preferredWidth: 150
                                        from: AppConstants.contrastMin
                                        to: AppConstants.contrastMax
                                        stepSize: AppConstants.brightnessContrastStep
                                        value: videoOutput.contrastLevel

                                        onMoved: videoOutput.contrastLevel = value
                                    }
                                    Text {
                                        Layout.preferredWidth: 35
                                        color: Material.foreground
                                        font.pixelSize: 11
                                        horizontalAlignment: Text.AlignRight
                                        text: contrastSlider.value.toFixed(2)
                                    }
                                }

                                // Reset button
                                Button {
                                    Layout.alignment: Qt.AlignHCenter
                                    Layout.preferredHeight: 25
                                    Material.roundedScale: Material.SmallScale
                                    font.pixelSize: 11
                                    text: qsTr("Reset")

                                    onClicked: {
                                        videoOutput.brightnessLevel = AppConstants.defaultBrightness;
                                        videoOutput.contrastLevel = AppConstants.defaultContrast;
                                        brightnessSlider.value = AppConstants.defaultBrightness;
                                        contrastSlider.value = AppConstants.defaultContrast;
                                    }
                                }
                            }
                        }
                    }
                    Rectangle {
                        Layout.preferredHeight: parent.height
                        Layout.preferredWidth: 2
                        color: Material.dividerColor
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

                        Accessible.description: qsTr("Enter or exit fullscreen mode")
                        Accessible.name: qsTr("Toggle fullscreen")
                        Accessible.role: Accessible.Button
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

                        onClicked: {
                            mainWindow.visibility = mainWindow.visibility === Window.FullScreen ? Window.Windowed : Window.FullScreen;
                        }
                    }
                }
            }
        }
    }
}
