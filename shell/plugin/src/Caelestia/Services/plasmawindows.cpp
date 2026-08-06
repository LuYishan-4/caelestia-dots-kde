// SPDX-License-Identifier: GPL-3.0-only
#include "plasmawindows.hpp"

#include <QCoreApplication>
#include <QLoggingCategory>
#include <QUuid>

namespace caelestia::services {

namespace {

Q_LOGGING_CATEGORY(logPlasmaWindows, "caelestia.services.plasmawindows");

// get_window_by_uuid, which is what lets us address a window without mirroring
// KWin's entire window list. get_icon needs 7, so 12 covers both.
constexpr int kRequiredVersion = 12;

// Set once the Wayland connection is on its way out. Releasing a proxy is
// itself a request, so doing it afterwards marshals onto freed memory and takes
// the shell down with a SIGSEGV. Everything is released up front from
// aboutToQuit, while the connection is still live; anything that survives past
// that point must let its proxy leak rather than touch it.
bool s_connectionGone = false;

} // namespace

PlasmaWindowHandle::PlasmaWindowHandle(::org_kde_plasma_window* window)
    : QtWayland::org_kde_plasma_window(window) {}

PlasmaWindowHandle::~PlasmaWindowHandle() {
    if (!s_connectionGone && isInitialized()) {
        destroy();
    }
}

void PlasmaWindowHandle::org_kde_plasma_window_unmapped() {
    emit unmapped();
}

PlasmaWindowManagement::PlasmaWindowManagement(QObject* parent)
    : QWaylandClientExtensionTemplate<PlasmaWindowManagement>(kRequiredVersion) {
    setParent(parent);
    initialize();
    if (!isInitialized() || !isActive()) {
        qCWarning(logPlasmaWindows)
            << "org_kde_plasma_window_management is not available (isInitialized:" << isInitialized()
            << ", isActive:" << isActive() << ")."
            << "The compositor may not support it at version" << kRequiredVersion
            << "or this app is missing org_kde_plasma_window_management from"
               " X-KDE-Wayland-Interfaces in its desktop file.";
    }
}

// org_kde_plasma_window_management has no destructor request, so there is
// nothing to release here beyond the base class teardown.
PlasmaWindowManagement::~PlasmaWindowManagement() = default;

PlasmaWindows::PlasmaWindows(QObject* parent)
    : QObject(parent) {
    if (auto* app = QCoreApplication::instance()) {
        connect(app, &QCoreApplication::aboutToQuit, this, &PlasmaWindows::shutdown);
    }
}

PlasmaWindows* PlasmaWindows::instance() {
    static auto* s_instance = new PlasmaWindows();
    return s_instance;
}

QString PlasmaWindows::normaliseUuid(const QString& uuid) {
    // A uuid that differs only in its braces resolves to a window that is
    // immediately unmapped, which looks exactly like the protocol silently
    // doing nothing.
    const auto parsed = QUuid::fromString(uuid);
    return parsed.isNull() ? uuid : parsed.toString(QUuid::WithBraces);
}

bool PlasmaWindows::available() {
    if (s_connectionGone) {
        return false;
    }
    if (!m_management) {
        m_management = new PlasmaWindowManagement(this);
    }
    return m_management->isActive();
}

PlasmaWindowHandle* PlasmaWindows::handleFor(const QString& uuid) {
    if (uuid.isEmpty() || !available()) {
        return nullptr;
    }

    const auto key = normaliseUuid(uuid);
    if (const auto it = m_handles.constFind(key); it != m_handles.constEnd()) {
        return *it;
    }

    auto* handle = new PlasmaWindowHandle(m_management->get_window_by_uuid(key));
    handle->setParent(this);
    // The compositor answers an unknown uuid with an immediately unmapped
    // window rather than an error, so this doubles as the "no such window"
    // path: drop it and let the next call ask again.
    connect(handle, &PlasmaWindowHandle::unmapped, this, [this, key]() { forget(key); });
    m_handles.insert(key, handle);
    return handle;
}

void PlasmaWindows::forget(const QString& uuid) {
    if (auto* handle = m_handles.take(uuid)) {
        handle->deleteLater();
    }
    emit handleLost(uuid);
}

void PlasmaWindows::shutdown() {
    if (s_connectionGone) {
        return;
    }

    const auto uuids = m_handles.keys();
    for (const auto& uuid : uuids) {
        delete m_handles.take(uuid);
    }

    delete m_management;
    m_management = nullptr;

    s_connectionGone = true;
}

} // namespace caelestia::services
