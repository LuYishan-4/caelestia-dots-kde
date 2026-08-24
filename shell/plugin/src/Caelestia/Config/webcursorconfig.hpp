#pragma once

#include "configobject.hpp"
#include <qhashfunctions.h>
#include <qstandardpaths.h>

namespace caelestia::config {

class WebCursor : public ConfigObject {
    Q_OBJECT
    QML_ANONYMOUS

    CONFIG_GLOBAL_PROPERTY(bool, enabled, true)
    CONFIG_GLOBAL_PROPERTY(int, width, 128)
    CONFIG_GLOBAL_PROPERTY(int, height, 128)
    CONFIG_GLOBAL_PROPERTY(QString, selectTheme, QStringLiteral("default"))
    CONFIG_GLOBAL_PROPERTY(QString, sdkThemesDir,
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
            QStringLiteral("/kwin/effects/ultralightwebcursor"))

public:
    explicit WebCursor(QObject* parent = nullptr)
        : ConfigObject(parent) {}
};

class WebCursorConfig : public ConfigObject {
    Q_OBJECT
    QML_ANONYMOUS

    CONFIG_SUBOBJECT(WebCursor, cursor)

public:
    explicit WebCursorConfig(QObject* parent = nullptr)
        : ConfigObject(parent)
        , m_cursor(new WebCursor(this)) {}
};

} // namespace caelestia::config
