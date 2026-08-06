// SPDX-License-Identifier: GPL-3.0-only
#include "minimizegeometry.hpp"

#include "plasmawindows.hpp"

#include <QGuiApplication>
#include <QQuickWindow>
#include <QWindow>

#include <qpa/qplatformnativeinterface.h>

namespace caelestia::services {

namespace {

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

} // namespace

MinimizeGeometry::MinimizeGeometry(QObject* parent)
    : QObject(parent) {
    // A handle that goes away takes the published rect with it, so the next
    // call has to send afresh rather than dedupe against a stale value.
    connect(PlasmaWindows::instance(), &PlasmaWindows::handleLost, this,
        [this](const QString& uuid) { m_published.remove(uuid); });
}

void MinimizeGeometry::setGeometry(QQuickItem* anchor, const QString& uuid, int x, int y, int width, int height) {
    if (!anchor || uuid.isEmpty() || width <= 0 || height <= 0) {
        return;
    }

    auto* surface = surfaceFor(anchor->window());
    if (!surface) {
        return;
    }

    // Scene coordinates are surface coordinates: the anchor's window is the
    // surface the rect is declared against.
    const auto key = PlasmaWindows::normaliseUuid(uuid);
    const QRect rect(x, y, width, height);
    if (m_published.value(key) == rect) {
        return;
    }

    auto* handle = PlasmaWindows::instance()->handleFor(key);
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

void MinimizeGeometry::clearGeometry(QQuickItem* anchor, const QString& uuid) {
    if (uuid.isEmpty()) {
        return;
    }

    const auto key = PlasmaWindows::normaliseUuid(uuid);
    if (!m_published.contains(key)) {
        return;
    }

    if (auto* surface = anchor ? surfaceFor(anchor->window()) : nullptr) {
        if (auto* handle = PlasmaWindows::instance()->handleFor(key)) {
            handle->unset_minimized_geometry(surface);
        }
    }
    m_published.remove(key);
}

} // namespace caelestia::services
