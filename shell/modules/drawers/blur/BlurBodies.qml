import QtQuick
import Quickshell
import Caelestia.Config

Region {
    required property real bX
    required property real bY
    required property real bW
    required property real bH
    required property real inLeft
    required property real inRight
    required property real inTop
    required property real inBottom
    // These are big rectangles bluring most of the body

    Region {
        // Horizontal
        x: bX
        y: GlobalConfig.appearance.blurMask ? inTop : bY
        width: Math.max(0, bW)
        height: GlobalConfig.appearance.blurMask ? Math.max(0, inBottom - inTop) : Math.max(0, bH)
    }
    Region {
        // Vertical
        x: inLeft
        y: bY
        width: GlobalConfig.appearance.blurMask ? Math.max(0, inRight - inLeft) : 0
        height: GlobalConfig.appearance.blurMask ? Math.max(0, bH) : 0
    }
}
