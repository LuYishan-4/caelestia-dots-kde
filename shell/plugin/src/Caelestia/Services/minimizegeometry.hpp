// SPDX-License-Identifier: GPL-3.0-only
#pragma once

// Tells KWin where each taskbar entry sits, so minimize/restore effects have
// something to animate towards.
//
// KWin's Magic Lamp effect animates a window into its "icon geometry" — the
// rect the taskbar occupies for that window. Nothing publishes that here: the
// dock is a Quickshell surface, not a Plasma panel, so KWin has no icon
// geometry for any window and the effect falls back to guessing from the
// cursor. That is why the animation follows the pointer around, and why
// putting a real Plasma panel underneath the dock "fixes" it.
//
// The taskbar side of the plasma-window-management protocol is what Plasma's
// own Task Manager uses for this (libtaskmanager sets it from the task
// delegate's rect).

#include <QHash>
#include <QObject>
#include <QQmlEngine>
// Full definition, not a forward declaration: moc needs it to register the
// Q_INVOKABLE pointer argument below as a metatype.
#include <QQuickItem>
#include <QRect>

namespace caelestia::services {

class MinimizeGeometry : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit MinimizeGeometry(QObject* parent = nullptr);

    /**
     * Publish the taskbar rect for @p uuid, which is KWin's internal window id
     * — the same value the KWin bridge reports as a window's address.
     *
     * @p anchor only identifies the surface the rect belongs to; QML cannot
     * hand a QWindow across to C++, so it passes any item in the right window.
     * The rect itself is in scene coordinates, which for a full-screen shell
     * window are the surface coordinates the protocol asks for.
     *
     * Repeat calls with an unchanged rect are dropped, because the QML side
     * polls this far more often than the geometry actually moves.
     */
    Q_INVOKABLE void setGeometry(QQuickItem* anchor, const QString& uuid, int x, int y, int width, int height);

    /// Withdraw a rect published earlier, e.g. when its tile goes away.
    Q_INVOKABLE void clearGeometry(QQuickItem* anchor, const QString& uuid);

private:
    QHash<QString, QRect> m_published;
};

} // namespace caelestia::services
