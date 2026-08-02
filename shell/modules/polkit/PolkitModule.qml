pragma ComponentBehavior: Bound

import QtQuick
import Quickshell
import Quickshell.Services.Polkit 0.1
import Caelestia.Config
import qs.components

Scope {
    id: root

    PolkitAgent {
        id: agent
        // PolkitAgent handles dbus registration automatically.
    }

    PolkitDialog {
        agent: agent
        screen: {
            // Prefer a screen enabled in Caelestia's per-screen config;
            // fall back to the first screen in Wayland's enumeration order.
            const enabled = Quickshell.screens.filter(s => GlobalConfig.forScreen(s.name).enabled);
            return enabled.length > 0 ? enabled[0] : Quickshell.screens[0];
        }
    }
}
