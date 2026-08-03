import QtQuick
import Quickshell.Widgets
import Caelestia.Config
import qs.components
import qs.components.effects
import qs.services
import qs.utils

Item {
    id: root

    implicitWidth: Math.round(Tokens.font.body.large.pointSize * 1.2)
    implicitHeight: Math.round(Tokens.font.body.large.pointSize * 1.2)

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            const visibilities = Visibilities.getForActive();
            visibilities.launcher = !visibilities.launcher;
        }
    }

    Loader {
        asynchronous: true
        anchors.centerIn: parent
        sourceComponent: {
            if (SysInfo.isDefaultLogo) {
                return caelestiaLogo;
            } else if (GlobalConfig.general.logo && GlobalConfig.general.logo !== "caelestia") {
                return customIcon;
            } else {
                return distroIcon;
            }
        }
    }

    Component {
        id: caelestiaLogo

        Logo {
            implicitWidth: Math.round(Tokens.font.body.large.pointSize * 1.6)
            implicitHeight: Math.round(Tokens.font.body.large.pointSize * 1.6)
        }
    }

    Component {
        id: distroIcon

        ColouredIcon {
            source: SysInfo.osLogo
            implicitSize: Math.round(Tokens.font.body.large.pointSize * 1.2)
            colour: Colours.palette.m3tertiary
        }
    }

    Component {
        id: customIcon

        Loader {
            sourceComponent: SysInfo.recolourCustomLogo ? colouredIconComponent : iconImageComponent
        }
    }

    Component {
        id: colouredIconComponent

        ColouredIcon {
            source: SysInfo.osLogo
            implicitSize: Math.round(Tokens.font.body.large.pointSize * 1.2 * (SysInfo.customLogoSize / 100))
            colour: Colours.palette.m3tertiary
        }
    }

    Component {
        id: iconImageComponent

        IconImage {
            source: SysInfo.osLogo
            implicitWidth: Math.round(Tokens.font.body.large.pointSize * 1.2 * (SysInfo.customLogoSize / 100))
            implicitHeight: Math.round(Tokens.font.body.large.pointSize * 1.2 * (SysInfo.customLogoSize / 100))
        }
    }
}
