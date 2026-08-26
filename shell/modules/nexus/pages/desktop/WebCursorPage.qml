pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Caelestia.Config
import Caelestia.Services
import qs.components
import qs.components.controls
import qs.services
import qs.modules.nexus.common

PageBase {
    id: root
    title: qsTr("Web Cursor")
    isSubPage: true

    ScrollView {
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            width: root.cappedWidth
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Tokens.spacing.extraSmall / 2
            FolderDialog {
                id: themeUploadDialog
                title: qsTr("Choose a cursor theme folder")
                onAccepted: WebCursorManager.uploadTheme(
                    typeof selectedFolder === "string"
                        ? selectedFolder
                        : selectedFolder.toString().replace(/^file:\/\//, ""))
            }

            ToggleRow {
                Layout.fillWidth: true
                first: true
                text: qsTr("Enable Web Cursor")
                subtext: qsTr("Render the selected HTML/CSS cursor through KWin")
                checked: Config.webCursor.cursor.enabled
                onToggled: checked ? WebCursorManager.enable() : WebCursorManager.disable()
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: Tokens.spacing.medium
                spacing: Tokens.spacing.small

                SectionHeader { Layout.fillWidth: true; text: qsTr("Cursor Theme") }
                IconButton { icon: "add"; type: IconButton.Tonal; onClicked: themeUploadDialog.open() }
            }

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
                        StyledText { text: qsTr("Current theme"); font: Tokens.font.label.medium; color: Colours.palette.m3onSurfaceVariant }
                        StyledText { text: WebCursorManager.currentTheme; font: Tokens.font.body.large; elide: Text.ElideRight }
                    }
                    IconButton { icon: "refresh"; type: IconButton.Text; onClicked: WebCursorManager.reload() }
                }
            }

            Repeater {
                model: WebCursorManager.themeList
                delegate: ConnectedRect {
                    required property string modelData
                    readonly property var details: WebCursorManager.getThemeDetails(modelData)
                    Layout.fillWidth: true
                    implicitHeight: themeRow.implicitHeight + Tokens.padding.medium * 2
                    RowLayout {
                        id: themeRow
                        anchors.fill: parent
                        anchors.margins: Tokens.padding.medium
                        anchors.leftMargin: Tokens.padding.largeIncreased
                        anchors.rightMargin: Tokens.padding.largeIncreased
                        spacing: Tokens.spacing.medium
                        Image {
                            id: themeIcon
                            readonly property real baseSize: Tokens.padding.large * 3
                            readonly property real sourceRatio: sourceSize.height > 0
                                ? sourceSize.width / sourceSize.height : 1
                            readonly property real iconScale: Math.min(1, sourceRatio)
                            Layout.preferredWidth: baseSize * iconScale
                            Layout.preferredHeight: baseSize
                            Layout.alignment: Qt.AlignVCenter

                            sourceSize.width: baseSize * 2
                            sourceSize.height: baseSize * 2
                            source: details.iconPath || ""
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            visible: status === Image.Ready
                            cache: false
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Tokens.spacing.extraSmall / 2
                            StyledText { text: modelData; font: Tokens.font.body.medium; elide: Text.ElideRight }
                            StyledText { Layout.fillWidth: true; text: details.describe || qsTr("No description provided"); color: Colours.palette.m3onSurfaceVariant; font: Tokens.font.label.medium; elide: Text.ElideRight }
                            StyledText { text: qsTr("By %1 · minimum %2 × %3").arg(details.author).arg(details.minWidth).arg(details.minHeight); color: Colours.palette.m3onSurfaceVariant; font: Tokens.font.label.small }
                        }
                        IconButton { icon: WebCursorManager.currentTheme === modelData ? "check" : "play_arrow"; type: IconButton.Text; onClicked: WebCursorManager.useTheme(modelData) }
                        IconButton { icon: "folder_open"; type: IconButton.Text; onClicked: WebCursorManager.openThemeFolder(modelData) }
                        IconButton { visible: WebCursorManager.isUserTheme(modelData); icon: "delete"; type: IconButton.Text; onClicked: WebCursorManager.removeTheme(modelData) }
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
                font: Tokens.font.label.medium
                wrapMode: Text.WordWrap
            }
        }
    }
}
