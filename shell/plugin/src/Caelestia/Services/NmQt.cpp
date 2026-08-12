// SPDX-License-Identifier: GPL-3.0-only
#include "NmQt.hpp"

#include <NetworkManagerQt/Manager>
#include <NetworkManagerQt/Device>
#include <NetworkManagerQt/WirelessDevice>
#include <NetworkManagerQt/AccessPoint>
#include <NetworkManagerQt/WirelessNetwork>
#include <NetworkManagerQt/Connection>
#include <NetworkManagerQt/ConnectionSettings>
#include <NetworkManagerQt/Settings>
#include <NetworkManagerQt/ActiveConnection>
#include <NetworkManagerQt/WirelessSetting>
#include <NetworkManagerQt/WirelessSecuritySetting>

#include <QDBusConnection>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QJSEngine>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcNmQt, "caelestia.services.nmqt", QtInfoMsg)

namespace caelestia::services {

// ─────────────────────────────────────────────────────────────────────────────
//  Construction / destruction
// ─────────────────────────────────────────────────────────────────────────────

NmQt::NmQt(QObject* parent)
    : QObject(parent) {

    auto* notifier = NetworkManager::notifier();
    if (!notifier) {
        qCWarning(lcNmQt) << "NetworkManager notifier unavailable — is NetworkManager running?";
        return;
    }

    // ── Wireless enable state ────────────────────────────────────────
    connect(notifier, &NetworkManager::Notifier::wirelessEnabledChanged,
            this, &NmQt::onWirelessEnabledChanged);
    connect(notifier, &NetworkManager::Notifier::wirelessHardwareEnabledChanged,
            this, &NmQt::onWirelessHardwareEnabledChanged);

    // ── Device list changes ──────────────────────────────────────────
    connect(notifier, &NetworkManager::Notifier::deviceAdded,
            this, &NmQt::onNetworkDevicesChanged);
    connect(notifier, &NetworkManager::Notifier::deviceRemoved,
            this, &NmQt::onNetworkDevicesChanged);

    // ── Active connections ───────────────────────────────────────────
    connect(notifier, &NetworkManager::Notifier::activeConnectionsChanged,
            this, &NmQt::onActiveConnectionsChanged);

    // ── Connection list (saved profiles) ─────────────────────────────
    connect(notifier, &NetworkManager::Notifier::connectionAdded,
            this, &NmQt::onConnectionsChanged);
    connect(notifier, &NetworkManager::Notifier::connectionRemoved,
            this, &NmQt::onConnectionsChanged);

    // ── Access-point tracking per wireless device ────────────────────
    connect(notifier, &NetworkManager::Notifier::accessPointAdded,
            this, &NmQt::onAccessPointAppeared);
    connect(notifier, &NetworkManager::Notifier::accessPointRemoved,
            this, &NmQt::onAccessPointDisappeared);

    // ── Read initial state from NetworkManagerQt caches ──────────────
    m_wifiEnabled = NetworkManager::isWirelessEnabled();
    refreshDevices();
    refreshSavedConnections();
    refreshVpnConnections();
    refreshWirelessDeviceDetails();
    refreshEthernetDeviceDetails();

    m_initialised = true;

    qCInfo(lcNmQt) << "NmQt initialised (NetworkManagerQt D-Bus backend)";
}

NmQt::~NmQt() = default;

// ─────────────────────────────────────────────────────────────────────────────
//  Property accessors
// ─────────────────────────────────────────────────────────────────────────────

bool NmQt::isConnected() const {
    return NetworkManager::status() == NetworkManager::Status::Connected
        || NetworkManager::status() == NetworkManager::Status::ConnectedLinkLocal
        || NetworkManager::status() == NetworkManager::Status::ConnectedSiteOnly;
}

bool NmQt::wifiEnabled() const { return m_wifiEnabled; }
bool NmQt::scanning() const { return m_scanning; }
QString NmQt::connectingSsid() const { return m_connectingSsid; }

QVariantList NmQt::networks() const { return m_networks; }
QVariantMap NmQt::active() const { return m_active; }

QStringList NmQt::savedConnections() const { return m_savedConnections; }
QStringList NmQt::savedConnectionSsids() const { return m_savedConnectionSsids; }

QVariantMap NmQt::activeEthernet() const { return m_activeEthernet; }
QVariantList NmQt::ethernetDevices() const { return m_ethernetDevices; }

QVariantList NmQt::vpnConnections() const { return m_vpnConnections; }
QVariantMap NmQt::activeVpn() const { return m_activeVpn; }
QString NmQt::vpnPendingConnection() const { return m_vpnPendingConnection; }

QVariantMap NmQt::wirelessDeviceDetails() const { return m_wirelessDeviceDetails; }
QVariantMap NmQt::ethernetDeviceDetails() const { return m_ethernetDeviceDetails; }

// ─────────────────────────────────────────────────────────────────────────────
//  QML-invokable actions
// ─────────────────────────────────────────────────────────────────────────────

void NmQt::getNetworks(QJSValue callback) {
    refreshNetworks();
    if (callback.isCallable()) {
        auto arr = qjsEngine(this)->toScriptValue(m_networks);
        callback.call({arr});
    }
}

void NmQt::connectToNetwork(const QString& ssid, const QString& password,
                             const QString& bssid, QJSValue callback) {
    // Locate the wireless device
    NetworkManager::WirelessDevice::Ptr wifiDev;
    for (const auto& uni : NetworkManager::networkInterfaces()) {
        auto dev = NetworkManager::findNetworkInterface(uni);
        auto wd = dev.dynamicCast<NetworkManager::WirelessDevice>();
        if (wd) {
            wifiDev = wd;
            break;
        }
    }

    if (!wifiDev) {
        qCWarning(lcNmQt) << "connectToNetwork: no wireless device found";
        invokeCallback(callback, false, {}, "No wireless device", -1);
        return;
    }

    // Look for a saved connection matching this SSID
    NetworkManager::Connection::Ptr existingConn;
    {
        const auto connPaths = NetworkManager::listConnections();
        for (const auto& path : connPaths) {
            auto conn = NetworkManager::findConnection(path);
            if (!conn || !conn->settings())
                continue;
            auto ws = conn->settings()->setting(NetworkManager::Setting::SettingType::Wireless);
            if (!ws)
                continue;
            auto* wirelessSetting = static_cast<NetworkManager::WirelessSetting*>(ws.data());
            if (wirelessSetting->ssid() == ssid) {
                existingConn = conn;
                break;
            }
        }
    }

    if (existingConn) {
        // Activate existing connection
        m_connectingSsid = ssid;
        emit connectingSsidChanged();

        QDBusPendingReply<QDBusObjectPath> reply =
            NetworkManager::activateConnection(existingConn->path(),
                                                wifiDev->uni(),
                                                QString());
        auto* watcher = new QDBusPendingCallWatcher(reply, this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this,
                [this, ssid, callback](QDBusPendingCallWatcher* w) {
            w->deleteLater();
            QDBusPendingReply<QDBusObjectPath> r = *w;
            if (r.isError()) {
                qCWarning(lcNmQt) << "activateConnection failed:" << r.error().message();
                m_connectingSsid.clear();
                emit connectingSsidChanged();
                invokeCallback(callback, false, {}, r.error().message(), -1);
            } else {
                invokeCallback(callback, true, "Connection activated");
            }
        });
        return;
    }

    if (password.isEmpty()) {
        // No saved connection and no password — needs one
        qCInfo(lcNmQt) << "connectToNetwork:" << ssid << "needs password";
        m_connectingSsid.clear();
        emit connectingSsidChanged();
        invokeCallback(callback, false, {}, "Secrets were required, but not provided",
                       -1, true);
        return;
    }

    // Create a new connection with the provided password
    NMStringMap connParams;
    connParams["type"] = "802-11-wireless";
    connParams["con-name"] = ssid;
    connParams["ssid"] = ssid;

    if (!bssid.isEmpty()) {
        connParams["802-11-wireless.bssid"] = bssid;
    }

    QDBusPendingReply<QDBusObjectPath, QDBusObjectPath> reply =
        NetworkManager::addAndActivateConnection(connParams,
                                                  wifiDev->uni(),
                                                  QString());
    m_connectingSsid = ssid;
    emit connectingSsidChanged();

    auto* watcher = new QDBusPendingCallWatcher(reply, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, ssid, callback](QDBusPendingCallWatcher* w) {
        w->deleteLater();
        QDBusPendingReply<QDBusObjectPath, QDBusObjectPath> r = *w;
        if (r.isError()) {
            qCWarning(lcNmQt) << "addAndActivateConnection failed:" << r.error().message();
            m_connectingSsid.clear();
            emit connectingSsidChanged();
            const auto& errMsg = r.error().message();
            bool needsPw = errMsg.contains("Secrets")
                        || errMsg.contains("password")
                        || errMsg.contains("802-11-wireless-security");
            invokeCallback(callback, false, {}, errMsg, -1, needsPw);
        } else {
            invokeCallback(callback, true, "Connection created and activated");
        }
    });
}

