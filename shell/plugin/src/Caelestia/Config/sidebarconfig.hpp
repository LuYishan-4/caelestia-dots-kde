#pragma once

#include "configobject.hpp"

namespace caelestia::config {

class SidebarConfig : public ConfigObject {
    Q_OBJECT
    QML_ANONYMOUS

    CONFIG_PROPERTY(bool, enabled, true)
    CONFIG_PROPERTY(int, dragThreshold, 50)
    CONFIG_PROPERTY(int, grabWidth, 12)

public:
    explicit SidebarConfig(QObject* parent = nullptr)
        : ConfigObject(parent) {}
};

} // namespace caelestia::config
