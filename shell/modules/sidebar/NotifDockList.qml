pragma ComponentBehavior: Bound

import QtQuick
import Quickshell
import Caelestia.Components
import Caelestia.Config
import qs.components
import qs.services

LazyListView {
    id: root

    required property Props props
    required property Flickable container
    required property DrawerVisibilities visibilities

    anchors.left: parent?.left
    anchors.right: parent?.right
    implicitHeight: contentHeight

    spacing: Tokens.spacing.small
    readyDelay: 1
    cacheBuffer: 400
    asynchronous: true

    onViewportAdjustNeeded: d => {
        if (contentYAnim.running)
            contentYAnim.complete();
        contentYAnim.to = Math.max(0, container.contentY + d);
        contentYAnim.start();
    }

    useCustomViewport: true
    viewport: Qt.rect(0, container.contentY, width, container.height)

    removeDuration: Tokens.anim.durations.normal

    model: ScriptModel {
        values: {
            const map = new Map();
            for (const n of Notifs.list.filter(n => !n.closed))
                map.set(n.appName, null);
            for (const n of Notifs.list)
                map.set(n.appName, null);
            return [...map.keys()];
        }
    }

    delegate: Component {
        MouseArea {
            id: notif

            required property int index
            required property string modelData

            readonly property bool closed: notifInner.notifCount === 0
            property int startY

            function closeAll(): void {
                clearTimer.start();
            }

            LazyListView.trackViewport: !notifInner.expanded && notifInner.nonAnimHeight < notifInner.implicitHeight
            LazyListView.preferredHeight: closed ? 0 : notifInner.nonAnimHeight
            LazyListView.visibleHeight: notifInner.implicitHeight
            implicitHeight: notifInner.implicitHeight

            opacity: LazyListView.removing || closed || LazyListView.adding ? 0 : 1
            scale: LazyListView.removing || closed ? 0.6 : LazyListView.adding ? 0 : 1

            hoverEnabled: true
            cursorShape: pressed ? Qt.ClosedHandCursor : undefined
            acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
            preventStealing: true
            enabled: !closed

            drag.target: this
            drag.axis: Drag.XAxis

            onPressed: event => {
                startY = event.y;
                if (event.button === Qt.RightButton)
                    notifInner.toggleExpand(!notifInner.expanded);
                else if (event.button === Qt.MiddleButton)
                    closeAll();
            }
            onPositionChanged: event => {
                if (pressed) {
                    const diffY = event.y - startY;
                    if (Math.abs(diffY) > Config.notifs.expandThreshold)
                        notifInner.toggleExpand(diffY > 0);
                }
            }
            onReleased: event => {
                if (Math.abs(x) < width * Config.notifs.clearThreshold)
                    x = 0;
                else
                    closeAll();
            }

            Timer {
                id: clearTimer

                // One-shot: detach-first mirrors Notifs.clear() to avoid the
                // O(n) self-removal inside NotifData.close() firing per item.
                interval: 15
                repeat: false
                triggeredOnStart: true
                onTriggered: {
                    // Collect targets, remove from list in one assignment,
                    // then close each \u2014 NotifData.close() skips its own filter
                    // path when the item is no longer in Notifs.list.
                    const toClose = Notifs.list.filter(n => !n.closed && n.appName === notif.modelData);
                    if (toClose.length === 0)
                        return;
                    Notifs.list = Notifs.list.filter(n => !toClose.includes(n));
                    Notifs.openCount = Math.max(0, Notifs.openCount - toClose.length);
                    for (const n of toClose) n.close();
                }
            }

            NotifGroup {
                id: notifInner

                modelData: notif.modelData
                props: root.props
                container: root.container
                visibilities: root.visibilities
            }

            Behavior on y {
                enabled: notif.LazyListView.ready

                Anim {}
            }

            Behavior on opacity {
                Anim {
                    type: Anim.DefaultEffects
                }
            }

            Behavior on scale {
                Anim {}
            }

            Behavior on x {
                Anim {}
            }
        }
    }

    Anim {
        id: contentYAnim

        target: root.container
        property: "contentY"
    }
}
