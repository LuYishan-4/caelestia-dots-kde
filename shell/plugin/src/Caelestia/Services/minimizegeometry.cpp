// SPDX-License-Identifier: GPL-3.0-only
#include "minimizegeometry.hpp"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QUuid>
#include <QQuickWindow>
#include <QWindow>

#include <qpa/qplatformnativeinterface.h>

namespace caelestia::services {

namespace {

Q_LOGGING_CATEGORY(logMinimizeGeometry, "caelestia.services.minimizegeometry");

// get_window_by_uuid, which is what lets us address a window without mirroring
// KWin's entire window list.
constexpr int kRequiredVersion = 12;

// Set once the Wayland connection is on its way out. Releasing a proxy is
// itself a request, so doing it after the connection is gone marshals onto
// freed memory and takes the whole shell down with a SIGSEGV — which is
// exactly what happened before this existed. Everything is released up front
// from aboutToQuit, while the connection is still live; anything that survives
// past that point must let its proxy leak rather than touch it.
bool s_connectionGone = false;

/// The wl_surface backing a QWindow, or nullptr before it has been shown.
wl_surface* surfaceFor(QWindow* window) {
    if (!window || !window->handle()) {
        return nullptr;
    }

    auto* native = QGuiApplication::platformNativeInterface();
    if (!native) {
        return nullptr;
    }
    return static_cast<wl_surface*>(native->nativeResourceForWindow(QByteArrayLiteral("surface"), window));
}

/// KWin hands out window ids as QUuid::toString(), i.e. brace-wrapped. Callers
/// pass those through from the KWin bridge, but normalise anyway: a uuid that
/// differs only in its braces resolves to a window that is immediately
/// unmapped, which looks exactly like the protocol silently doing nothing.
QString normaliseUuid(const QString& uuid) {
    const auto parsed = QUuid::fromString(uuid);
    return parsed.isNull() ? uuid : parsed.toString(QUuid::WithBraces);
}

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
        qCWarning(logMinimizeGeometry)
            << "org_kde_plasma_window_management is not available (isInitialized:" << isInitialized()
            << ", isActive:" << isActive() << "). Minimize animations will fall back to the cursor."
            << "The compositor may not support it at version" << kRequiredVersion
            << "or this app is missing org_kde_plasma_window_management from"
               " X-KDE-Wayland-Interfaces in its desktop file.";
    }
}

// org_kde_plasma_window_management has no destructor request, so there is
// nothing to release here beyond the base class teardown.
PlasmaWindowManagement::~PlasmaWindowManagement() = default;

MinimizeGeometry::MinimizeGeometry(QObject* parent)
    : QObject(parent) {
    // Tear down while the connection is still usable. QML singletons outlive
    // the Wayland display on the way out, so waiting for the destructor is too
    // late to release anything.
    if (auto* app = QCoreApplication::instance()) {
        connect(app, &QCoreApplication::aboutToQuit, this, &MinimizeGeometry::shutdown);
    }
}

void MinimizeGeometry::shutdown() {
    if (s_connectionGone) {
        return;
    }

    const auto uuids = m_handles.keys();
    for (const auto& uuid : uuids) {
        delete m_handles.take(uuid);
    }
    m_published.clear();

    delete m_management;
    m_management = nullptr;

    s_connectionGone = true;
}

PlasmaWindowHandle* MinimizeGeometry::handleFor(const QString& uuid) {
    if (s_connectionGone) {
        return nullptr;
    }
    if (const auto it = m_handles.constFind(uuid); it != m_handles.constEnd()) {
        return *it;
    }

    if (!m_management) {
        m_management = new PlasmaWindowManagement(this);
    }
    if (!m_management->isActive()) {
        return nullptr;
    }

    auto* handle = new PlasmaWindowHandle(m_management->get_window_by_uuid(uuid));
    handle->setParent(this);
    // The compositor answers an unknown uuid with an immediately unmapped
    // window rather than an error, so this doubles as the "no such window"
    // path: drop it and let the next call ask again.
    connect(handle, &PlasmaWindowHandle::unmapped, this, [this, uuid]() { forget(uuid); });
    m_handles.insert(uuid, handle);
    return handle;
}

void MinimizeGeometry::forget(const QString& uuid) {
    if (auto* handle = m_handles.take(uuid)) {
        handle->deleteLater();
    }
    m_published.remove(uuid);
}

void MinimizeGeometry::setGeometry(QQuickItem* tile, const QString& uuid) {
    if (!tile || uuid.isEmpty() || tile->width() <= 0 || tile->height() <= 0) {
        return;
    }

    auto* surface = surfaceFor(tile->window());
    if (!surface) {
        return;
    }

    // Scene coordinates are surface coordinates: the item's window is the
    // surface the rect is declared against.
    const auto topLeft = tile->mapToScene(QPointF(0, 0));
    const QRect rect(qRound(topLeft.x()), qRound(topLeft.y()), qRound(tile->width()), qRound(tile->height()));

    const auto key = normaliseUuid(uuid);
    if (m_published.value(key) == rect) {
        return;
    }

    auto* handle = handleFor(key);
    if (!handle) {
        return;
    }

    // The protocol takes unsigned coordinates, so a tile scrolled or animated
    // off the surface's top/left would wrap into a huge positive number.
    handle->set_minimized_geometry(surface, static_cast<uint32_t>(std::max(0, rect.x())),
        static_cast<uint32_t>(std::max(0, rect.y())), static_cast<uint32_t>(rect.width()),
        static_cast<uint32_t>(rect.height()));
    m_published.insert(key, rect);
}

void MinimizeGeometry::clearGeometry(QQuickItem* tile, const QString& uuid) {
    if (uuid.isEmpty()) {
        return;
    }

    const auto key = normaliseUuid(uuid);
    if (!m_published.contains(key)) {
        return;
    }

    if (auto* surface = tile ? surfaceFor(tile->window()) : nullptr) {
        if (const auto it = m_handles.constFind(key); it != m_handles.constEnd()) {
            (*it)->unset_minimized_geometry(surface);
        }
    }
    forget(key);
}

} // namespace caelestia::services