void NmQt::connectToNetworkWithPasswordCheck(const QString& ssid, bool isSecure,
                                              QJSValue callback, const QString& bssid) {
    if (!isSecure) {
        connectToNetwork(ssid, QString(), bssid, callback);
        return;
    }

    // Try with saved password first
    NetworkManager::Connection::Ptr savedConn;
    {
        const auto connPaths = NetworkManager::listConnections();
        for (const auto& path : connPaths) {
            auto conn = NetworkManager::findConnection(path);
            if (!conn || !conn->settings())
                continue;
            auto ws = conn->settings()->setting(NetworkManager::Setting::SettingType::Wireless);
            if (!ws)
                continue;
            auto* wirelessSetting = static_cast<NetworkManager::WirelessSetting*>(ws.data());
            if (wirelessSetting->ssid() == ssid) {
                savedConn = conn;
                break;
            }
        }
    }

    if (savedConn) {
        // Has a saved profile — try activating it; NM will use stored secrets
        connectToNetwork(ssid, QString(), bssid, callback);
    } else {
        // No saved profile — caller needs to provide password
        invokeCallback(callback, false, {}, "Secrets were required, but not provided",
                       -1, true);
    }
}

void NmQt::disconnectFromNetwork() {
    // Find any active wireless connection and deactivate it
    const auto activeConns = NetworkManager::activeConnections();
    for (const auto& path : activeConns) {
        NetworkManager::ActiveConnection::Ptr ac =
            NetworkManager::findActiveConnection(path);
        if (!ac)
            continue;

        auto devs = NetworkManager::devices();
        for (const auto& dev : devs) {
            auto wd = dev.dynamicCast<NetworkManager::WirelessDevice>();
            if (wd && !wd->activeAccessPoint().isNull()) {
                NetworkManager::deactivateConnection(path);
                return;
            }
        }
    }

    // Fallback: deactivate the wireless device
    NetworkManager::WirelessDevice::Ptr wifiDev;
    for (const auto& uni : NetworkManager::networkInterfaces()) {
        auto dev = NetworkManager::findNetworkInterface(uni);
        auto wd = dev.dynamicCast<NetworkManager::WirelessDevice>();
        if (wd) {
            wd->disconnectInterface();
            break;
        }
    }
}

