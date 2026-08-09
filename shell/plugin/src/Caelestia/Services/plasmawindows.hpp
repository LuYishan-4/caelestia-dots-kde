// SPDX-License-Identifier: GPL-3.0-only
#pragma once

// Shared access to KWin's taskbar-side window handles.
//
// org_kde_plasma_window is what a taskbar uses to talk about a window it did
// not create: publishing where its entry sits, asking for its icon, and so on.
// More than one service here needs the same handle for the same window, so the
// handles live in one place rather than each service opening its own.
//
// The shell already requests org_kde_plasma_window_management in its desktop
// file, which is what makes KWin advertise this restricted interface to it.

#include <QHash>
#include <QObject>
#include <QtWaylandClient/QWaylandClientExtension>

#include "qwayland-plasma-window-management.h"

namespace caelestia::services {

/// One org_kde_plasma_window handle, held for as long as something has a use
/// for the window it names.
class PlasmaWindowHandle : public QObject, public QtWayland::org_kde_plasma_window {
    Q_OBJECT

public:
    explicit PlasmaWindowHandle(::org_kde_plasma_window* window);
    ~PlasmaWindowHandle() override;

signals:
    /// The compositor dropped the window; the handle is spent after this.
    void unmapped();

protected:
    void org_kde_plasma_window_unmapped() override;
};

class PlasmaWindowManagement : public QWaylandClientExtensionTemplate<PlasmaWindowManagement>,
                               public QtWayland::org_kde_plasma_window_management {
    Q_OBJECT

public:
    explicit PlasmaWindowManagement(QObject* parent = nullptr);
    ~PlasmaWindowManagement() override;
};

/**
 * Hands out one handle per window, keyed on KWin's internal window id — the
 * same value the KWin bridge reports as a window's address.
 *
 * Deliberately leaked rather than held in a destructible static. Releasing a
 * Wayland proxy is itself a request, and a static is torn down from an exit
 * handler, long after Qt has closed the display; marshalling there segfaults
 * the shell on its way out. Handles are released early, from aboutToQuit,
 * while the connection is still live.
 */
class PlasmaWindows : public QObject {
    Q_OBJECT

public:
    static PlasmaWindows* instance();

    /// Whether the compositor actually gave us the interface.
    bool available();

    /// The handle for @p uuid, or nullptr if the interface is unavailable or
    /// the connection is already gone. Uuids are normalised, so callers need
    /// not care whether theirs arrived brace-wrapped.
    PlasmaWindowHandle* handleFor(const QString& uuid);

    /// KWin hands out window ids as QUuid::toString(), i.e. brace-wrapped.
    static QString normaliseUuid(const QString& uuid);

signals:
    /// A handle went away, so anything a service cached against @p uuid is
    /// stale. The uuid is the normalised form.
    void handleLost(const QString& uuid);

private:
    explicit PlasmaWindows(QObject* parent = nullptr);

    void forget(const QString& uuid);
    void shutdown();

    PlasmaWindowManagement* m_management = nullptr;
    QHash<QString, PlasmaWindowHandle*> m_handles;
};

} // namespace caelestia::services
