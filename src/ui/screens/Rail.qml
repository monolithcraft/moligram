import QtQuick
import QtQuick.Effects
import Moligram.Design
import Moligram.Controls

Rectangle {
    id: root

    property bool ghostMode
    property bool streamerMode

    signal ghostModeToggled
    signal streamerModeToggled
    signal themeToggled

    implicitWidth: 56
    color: Theme.surface.raised

    Image {
        anchors.top: parent.top
        anchors.topMargin: Theme.space.md
        anchors.horizontalCenter: parent.horizontalCenter
        width: 32
        height: 32
        sourceSize: Qt.size(width, height)
        source: "qrc:/moligram/branding/logo.png"
        opacity: Theme.dark ? 0.9 : 0.8
        layer.enabled: !Theme.dark
        layer.effect: MultiEffect {
            brightness: -1
        }
    }

    Column {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.space.md
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: Theme.space.sm

        IconButton {
            icon: Icons.ghost
            hint: qsTr("Ghost Mode")
            checked: root.ghostMode
            onClicked: root.ghostModeToggled()
        }

        IconButton {
            icon: Icons.broadcast
            hint: qsTr("Streamer Mode")
            checked: root.streamerMode
            onClicked: root.streamerModeToggled()
        }

        IconButton {
            icon: Theme.dark ? Icons.moon : Icons.sun
            hint: Theme.dark ? qsTr("Switch to light theme") : qsTr("Switch to dark theme")
            onClicked: root.themeToggled()
        }
    }

    Rectangle {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: 1
        color: Theme.line.subtle
    }
}