void NmQt::forgetNetwork(const QString& ssid, QJSValue callback) {
    const auto connPaths = NetworkManager::listConnections();
    for (const auto& path : connPaths) {
        auto conn = NetworkManager::findConnection(path);
        if (!conn || !conn->settings())
            continue;

        auto ws = conn->settings()->setting(NetworkManager::Setting::SettingType::Wireless);
        if (!ws)
            continue;

        auto* wirelessSetting = static_cast<NetworkManager::WirelessSetting*>(ws.data());
        if (wirelessSetting->ssid() == ssid) {
            QDBusPendingReply<> reply = conn->remove();
            auto* watcher = new QDBusPendingCallWatcher(reply, this);
            connect(watcher, &QDBusPendingCallWatcher::finished, this,
                    [this, callback](QDBusPendingCallWatcher* w) {
                w->deleteLater();
                QDBusPendingReply<> r = *w;
                invokeCallback(callback, !r.isError(),
                               r.isError() ? QString() : QStringLiteral("Deleted"),
                               r.isError() ? r.error().message() : QString());
                refreshSavedConnections();
            });
            return;
        }
    }

    invokeCallback(callback, false, {}, "No connection found for SSID", -1);
}

void NmQt::enableWifi(bool enabled, QJSValue callback) {
    QDBusPendingReply<> reply = NetworkManager::setWirelessEnabled(enabled);
    auto* watcher = new QDBusPendingCallWatcher(reply, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, callback](QDBusPendingCallWatcher* w) {
        w->deleteLater();
        QDBusPendingReply<> r = *w;
        invokeCallback(callback, !r.isError(),
                       r.isError() ? QString() : QStringLiteral("OK"),
                       r.isError() ? r.error().message() : QString());
    });
}

void NmQt::toggleWifi(QJSValue callback) {
    enableWifi(!m_wifiEnabled, callback);
}

