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

    // Playback speed options
    readonly property var playbackSpeeds: [0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0]

    // Helper functions
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
