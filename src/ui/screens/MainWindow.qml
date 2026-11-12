import QtQuick
import QtCore
import Moligram.Design
import Moligram.Controls

Window {
    id: window

    width: 1280
    height: 800
    minimumWidth: 720
    minimumHeight: 480
    visible: true
    title: "Moligram"
    color: Theme.surface.base

    Settings {
        id: preferences

        category: "interface"

        property int themeMode: Theme.Dark
        property bool ghostMode: false
        property bool streamerMode: false
        property real chatListWidth: 340
    }

    Binding {
        target: Theme
        property: "mode"
        value: preferences.themeMode
    }

    Shortcut {
        sequence: "Ctrl+K"
        onActivated: chatList.focusSearch()
    }

    Grain {
        anchors.fill: parent
    }

    Rail {
        id: rail

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        ghostMode: preferences.ghostMode
        streamerMode: preferences.streamerMode
        onGhostModeToggled: preferences.ghostMode = !preferences.ghostMode
        onStreamerModeToggled: preferences.streamerMode = !preferences.streamerMode
        onThemeToggled: preferences.themeMode = Theme.dark ? Theme.Light : Theme.Dark
    }

    ChatListPane {
        id: chatList

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: rail.right
        width: preferences.chatListWidth
    }

    PaneSplitter {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: chatList.right
        value: preferences.chatListWidth
        minimum: 280
        maximum: Math.min(420, window.width / 2)
        onResized: width => preferences.chatListWidth = width
    }

    ChatPane {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: chatList.right
        anchors.right: parent.right
    }
}
