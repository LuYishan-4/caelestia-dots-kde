// SPDX-License-Identifier: GPL-3.0-only
#pragma once

// Takes screen corners away from KWin for as long as Caelestia wants them.
//
// A corner can be claimed by several unrelated bits of KWin config at once:
// the built-in actions in [ElectricBorders], and a BorderActivate int-list —
// pointer or touch — in any [Effect-*], [Script-*] or [TabBox] group. KDE's
// own Overview effect defaults to [Effect-overview] BorderActivate=7
// (top-left), which is why the overview kept firing alongside ours no matter
// what [ElectricBorders] said.
//
// So claiming a corner means neutralising every claimant of that edge, not
// one key. The originals are stashed on disk and put back on release, on clean
// exit, and — if the shell died without getting the chance — on next startup,
// the same shape as the KWin global shortcuts we steal in globalshortcut.cpp.
//
// Nothing here takes effect until KWin re-reads kwinrc, and it only does that
// on an explicit org.kde.KWin.reconfigure() call: kwriteconfig6's --notify
// emits org.kde.kconfig.notify, which KWin does not listen to for kwinrc.

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QQmlEngine>
#include <QSet>
#include <QTimer>

namespace caelestia::services {

class ScreenEdges : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    QML_ELEMENT
    QML_SINGLETON

public:
    /// KWin's ElectricBorder enum, corners only — the only edges Caelestia
    /// ever asks for. Values match KWin so they can be written straight out.
    enum Corner {
        TopRight = 1,
        BottomRight = 3,
        BottomLeft = 5,
        TopLeft = 7,
    };
    Q_ENUM(Corner)

    explicit ScreenEdges(QObject* parent = nullptr);
    ~ScreenEdges() override;

    /// Whether KWin is on the bus to reconfigure at all.
    bool available() const;

    /// Take @p corner from KWin, stashing whatever held it. Idempotent.
    Q_INVOKABLE void claim(int corner);

    /// Give @p corner back exactly as it was found. Idempotent.
    Q_INVOKABLE void release(int corner);

    /// Whether Caelestia currently holds @p corner.
    Q_INVOKABLE bool holds(int corner) const;

private:
    /// Everything in kwinrc that had a hold on one corner, verbatim, so it can
    /// be written back byte for byte.
    struct StolenEdge {
        int corner = 0;
        // group -> key -> original value as it appeared in kwinrc. A null
        // QString means the key was absent and must be deleted on restore.
        QHash<QString, QHash<QString, QString>> entries;
    };

    void recoverFromCrash();
    void stealCorner(int corner);
    void restoreCorner(const StolenEdge& stolen);
    void restoreAll();

    QJsonObject toJson(const StolenEdge& stolen) const;
    StolenEdge fromJson(const QJsonObject& obj) const;

    void persist() const;
    void scheduleReconfigure();

    QHash<int, StolenEdge> m_stolen;
    QTimer* m_reconfigureTimer;
    bool m_restored = false;
};

} // namespace caelestia::services
