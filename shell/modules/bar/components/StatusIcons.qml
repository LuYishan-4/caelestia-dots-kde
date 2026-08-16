pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtCore
import Quickshell
import Quickshell.Io
import Quickshell.Bluetooth
import Quickshell.Services.UPower
import Caelestia
import Caelestia.Config
import qs.components
import qs.services
import qs.utils

StyledRect {
    id: root

    property color colour: Colours.palette.m3secondary
    readonly property alias items: iconColumn

    readonly property bool isHorizontal: Config.bar.position === "top" || Config.bar.position === "bottom"
    readonly property real rawScale: !isNaN(Config.bar.scale) ? Config.bar.scale : 1.0
    readonly property real scaleFactor: rawScale < 1.0 ? Math.sqrt(Math.max(0.1, rawScale)) : rawScale
    readonly property int barThickness: Math.round(Tokens.sizes.bar.innerWidth * scaleFactor)
    readonly property int baseThickness: Tokens.sizes.bar.innerWidth
    readonly property int effectiveThickness: rawScale < 1.0 ? baseThickness : barThickness
    readonly property int iconSize: Math.round(effectiveThickness * 0.42)

    property real hoverPos: -1
    property real hoverExpansion: 30 // Fixed px amount to expand the tray area when hovered

    readonly property bool isHovering: hoverPos !== -1
    property real currentHoverExpansion: isHovering ? hoverExpansion : 0
    property bool isDragging: false

    property bool lockstatusActive: Config.bar.status.showLockStatus && (Hypr.capsLock || Hypr.numLock)
    property bool audioActive: Config.bar.status.showAudio
    property bool microphoneActive: Config.bar.status.showMicrophone
    property bool kblayoutActive: Config.bar.status.showKbLayout && (Hypr.kbLayout || "").length > 0
    property bool networkActive: Config.bar.status.showNetwork && (!Nmcli.activeEthernet || Config.bar.status.showWifi)
    property bool ethernetActive: Config.bar.status.showNetwork && Nmcli.activeEthernet
    property bool bluetoothActive: Config.bar.status.showBluetooth
    property bool batteryActive: Config.bar.status.showBattery
    property bool peripheralBatteryActive: Config.bar.status.showPeripheralBattery
    property bool nightlightActive: Config.bar.status.showNightLight && HyprSunset.active
    property bool notificationsActive: Config.bar.status.showNotifications
    property string iconsOrderStr: ""

    function syncModel() {
        if (root.isDragging) return;

        const defaultOrder = ["lockstatus", "microphone", "kblayout", "network", "ethernet", "bluetooth", "audio", "battery", "peripheralBattery", "nightlight", "notifications"];
        let savedOrder = root.iconsOrderStr ? root.iconsOrderStr.split(",") : [];
        if (savedOrder.length === 1 && savedOrder[0] === "") savedOrder = [];

        for (let i = 0; i < defaultOrder.length; i++) {
            if (!savedOrder.includes(defaultOrder[i])) {
                savedOrder.push(defaultOrder[i]);
            }
        }
        savedOrder = savedOrder.filter(name => defaultOrder.includes(name));

        const activeItems = savedOrder.filter(name => {
            switch (name) {
                case "lockstatus": return root.lockstatusActive;
                case "audio": return root.audioActive;
                case "microphone": return root.microphoneActive;
                case "kblayout": return root.kblayoutActive;
                case "network": return root.networkActive;
                case "ethernet": return root.ethernetActive;
                case "bluetooth": return root.bluetoothActive;
                case "battery": return root.batteryActive;
                case "peripheralBattery": return root.peripheralBatteryActive;
                case "nightlight": return root.nightlightActive;
                case "notifications": return root.notificationsActive;
                default: return false;
            }
        });

        iconModel.clear();
        for (let i = 0; i < activeItems.length; i++) {
            iconModel.append({ "itemName": activeItems[i] });
        }
    }

    function saveOrder() {
        const defaultOrder = ["lockstatus", "microphone", "kblayout", "network", "ethernet", "bluetooth", "audio", "battery", "peripheralBattery", "nightlight", "notifications"];

        let activeOrder = [];
        for (let i = 0; i < iconModel.count; i++) {
            activeOrder.push(iconModel.get(i).itemName);
        }

        let previousOrder = root.iconsOrderStr ? root.iconsOrderStr.split(",") : defaultOrder.slice();
        if (previousOrder.length === 1 && previousOrder[0] === "") previousOrder = defaultOrder.slice();

        // Find which slots in the full order were occupied by currently-active icons.
        // We'll place the new active order into those same slots, so inactive icons
        // stay at their original positions.
        const activeSet = new Set(activeOrder);
        const activePositions = [];
        for (let i = 0; i < previousOrder.length; i++) {
            if (activeSet.has(previousOrder[i])) {
                activePositions.push(i);
            }
        }

        const newOrder = previousOrder.slice();
        for (let i = 0; i < activePositions.length; i++) {
            newOrder[activePositions[i]] = activeOrder[i];
        }

        root.iconsOrderStr = newOrder.join(",");
        saveProcess.command = ["bash", "-c", "mkdir -p ~/.config/caelestia && printf '%s' '" + root.iconsOrderStr + "' > ~/.config/caelestia/status_icons_order.txt"];
        saveProcess.running = true;
    }

    color: Colours.tPalette.m3surfaceContainer
    radius: Tokens.rounding.full
    clip: true
    implicitWidth: isHorizontal ? (iconColumn.implicitWidth + Tokens.padding.medium * 2 + currentHoverExpansion) : barThickness
    implicitHeight: isHorizontal ? barThickness : (iconColumn.implicitHeight + Tokens.padding.medium * 2 + currentHoverExpansion)

    onLockstatusActiveChanged: syncModel()
    onAudioActiveChanged: syncModel()
    onMicrophoneActiveChanged: syncModel()
    onKblayoutActiveChanged: syncModel()
    onNetworkActiveChanged: syncModel()
    onEthernetActiveChanged: syncModel()
    onBluetoothActiveChanged: syncModel()
    onBatteryActiveChanged: syncModel()
    onPeripheralBatteryActiveChanged: syncModel()
    onNightlightActiveChanged: syncModel()
    onNotificationsActiveChanged: syncModel()

    onIconsOrderStrChanged: {
        if (iconsOrderStr.length > 0) {
            root.syncModel();
        }
    }

    Component.onCompleted: {
        loadProcess.running = true;
    }

    Behavior on currentHoverExpansion { Anim { type: Anim.DefaultEffects } }

    Process {
        id: loadProcess

        command: ["bash", "-c", "cat ~/.config/caelestia/status_icons_order.txt 2>/dev/null || true"]

        stdout: StdioCollector {
            id: loadStdout
        }

        onExited: {
            const outText = (loadStdout.text || "").trim();
            if (outText.length > 0) {
                root.iconsOrderStr = outText;
            } else {
                root.syncModel();
            }
        }
    }

    Process {
        id: saveProcess
    }

    ListModel {
        id: iconModel
    }

    GridLayout {
        id: iconColumn

        readonly property real spacing: isHorizontal ? columnSpacing : rowSpacing

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: isHorizontal ? undefined : parent.bottom
        anchors.bottomMargin: isHorizontal ? 0 : Tokens.padding.medium
        anchors.top: isHorizontal ? undefined : parent.top
        anchors.topMargin: Tokens.padding.medium
        anchors.leftMargin: isHorizontal ? Tokens.padding.medium : 0
        anchors.rightMargin: isHorizontal ? Tokens.padding.medium : 0
        anchors.verticalCenter: isHorizontal ? parent.verticalCenter : undefined

        columns: isHorizontal ? -1 : 1
        rows: isHorizontal ? 1 : -1
        flow: isHorizontal ? GridLayout.LeftToRight : GridLayout.TopToBottom

        columnSpacing: Tokens.spacing.medium / 2
        rowSpacing: Tokens.spacing.medium / 2

        Repeater {
            model: iconModel
            
            delegate: Item {
                id: delegateContainer

                required property string itemName
                required property int index

                property string name: itemName

                implicitWidth: loader.implicitWidth
                implicitHeight: loader.implicitHeight

                Layout.fillWidth: root.isHorizontal
                Layout.fillHeight: !root.isHorizontal
                Layout.alignment: isHorizontal ? Qt.AlignVCenter : Qt.AlignHCenter

                DropArea {
                    anchors.fill: parent
                    onEntered: drag => {
                        console.log("StatusIcons DropArea onEntered from:", drag.source.delegateIndex, "to:", delegateContainer.index);
                        const from = drag.source.delegateIndex;
                        const to = delegateContainer.index;
                        if (from !== undefined && to !== undefined && from !== to) {
                            iconModel.move(from, to, 1);
                        }
                    }
                    onDropped: drag => {
                        console.log("StatusIcons DropArea onDropped");
                        root.saveOrder();
                    }
                }
            
                Item {
                    id: dragItem

                    property int delegateIndex: delegateContainer.index

                    width: delegateContainer.width
                    height: delegateContainer.height
                    
                    Drag.active: dragArea.held
                    Drag.source: dragItem
                    Drag.hotSpot.x: width / 2
                    Drag.hotSpot.y: height / 2

                    states: [
                        State {
                            when: dragArea.held

                            ParentChange {
                                target: dragItem
                                parent: root
                            }
                            PropertyChanges {
                                target: dragItem
                                opacity: 0.8
                                z: 999
                            }
                        }
                    ]

                    Loader {
                        id: loader

                        anchors.centerIn: parent

                        sourceComponent: {
                            switch(itemName) {
                                case "lockstatus": return lockstatusComp;
                                case "audio": return audioComp;
                                case "microphone": return microphoneComp;
                                case "kblayout": return kblayoutComp;
                                case "network": return networkComp;
                                case "ethernet": return ethernetComp;
                                case "bluetooth": return bluetoothComp;
                                case "battery": return batteryComp;
                                case "peripheralBattery": return peripheralBatteryComp;
                                case "nightlight": return nightlightComp;
                                case "notifications": return notificationsComp;
                                default: return null;
                            }
                        }
                    }

                    MouseArea {
                        id: dragArea

                        property bool held: false

                        anchors.fill: parent
                        drag.target: held ? dragItem : null
                        drag.axis: root.isHorizontal ? Drag.XAxis : Drag.YAxis
                        cursorShape: Qt.PointingHandCursor
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                    
                        onPressed: mouse => {
                            console.log("StatusIcons Drag onPressed");
                            if (mouse.button === Qt.LeftButton) {
                                held = true;
                                root.isDragging = true;
                            }
                        }
                        onPositionChanged: mouse => {
                            if (held) {
                                console.log("StatusIcons Drag onPositionChanged, dragItem x:", dragItem.x, "y:", dragItem.y);
                            }
                        }
                        onReleased: mouse => {
                            console.log("StatusIcons Drag onReleased");
                            if (mouse.button === Qt.LeftButton) {
                                held = false;
                                root.isDragging = false;
                                dragItem.x = 0;
                                dragItem.y = 0;
                                root.saveOrder();
                            }
                        }
                        onCanceled: {
                            console.log("StatusIcons Drag onCanceled");
                            held = false;
                            root.isDragging = false;
                            dragItem.x = 0;
                            dragItem.y = 0;
                        }
                        onClicked: mouse => {
                            console.log("StatusIcons Drag onClicked");
                            if (itemName === "notifications") {
                                if (mouse.button === Qt.RightButton) {
                                    Notifs.dnd = !Notifs.dnd;
                                } else {
                                    const vis = Visibilities.getForActive();
                                    vis.sidebar = !vis.sidebar;
                                }
                            }
                        }
                    }
                }
            }
        }

        Component {
            id: lockstatusComp

            GridLayout {
                columns: root.isHorizontal ? -1 : 1
                rows: root.isHorizontal ? 1 : -1
                flow: root.isHorizontal ? GridLayout.LeftToRight : GridLayout.TopToBottom
                columnSpacing: 0
                rowSpacing: 0

                Item {
                    implicitWidth: root.isHorizontal ? (Hypr.capsLock ? capslockIcon.implicitWidth : 0) : capslockIcon.implicitWidth
                    implicitHeight: root.isHorizontal ? capslockIcon.implicitHeight : (Hypr.capsLock ? capslockIcon.implicitHeight : 0)

                MaterialIcon {
                    id: capslockIcon

                    anchors.centerIn: parent

                    scale: Hypr.capsLock ? 1 : 0.5
                    opacity: Hypr.capsLock ? 1 : 0

                    text: "keyboard_capslock_badge"
                    color: root.colour

                    Behavior on opacity {
                        Anim {
                            type: Anim.DefaultEffects
                        }
                    }

                    Behavior on scale {
                        Anim {}
                    }
                }

                Behavior on implicitHeight {
                    enabled: !root.isHorizontal

                    Anim {}
                }

                Behavior on implicitWidth {
                    enabled: root.isHorizontal

                    Anim {}
                }
            }

            Item {
                Layout.topMargin: !root.isHorizontal && Hypr.capsLock && Hypr.numLock ? Tokens.spacing.medium / 2 : 0
                Layout.leftMargin: root.isHorizontal && Hypr.capsLock && Hypr.numLock ? Tokens.spacing.medium / 2 : 0

                implicitWidth: root.isHorizontal ? (Hypr.numLock ? numlockIcon.implicitWidth : 0) : numlockIcon.implicitWidth
                implicitHeight: root.isHorizontal ? numlockIcon.implicitHeight : (Hypr.numLock ? numlockIcon.implicitHeight : 0)

                MaterialIcon {
                    id: numlockIcon

                    anchors.centerIn: parent

                    scale: Hypr.numLock ? 1 : 0.5
                    opacity: Hypr.numLock ? 1 : 0

                    text: "looks_one"
                    color: root.colour

                    Behavior on opacity {
                        Anim {
                            type: Anim.DefaultEffects
                        }
                    }

                    Behavior on scale {
                        Anim {}
                    }
                }

                Behavior on implicitHeight {
                    enabled: !root.isHorizontal

                    Anim {}
                }

                Behavior on implicitWidth {
                    enabled: root.isHorizontal

                    Anim {}
                }
            }
        }
    }

    Component {
        id: audioComp

        MaterialIcon {
            animate: true
            text: Icons.getVolumeIcon(Audio.volume, Audio.muted)
            color: root.colour
        }
    }

    Component {
        id: microphoneComp

        MaterialIcon {
            animate: true
            text: Icons.getMicVolumeIcon(Audio.sourceVolume, Audio.sourceMuted)
            color: root.colour
        }
    }

    Component {
        id: kblayoutComp

        StyledText {
            animate: true
            text: Hypr.kbLayout
            color: root.colour
            font: Tokens.font.mono.medium
        }
    }

    Component {
        id: networkComp

        MaterialIcon {
            animate: true
            text: Nmcli.active ? Icons.getNetworkIcon(Nmcli.active.strength ?? 0) : "wifi_off"
            color: root.colour
        }
    }

    Component {
        id: ethernetComp

        MaterialIcon {
            animate: true
            text: "cable"
            color: root.colour
        }
    }

    Component {
        id: bluetoothComp

        GridLayout {
            columns: root.isHorizontal ? -1 : 1
            rows: root.isHorizontal ? 1 : -1
            flow: root.isHorizontal ? GridLayout.LeftToRight : GridLayout.TopToBottom
            columnSpacing: Tokens.spacing.medium / 2
            rowSpacing: Tokens.spacing.medium / 2

            MaterialIcon {
                visible: !Bluetooth.defaultAdapter?.enabled || !Bluetooth.devices.values.some(d => d.state !== BluetoothDeviceState.Disconnected) // qmllint disable unresolved-type
                animate: true
                text: {
                    if (!Bluetooth.defaultAdapter?.enabled) // qmllint disable unresolved-type
                        return "bluetooth_disabled";
                    return "bluetooth";
                }
                color: root.colour
            }

            Repeater {
                model: ScriptModel {
                    values: Bluetooth.devices.values.filter(d => d.state !== BluetoothDeviceState.Disconnected) // qmllint disable unresolved-type
                }

                MaterialIcon {
                    id: device

                    required property BluetoothDevice modelData

                    animate: true
                    text: Icons.getBluetoothIcon(modelData?.icon)
                    color: root.colour
                    fill: 1

                    SequentialAnimation on opacity {
                        running: device.modelData?.state !== BluetoothDeviceState.Connected // qmllint disable unresolved-type
                        alwaysRunToEnd: true
                        loops: Animation.Infinite

                        Anim {
                            from: 1
                            to: 0
                            duration: Tokens.anim.durations.large
                            easing: Tokens.anim.standardAccel
                        }
                        Anim {
                            from: 0
                            to: 1
                            duration: Tokens.anim.durations.large
                            easing: Tokens.anim.standardDecel
                        }
                    }
                }
            }
        }
    }

    Component {
        id: batteryComp

        MaterialIcon {
            animate: true
            text: {
                if (!UPower.displayDevice.isLaptopBattery) {
                    if (PowerProfiles.profile === PowerProfile.PowerSaver)
                        return "energy_savings_leaf";
                    if (PowerProfiles.profile === PowerProfile.Performance)
                        return "rocket_launch";
                    return "balance";
                }
                return Icons.getBatteryIcon(UPower.displayDevice.percentage, [UPowerDeviceState.Charging, UPowerDeviceState.FullyCharged, UPowerDeviceState.PendingCharge].includes(UPower.displayDevice.state));
            }
            color: !UPower.onBattery || UPower.displayDevice.percentage > 0.2 ? root.colour : Colours.palette.m3error
            fill: 1
        }
    }

    Component {
        id: peripheralBatteryComp

        GridLayout {
            id: peripheralColumn

            readonly property var excluded: Config.bar.status.peripheralBatteryExcluded

            columns: root.isHorizontal ? -1 : 1
            rows: root.isHorizontal ? 1 : -1
            flow: root.isHorizontal ? GridLayout.LeftToRight : GridLayout.TopToBottom
            columnSpacing: Tokens.spacing.medium / 2
            rowSpacing: Tokens.spacing.medium / 2

            Repeater {
                model: ScriptModel {
                    values: UPower.devices.values.filter(d => !d.isLaptopBattery && d.type !== UPowerDeviceType.LinePower && d.isPresent && !peripheralColumn.excluded.some(e => e === d.model || e === d.nativePath)) // qmllint disable unresolved-type
                }

                MaterialIcon {
                    required property UPowerDevice modelData

                    animate: true
                    text: {
                        if (modelData.state === UPowerDeviceState.Charging || modelData.state === UPowerDeviceState.PendingCharge)
                            return "battery_charging_full";
                        if (modelData.state === UPowerDeviceState.FullyCharged)
                            return "battery_full";
                        return Icons.getBatteryIcon(modelData.percentage, false);
                    }
                    color: modelData.percentage > 0.2 ? root.colour : Colours.palette.m3error
                    fill: 1
                }
            }
        }
    }

    Component {
        id: nightlightComp

        MaterialIcon {
            animate: true
            text: "bedtime"
            color: root.colour
        }
    }

    Component {
        id: notificationsComp

        MaterialIcon {
            id: notifIcon

            text: {
                if (Notifs.dnd)
                    return "notifications_off";
                if (Notifs.openCount > 0)
                    return "notifications_unread";
                return "notifications";
            }
            color: root.colour
        }
    }
}
}
