import QtQuick
import Moligram.Design
import Moligram.Controls

Rectangle {
    id: root

    function focusSearch() {
        search.focusInput();
    }

    color: Theme.surface.raised

    SearchField {
        id: search

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: Theme.space.md
        shortcut: "Ctrl K"
    }

    Label {
        anchors.centerIn: parent
        width: parent.width - Theme.space.xl * 2
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        text: qsTr("Chats will appear here after you sign in")
        color: Theme.text.tertiary
        font.pixelSize: Theme.font.small
    }
}
