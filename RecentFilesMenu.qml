import QtQuick
import QtQuick.Controls

import gavqml

Menu {
    id: root

    property var recent: PlaybackHistory.recentFiles()

    signal missingFile(string path)
    signal openRequested(url url)

    function displayName(path) {
        if (path.startsWith("http://") || path.startsWith("https://"))
            return path;
        return path.substring(Math.max(path.lastIndexOf('/'), path.lastIndexOf('\\')) + 1);
    }

    title: qsTr("Open Recent")

    Connections {
        function onRecentChanged() {
            root.recent = PlaybackHistory.recentFiles();
        }

        target: PlaybackHistory
    }
    Instantiator {
        model: root.recent

        delegate: MenuItem {
            required property string modelData

            text: root.displayName(modelData)

            onTriggered: {
                if (PlaybackHistory.exists(modelData))
                    root.openRequested(PlaybackHistory.urlFor(modelData));
                else
                    root.missingFile(modelData);
            }
        }

        onObjectAdded: function (index, object) {
            root.insertItem(index, object);
        }
        onObjectRemoved: function (index, object) {
            root.removeItem(object);
        }
    }
    MenuItem {
        enabled: false
        text: qsTr("No recent files")
        visible: root.recent.length === 0
        height: visible ? implicitHeight : 0
    }
    MenuSeparator {
    }
    MenuItem {
        enabled: root.recent.length > 0
        text: qsTr("Clear recent files")

        onTriggered: PlaybackHistory.clearRecent()
    }
}
