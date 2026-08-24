pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCore
import Quickshell
import Quickshell.Io
import qs.components
import qs.components.controls
import qs.services
import qs.modules.nexus.common

PageBase {
    id: root

    property bool enabled: false

    title: qsTr("Web Cursor")
    isSubPage: true

    Component.onCompleted: readEnabled.running = true

    Process {
        id: readEnabled

        command: ["kreadconfig6", "--file", "kwinrc", "--group", "Plugins", "--key", "ultralightwebcursorEnabled"]
        stdout: StdioCollector {
            id: readEnabledStdout
        }
        onExited: root.enabled = readEnabledStdout.text.trim() === "true"
    }

    ColumnLayout {
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        width: root.cappedWidth
        spacing: Tokens.spacing.extraSmall / 2

        ToggleRow {
            Layout.fillWidth: true
            first: true
            text: qsTr("Enable Web Cursor")
            subtext: qsTr("Experimental HTML/CSS animated cursor rendered by KWin")
            checked: root.enabled
            onToggled: {
                root.enabled = checked;
                Quickshell.execDetached([
                    "bash", "-c",
                    "kwriteconfig6 --file kwinrc --group Plugins --key ultralightwebcursorEnabled \"$1\" && qdbus6 org.kde.KWin /KWin reconfigure",
                    "--", checked ? "true" : "false"
                ]);
            }
        }

        SectionHeader {
            Layout.topMargin: Tokens.spacing.medium
            Layout.fillWidth: true
            text: qsTr("Experimental feature")
        }

        StyledText {
            Layout.fillWidth: true
            text: qsTr("Web Cursor is disabled by default. The first installation downloads the Ultralight SDK and enables the bundled Ciallo theme. Advanced theme, size and blacklist controls will be added to Nexus separately.")
            wrapMode: Text.WordWrap
            color: Colours.palette.m3onSurfaceVariant
        }
    }
}
