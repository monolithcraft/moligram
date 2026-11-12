import QtQuick
import Moligram.Controls

Item {
    EmptyState {
        anchors.centerIn: parent
        title: qsTr("No chat selected")
        description: qsTr("Pick one on the left or press Ctrl K")
    }
}
