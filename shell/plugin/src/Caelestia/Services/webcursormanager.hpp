#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantMap>
#include <qqmlintegration.h>

namespace caelestia::services {

// Nexus-facing replacement for Animated_UltralightWeb_Cursor's SettingsBackend.
// Persistent values live in Caelestia.Config.WebCursorConfig; this class owns
// filesystem operations and the KWin live-reload bridge only.
class WebCursorManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QStringList themeList READ themeList NOTIFY themeListChanged)
    Q_PROPERTY(QString currentTheme READ currentTheme NOTIFY currentThemeChanged)
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit WebCursorManager(QObject* parent = nullptr);

    QString statusMessage() const;
    QStringList themeList() const;
    QString currentTheme() const;

    Q_INVOKABLE void save();
    Q_INVOKABLE void reload();
    Q_INVOKABLE bool pathExists(const QString& path) const;
    Q_INVOKABLE bool uploadTheme(const QString& path);
    Q_INVOKABLE void useTheme(const QString& name);
    Q_INVOKABLE bool removeTheme(const QString& name);
    Q_INVOKABLE bool isUserTheme(const QString& name) const;
    Q_INVOKABLE void openThemeFolder(const QString& name);
    Q_INVOKABLE QVariantMap getThemeDetails(const QString& name) const;
    Q_INVOKABLE void addBlacklist(const QString& app);
    Q_INVOKABLE void removeBlacklist(const QString& app);
    Q_INVOKABLE void enable();
    Q_INVOKABLE void disable();
    Q_INVOKABLE void reconfigureKWin();

signals:
    void statusMessageChanged();
    void themeListChanged();
    void currentThemeChanged();

private:
    QString themePath(const QString& name) const;
    void loadThemes();
    void setStatusMessage(const QString& message);

    QString m_statusMessage;
    QStringList m_themeList;
};

} // namespace caelestia::services
