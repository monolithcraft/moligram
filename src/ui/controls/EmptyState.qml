import QtQuick
import QtQuick.Effects
import Moligram.Design

Column {
    id: root

    property alias title: heading.text
    property alias description: details.text

    spacing: Theme.space.md

    Image {
        anchors.horizontalCenter: parent.horizontalCenter
        width: 160
        height: 160
        sourceSize: Qt.size(width, height)
        source: "qrc:/moligram/branding/logo.png"
        opacity: Theme.dark ? 0.1 : 0.14
        layer.enabled: !Theme.dark
        layer.effect: MultiEffect {
            brightness: -1
        }
    }

    Label {
        id: heading

        anchors.horizontalCenter: parent.horizontalCenter
        font.pixelSize: Theme.font.title
        font.weight: Font.DemiBold
    }

    Label {
        id: details

        width: Math.min(implicitWidth, 320)
        anchors.horizontalCenter: parent.horizontalCenter
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        color: Theme.text.secondary
        font.pixelSize: Theme.font.small
    }
}
