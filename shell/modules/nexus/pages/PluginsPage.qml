pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Caelestia.Config
import qs.components
import qs.modules.nexus.common

PageBase {
    id: root

    readonly property color foreground: Colours.on(Colours.tPalette.m3surface)

    title: qsTr("Plugins")

    ColumnLayout {
        anchors.centerIn: parent
        spacing: Tokens.spacing.medium

        StyledText {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Plugins are coming soon")
            font: Tokens.font.headline.small
            animate: true
        }

        StyledText {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Stay tuned for the ability to extend your shell with community plugins.")
            font: Tokens.font.body.medium
            horizontalAlignment: Text.AlignHCenter
            animate: true
        }
    }
}
