pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Caelestia.Config
import Caelestia.Services
import qs.components
import qs.components.controls
import qs.modules.nexus.common

PageBase {
    id: root
    title: qsTr("Web Cursor")
    isSubPage: true

    Item {
        FolderDialog {
            id: themeUploadDialog
            title: qsTr("Choose a cursor theme folder")
            onAccepted: {
                const path = selectedFolder.toLocalFile();
                const name = path.split("/").filter(p => p.length > 0).pop();
                WebCursorManager.uploadTheme(path, name);
            }
        }
    }

    ScrollView {
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            width: root.cappedWidth
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Tokens.spacing.extraSmall / 2

            ToggleRow {
                Layout.fillWidth: true
                first: true
                text: qsTr("Enable Web Cursor")
                subtext: qsTr("Render the selected HTML/CSS cursor through KWin")
                checked: Config.webCursor.cursor.enabled
                onToggled: checked ? WebCursorManager.enable() : WebCursorManager.disable()
            }

            SectionHeader { Layout.topMargin: Tokens.spacing.medium; Layout.fillWidth: true; text: qsTr("Cursor Theme") }

            ConnectedRect {
                Layout.fillWidth: true
                implicitHeight: themeHeader.implicitHeight + Tokens.padding.medium * 2
                RowLayout {
                    id: themeHeader
                    anchors.fill: parent
                    anchors.margins: Tokens.padding.medium
                    anchors.leftMargin: Tokens.padding.largeIncreased
                    anchors.rightMargin: Tokens.padding.largeIncreased
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        StyledText { text: qsTr("Current theme"); font: Tokens.font.body.small }
                        StyledText {
                            text: WebCursorManager.currentTheme.length > 0 ? WebCursorManager.currentTheme : qsTr("None selected")
                            color: Colours.palette.m3onSurfaceVariant
                            font: Tokens.font.label.small
                        }
                    }
                    IconButton { icon: "folder_open"; type: IconButton.Text; onClicked: themeUploadDialog.open() }
                    IconButton { icon: "refresh"; type: IconButton.Text; onClicked: WebCursorManager.reload() }
                }
            }

            StyledText {
                Layout.fillWidth: true
                Layout.topMargin: Tokens.spacing.small
                visible: WebCursorManager.themeList.length === 0
                text: qsTr("No themes installed yet. Use the folder icon above to add one.")
                color: Colours.palette.m3onSurfaceVariant
                font: Tokens.font.label.small
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }

            Repeater {
                model: WebCursorManager.themeList
                delegate: ConnectedRect {
                    id: themeCard
                    required property string modelData
                    readonly property var details: WebCursorManager.getThemeDetails(modelData)
                    readonly property bool isActive: WebCursorManager.currentTheme === modelData

                    Layout.fillWidth: true
                    implicitHeight: themeRow.implicitHeight + Tokens.padding.medium * 2
                    border.width: isActive ? 2 : 0
                    border.color: Colours.palette.m3primary

                    RowLayout {
                        id: themeRow
                        anchors.fill: parent
                        anchors.margins: Tokens.padding.medium
                        anchors.leftMargin: Tokens.padding.largeIncreased
                        anchors.rightMargin: Tokens.padding.largeIncreased
                        spacing: Tokens.spacing.medium

                        Rectangle {
                            Layout.preferredWidth: Tokens.padding.large * 2
                            Layout.preferredHeight: width
                            radius: Tokens.radius.medium
                            color: Colours.palette.m3surfaceVariant
                            clip: true
                            Image {
                                anchors.fill: parent
                                anchors.margins: 4
                                source: themeCard.details.iconPath
                                fillMode: Image.PreserveAspectFit
                                visible: status === Image.Ready
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            RowLayout {
                                spacing: Tokens.spacing.small
                                StyledText { text: themeCard.modelData; font: Tokens.font.body.medium }
                                Rectangle {
                                    visible: themeCard.isActive
                                    radius: height / 2
                                    color: Colours.palette.m3primaryContainer
                                    implicitWidth: activeLabel.implicitWidth + Tokens.padding.small * 2
                                    implicitHeight: activeLabel.implicitHeight + 4
                                    StyledText { id: activeLabel; anchors.centerIn: parent; text: qsTr("Active"); font: Tokens.font.label.small; color: Colours.palette.m3onPrimaryContainer }
                                }
                            }
                            StyledText { Layout.fillWidth: true; text: themeCard.details.describe || qsTr("No description provided"); color: Colours.palette.m3onSurfaceVariant; font: Tokens.font.label.small; elide: Text.ElideRight }
                            StyledText { text: qsTr("By %1 · minimum %2 × %3").arg(themeCard.details.author).arg(themeCard.details.minWidth).arg(themeCard.details.minHeight); color: Colours.palette.m3onSurfaceVariant; font: Tokens.font.label.small }
                        }

                        IconButton {
                            icon: themeCard.isActive ? "check" : "play_arrow"
                            type: IconButton.Text
                            onClicked: WebCursorManager.useTheme(themeCard.modelData)
                        }
                        IconButton {
                            icon: "folder_open"
                            type: IconButton.Text
                            onClicked: WebCursorManager.openThemeFolder(themeCard.modelData)
                        }
                        IconButton {
                            visible: WebCursorManager.isUserTheme(themeCard.modelData)
                            icon: "delete"
                            type: IconButton.Text
                            onClicked: WebCursorManager.removeTheme(themeCard.modelData)
                        }
                    }
                }
            }

            SectionHeader { Layout.topMargin: Tokens.spacing.medium; Layout.fillWidth: true; text: qsTr("Size") }
            StepperRow {
                Layout.fillWidth: true; first: true
                label: qsTr("Cursor width"); subtext: qsTr("Render width in pixels")
                from: 1; to: 1920; value: Config.webCursor.cursor.width
                onMoved: value => {
                    Config.webCursor.cursor.width = value;
                    GlobalConfig.webCursor.cursor.width = value;
                    WebCursorManager.save();
                }
            }
            StepperRow {
                Layout.fillWidth: true; last: true
                label: qsTr("Cursor height"); subtext: qsTr("Render height in pixels")
                from: 1; to: 1080; value: Config.webCursor.cursor.height
                onMoved: value => {
                    Config.webCursor.cursor.height = value;
                    GlobalConfig.webCursor.cursor.height = value;
                    WebCursorManager.save();
                }
            }

            SectionHeader { Layout.topMargin: Tokens.spacing.medium; Layout.fillWidth: true; text: qsTr("Ignored Applications") }
            ConnectedRect {
                Layout.fillWidth: true
                implicitHeight: blacklistColumn.implicitHeight + Tokens.padding.medium * 2
                ColumnLayout {
                    id: blacklistColumn
                    anchors.fill: parent
                    anchors.margins: Tokens.padding.medium
                    anchors.leftMargin: Tokens.padding.largeIncreased
                    anchors.rightMargin: Tokens.padding.largeIncreased
                    spacing: Tokens.spacing.small
                    Repeater {
                        model: Config.webCursor.cursor.blacklist
                        delegate: RowLayout {
                            required property string modelData
                            Layout.fillWidth: true
                            StyledText { Layout.fillWidth: true; text: modelData; font: Tokens.font.body.small }
                            IconButton { icon: "close"; type: IconButton.Text; onClicked: WebCursorManager.removeBlacklist(modelData) }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        StyledTextField {
                            id: blacklistInput
                            Layout.fillWidth: true
                            placeholderText: qsTr("Window class or application name")
                            onAccepted: { WebCursorManager.addBlacklist(text); clear(); }
                        }
                        IconButton { icon: "add"; type: IconButton.Text; onClicked: { WebCursorManager.addBlacklist(blacklistInput.text); blacklistInput.clear(); } }
                    }
                }
            }

            StyledText {
                Layout.fillWidth: true
                visible: WebCursorManager.statusMessage.length > 0
                text: WebCursorManager.statusMessage
                color: Colours.palette.m3onSurfaceVariant
                font: Tokens.font.label.small
                wrapMode: Text.WordWrap
            }
        }
    }
}
