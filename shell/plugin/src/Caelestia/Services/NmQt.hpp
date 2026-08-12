// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QStringList>
#include <QJSValue>
#include <QDateTime>
#include <qqmlintegration.h>
#include <NetworkManagerQt/Device>

namespace caelestia::services {

/**
 * NetworkManager Qt / D-Bus singleton replacing the nmcli-shelling-out
 * approach of the old Nmcli.qml.
 *
 * All properties reactively update via NetworkManagerQt signals — no
 * command-line parsing, no locale assumptions, no repeated process spawning.
 */
class NmQt : public QObject {
    Q_OBJECT

    // -- General state --
    Q_PROPERTY(bool isConnected READ isConnected NOTIFY isConnectedChanged)
    Q_PROPERTY(bool wifiEnabled READ wifiEnabled NOTIFY wifiEnabledChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(QString connectingSsid READ connectingSsid NOTIFY connectingSsidChanged)

    // -- WiFi networks --
    Q_PROPERTY(QVariantList networks READ networks NOTIFY networksChanged)
    Q_PROPERTY(QVariantMap active READ active NOTIFY activeChanged)

    // -- Saved connections --
    Q_PROPERTY(QStringList savedConnections READ savedConnections NOTIFY savedConnectionsChanged)
    Q_PROPERTY(QStringList savedConnectionSsids READ savedConnectionSsids NOTIFY savedConnectionSsidsChanged)

    // -- Ethernet --
    Q_PROPERTY(QVariantMap activeEthernet READ activeEthernet NOTIFY activeEthernetChanged)
    Q_PROPERTY(QVariantList ethernetDevices READ ethernetDevices NOTIFY ethernetDevicesChanged)

    // -- VPN --
    Q_PROPERTY(QVariantList vpnConnections READ vpnConnections NOTIFY vpnConnectionsChanged)
    Q_PROPERTY(QVariantMap activeVpn READ activeVpn NOTIFY activeVpnChanged)
    Q_PROPERTY(QString vpnPendingConnection READ vpnPendingConnection NOTIFY vpnPendingConnectionChanged)

    // -- Device details --
    Q_PROPERTY(QVariantMap wirelessDeviceDetails READ wirelessDeviceDetails NOTIFY wirelessDeviceDetailsChanged)
    Q_PROPERTY(QVariantMap ethernetDeviceDetails READ ethernetDeviceDetails NOTIFY ethernetDeviceDetailsChanged)

    QML_ELEMENT
    QML_SINGLETON

public:
    explicit NmQt(QObject* parent = nullptr);
    ~NmQt() override;

    // -- Property accessors --
    bool isConnected() const;
    bool wifiEnabled() const;
    bool scanning() const;
    QString connectingSsid() const;

    QVariantList networks() const;
    QVariantMap active() const;

    QStringList savedConnections() const;
    QStringList savedConnectionSsids() const;

    QVariantMap activeEthernet() const;
    QVariantList ethernetDevices() const;

    QVariantList vpnConnections() const;
    QVariantMap activeVpn() const;
    QString vpnPendingConnection() const;

    QVariantMap wirelessDeviceDetails() const;
    QVariantMap ethernetDeviceDetails() const;

    // -- QML-invokable actions --

    /// Refresh the WiFi network list. Optionally invoke callback(list).
    Q_INVOKABLE void getNetworks(QJSValue callback = {});

    /// Connect to a WiFi network.
    Q_INVOKABLE void connectToNetwork(const QString& ssid, const QString& password,
                                      const QString& bssid, QJSValue callback = {});

    /// Try connecting with a saved password first; fall back to asking.
    Q_INVOKABLE void connectToNetworkWithPasswordCheck(const QString& ssid, bool isSecure,
                                                       QJSValue callback = {},
                                                       const QString& bssid = {});

    /// Disconnect from the currently active WiFi network.
    Q_INVOKABLE void disconnectFromNetwork();

    /// Remove a saved connection profile by SSID.
    Q_INVOKABLE void forgetNetwork(const QString& ssid, QJSValue callback = {});

    /// Enable or disable WiFi radio.
    Q_INVOKABLE void enableWifi(bool enabled, QJSValue callback = {});

    /// Toggle WiFi radio state.
    Q_INVOKABLE void toggleWifi(QJSValue callback = {});

    /// Trigger a WiFi rescan.
    Q_INVOKABLE void rescanWifi();

    /// Connect an Ethernet device.
    Q_INVOKABLE void connectEthernet(const QString& connectionName, const QString& interfaceName,
                                     QJSValue callback = {});

    /// Disconnect an Ethernet device.
    Q_INVOKABLE void disconnectEthernet(const QString& connectionName, QJSValue callback = {});

    /// Connect a VPN connection.
    Q_INVOKABLE void connectVpn(const QString& connectionName, QJSValue callback = {});

    /// Disconnect a VPN connection.
    Q_INVOKABLE void disconnectVpn(const QString& connectionName, QJSValue callback = {});

    /// Reload the saved-connections lists.
    Q_INVOKABLE void loadSavedConnections(QJSValue callback = {});

    /// Reload the VPN connections list.
    Q_INVOKABLE void loadVpnConnections(QJSValue callback = {});

    /// Check whether a saved profile exists for the given SSID.
    Q_INVOKABLE bool hasSavedProfile(const QString& ssid) const;

    /// Retrieve detailed info for a wireless device.
    Q_INVOKABLE void getWirelessDeviceDetails(const QString& interfaceName,
                                              QJSValue callback = {});

    /// Retrieve detailed info for an ethernet device.
    Q_INVOKABLE void getEthernetDeviceDetails(const QString& interfaceName,
                                              QJSValue callback = {});

signals:
    void isConnectedChanged();
    void wifiEnabledChanged();
    void scanningChanged();
    void connectingSsidChanged();

    void networksChanged();
    void activeChanged();

    void savedConnectionsChanged();
    void savedConnectionSsidsChanged();

    void activeEthernetChanged();
    void ethernetDevicesChanged();

    void vpnConnectionsChanged();
    void activeVpnChanged();

    void vpnPendingConnectionChanged();

    void wirelessDeviceDetailsChanged();
    void ethernetDeviceDetailsChanged();

    /// Emitted when a connection attempt fails outright.
    void connectionFailed(const QString& ssid);

private slots:
    // -- NetworkManager signal handlers --
    void onWirelessEnabledChanged(bool enabled);
    void onWirelessHardwareEnabledChanged(bool enabled);
    void onNetworkDevicesChanged();
    void onActiveConnectionsChanged();
    void onConnectionsChanged();
    void onDeviceStateChanged(NetworkManager::Device::State newState,
                              NetworkManager::Device::State oldState,
                              NetworkManager::Device::StateChangeReason reason);
    void onScanFinished(const QDateTime& dateTime);
    void onAccessPointAppeared(const QString& apPath);
    void onAccessPointDisappeared(const QString& apPath);

    /// Re-sync all state once NetworkManagerQt/NM itself is fully settled.
    void onNetworkManagerReady();

private:
    // -- Helpers --
    void refreshNetworks();
    void refreshDevices();
    void refreshEthernetDevices();
    void refreshSavedConnections();
    void refreshVpnConnections();
    void refreshWirelessDeviceDetails(const QString& interfaceName = {});
    void refreshEthernetDeviceDetails(const QString& interfaceName = {});

    /// Build a QVariantMap for a single access point.
    static QVariantMap buildApMap(const QString& ssid, const QString& bssid,
                                  int strength, int frequency,
                                  bool active, const QString& security);

    /// Build a standardised result object for a JS callback.
    static QJSValue buildResult(QJSEngine* engine, bool success,
                                const QString& output = {},
                                const QString& error = {},
                                int exitCode = 0,
                                bool needsPassword = false);

    void invokeCallback(QJSValue callback, bool success,
                        const QString& output = {},
                        const QString& error = {},
                        int exitCode = 0,
                        bool needsPassword = false);

    // -- State --
    QVariantList m_networks;
    QVariantMap m_active;
    QStringList m_savedConnections;
    QStringList m_savedConnectionSsids;
    QVariantMap m_activeEthernet;
    QVariantList m_ethernetDevices;
    QVariantList m_vpnConnections;
    QVariantMap m_activeVpn;
    QString m_vpnPendingConnection;
    QVariantMap m_wirelessDeviceDetails;
    QVariantMap m_ethernetDeviceDetails;
    QString m_connectingSsid;
    bool m_wifiEnabled = true;
    bool m_scanning = false;
    bool m_initialised = false;

    // Track the wireless device UNI for scan/connection operations.
    QString m_wirelessDeviceUni;
    QString m_ethernetDeviceUni;
};

} // namespace caelestia::services
