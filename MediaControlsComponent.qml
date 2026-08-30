import QtQuick
import QtQuick.Controls
import QtMultimedia
import QtQuick.Layouts
import QtQuick.Controls.Material

import gavqml

Item {
    id: root

    required property var audioOutput
    required property var collageTarget
    property bool containsMouse: controlMouseArea.containsMouse
    property int currentSpeedIndex: 3  // Index of 1.0x in playbackSpeeds array

    property real fastForwardRate: 1.0
    property bool isFastForwarding: false
    property bool isFastRewinding: false
    required property bool mediaLoaded
    property bool miniPlayerActive: false
    property bool nerdStatsActive: false
    required property var player
    required property int playlistCount
    required property int playlistCurrentIndex
    property int repeatMode: 0  // 0=none, 1=once, 2=loop, 3=range
    property int rewindMultiplier: 1
    required property var videoOutput

    signal miniPlayerRequested
    signal nerdStatsToggleRequested
    signal nextTrack
    signal playlistToggleRequested
    signal previousTrack

    function stopFastForwarding() {
        if (isFastForwarding) {
            var restoreRate = (currentSpeedIndex >= 0 && currentSpeedIndex < AppConstants.playbackSpeeds.length) ? AppConstants.playbackSpeeds[currentSpeedIndex] : 1.0;
            player.playbackRate = restoreRate;
            isFastForwarding = false;
            fastForwardRate = restoreRate;
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

    height: 90
    width: parent.width

    Timer {
        id: forwardHoldTimer

        interval: AppConstants.holdTimerInterval
        repeat: true

        onTriggered: player.position = Math.min(player.position + AppConstants.seekStepSmall, player.duration)
    }
    Timer {
        id: rewindHoldTimer

        interval: AppConstants.holdTimerInterval
        repeat: true

        onTriggered: player.position = Math.max(player.position - AppConstants.seekStepSmall, 0)
    }
    Timer {
        id: rewindSeekTimer

        interval: AppConstants.rewindSeekInterval
        repeat: true

        onTriggered: {
            var nextPos = player.position - (AppConstants.rewindSeekMultiplier * rewindMultiplier);
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
        function onDurationChanged() {
            if (repeatMode === 3 && player.duration > 0 && seekBar.rangeSlider) {
                seekBar.rangeSlider.setValues(0, player.duration);
            }
        }
        function onMediaStatusChanged(status) {
            if (status === MediaPlayer.EndOfMedia) {
                stopFastForwarding();
                stopFastRewinding();
                if (repeatMode === 1) {
                    player.position = 0;
                    player.play();
                    repeatMode = 0;
                } else if (repeatMode === 2) {
                    player.position = 0;
                    player.play();
                } else if (repeatMode === 3 && seekBar.rangeSlider) {
                    player.position = seekBar.rangeSlider.first.value;
                    player.play();
                } else {
                    seekBar.resetPreview();
                    if (playlistCurrentIndex >= 0 && playlistCurrentIndex < playlistCount - 1) {
                        nextTrack();
                    }
                }
            }
        }
        function onPositionChanged() {
            if (repeatMode === 3 && seekBar.rangeSlider && seekBar.rangeSlider.second.value > seekBar.rangeSlider.first.value && player.position >= seekBar.rangeSlider.second.value) {
                player.position = seekBar.rangeSlider.first.value;
            }
        }

        target: player
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
            anchors.bottomMargin: 8
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            anchors.topMargin: 8
            spacing: 15

            // Seek row
            SeekBarComponent {
                id: seekBar

                Layout.fillWidth: true
                mediaLoaded: root.mediaLoaded
                player: root.player
                repeatMode: root.repeatMode
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
                                font: fastRewindButton.font
                                opacity: fastRewindButton.enabled ? 1.0 : 0.5
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
                                rewindMultiplier = AppConstants.fastSpeedMultiplier;
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

                            interval: AppConstants.singleClickDelay

                            onTriggered: player.position = Math.max(player.position - AppConstants.seekStepSmall, 0)
                        }
                    }
                    Button {
                        id: stopButton

                        Layout.preferredHeight: 30
                        Layout.preferredWidth: 25
                        Material.roundedScale: Material.NotRounded
                        ToolTip.delay: AppConstants.tooltipDelay
                        ToolTip.text: qsTr("Stop")
                        ToolTip.timeout: AppConstants.tooltipTimeout
                        ToolTip.visible: hovered
                        enabled: player.playbackState !== MediaPlayer.StoppedState
                        font.family: materialSymbolsOutlined.name
                        font.weight: Font.Light
                        hoverEnabled: true
                        scale: 1.5
                        text: "\ue047"

                        onClicked: {
                            player.stop();
                            seekBar.resetPreview();
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
                                font: fastForwardButton.font
                                opacity: fastForwardButton.enabled ? 1.0 : 0.5
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
                                var restoreRate = (currentSpeedIndex >= 0 && currentSpeedIndex < AppConstants.playbackSpeeds.length) ? AppConstants.playbackSpeeds[currentSpeedIndex] : 1.0;
                                fastForwardRate = restoreRate;
                                player.playbackRate = restoreRate;
                            }
                        }
                        onPressAndHold: forwardHoldTimer.start()
                        onReleased: forwardHoldTimer.stop()

                        Timer {
                            id: ffwSingleClickTimer

                            interval: AppConstants.singleClickDelay

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

                        onClicked: playlistToggleRequested()

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
                                font: repeatButton.font
                                opacity: repeatMode === 0 ? 0.5 : 1.0
                                text: repeatMode === 1 ? "\ue9d7" : (repeatMode >= 2 ? "\ue9d6" : "\ue040")
                            }
                            // Rectangle {
                            //     anchors.bottom: parent.bottom
                            //     anchors.bottomMargin: 2
                            //     anchors.right: parent.right
                            //     anchors.rightMargin: -2
                            //     // color: Material.accent
                            //     height: 10
                            //     radius: 2
                            //     visible: repeatMode === 3
                            //     width: 10

                            Text {
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 2
                                anchors.centerIn: parent
                                anchors.right: parent.right
                                anchors.rightMargin: -2
                                color: isDarkTheme ? "#000000" : "#FFFFFF"
                                // font.bold: true
                                font.pixelSize: 5
                                text: "R"
                                visible: repeatMode === 3
                            }
                            // }
                        }

                        onClicked: {
                            repeatMode = (repeatMode + 1) % 4;
                            if (repeatMode === 3 && player.duration > 0 && seekBar.rangeSlider) {
                                seekBar.rangeSlider.setValues(0, player.duration);
                            }
                        }

                        ToolTip {
                            delay: AppConstants.tooltipDelay
                            text: {
                                if (repeatMode === 0)
                                    return qsTr("Repeat: Off");
                                if (repeatMode === 1)
                                    return qsTr("Repeat: Once");
                                if (repeatMode === 2)
                                    return qsTr("Repeat: Loop");
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
                    PlaybackSpeedMenu {
                        mediaLoaded: root.mediaLoaded
                        player: root.player

                        onSpeedChanged: function (speed) {
                            stopFastForwarding();
                            stopFastRewinding();
                            var idx = AppConstants.playbackSpeeds.indexOf(speed);
                            if (idx >= 0) {
                                currentSpeedIndex = idx;
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
                            delay: AppConstants.tooltipDelay
                            text: qsTr("Capture a frame")
                            timeout: AppConstants.tooltipTimeout
                            visible: captureButton.hovered
                        }
                    }
                    CollageButton {
                        collageTarget: root.collageTarget
                        sourceUrls: player.hasVideo ? [player.source] : []
                    }
                    BrightnessContrastPopup {
                        hasVideo: player.hasVideo
                        videoOutput: root.videoOutput
                    }
                    Button {
                        id: nerdStatsButton

                        Layout.preferredHeight: 30
                        Layout.preferredWidth: 25
                        Material.roundedScale: Material.NotRounded
                        checkable: true
                        enabled: player.hasVideo
                        font.family: materialSymbolsOutlined.name
                        font.weight: Font.Light
                        hoverEnabled: true
                        scale: 1.5
                        text: "\uf2c7"

                        onClicked: {
                            nerdStatsToggleRequested();
                        }

                        Binding {
                            target: nerdStatsButton
                            property: "checked"
                            value: root.nerdStatsActive
                            restoreMode: Binding.RestoreBindingOrValue
                        }

                        ToolTip {
                            delay: AppConstants.tooltipDelay
                            text: qsTr("Stats for nerds")
                            timeout: AppConstants.tooltipTimeout
                            visible: nerdStatsButton.hovered
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
                            delay: AppConstants.tooltipDelay
                            text: qsTr("Skip back")
                            timeout: AppConstants.tooltipTimeout
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
                            delay: AppConstants.tooltipDelay
                            text: qsTr("Skip next")
                            timeout: AppConstants.tooltipTimeout
                            visible: nextTrackButton.hovered
                        }
                    }
                }
                Item {
                    Layout.fillWidth: true
                }

                // Volume, mini player, and fullscreen
                RowLayout {
                    Layout.alignment: Qt.AlignRight
                    spacing: 5

                    VolumeControlComponent {
                        audioOutput: root.audioOutput
                    }
                    Button {
                        id: miniPlayerButton

                        Accessible.description: miniPlayerActive ? qsTr("Mini player is active") : qsTr("Open mini player")
                        Accessible.name: qsTr("Mini player")
                        Accessible.role: Accessible.Button
                        Layout.preferredHeight: 36
                        Layout.preferredWidth: 36
                        Material.background: "transparent"
                        Material.foreground: miniPlayerActive ? Material.accent : undefined
                        Material.roundedScale: Material.NotRounded
                        ToolTip.delay: AppConstants.tooltipDelay
                        ToolTip.text: miniPlayerActive ? qsTr("Mini player is active") : qsTr("Open mini player")
                        ToolTip.timeout: AppConstants.tooltipTimeout
                        ToolTip.visible: hovered
                        enabled: mediaLoaded && !miniPlayerActive
                        font.family: materialSymbolsOutlined.name
                        font.pixelSize: 22
                        hoverEnabled: true
                        padding: 0
                        text: "\ue911"

                        onClicked: miniPlayerRequested()
                    }
                    Button {
                        id: fullscreenButton

                        Accessible.description: qsTr("Enter or exit fullscreen mode")
                        Accessible.name: qsTr("Toggle fullscreen")
                        Accessible.role: Accessible.Button
                        Layout.preferredHeight: 36
                        Layout.preferredWidth: 36
                        Material.background: "transparent"
                        Material.roundedScale: Material.NotRounded
                        ToolTip.delay: AppConstants.tooltipDelay
                        ToolTip.text: qsTr("Toggle fullscreen")
                        ToolTip.timeout: AppConstants.tooltipTimeout
                        ToolTip.visible: hovered
                        font.family: materialSymbolsOutlined.name
                        font.pixelSize: 22
                        hoverEnabled: true
                        padding: 0
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
