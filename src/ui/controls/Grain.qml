import QtQuick
import Moligram.Design

Image {
    source: "qrc:/moligram/textures/grain.png"
    fillMode: Image.Tile
    smooth: false
    opacity: Theme.dark ? 0.035 : 0.04
}
