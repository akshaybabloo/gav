import QtQuick
import QtQuick.Controls
import QtMultimedia
import QtQuick.Layouts
import QtQuick.Controls.Material

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
    property real previousVolume: 0.5
    property int rewindMultiplier: 1
    required property var videoOutput

    signal nextTrack
    signal previousTrack

    function formatTime(ms) {
        var seconds = Math.floor(ms / 1000);
        var minutes = Math.floor(seconds / 60);
        var hours = Math.floor(minutes / 60);
        seconds = seconds % 60;
        return Qt.formatTime(new Date(0, 0, hours, 0, minutes, seconds), "hh:mm:ss");
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

                    Layout.fillWidth: true
                    Layout.preferredHeight: 10
                    enabled: mediaLoaded
                    from: 0
                    to: player.duration

                    onMoved: player.position = value

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
                    MouseArea {
                        acceptedButtons: Qt.NoButton
                        anchors.fill: parent
                        propagateComposedEvents: true

                        onWheel: function (wheel) {
                            if (player.playbackState === MediaPlayer.PlayingState) {
                                const seekAmount = 1000;  // 1 second
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
                        ToolTip.delay: 1000
                        ToolTip.text: player.playbackState === MediaPlayer.PlayingState ? qsTr("Pause") : qsTr("Play")
                        ToolTip.timeout: 5000
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

                        Layout.preferredHeight: 30
                        Layout.preferredWidth: 25
                        Material.roundedScale: Material.NotRounded
                        ToolTip.delay: 1000
                        ToolTip.text: qsTr("Fast rewind")
                        ToolTip.timeout: 5000
                        ToolTip.visible: hovered
                        enabled: player.playbackState !== MediaPlayer.StoppedState
                        font.family: materialSymbolsOutlined.name
                        font.weight: Font.Light
                        hoverEnabled: true
                        scale: 1.5
                        text: isFastRewinding ? "\ue020" + "<sub>\ue059</sub>" : "\ue020"

                        contentItem: Text {
                            color: parent.enabled ? "white" : "#a0a0a0"
                            font: parent.font
                            horizontalAlignment: Text.AlignHCenter
                            text: parent.text
                            textFormat: Text.RichText
                            verticalAlignment: Text.AlignVCenter
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

                            onTriggered: player.position = Math.max(player.position - 1000, 0)
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
                        ToolTip.delay: 1000
                        ToolTip.text: qsTr("Fast forward")
                        ToolTip.timeout: 5000
                        ToolTip.visible: hovered
                        enabled: player.playbackState !== MediaPlayer.StoppedState
                        font.family: materialSymbolsOutlined.name
                        font.weight: Font.Light
                        hoverEnabled: true
                        scale: 1.5
                        text: isFastForwarding ? "\ue01f" + "<sub>\ue056</sub>" : "\ue01f"

                        contentItem: Text {
                            color: parent.enabled ? "white" : "#a0a0a0"
                            font: parent.font
                            horizontalAlignment: Text.AlignHCenter
                            text: parent.text
                            textFormat: Text.RichText
                            verticalAlignment: Text.AlignVCenter
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

                            onTriggered: player.position = Math.min(player.position + 1000, player.duration)
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

                        onClicked: {
                            playlistComponent.visible = !playlistComponent.visible;
                        }

                        ToolTip {
                            delay: 1000
                            text: qsTr("Toggle playlist")
                            timeout: 5000
                            visible: playListButton.hovered
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
                                    volumeSlider.value = Math.min(volumeSlider.value + 0.05, 1.0);
                                } else if (wheel.angleDelta.y < 0) {
                                    volumeSlider.value = Math.max(volumeSlider.value - 0.05, 0.0);
                                }
                            }
                        }
                    }
                    Button {
                        Layout.preferredHeight: 30
                        Layout.preferredWidth: 25
                        Material.background: "transparent"
                        Material.roundedScale: Material.NotRounded
                        ToolTip.delay: 1000
                        ToolTip.text: qsTr("Toggle fullscreen")
                        ToolTip.timeout: 5000
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
