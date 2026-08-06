pragma Singleton

import QtQuick
import Quickshell
import Caelestia.Services

// Icons pulled straight out of a window's own _NET_WM_ICON (XWayland) for apps
// that have no resolvable desktop entry or themed icon — Minecraft, most Steam
// games, launcher-spawned windows and so on.
//
// This lives in a singleton rather than on the Dock because the Dock is
// instantiated more than once (one per bar/screen) and the popouts are built
// separately again: a per-Dock map meant whichever instance synced last won,
// and an instance that never ran the extractor would clobber the populated map
// with an empty one. A singleton gives every Dock tile and every hover popup
// the same map and the same "already tried" bookkeeping.
Singleton {
    id: root

    // key -> extracted png path
    property var paths: ({})

    // key -> extraction already attempted (don't re-spawn the helper)
    property var tried: ({})

    // Icons are tracked per window, not per class. Steam reports every Proton
    // title it cannot map to an appid as "steam_app_default", so a class-keyed
    // map served whichever of those games happened to be extracted first to all
    // the others. The pid is unique per running window and is what the plugin
    // matches on; the class is only a fallback for windows that have no pid.
    function keyFor(appClass: string, pid: int): string {
        return pid > 0 ? String(pid) : (appClass || "");
    }

    // Ask the plugin for the icon, at most once per window. The extraction is a
    // direct XGetWindowProperty read on _NET_WM_ICON — no helper process, and no
    // python-xlib/Pillow to have installed.
    function request(appClass: string, title: string, pid: int): void {
        const key = root.keyFor(appClass, pid ?? 0);
        if (!key || root.tried[key])
            return;

        const t = root.tried;
        t[key] = true;
        root.tried = t;

        const path = WindowIcon.extract(appClass || "", title || "", pid ?? 0);
        if (path)
            root.register(key, path);
    }

    // extract() returns the path directly; the signal carries the same result
    // for any caller that did not go through request().
    Connections {
        target: WindowIcon

        function onExtracted(key: string, path: string): void {
            root.register(key, path);
        }
    }

    // Record a freshly extracted icon. Reassigning a copy is what notifies the
    // bindings that read paths[...] — mutating in place would not.
    function register(key: string, path: string): void {
        if (!path || path === "" || root.paths[key] === path)
            return;
        const m = root.paths;
        m[key] = path;
        root.paths = Object.assign({}, m);
    }

    // Resolve an icon for a dock entry, in the order the taskbar tile and the
    // hover popup must agree on: desktop entry icon, then extracted window
    // icon, then the window class as a themed-icon name.
    function sourceFor(entry: var, appClass: string, iconName: string, pid: int): string {
        if (entry && entry.icon)
            return Quickshell.iconPath(entry.icon, "application-x-executable");
        const wp = root.paths[root.keyFor(appClass, pid ?? 0)];
        if (wp)
            return "file://" + wp;
        return Quickshell.iconPath(iconName, "application-x-executable");
    }
}
