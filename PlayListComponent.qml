import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    required property ListModel playList
    property alias playListView: playListView

    function toPathList(model) {
        var dataArray = [];
        for (var i = 0; i < model.count; ++i) {
            dataArray.push(model.get(i).path);
        }
        return dataArray;
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 5
        visible: playList.count === 0

        Text {
            Layout.alignment: Qt.AlignHCenter
            color: "white"
            font.family: materialSymbolsOutlined.name
            font.pixelSize: 80
            font.weight: Font.ExtraLight
            text: "\uf523"
        }
        Text {
            Layout.alignment: Qt.AlignHCenter
            color: "white"
            font.pixelSize: 24
            text: "Add video or audio files to play"
        }
    }
    ListView {
        id: playListView

        anchors.fill: parent
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        model: playList
        visible: playList.count > 0

        ScrollBar.vertical: ScrollBar {
        }
        delegate: ItemDelegate {
            height: 40
            padding: 8
            width: parent?.width

            background: Rectangle {
                color: parent.down ? "#4a4a4e" : (parent.hovered ? "#2a2a2e" : (parent.ListView.isCurrentItem ? "#383838" : "transparent"))
                radius: 4
            }
            contentItem: Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 12

                Text {
                    color: "white"
                    font.family: materialSymbolsOutlined.name
                    font.pixelSize: 24
                    text: model.icon
                }
                Text {
                    color: "white"
                    elide: Text.ElideRight
                    font.pixelSize: 14
                    text: model.name
                }
            }

            onClicked: {
                playListView.currentIndex = index;
            }
            onDoubleClicked: {
                mediaComponent.mediaPlayer.play();
            }
        }
        header: Rectangle {
            color: "#80000000"
            height: 30
            width: parent.width

            RowLayout {
                anchors.left: parent.left
                anchors.margins: 10
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: 15

                Item {
                    Layout.fillWidth: true
                }
                Button {
                    id: stopButton

                    Layout.preferredHeight: 30
                    Layout.preferredWidth: 25
                    Material.roundedScale: Material.NotRounded
                    font.family: materialSymbolsOutlined.name
                    font.weight: Font.Light
                    scale: 1.5
                    text: "\ue12d"

                    onClicked: {
                        playList.clear();
                    }
                }
                Button {
                    id: collageButton

                    property bool isLoading: false

                    Layout.preferredHeight: 30
                    Layout.preferredWidth: 25
                    Material.roundedScale: Material.NotRounded
                    enabled: !collageButton.isLoading
                    font.family: materialSymbolsOutlined.name
                    font.weight: Font.Light
                    hoverEnabled: true
                    scale: 1.5
                    text: collageButton.isLoading ? "" : "\uefb2"

                    onClicked: {
                        collageButton.isLoading = true;
                        collage.toCollage(toPathList(playList));
                    }

                    // Loading spinner
                    BusyIndicator {
                        anchors.centerIn: parent
                        height: parent.height * 0.8
                        running: collageButton.isLoading
                        visible: collageButton.isLoading
                        width: parent.width * 0.8
                    }
                    ToolTip {
                        delay: 1000
                        text: collageButton.isLoading ? qsTr("Creating collages...") : qsTr("Create collages for all videos")
                        timeout: 5000
                        visible: collageButton.hovered
                    }
                    Connections {
                        function onCollageFinished(successCount, failCount) {
                            collageButton.isLoading = false;
                        }

                        target: collage
                    }
                }
            }
        }

        onCurrentIndexChanged: {
            if (currentIndex !== -1) {
                var item = playList.get(currentIndex);
                mediaComponent.path = item.path;
                mainWindow.title = "GAV - " + item.name;
                mediaComponent.mediaPlayer.play();
            }
        }
    }
}
