import QtQuick
import QtQuick.Layouts
import Quickshell
import Caelestia.Config
import qs.components
import qs.services
import qs.utils

Item {
    id: root

    required property var list
    required property var modelData

    function clicked() {
        if (!root.modelData || !root.modelData.action)
            return;
        root.list.visibilities.launcher = false;

        const isKDE = typeof KWinActiveWindowBridge !== "undefined";
        let actionStr = root.modelData.action;

        if (actionStr.startsWith("command(") && actionStr.endsWith(")")) {
            // Shell command wrapper — execute the command directly
            actionStr = actionStr.substring(8, actionStr.length - 1);
            Quickshell.execDetached(["sh", "-c", actionStr]);
        } else if (isKDE) {
            // On KDE the action field is a shortcut name registered with
            // kglobalaccel. The shortcut already responds to its key
            // combination natively — nothing to dispatch. Just dismiss
            // the launcher (already done above).
        } else {
            // On Hyprland the action field is a dispatcher command string.
            Quickshell.execDetached(["sh", "-c", "hyprctl dispatch " + actionStr]);
        }
    }

    implicitHeight: Tokens.sizes.launcher.itemHeight

    anchors.left: parent?.left
    anchors.right: parent?.right

    StateLayer {
        radius: Tokens.rounding.large
        onClicked: root.clicked()
    }

    Item {
        anchors.fill: parent
        anchors.leftMargin: Tokens.padding.medium
        anchors.rightMargin: Tokens.padding.medium
        anchors.margins: Tokens.padding.small

        MaterialIcon {
            id: icon

            text: "keyboard"
            fontStyle: Tokens.font.icon.builders.large.scale(1.3).build()

            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
        }

        ColumnLayout {
            anchors.left: icon.right
            anchors.leftMargin: Tokens.spacing.medium
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter

            spacing: 0

            StyledText {
                text: (modelData && modelData.bind) ? modelData.bind.replace(/\b[a-z]/g, l => l.toUpperCase()) : qsTr("No keybinds")
                font: Tokens.font.body.medium
                color: Colours.palette.m3onSurface
                elide: Text.ElideRight
            }

            StyledText {
                text: (modelData && modelData.description) ? Strings.localizeEnglishSpelling(modelData.description) : ((modelData && modelData.action) ? modelData.action : "")
                font: Tokens.font.body.small
                color: Colours.palette.m3onSurfaceVariant
                elide: Text.ElideRight
            }
        }
    }
}