void NmQt::rescanWifi() {
    if (m_scanning) {
        qCInfo(lcNmQt) << "rescanWifi: already scanning, request queued";
        return;
    }

    NetworkManager::WirelessDevice::Ptr wifiDev;
    for (const auto& uni : NetworkManager::networkInterfaces()) {
        auto dev = NetworkManager::findNetworkInterface(uni);
        auto wd = dev.dynamicCast<NetworkManager::WirelessDevice>();
        if (wd) {
            wifiDev = wd;
            m_wirelessDeviceUni = uni;
            break;
        }
    }

    if (!wifiDev) {
        qCWarning(lcNmQt) << "rescanWifi: no wireless device found";
        return;
    }

    m_scanning = true;
    emit scanningChanged();

    // Wire up scan-finished signal once
    connect(wifiDev.data(), &NetworkManager::WirelessDevice::scanFinished,
            this, &NmQt::onScanFinished,
            Qt::UniqueConnection);

    wifiDev->requestScan();
}

void NmQt::connectEthernet(const QString& connectionName, const QString& interfaceName,
                            QJSValue callback) {
    if (!connectionName.isEmpty()) {
        const auto connPaths = NetworkManager::listConnections();
        for (const auto& path : connPaths) {
            auto conn = NetworkManager::findConnection(path);
            if (conn && conn->name() == connectionName) {
                QDBusPendingReply<QDBusObjectPath> reply =
                    NetworkManager::activateConnection(path, interfaceName, QString());
                auto* watcher = new QDBusPendingCallWatcher(reply, this);
                connect(watcher, &QDBusPendingCallWatcher::finished, this,
                        [this, callback](QDBusPendingCallWatcher* w) {
                    w->deleteLater();
                    QDBusPendingReply<QDBusObjectPath> r = *w;
                    invokeCallback(callback, !r.isError(),
                                   r.isError() ? QString() : QStringLiteral("Connected"),
                                   r.isError() ? r.error().message() : QString());
                    refreshEthernetDevices();
                });
                return;
            }
        }
    }

    if (!interfaceName.isEmpty()) {
        // Just connect the interface directly
        auto dev = NetworkManager::findNetworkInterface(interfaceName);
        if (dev) {
            QDBusPendingReply<QDBusObjectPath> reply = dev->connect();
            auto* watcher = new QDBusPendingCallWatcher(reply, this);
            connect(watcher, &QDBusPendingCallWatcher::finished, this,
                    [this, callback](QDBusPendingCallWatcher* w) {
                w->deleteLater();
                QDBusPendingReply<QDBusObjectPath> r = *w;
                invokeCallback(callback, !r.isError(),
                               r.isError() ? QString() : QStringLiteral("Connected"),
                               r.isError() ? r.error().message() : QString());
                refreshEthernetDevices();
            });
            return;
        }
    }

    invokeCallback(callback, false, {}, "No connection name or interface specified", -1);
}

void NmQt::disconnectEthernet(const QString& connectionName, QJSValue callback) {
    if (connectionName.isEmpty()) {
        invokeCallback(callback, false, {}, "No connection name specified", -1);
        return;
    }

    const auto activeConns = NetworkManager::activeConnections();
    for (const auto& path : activeConns) {
        NetworkManager::ActiveConnection::Ptr ac =
            NetworkManager::findActiveConnection(path);
        if (!ac)
            continue;
        if (ac->id() == connectionName || ac->uuid() == connectionName) {
            NetworkManager::deactivateConnection(path);
            invokeCallback(callback, true, "Disconnected");
            refreshEthernetDevices();
            return;
        }
    }

    invokeCallback(callback, false, {}, "Connection not active", -1);
}

void NmQt::connectVpn(const QString& connectionName, QJSValue callback) {
    if (connectionName.isEmpty()) {
        invokeCallback(callback, false, {}, "No VPN connection name specified", -1);
        return;
    }

    m_vpnPendingConnection = connectionName;
    emit vpnPendingConnectionChanged();

    const auto connPaths = NetworkManager::listConnections();
    for (const auto& path : connPaths) {
        auto conn = NetworkManager::findConnection(path);
        if (conn && (conn->name() == connectionName || conn->uuid() == connectionName)) {
            QDBusPendingReply<QDBusObjectPath> reply =
                NetworkManager::activateConnection(path, QString(), QString());
            auto* watcher = new QDBusPendingCallWatcher(reply, this);
            connect(watcher, &QDBusPendingCallWatcher::finished, this,
                    [this, connectionName, callback](QDBusPendingCallWatcher* w) {
                w->deleteLater();
                QDBusPendingReply<QDBusObjectPath> r = *w;
                invokeCallback(callback, !r.isError(),
                               r.isError() ? QString() : QStringLiteral("Connected"),
                               r.isError() ? r.error().message() : QString());
                if (m_vpnPendingConnection == connectionName) {
                    m_vpnPendingConnection.clear();
                    emit vpnPendingConnectionChanged();
                }
                refreshVpnConnections();
            });
            return;
        }
    }

    invokeCallback(callback, false, {}, "VPN connection not found", -1);
    m_vpnPendingConnection.clear();
    emit vpnPendingConnectionChanged();
}

