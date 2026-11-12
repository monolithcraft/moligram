import QtQuick
import Moligram.Design

Rectangle {
    id: root

    property alias icon: glyph.path
    property string hint
    property bool checked: false

    signal clicked

    implicitWidth: 36
    implicitHeight: 36
    radius: Theme.radius.md
    color: checked ? Theme.surface.inverse
         : tap.pressed ? Theme.surface.pressed
         : hover.hovered ? Theme.surface.hover
         : "transparent"
    border.width: 1
    border.color: activeFocus ? Theme.line.focus : "transparent"

    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: hint
    Accessible.checked: checked
    Keys.onSpacePressed: clicked()
    Keys.onReturnPressed: clicked()

    Behavior on color {
        ColorAnimation {
            duration: Theme.motion.quick
        }
    }

    Icon {
        id: glyph

        anchors.centerIn: parent
        color: root.checked ? Theme.text.inverse
             : hover.hovered ? Theme.text.primary
             : Theme.text.secondary
    }

    HoverHandler {
        id: hover

        cursorShape: Qt.PointingHandCursor
    }

    TapHandler {
        id: tap

        onTapped: root.clicked()
    }
}
