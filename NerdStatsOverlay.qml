pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import gavqml

Rectangle {
    id: root

    property bool hasVideo: false
    property var videoOutput: null
    property var player: null
    property var audioOutput: null
    property real maximumHeight: 600

    readonly property int historyLength: 60
    property var fpsHistory: []
    property var cpuHistory: []
    property var gpuHistory: []

    readonly property int maximumRecordedSamples: 86400
    property var recording: []
    property double recordingStartedAt: 0
    property int recordedSampleCount: 0

    readonly property var sections: [
        {
            title: qsTr("Video"),
            visible: () => root.hasVideo,
            rows: [
                { label: qsTr("Resolution"), value: () => root.videoOutput && root.videoOutput.sourceRect.width > 0 ? root.videoOutput.sourceRect.width + "x" + root.videoOutput.sourceRect.height : "" },
                { label: qsTr("Codec"), value: () => root.mediaValue("videoCodec") },
                { label: qsTr("Bitrate"), value: () => root.formatBitRate(root.mediaValue("videoBitRate")) },
                { label: qsTr("Frame rate"), value: () => root.frameRateText() },
                { label: qsTr("Dropped frames"), value: () => root.sourceFrameRate() > 0 ? String(root.frameValue("droppedFrames") || 0) : "" },
                { label: qsTr("Decoding"), value: () => root.frameValue("decoding") },
                { label: qsTr("Pixel format"), value: () => root.frameValue("pixelFormat") },
                { label: qsTr("Colour"), value: () => root.colorText(), tooltip: () => root.frameValue("colorAssumed") ? qsTr("* Not tagged in the file, value assumed") : "" },
                { label: qsTr("HDR"), value: () => root.player && root.player.mediaLoaded ? ((root.mediaValue("hdr") || root.frameValue("hdr")) ? qsTr("Yes") : qsTr("No")) : "" },
                { label: qsTr("Rotation"), value: () => root.mediaValue("rotation") !== "" ? root.mediaValue("rotation") + "°" : "" }
            ]
        },
        {
            title: qsTr("Audio"),
            visible: () => true,
            rows: [
                { label: qsTr("Codec"), value: () => root.mediaValue("audioCodec") },
                { label: qsTr("Bitrate"), value: () => root.formatBitRate(root.mediaValue("audioBitRate")) },
                { label: qsTr("Format"), value: () => root.audioFormatText() },
                { label: qsTr("Output"), value: () => root.audioOutput ? root.audioOutput.device.description : "" },
                { label: qsTr("Volume"), value: () => root.audioOutput ? (root.audioOutput.muted ? qsTr("Muted") : Math.round(root.audioOutput.volume * 100) + "%") : "" }
            ]
        },
        {
            title: qsTr("Playback"),
            visible: () => true,
            rows: [
                { label: qsTr("Container"), value: () => root.mediaValue("container") },
                { label: qsTr("File size"), value: () => root.formatBytes(root.mediaValue("fileSize")) },
                { label: qsTr("Position"), value: () => root.positionText() },
                { label: qsTr("Speed"), value: () => root.player ? root.player.playbackRate + "x" : "" },
                { label: qsTr("Renderer"), value: () => root.graphicsApiName(GraphicsInfo.api) }
            ]
        },
        {
            title: qsTr("System"),
            visible: () => true,
            rows: [
                { label: qsTr("CPU"), value: () => SystemStats.cpuUsage },
                { label: qsTr("GPU"), value: () => SystemStats.gpuUsage },
                { label: qsTr("GPU memory"), value: () => SystemStats.gpuMemory },
                { label: qsTr("RAM"), value: () => SystemStats.ramUsage },
                { label: qsTr("IO"), value: () => SystemStats.ioUsage },
                { label: qsTr("Threads"), value: () => SystemStats.threadCount }
            ]
        }
    ]

    function mediaValue(key) {
        const info = player ? player.mediaInfo : null;
        return info && info[key] !== undefined ? info[key] : "";
    }
    function frameValue(key) {
        const info = player ? player.frameInfo : null;
        return info && info[key] !== undefined ? info[key] : "";
    }
    function sourceFrameRate() {
        return Number(mediaValue("frameRate")) || Number(frameValue("streamFrameRate")) || 0;
    }
    function frameRateText() {
        const source = sourceFrameRate();
        const measured = player && player.fps > 0 ? player.fps.toFixed(0) : "-";
        return source > 0 ? measured + " / " + Number(source.toFixed(3)) + " fps" : (measured !== "-" ? measured + " fps" : "");
    }
    function colorText() {
        const space = frameValue("colorSpace");
        return space === "" ? "" : [space, frameValue("colorTransfer"), frameValue("colorRange")].join(" · ");
    }
    function audioFormatText() {
        const sampleRate = Number(mediaValue("sampleRate"));
        if (!(sampleRate > 0))
            return "";
        const channels = Number(mediaValue("channels"));
        const layouts = { 1: qsTr("Mono"), 2: qsTr("Stereo"), 6: "5.1", 8: "7.1" };
        const layout = layouts[channels] !== undefined ? layouts[channels] : qsTr("%1 channels").arg(channels);
        return Number((sampleRate / 1000).toFixed(1)) + " kHz, " + layout;
    }
    function positionText() {
        if (!player || !player.mediaLoaded)
            return "";
        const duration = player.duration;
        const percent = duration > 0 ? " (" + Math.floor(player.position * 100 / duration) + "%)" : "";
        return AppConstants.formatTime(player.position) + " / " + AppConstants.formatTime(duration) + percent;
    }
    function formatBitRate(bitsPerSecond) {
        const value = Number(bitsPerSecond);
        if (!(value > 0))
            return "";
        return value >= 1e6 ? (value / 1e6).toFixed(2) + " Mbps" : Math.round(value / 1e3) + " kbps";
    }
    function formatBytes(bytes) {
        const value = Number(bytes);
        if (!(value > 0))
            return "";
        if (value >= 1024 * 1024 * 1024)
            return (value / (1024 * 1024 * 1024)).toFixed(2) + " GB";
        if (value >= 1024 * 1024)
            return (value / (1024 * 1024)).toFixed(1) + " MB";
        return (value / 1024).toFixed(1) + " KB";
    }
    function graphicsApiName(api) {
        switch (api) {
        case GraphicsInfo.Software:
            return qsTr("Software");
        case GraphicsInfo.OpenGL:
            return "OpenGL";
        case GraphicsInfo.Direct3D11:
            return "Direct3D 11";
        case GraphicsInfo.Direct3D12:
            return "Direct3D 12";
        case GraphicsInfo.Vulkan:
            return "Vulkan";
        case GraphicsInfo.Metal:
            return "Metal";
        default:
            return "";
        }
    }
    function displayValue(value) {
        return value === "" || value === undefined || value === null ? "N/A" : String(value);
    }
    function statsText() {
        const lines = ["GAV " + Qt.application.version + " (Qt " + BuildInfo.qtVersion + (BuildInfo.ffmpegVersion ? ", FFmpeg " + BuildInfo.ffmpegVersion : "") + ")"];
        for (const section of sections) {
            if (!section.visible())
                continue;
            lines.push("", "[" + section.title + "]");
            for (const row of section.rows) {
                const tooltip = row.tooltip ? row.tooltip() : "";
                lines.push(row.label + ": " + displayValue(row.value()) + (tooltip ? " (" + tooltip + ")" : ""));
            }
        }
        if (recording.length > 0) {
            lines.push("", "[History]", qsTr("%1 samples, one per second").arg(recording.length), historyCsv());
        }
        return lines.join("\n");
    }
    function recordSample() {
        const source = player ? player.source.toString() : "";
        recording.push({
            time: (Date.now() - recordingStartedAt) / 1000,
            file: source === "" ? "" : decodeURIComponent(source.substring(source.lastIndexOf("/") + 1)),
            state: player ? ["stopped", "playing", "paused"][player.playbackState] || "" : "",
            position: player && player.mediaLoaded ? player.position : -1,
            fps: player && hasVideo ? player.fps : -1,
            droppedFrames: hasVideo && sourceFrameRate() > 0 ? Number(frameValue("droppedFrames")) || 0 : -1,
            cpu: SystemStats.cpuPercent,
            gpu: SystemStats.gpuPercent,
            gpuMemory: SystemStats.gpuMemoryBytes,
            ram: SystemStats.ramBytes,
            ioRead: SystemStats.ioReadRate,
            ioWrite: SystemStats.ioWriteRate,
            threads: SystemStats.threads
        });
        if (recording.length > maximumRecordedSamples)
            recording.shift();
        recordedSampleCount = recording.length;
    }
    function clearRecording() {
        recording = [];
        recordedSampleCount = 0;
        recordingStartedAt = Date.now();
        fpsHistory = [];
        cpuHistory = [];
        gpuHistory = [];
    }
    function historyCsv() {
        const number = (value, divisor, digits) => value >= 0 ? (value / divisor).toFixed(digits) : "";
        const text = value => value === "" ? "" : "\"" + value.replace(/"/g, "\"\"") + "\"";
        const rows = ["time_s,file,state,position_s,fps,dropped_frames,cpu_percent,gpu_percent,gpu_memory_mb,ram_mb,io_read_kbps,io_write_kbps,threads"];
        for (const sample of recording) {
            rows.push([
                sample.time.toFixed(1),
                text(sample.file),
                sample.state,
                number(sample.position, 1000, 3),
                number(sample.fps, 1, 0),
                number(sample.droppedFrames, 1, 0),
                number(sample.cpu, 1, 1),
                number(sample.gpu, 1, 1),
                number(sample.gpuMemory, 1024 * 1024, 1),
                number(sample.ram, 1024 * 1024, 1),
                number(sample.ioRead, 1024, 1),
                number(sample.ioWrite, 1024, 1),
                number(sample.threads, 1, 0)
            ].join(","));
        }
        return rows.join("\n");
    }
    function pushSample(history, value) {
        const next = history.slice(-(historyLength - 1));
        next.push(value);
        return next;
    }

    width: 360
    height: Math.min((contentLoader.item ? contentLoader.item.implicitHeight : 0) + 20, maximumHeight)
    color: Qt.rgba(0, 0, 0, 0.7)
    radius: 8
    border.color: Material.dividerColor
    border.width: 1

    visible: false

    onVisibleChanged: clearRecording()

    Binding {
        target: SystemStats
        property: "active"
        value: root.visible
    }

    Binding {
        target: root.player
        property: "statsEnabled"
        value: root.visible
        when: root.player !== null
    }

    Connections {
        enabled: root.visible
        target: SystemStats

        function onSampled() {
            root.recordSample();
            root.fpsHistory = root.pushSample(root.fpsHistory, root.player ? root.player.fps : 0);
            root.cpuHistory = root.pushSample(root.cpuHistory, SystemStats.cpuPercent);
            root.gpuHistory = root.pushSample(root.gpuHistory, SystemStats.gpuPercent);
        }
    }

    Timer {
        id: copiedResetTimer

        interval: 1500
    }

    component Sparkline: ColumnLayout {
        id: sparkline

        property string label
        property var values: []
        property int historyLength: 60
        property real minimumMax: 100
        property string unit

        spacing: 2

        onValuesChanged: canvas.requestPaint()

        RowLayout {
            Layout.fillWidth: true

            Text {
                Layout.fillWidth: true
                color: Material.foreground
                font.pixelSize: 12
                opacity: 0.8
                text: sparkline.label
            }
            Text {
                color: Material.foreground
                font.family: "monospace"
                font.pixelSize: 12
                text: {
                    const last = sparkline.values.length > 0 ? sparkline.values[sparkline.values.length - 1] : -1;
                    return last >= 0 ? last.toFixed(sparkline.unit === "%" ? 1 : 0) + sparkline.unit : "N/A";
                }
            }
        }
        Canvas {
            id: canvas

            Layout.fillWidth: true
            Layout.preferredHeight: 32

            onWidthChanged: requestPaint()
            onPaint: {
                const ctx = getContext("2d");
                ctx.reset();
                ctx.fillStyle = Qt.rgba(1, 1, 1, 0.06);
                ctx.fillRect(0, 0, width, height);

                const values = sparkline.values;
                let maxValue = sparkline.minimumMax;
                for (const value of values) {
                    if (value >= 0)
                        maxValue = Math.max(maxValue, value);
                }

                const step = width / (sparkline.historyLength - 1);
                const offset = sparkline.historyLength - values.length;
                ctx.strokeStyle = Material.accent;
                ctx.lineWidth = 1.5;
                ctx.beginPath();
                let started = false;
                for (let i = 0; i < values.length; ++i) {
                    if (!(values[i] >= 0)) {
                        started = false;
                        continue;
                    }
                    const x = (offset + i) * step;
                    const y = height - 1 - (values[i] / maxValue) * (height - 2);
                    if (started) {
                        ctx.lineTo(x, y);
                    } else {
                        ctx.moveTo(x, y);
                        started = true;
                    }
                }
                ctx.stroke();
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons

        onWheel: function (wheel) {
            wheel.accepted = true;
        }
    }

    Loader {
        id: contentLoader

        anchors.fill: parent
        anchors.margins: 10
        active: root.visible

        sourceComponent: Component {
            ColumnLayout {
                id: contentLayout

                spacing: 5

                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        text: qsTr("Stats for nerds")
                        color: Material.foreground
                        font.bold: true
                        font.pixelSize: 14
                        Layout.fillWidth: true
                    }
                    Button {
                        id: copyButton

                        readonly property bool copied: copiedResetTimer.running

                        text: copied ? "" : ""
                        font.family: materialSymbolsOutlined.name
                        font.pixelSize: 16
                        flat: true
                        hoverEnabled: true
                        padding: 0
                        Layout.preferredWidth: 24
                        Layout.preferredHeight: 24

                        ToolTip.delay: AppConstants.tooltipDelay
                        ToolTip.text: copied ? qsTr("Copied") : qsTr("Copy stats and %1 s of history").arg(root.recordedSampleCount)
                        ToolTip.timeout: AppConstants.tooltipTimeout
                        ToolTip.visible: hovered

                        contentItem: Text {
                            text: copyButton.text
                            font: copyButton.font
                            color: Material.foreground
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            color: copyButton.hovered ? Qt.rgba(1, 1, 1, 0.12) : "transparent"
                            radius: 4
                        }

                        onClicked: {
                            SystemStats.copyToClipboard(root.statsText());
                            copiedResetTimer.restart();
                        }
                    }
                    Button {
                        id: closeButton
                        text: ""
                        font.family: materialSymbolsOutlined.name
                        font.pixelSize: 16
                        flat: true
                        hoverEnabled: true
                        padding: 0
                        Layout.preferredWidth: 24
                        Layout.preferredHeight: 24

                        ToolTip.delay: AppConstants.tooltipDelay
                        ToolTip.text: qsTr("Close")
                        ToolTip.timeout: AppConstants.tooltipTimeout
                        ToolTip.visible: hovered

                        contentItem: Text {
                            text: closeButton.text
                            font: closeButton.font
                            color: closeButton.hovered ? (Material.theme === Material.Dark ? "#ff6b6b" : "#e02424") : Material.foreground
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            color: closeButton.hovered ? Qt.rgba(1, 1, 1, 0.12) : "transparent"
                            radius: 4
                        }

                        onClicked: root.visible = false
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: Material.dividerColor
                }

                Flickable {
                    id: flickable

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredHeight: body.implicitHeight
                    boundsBehavior: Flickable.StopAtBounds
                    clip: true
                    contentHeight: body.implicitHeight

                    ScrollBar.vertical: ScrollBar {
                        policy: flickable.contentHeight > flickable.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
                    }

                    ColumnLayout {
                        id: body

                        width: flickable.width
                        spacing: 8

                        Repeater {
                            model: root.sections

                            delegate: ColumnLayout {
                                id: sectionDelegate

                                required property var modelData

                                Layout.fillWidth: true
                                spacing: 2
                                visible: modelData.visible()

                                Text {
                                    color: Material.accent
                                    font.bold: true
                                    font.pixelSize: 12
                                    text: sectionDelegate.modelData.title
                                }

                                Repeater {
                                    model: sectionDelegate.modelData.rows

                                    delegate: RowLayout {
                                        id: rowDelegate

                                        required property var modelData

                                        Layout.fillWidth: true
                                        spacing: 10

                                        Text {
                                            Layout.alignment: Qt.AlignTop
                                            Layout.preferredWidth: 110
                                            color: Material.foreground
                                            font.pixelSize: 12
                                            opacity: 0.8
                                            text: rowDelegate.modelData.label
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            ToolTip.delay: AppConstants.tooltipDelay
                                            ToolTip.text: rowDelegate.modelData.tooltip ? rowDelegate.modelData.tooltip() : ""
                                            ToolTip.timeout: AppConstants.tooltipTimeout
                                            ToolTip.visible: valueHover.hovered && ToolTip.text !== ""
                                            color: Material.foreground
                                            font.family: "monospace"
                                            font.pixelSize: 12
                                            text: root.displayValue(rowDelegate.modelData.value())
                                            wrapMode: Text.Wrap

                                            HoverHandler {
                                                id: valueHover
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            Text {
                                color: Material.accent
                                font.bold: true
                                font.pixelSize: 12
                                text: qsTr("History")
                            }
                            Sparkline {
                                Layout.fillWidth: true
                                historyLength: root.historyLength
                                label: qsTr("FPS")
                                minimumMax: Math.max(1, root.sourceFrameRate())
                                values: root.fpsHistory
                                visible: root.hasVideo
                            }
                            Sparkline {
                                Layout.fillWidth: true
                                historyLength: root.historyLength
                                label: qsTr("CPU")
                                unit: "%"
                                values: root.cpuHistory
                            }
                            Sparkline {
                                Layout.fillWidth: true
                                historyLength: root.historyLength
                                label: qsTr("GPU")
                                unit: "%"
                                values: root.gpuHistory
                            }
                        }
                    }
                }
            }
        }
    }
}