void NmQt::disconnectVpn(const QString& connectionName, QJSValue callback) {
    if (connectionName.isEmpty()) {
        invokeCallback(callback, false, {}, "No VPN connection name specified", -1);
        return;
    }

    m_vpnPendingConnection = connectionName;
    emit vpnPendingConnectionChanged();

    const auto activeConns = NetworkManager::activeConnections();
    for (const auto& path : activeConns) {
        NetworkManager::ActiveConnection::Ptr ac =
            NetworkManager::findActiveConnection(path);
        if (ac && (ac->id() == connectionName || ac->uuid() == connectionName)) {
            NetworkManager::deactivateConnection(path);
            invokeCallback(callback, true, "Disconnected");
            if (m_vpnPendingConnection == connectionName) {
                m_vpnPendingConnection.clear();
                emit vpnPendingConnectionChanged();
            }
            refreshVpnConnections();
            return;
        }
    }

    invokeCallback(callback, false, {}, "VPN connection not active", -1);
    m_vpnPendingConnection.clear();
    emit vpnPendingConnectionChanged();
}

void NmQt::loadSavedConnections(QJSValue callback) {
    refreshSavedConnections();
    if (callback.isCallable()) {
        auto arr = qjsEngine(this)->toScriptValue(m_savedConnectionSsids);
        callback.call({arr});
    }
}

void NmQt::loadVpnConnections(QJSValue callback) {
    refreshVpnConnections();
    if (callback.isCallable()) {
        auto arr = qjsEngine(this)->toScriptValue(m_vpnConnections);
        callback.call({arr});
    }
}

bool NmQt::hasSavedProfile(const QString& ssid) const {
    if (ssid.isEmpty())
        return false;

    // Check if currently connected to this SSID
    if (!m_active.isEmpty() && m_active.value("ssid").toString() == ssid)
        return true;

    // Check saved SSID list
    const auto ssidLower = ssid.toLower().trimmed();
    for (const auto& saved : m_savedConnectionSsids) {
        if (saved.toLower().trimmed() == ssidLower)
            return true;
    }

    // Check saved connection names
    for (const auto& conn : m_savedConnections) {
        if (conn.toLower().trimmed() == ssidLower)
            return true;
    }

    return false;
}

void NmQt::getWirelessDeviceDetails(const QString& interfaceName, QJSValue callback) {
    refreshWirelessDeviceDetails();
    if (callback.isCallable()) {
        auto engine = qjsEngine(this);
        auto obj = engine->toScriptValue(m_wirelessDeviceDetails);
        callback.call({obj});
    }
}

