// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QVariantMap>
#include <algorithm>
#include <limits>

namespace caelestia::layouts {

/**
 * Slides a finished layout so its bounding box sits centred in the area.
 *
 * Both packers place rows as they go and try to keep the block centred while
 * they do it, which only works out when nothing gets clamped on the way. Once a
 * row is scaled down to fit the width, the running offsets no longer describe
 * where the block actually ended up — GNOME's leaves a two-row layout flush
 * against the bottom with the whole slack piled up above it. Measuring the
 * result and centring that is exact whatever the packer did, and costs one more
 * pass over a handful of rects.
 */
inline void centreLayout(QVariantMap& layout, double areaWidth, double areaHeight) {
    if (layout.isEmpty()) {
        return;
    }

    double minX = std::numeric_limits<double>::max();
    double minY = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double maxY = std::numeric_limits<double>::lowest();

    for (auto it = layout.constBegin(); it != layout.constEnd(); ++it) {
        const QVariantMap box = it.value().toMap();
        const double x = box.value(QStringLiteral("x")).toDouble();
        const double y = box.value(QStringLiteral("y")).toDouble();
        minX = std::min(minX, x);
        minY = std::min(minY, y);
        maxX = std::max(maxX, x + box.value(QStringLiteral("width")).toDouble());
        maxY = std::max(maxY, y + box.value(QStringLiteral("height")).toDouble());
    }

    const double dx = (areaWidth - (maxX - minX)) / 2.0 - minX;
    const double dy = (areaHeight - (maxY - minY)) / 2.0 - minY;
    if (dx == 0.0 && dy == 0.0) {
        return;
    }

    for (auto it = layout.begin(); it != layout.end(); ++it) {
        QVariantMap box = it.value().toMap();
        box[QStringLiteral("x")] = box.value(QStringLiteral("x")).toDouble() + dx;
        box[QStringLiteral("y")] = box.value(QStringLiteral("y")).toDouble() + dy;
        it.value() = box;
    }
}

} // namespace caelestia::layouts
