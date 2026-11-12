import QtQuick
import QtQuick.Shapes
import Moligram.Design

Item {
    id: root

    property string path
    property color color: Theme.text.primary
    property real size: 20

    implicitWidth: size
    implicitHeight: size

    Shape {
        readonly property real grid: 20

        width: grid
        height: grid
        scale: root.size / grid
        transformOrigin: Item.TopLeft
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: root.color
            strokeWidth: 1.5
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin

            PathSvg {
                path: root.path
            }
        }
    }
}