void NmQt::getEthernetDeviceDetails(const QString& interfaceName, QJSValue callback) {
    refreshEthernetDeviceDetails();
    if (callback.isCallable()) {
        auto engine = qjsEngine(this);
        auto obj = engine->toScriptValue(m_ethernetDeviceDetails);
        callback.call({obj});
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  NetworkManager signal handlers
// ─────────────────────────────────────────────────────────────────────────────

void NmQt::onWirelessEnabledChanged(bool enabled) {
    m_wifiEnabled = enabled;
    emit wifiEnabledChanged();
}

void NmQt::onWirelessHardwareEnabledChanged(bool enabled) {
    if (!enabled) {
        m_wifiEnabled = false;
        emit wifiEnabledChanged();
    }
}

void NmQt::onNetworkDevicesChanged() {
    refreshDevices();
    refreshNetworks();
}

void NmQt::onActiveConnectionsChanged() {
    refreshNetworks();
    refreshDevices();
    refreshVpnConnections();
}

void NmQt::onConnectionsChanged() {
    refreshSavedConnections();
}

void NmQt::onDeviceStateChanged(NetworkManager::Device::State newState,
                                 NetworkManager::Device::State oldState,
                                 NetworkManager::Device::StateReason /*reason*/) {
    Q_UNUSED(oldState)
    const bool wasConnecting = (oldState == NetworkManager::Device::State::NeedAuth
                             || oldState == NetworkManager::Device::State::Config
                             || oldState == NetworkManager::Device::State::IpConfig);

    if (wasConnecting) {
        m_connectingSsid.clear();
        emit connectingSsidChanged();
    }

    switch (newState) {
    case NetworkManager::Device::State::Activated:
        // Connection succeeded
        refreshNetworks();
        refreshWirelessDeviceDetails();
        refreshEthernetDeviceDetails();
        break;
    case NetworkManager::Device::State::Failed:
        // Connection failed — extract failure info
        if (auto* dev = qobject_cast<NetworkManager::Device*>(sender())) {
            auto wd = NetworkManager::Device::Ptr(dev).dynamicCast<NetworkManager::WirelessDevice>();
            if (wd) {
                emit connectionFailed(wd->activeAccessPoint()
                                      ? wd->activeAccessPoint()->ssid()
                                      : QString());
            }
        }
        break;
    default:
        break;
    }

    emit isConnectedChanged();
}

void NmQt::onScanFinished() {
    m_scanning = false;
    emit scanningChanged();
    refreshNetworks();
}

void NmQt::onAccessPointAppeared(const QString& /*apPath*/) {
    refreshNetworks();
}

void NmQt::onAccessPointDisappeared(const QString& /*apPath*/) {
    refreshNetworks();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Refresh helpers
// ─────────────────────────────────────────────────────────────────────────────

void NmQt::refreshNetworks() {
    NetworkManager::WirelessDevice::Ptr wifiDev;
    for (const auto& uni : NetworkManager::networkInterfaces()) {
        auto dev = NetworkManager::findNetworkInterface(uni);
        auto wd = dev.dynamicCast<NetworkManager::WirelessDevice>();
        if (wd) {
            wifiDev = wd;
            m_wirelessDeviceUni = uni;
            break;
        }
    }

    if (!wifiDev) {
        if (!m_networks.isEmpty()) {
            m_networks.clear();
            emit networksChanged();
        }
        return;
    }

    // Connect device state changes (unique connection guards against duplicates)
    connect(wifiDev.data(), &NetworkManager::Device::stateChanged,
            this, &NmQt::onDeviceStateChanged,
            Qt::UniqueConnection);

    // Build AP list from NM cache
    QVariantList newList;
    QVariantMap activeAp;
    const auto aps = wifiDev->accessPoints();
    QSet<QString> seenBssids;

    for (const auto& apPath : aps) {
        NetworkManager::AccessPoint::Ptr ap =
            wifiDev->findAccessPoint(apPath);
        if (!ap || ap->ssid().isEmpty())
            continue;

        // Deduplicate by BSSID+SSID combo
        const auto key = ap->ssid() + QString::number(ap->frequency());
        if (seenBssids.contains(key))
            continue;
        seenBssids.insert(key);

        int strength = ap->strength();
        int frequency = static_cast<int>(ap->frequency());
        bool isActive = (apPath == wifiDev->activeAccessPoint());

        // Determine security — check WPA flags
        QString security;
        auto wpaFlags = ap->wpaFlags();
        auto rsnFlags = ap->rsnFlags();
        if (wpaFlags || rsnFlags) {
            if (rsnFlags.testFlag(NetworkManager::AccessPoint::Wpa2)
                || rsnFlags.testFlag(NetworkManager::AccessPoint::Wpa3))
                security = QStringLiteral("WPA2/WPA3");
            else if (wpaFlags.testFlag(NetworkManager::AccessPoint::Wpa2))
                security = QStringLiteral("WPA2");
            else if (wpaFlags.testFlag(NetworkManager::AccessPoint::Wpa))
                security = QStringLiteral("WPA");
            else
                security = QStringLiteral("encrypted");
        }

        auto map = buildApMap(ap->ssid(), ap->hardwareAddress(),
                              strength, frequency, isActive, security);
        newList.append(map);

        if (isActive)
            activeAp = map;
    }

    // Sort: active first, then by strength descending
    std::sort(newList.begin(), newList.end(), [](const QVariant& a, const QVariant& b) {
        auto ma = a.toMap();
        auto mb = b.toMap();
        if (ma.value("active").toBool() != mb.value("active").toBool())
            return ma.value("active").toBool();
        return ma.value("strength").toInt() > mb.value("strength").toInt();
    });

    bool changed = (m_networks != newList);
    if (changed) {
        m_networks = newList;
        emit networksChanged();
    }

    if (m_active != activeAp) {
        m_active = activeAp;
        emit activeChanged();
    }

    // If we were tracking a connecting ssid and it's now active, clear it
    if (!m_connectingSsid.isEmpty() && m_active.value("ssid").toString() == m_connectingSsid) {
        m_connectingSsid.clear();
        emit connectingSsidChanged();
    }
}

void NmQt::refreshDevices() {
    refreshEthernetDevices();
    refreshNetworks();
}

void NmQt::refreshEthernetDevices() {
    QVariantList devices;
    QVariantMap activeEth;

    for (const auto& uni : NetworkManager::networkInterfaces()) {
        auto dev = NetworkManager::findNetworkInterface(uni);
        if (!dev)
            continue;

        auto wd = dev.dynamicCast<NetworkManager::WirelessDevice>();
        if (wd)
            continue; // skip wireless

        QVariantMap info;
        info["interface"] = uni;
        info["type"] = QStringLiteral("ethernet");
        info["state"] = static_cast<int>(dev->state());
        info["connected"] = (dev->state() == NetworkManager::Device::State::Activated);

        // Try to get connection name from active connection
        if (dev->state() == NetworkManager::Device::State::Activated) {
            const auto activeConns = NetworkManager::activeConnections();
            for (const auto& path : activeConns) {
                auto ac = NetworkManager::findActiveConnection(path);
                if (ac && ac->devices().contains(uni)) {
                    info["connection"] = ac->id();
                    break;
                }
            }
        }

        devices.append(info);

        if (info["connected"].toBool() && activeEth.isEmpty()) {
            activeEth = info;
        }
    }

    bool ethChanged = (m_ethernetDevices != devices);
    bool activeEthChanged = (m_activeEthernet != activeEth);

    if (ethChanged) {
        m_ethernetDevices = devices;
        emit ethernetDevicesChanged();
    }
    if (activeEthChanged) {
        m_activeEthernet = activeEth;
        emit activeEthernetChanged();
    }
}

void NmQt::refreshSavedConnections() {
    QStringList connNames;
    QStringList ssids;

    const auto connPaths = NetworkManager::listConnections();
    for (const auto& path : connPaths) {
        auto conn = NetworkManager::findConnection(path);
        if (!conn || !conn->settings())
            continue;

        connNames.append(conn->name());

        // Extract SSID from wireless connections
        auto ws = conn->settings()->setting(NetworkManager::Setting::SettingType::Wireless);
        if (ws) {
            auto* wirelessSetting = static_cast<NetworkManager::WirelessSetting*>(ws.data());
            if (!wirelessSetting->ssid().isEmpty())
                ssids.append(wirelessSetting->ssid());
        }
    }

    if (m_savedConnections != connNames) {
        m_savedConnections = connNames;
        emit savedConnectionsChanged();
    }

    if (m_savedConnectionSsids != ssids) {
        m_savedConnectionSsids = ssids;
        emit savedConnectionSsidsChanged();
    }
}

void NmQt::refreshVpnConnections() {
    QVariantList vpnList;
    QVariantMap activeVpn;

    // Collect active VPN connection names for status lookup
    QSet<QString> activeVpnNames;
    const auto activeConns = NetworkManager::activeConnections();
    for (const auto& path : activeConns) {
        auto ac = NetworkManager::findActiveConnection(path);
        if (!ac)
            continue;
        // Check if it's a VPN type by examining connection settings
        auto conn = NetworkManager::findConnection(ac->connection());
        if (conn && conn->settings()) {
            auto vs = conn->settings()->setting(NetworkManager::Setting::SettingType::Vpn);
            if (vs) {
                activeVpnNames.insert(ac->id().toLower().trimmed());
            }
        }
    }

    const auto connPaths = NetworkManager::listConnections();
    for (const auto& path : connPaths) {
        auto conn = NetworkManager::findConnection(path);
        if (!conn || !conn->settings())
            continue;

        auto vs = conn->settings()->setting(NetworkManager::Setting::SettingType::Vpn);
        if (!vs)
            continue;

        QVariantMap info;
        info["name"] = conn->name();
        info["type"] = QStringLiteral("vpn");
        info["connected"] = activeVpnNames.contains(conn->name().toLower().trimmed());
        vpnList.append(info);
        vpnList.append(info);

        if (info["connected"].toBool())
            activeVpn = info;
    }

    // Sort: connected first, then alphabetically
    std::sort(vpnList.begin(), vpnList.end(), [](const QVariant& a, const QVariant& b) {
        auto ma = a.toMap();
        auto mb = b.toMap();
        if (ma.value("connected").toBool() != mb.value("connected").toBool())
            return ma.value("connected").toBool();
        return ma.value("name").toString() < mb.value("name").toString();
    });

    if (m_vpnConnections != vpnList) {
        m_vpnConnections = vpnList;
        emit vpnConnectionsChanged();
    }

    if (m_activeVpn != activeVpn) {
        m_activeVpn = activeVpn;
        emit activeVpnChanged();
    }
}

void NmQt::refreshWirelessDeviceDetails() {
    NetworkManager::WirelessDevice::Ptr wifiDev;
    for (const auto& uni : NetworkManager::networkInterfaces()) {
        auto dev = NetworkManager::findNetworkInterface(uni);
        auto wd = dev.dynamicCast<NetworkManager::WirelessDevice>();
        if (wd && dev->state() == NetworkManager::Device::State::Activated) {
            wifiDev = wd;
            break;
        }
    }

    if (!wifiDev) {
        m_wirelessDeviceDetails = {};
        emit wirelessDeviceDetailsChanged();
        return;
    }

    QVariantMap details;
    details["ipAddress"] = {};
    details["gateway"] = {};
    details["dns"] = QVariantList();
    details["subnet"] = {};
    details["macAddress"] = {};

    // IP info comes from the IP config via active connection
    const auto activeConns = NetworkManager::activeConnections();
    for (const auto& path : activeConns) {
        auto ac = NetworkManager::findActiveConnection(path);
        if (!ac || !ac->devices().contains(wifiDev->uni()))
            continue;

        // IP v4 config
        auto ipv4Config = ac->ipV4Config();
        if (ipv4Config.isValid()) {
            if (!ipv4Config.addresses().isEmpty()) {
                const auto& addr = ipv4Config.addresses().first();
                details["ipAddress"] = addr.ip().toString();
                details["subnet"] = addr.netmask().toString();
            }
            if (!ipv4Config.gateway().isEmpty())
                details["gateway"] = ipv4Config.gateway();

            QVariantList dnsList;
            for (const auto& ns : ipv4Config.nameservers())
                dnsList.append(ns.toString());
            details["dns"] = dnsList;
        }
        break;
    }

    if (m_wirelessDeviceDetails != details) {
        m_wirelessDeviceDetails = details;
        emit wirelessDeviceDetailsChanged();
    }
}

void NmQt::refreshEthernetDeviceDetails() {
    NetworkManager::Device::Ptr ethDev;
    for (const auto& uni : NetworkManager::networkInterfaces()) {
        auto dev = NetworkManager::findNetworkInterface(uni);
        if (!dev)
            continue;
        auto wd = dev.dynamicCast<NetworkManager::WirelessDevice>();
        if (!wd && dev->state() == NetworkManager::Device::State::Activated) {
            ethDev = dev;
            break;
        }
    }

    if (!ethDev) {
        m_ethernetDeviceDetails = {};
        emit ethernetDeviceDetailsChanged();
        return;
    }

    QVariantMap details;
    details["ipAddress"] = {};
    details["gateway"] = {};
    details["dns"] = QVariantList();
    details["subnet"] = {};
    details["macAddress"] = {};

    const auto activeConns = NetworkManager::activeConnections();
    for (const auto& path : activeConns) {
        auto ac = NetworkManager::findActiveConnection(path);
        if (!ac || !ac->devices().contains(ethDev->uni()))
            continue;

        auto ipv4Config = ac->ipV4Config();
        if (ipv4Config.isValid()) {
            if (!ipv4Config.addresses().isEmpty()) {
                const auto& addr = ipv4Config.addresses().first();
                details["ipAddress"] = addr.ip().toString();
                details["subnet"] = addr.netmask().toString();
            }
            if (!ipv4Config.gateway().isEmpty())
                details["gateway"] = ipv4Config.gateway();

            QVariantList dnsList;
            for (const auto& ns : ipv4Config.nameservers())
                dnsList.append(ns.toString());
            details["dns"] = dnsList;
        }
        break;
    }

    if (m_ethernetDeviceDetails != details) {
        m_ethernetDeviceDetails = details;
        emit ethernetDeviceDetailsChanged();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Static helpers
// ─────────────────────────────────────────────────────────────────────────────

QVariantMap NmQt::buildApMap(const QString& ssid, const QString& bssid,
                              int strength, int frequency,
                              bool active, const QString& security) {
    QVariantMap map;
    map["ssid"] = ssid;
    map["bssid"] = bssid;
    map["strength"] = strength;
    map["frequency"] = frequency;
    map["active"] = active;
    map["security"] = security;
    map["isSecure"] = !security.isEmpty();
    return map;
}

void NmQt::invokeCallback(QJSValue callback, bool success,
                           const QString& output, const QString& error,
                           int exitCode, bool needsPassword) {
    if (!callback.isCallable())
        return;

    auto* engine = qjsEngine(this);
    if (!engine)
        return;

    auto result = engine->newObject();
    result.setProperty("success", success);
    result.setProperty("output", output);
    result.setProperty("error", error);
    result.setProperty("exitCode", exitCode);
    result.setProperty("needsPassword", needsPassword);

    callback.call({result});
}

} // namespace caelestia::services
