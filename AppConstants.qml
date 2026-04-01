pragma Singleton
import QtQuick

QtObject {
    // Supported file extensions
    readonly property var videoExtensions: ["mp4", "avi", "mkv", "mov", "wmv", "flv", "webm", "m4v", "mpg"]
    readonly property var audioExtensions: ["mp3", "wav", "ogg", "flac", "aac", "wma", "m4a"]

    // UI timing constants
    readonly property int controlsHideDelay: 3000
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

    // Helper functions
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
    function getVideoExtensionsFilter(): string {
        return "Video Files (*." + videoExtensions.join(" *.") + ")";
    }

    function getAudioExtensionsFilter(): string {
        return "Audio Files (*." + audioExtensions.join(" *.") + ")";
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
