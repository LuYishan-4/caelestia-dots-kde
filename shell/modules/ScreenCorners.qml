import QtQuick
import Quickshell
import Caelestia.Config
import Caelestia.Services

// Keeps KWin's hold on the four screen corners in sync with which ones the
// overview wants. Bindings, not one-shot writes on toggle, so a corner is
// taken at startup too and given back the moment it stops being wanted —
// ScreenEdges itself handles putting KWin's config back exactly as it found
// it, including after a crash.
Scope {
    id: root

    readonly property bool overviewOn: Config.overview.enabled
    readonly property bool wantTopLeft: overviewOn && Config.overview.hoverTopLeft
    readonly property bool wantTopRight: overviewOn && Config.overview.hoverTopRight
    readonly property bool wantBottomLeft: overviewOn && Config.overview.hoverBottomLeft
    readonly property bool wantBottomRight: overviewOn && Config.overview.hoverBottomRight

    function sync(corner: int, wanted: bool): void {
        if (wanted)
            ScreenEdges.claim(corner);
        else
            ScreenEdges.release(corner);
    }

    onWantTopLeftChanged: sync(ScreenEdges.TopLeft, wantTopLeft)
    onWantTopRightChanged: sync(ScreenEdges.TopRight, wantTopRight)
    onWantBottomLeftChanged: sync(ScreenEdges.BottomLeft, wantBottomLeft)
    onWantBottomRightChanged: sync(ScreenEdges.BottomRight, wantBottomRight)

    Component.onCompleted: {
        sync(ScreenEdges.TopLeft, wantTopLeft);
        sync(ScreenEdges.TopRight, wantTopRight);
        sync(ScreenEdges.BottomLeft, wantBottomLeft);
        sync(ScreenEdges.BottomRight, wantBottomRight);
    }
}
