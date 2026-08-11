pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Caelestia.Config
import qs.components
import qs.modules.nexus.common

PageBase {
    id: root

    title: qsTr("Plugins")

    ColumnLayout {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        width: root.cappedWidth
        spacing: Tokens.spacing.extraSmall / 2

        SectionHeader {
            first: true
            text: qsTr("Plugin support")
        }

        ConnectedRect {
            Layout.fillWidth: true
            first: true
            last: true
            implicitHeight: status.implicitHeight + Tokens.padding.largeIncreased * 2

            RowLayout {
                id: status

                anchors.fill: parent
                anchors.margins: Tokens.padding.largeIncreased
                spacing: Tokens.spacing.medium

                MaterialIcon {
                    Layout.alignment: Qt.AlignTop
                    text: "extension_off"
                    color: Colours.palette.m3onSurfaceVariant
                    fontStyle: Tokens.font.icon.large
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Tokens.spacing.extraSmall

                    StyledText {
                        Layout.fillWidth: true
                        text: qsTr("Plugins are not available yet")
                        font: Tokens.font.title.medium
                    }

                    StyledText {
                        Layout.fillWidth: true
                        text: qsTr("Caelestia does not currently load or manage plugins.")
                        color: Colours.palette.m3onSurfaceVariant
                        font: Tokens.font.body.small
                        wrapMode: Text.Wrap
                    }
                }
            }
        }
    }
}
