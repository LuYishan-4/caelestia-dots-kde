pragma ComponentBehavior: Bound

import "modules/lock"
import QtQml
import Quickshell
import Caelestia.Config

ShellRoot {
    Variants {
        model: Quickshell.screens
        
        LockBackgroundWindow {
            required property var modelData

            screen: modelData
        }
    }
}
