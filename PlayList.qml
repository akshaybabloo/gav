import QtQuick
import QtQuick.Controls

Item {

    required property ListModel playList
    property alias playListView: playListView

    ListView {
        id: playListView
        anchors.fill: parent
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        visible: true // Hidden if video

        model: playList

        ScrollBar.vertical: ScrollBar {
        }

        onCurrentIndexChanged: {
            if (currentIndex !== -1) {
                var item = playList.get(currentIndex)
                mediaScreen.path = item.path
                mainWindow.title = "GAV - " + item.name
            }
        }

        delegate: ItemDelegate {
            width: parent.width
            height: 40
            padding: 8

            onDoubleClicked: {
                mediaScreen.player.play()
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
