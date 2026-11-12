import QtQuick
import Moligram.Design

Item {
    id: root

    required property real value
    property real minimum
    property real maximum

    signal resized(real width)

    width: 8

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        width: 1
        height: parent.height
        color: hover.hovered || drag.active ? Theme.line.strong : Theme.line.subtle

        Behavior on color {
            ColorAnimation {
                duration: Theme.motion.quick
            }
        }
    }

    HoverHandler {
        id: hover

        cursorShape: Qt.SplitHCursor
    }

    DragHandler {
        id: drag

        property real origin

        target: null
        yAxis.enabled: false
        cursorShape: Qt.SplitHCursor
        onActiveChanged: if (active) origin = root.value
        onTranslationChanged: {
            const width = origin + translation.x;
            root.resized(Math.max(root.minimum, Math.min(root.maximum, width)));
        }
    }
}
