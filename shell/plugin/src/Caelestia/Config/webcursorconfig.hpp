#pragma once

#include "configobject.hpp"
#include <qstandardpaths.h>

namespace caelestia::config {

// The cursor effect is a separate KWin process. Keep its complete user-facing
// state here so both Nexus and the effect consume the same shell.json section.
class WebCursorMain : public ConfigObject {
    Q_OBJECT
    QML_ANONYMOUS

    CONFIG_GLOBAL_PROPERTY(bool, enabled)
    CONFIG_GLOBAL_PROPERTY(int, width)
    CONFIG_GLOBAL_PROPERTY(int, height)
    CONFIG_GLOBAL_PROPERTY(QString, selectTheme)
    CONFIG_GLOBAL_PROPERTY(QStringList, blacklist)
    // User-installed themes are deliberately kept outside the system KWin data
    // directory, which may be read-only.
    CONFIG_GLOBAL_PROPERTY(QString, themesDir)

public:
    explicit WebCursorMain(QObject* parent = nullptr)
        : ConfigObject(parent) {
        // Set defaults through the normal setters. This makes a first save write
        // the complete webCursor section, including the custom-theme directory.
        set_enabled(false);
        set_width(128);
        set_height(128);
        set_selectTheme(QStringLiteral("variant4-ciallo"));
        set_themesDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
            + QStringLiteral("/caelestia/webcursor"));
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
