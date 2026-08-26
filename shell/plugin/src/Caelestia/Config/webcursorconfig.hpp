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

    CONFIG_PROPERTY(bool, enabled, true)
    CONFIG_PROPERTY(int, width, 128)
    CONFIG_PROPERTY(int, height, 128)
    CONFIG_PROPERTY(QString, selectTheme, QStringLiteral("variant4-ciallo"))
    CONFIG_PROPERTY(QStringList, blacklist)
    CONFIG_PROPERTY(QString, themesDir,
        QString(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
                QStringLiteral("/caelestia/webcursor")))
    CONFIG_PROPERTY(QString, configPath,
        QString(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
                QStringLiteral("/caelestia/shell.json")))

public:
    explicit WebCursorMain(QObject* parent = nullptr)
        : ConfigObject(parent) {
        set_enabled(true);
        set_width(128);
        set_height(128);
        set_selectTheme(QStringLiteral("variant4-ciallo"));
        set_themesDir(QString(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
                              QStringLiteral("/caelestia/webcursor")));
        set_configPath(QString(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
                               QStringLiteral("/caelestia/shell.json")));
    }
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
