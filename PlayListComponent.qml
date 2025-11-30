import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    required property ListModel playList
    property alias playListView: playListView

    function toPathList(model) {
        var dataArray = []
        for (var i = 0; i < model.count; ++i) {
            dataArray.push(model.get(i).path)
        }
        return dataArray
    }

    ColumnLayout {
        anchors.centerIn: parent

        spacing: 5
        visible: playList.count === 0
        Text {
            text: "\uf523"
            font.family: materialSymbolsOutlined.name
            font.pixelSize: 80
            color: "white"
            Layout.alignment: Qt.AlignHCenter
            font.weight: Font.ExtraLight
        }
        Text {
            text: "Add video or audio files to play"
            font.pixelSize: 24
            color: "white"
            Layout.alignment: Qt.AlignHCenter
        }
    }

    ListView {
        id: playListView
        visible: playList.count > 0
        anchors.fill: parent
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        model: playList

        header: Rectangle {
            width: parent.width
            height: 30
            color: "#80000000"

            RowLayout {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: 10
                spacing: 15

                Item {
                    Layout.fillWidth: true
                }
                Button {
                    id: stopButton
                    text: "\ue12d"
                    font.family: materialSymbolsOutlined.name
                    font.weight: Font.Light
                    scale: 1.5
                    onClicked: {
                        playList.clear()
                    }
                    Material.roundedScale: Material.NotRounded
                    Layout.preferredWidth: 25
                    Layout.preferredHeight: 30
                }
                Button {
                    id: collageButton
                    text: "\uefb2"
                    font.family: materialSymbolsOutlined.name
                    font.weight: Font.Light
                    scale: 1.5
                    onClicked: {
                        collage.toCollage(toPathList(playList))
                    }
                    Material.roundedScale: Material.NotRounded
                    Layout.preferredWidth: 25
                    Layout.preferredHeight: 30
                }
            }
        }

        ScrollBar.vertical: ScrollBar {}

        onCurrentIndexChanged: {
            if (currentIndex !== -1) {
                var item = playList.get(currentIndex)
                mediaComponent.path = item.path
                mainWindow.title = "GAV - " + item.name
                mediaComponent.mediaPlayer.play()
            }
        }

        delegate: ItemDelegate {
            width: parent?.width
            height: 40
            padding: 8

            onDoubleClicked: {
                mediaComponent.mediaPlayer.play()
            }

            onClicked: {
                playListView.currentIndex = index
            }

            contentItem: Row {
                spacing: 12
                anchors.verticalCenter: parent.verticalCenter

                Text {
                    text: model.icon
                    font.family: materialSymbolsOutlined.name
                    font.pixelSize: 24
                    color: "white"
                }
                Text {
                    text: model.name
                    color: "white"
                    font.pixelSize: 14
                    elide: Text.ElideRight
                }
            }

            background: Rectangle {
                color: parent.down ? "#4a4a4e" : (parent.hovered ? "#2a2a2e" : (parent.ListView.isCurrentItem ? "#383838" : "transparent"))
                radius: 4
            }
        }
    }
}
