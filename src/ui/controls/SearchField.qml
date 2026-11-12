import QtQuick
import Moligram.Design

Rectangle {
    id: root

    property alias text: input.text
    property string placeholder: qsTr("Search")
    property string shortcut

    function focusInput() {
        input.forceActiveFocus();
        input.selectAll();
    }

    implicitHeight: 36
    radius: Theme.radius.md
    color: input.activeFocus ? Theme.surface.overlay : Theme.surface.hover
    border.width: 1
    border.color: input.activeFocus ? Theme.line.strong : "transparent"

    Behavior on color {
        ColorAnimation {
            duration: Theme.motion.quick
        }
    }

    Icon {
        id: glyph

        anchors.left: parent.left
        anchors.leftMargin: Theme.space.md
        anchors.verticalCenter: parent.verticalCenter
        path: Icons.search
        size: 16
        color: Theme.text.tertiary
    }

    TextInput {
        id: input

        anchors.left: glyph.right
        anchors.leftMargin: Theme.space.sm
        anchors.right: shortcutBadge.visible ? shortcutBadge.left : parent.right
        anchors.rightMargin: Theme.space.md
        anchors.verticalCenter: parent.verticalCenter
        clip: true
        color: Theme.text.primary
        selectionColor: Theme.surface.inverse
        selectedTextColor: Theme.text.inverse
        selectByMouse: true
        font.family: Theme.font.ui
        font.pixelSize: Theme.font.body
        renderType: Text.NativeRendering
        Keys.onEscapePressed: {
            text = "";
            focus = false;
        }

        Label {
            anchors.fill: parent
            text: root.placeholder
            color: Theme.text.tertiary
            visible: !input.text && !input.preeditText
        }
    }

    Rectangle {
        id: shortcutBadge

        anchors.right: parent.right
        anchors.rightMargin: Theme.space.sm
        anchors.verticalCenter: parent.verticalCenter
        width: shortcutLabel.implicitWidth + Theme.space.sm * 2
        height: 20
        radius: Theme.radius.sm
        color: "transparent"
        border.width: 1
        border.color: Theme.line.subtle
        visible: root.shortcut && !input.activeFocus

        Label {
            id: shortcutLabel

            anchors.centerIn: parent
            text: root.shortcut
            color: Theme.text.tertiary
            font.pixelSize: Theme.font.micro
        }
    }
}
