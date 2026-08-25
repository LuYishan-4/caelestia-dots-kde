#pragma once

#include "configobject.hpp"
#include <qhashfunctions.h>
#include <qstandardpaths.h>

namespace caelestia::config {

// The cursor effect is a separate KWin process. Keep its complete user-facing
// state here so both Nexus and the effect consume the same shell.json section.
class WebCursorMain : public ConfigObject {
    Q_OBJECT
    QML_ANONYMOUS

    CONFIG_GLOBAL_PROPERTY(bool, enabled, true)
    CONFIG_GLOBAL_PROPERTY(int, width, 128)
    CONFIG_GLOBAL_PROPERTY(int, height, 128)
    CONFIG_GLOBAL_PROPERTY(QString, selectTheme, QStringLiteral("variant4-ciallo"))
    CONFIG_GLOBAL_PROPERTY(QStringList, blacklist)
    // User-installed themes are deliberately kept outside the system KWin data
    // directory, which may be read-only.
    CONFIG_GLOBAL_PROPERTY(QString, themesDir, QStringLiteral("/usr/share/caelestia/webcursor"))
    CONFIG_GLOBAL_PROPERTY(QString, configPath,
        QString(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
                QStringLiteral("/caelestia/shell.json")))

public:
    explicit WebCursorMain(QObject* parent = nullptr)
        : ConfigObject(parent) {}
};

class WebCursorConfig : public ConfigObject {
    Q_OBJECT
    QML_ANONYMOUS

    CONFIG_SUBOBJECT(WebCursorMain, cursor)

public:
    explicit WebCursorConfig(QObject* parent = nullptr)
        : ConfigObject(parent)
        , m_cursor(new WebCursorMain(this)) {}
};

} // namespace caelestia::config
