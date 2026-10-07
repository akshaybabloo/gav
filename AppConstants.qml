pragma Singleton
import QtQuick

QtObject {
    // Supported file extensions
    readonly property var videoExtensions: ["mp4", "avi", "mkv", "mov", "wmv", "flv", "webm", "m4v", "mpg"]
    readonly property var audioExtensions: ["mp3", "wav", "ogg", "flac", "aac", "wma", "m4a"]
    readonly property var subtitleExtensions: ["srt", "ass", "ssa", "vtt"]
    readonly property var playlistExtensions: ["m3u", "m3u8"]

    // UI timing constants
    readonly property int controlsHideDelay: 3000

    // Playlist panel (in pixels, except the fraction of the window width)
    readonly property int playlistPanelDefaultWidth: 360
    readonly property int playlistPanelMinWidth: 280
    readonly property int playlistPanelResizeStep: 20
    readonly property int playlistGroupHeaderHeight: 36
    readonly property int playlistRowHeight: 68
    readonly property real playlistPanelMaxFraction: 0.6
    readonly property int playlistNarrowWindowWidth: 560
    readonly property int tooltipDelay: 1000
    readonly property int tooltipTimeout: 5000
    readonly property int snackbarDuration: 3000
    readonly property int volumeDisplayDuration: 2000
    readonly property int zoomDisplayDuration: 2000
    readonly property int animationDuration: 300

    // Volume constants
    readonly property real volumeStep: 0.05
    readonly property real defaultVolume: 0.5

    // Zoom constants
    readonly property real zoomMin: 1.0
    readonly property real zoomMax: 5.0
    readonly property real zoomFactor: 1.25

    // Brightness and contrast constants
    readonly property real defaultBrightness: 0.0
    readonly property real defaultContrast: 0.0
    readonly property real brightnessMin: -1.0
    readonly property real brightnessMax: 1.0
    readonly property real contrastMin: -1.0
    readonly property real contrastMax: 1.0
    readonly property real brightnessContrastStep: 0.01

    // Subtitle constants
    readonly property int subtitleDelayStep: 100
    readonly property int subtitleDelayLimit: 600000
    readonly property real subtitleScaleMin: 0.5
    readonly property real subtitleScaleMax: 3.0
    readonly property real subtitleScaleStep: 0.1

    // Navigation and streaming constants (in milliseconds)
    readonly property int chapterPreviousThreshold: 3000
    readonly property int streamLoadTimeout: 120000

    // Seek constants (in milliseconds)
    readonly property int seekStep: 5000
    readonly property int seekStepSmall: 1000

    // Timer intervals (in milliseconds)
    readonly property int holdTimerInterval: 200
    readonly property int rewindSeekInterval: 100
    readonly property int seekSliderUpdateInterval: 500
    readonly property int previewRequestInterval: 50
    readonly property int singleClickDelay: 250
    readonly property int rewindSeekMultiplier: 100
    readonly property int fastSpeedMultiplier: 10

    // Overlay colors
    readonly property color overlayTextColor: "white"
    readonly property color overlayOutlineColor: "black"
    readonly property color overlayBackgroundColor: "#2a2a2a"
    readonly property color volumeBarColor: "#4CAF50"

    // Playback speed options
    readonly property var playbackSpeeds: [0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0]

    readonly property var shortcutReference: [
        { "keys": "Space", "action": qsTr("Play / pause") },
        { "keys": "Left / Right", "action": qsTr("Seek 5 seconds") },
        { "keys": "E / Shift+E", "action": qsTr("Next / previous frame") },
        { "keys": "Ctrl+T", "action": qsTr("Go to time") },
        { "keys": "Shift+N / Shift+P", "action": qsTr("Next / previous chapter") },
        { "keys": "N / P", "action": qsTr("Next / previous playlist item") },
        { "keys": "Ctrl+L", "action": qsTr("Show / hide playlist") },
        { "keys": "Ctrl+F", "action": qsTr("Search the playlist") },
        { "keys": "Up / Down / Page Up / Page Down / Home / End", "action": qsTr("Playlist: move through the rows (Shift extends the selection)") },
        { "keys": "Enter / Q / Delete", "action": qsTr("Playlist: play / play next / remove") },
        { "keys": "Ctrl+A / Ctrl+Space", "action": qsTr("Playlist: select all / toggle selection") },
        { "keys": "Ctrl+Z", "action": qsTr("Playlist: undo the last removal") },
        { "keys": "Alt+Up / Alt+Down", "action": qsTr("Playlist: move the selection") },
        { "keys": "Menu / Shift+F10", "action": qsTr("Playlist: open the row menu") },
        { "keys": "Left / Right", "action": qsTr("Playlist: collapse / expand the focused group, or resize the panel from its edge") },
        { "keys": "/", "action": qsTr("Playlist: search") },
        { "keys": "[ / ] / =", "action": qsTr("Slower / faster / normal speed") },
        { "keys": "Scroll", "action": qsTr("Volume") },
        { "keys": "Ctrl+Up / Ctrl+Down", "action": qsTr("Volume up / down") },
        { "keys": "M", "action": qsTr("Mute") },
        { "keys": "V", "action": qsTr("Cycle subtitle track") },
        { "keys": "B", "action": qsTr("Cycle audio track") },
        { "keys": "G / H", "action": qsTr("Subtitle delay −/+ 100 ms") },
        { "keys": "F / Double-click", "action": qsTr("Full-screen") },
        { "keys": "Esc", "action": qsTr("Exit full-screen") },
        { "keys": "Ctrl+Scroll", "action": qsTr("Zoom") },
        { "keys": "I", "action": qsTr("Stats for nerds") },
        { "keys": "Ctrl+O", "action": qsTr("Open file") },
        { "keys": "Ctrl+N", "action": qsTr("Open URL") }
    ]

    // Helper functions
    function formatTime(ms) {
        var value = Number(ms);
        if (!isFinite(value) || value < 0)
            value = 0;
        var totalSecs = Math.floor(value / 1000);
        var h = Math.floor(totalSecs / 3600);
        var m = Math.floor((totalSecs % 3600) / 60);
        var s = totalSecs % 60;
        var pad = function (num) {
            return String(num).padStart(2, '0');
        };
        return h > 0 ? h + ":" + pad(m) + ":" + pad(s) : pad(m) + ":" + pad(s);
    }
    function getVideoExtensionsFilter(): string {
        return "Video Files (*." + videoExtensions.join(" *.") + ")";
    }

    function getAudioExtensionsFilter(): string {
        return "Audio Files (*." + audioExtensions.join(" *.") + ")";
    }

    function getSubtitleExtensionsFilter(): string {
        return "Subtitle Files (*." + subtitleExtensions.join(" *.") + ")";
    }

    function getPlaylistExtensionsFilter(): string {
        return "Playlists (*." + playlistExtensions.join(" *.") + ")";
    }

    function isPlaylistExtension(ext: string): bool {
        return playlistExtensions.indexOf(ext.toLowerCase()) !== -1;
    }

    function isSubtitleExtension(ext: string): bool {
        return subtitleExtensions.indexOf(ext.toLowerCase()) !== -1;
    }

    function formatDelay(ms: int): string {
        return (ms > 0 ? "+" : "") + ms + " ms";
    }

    function getSupportedFormatsString(): string {
        var allExts = videoExtensions.concat(audioExtensions);
        return allExts.map(ext => ext.toUpperCase()).join(", ");
    }

    function isVideoExtension(ext: string): bool {
        return videoExtensions.indexOf(ext.toLowerCase()) !== -1;
    }

    function isAudioExtension(ext: string): bool {
        return audioExtensions.indexOf(ext.toLowerCase()) !== -1;
    }

    function isSupportedExtension(ext: string): bool {
        return isVideoExtension(ext) || isAudioExtension(ext);
    }
}
