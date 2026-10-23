#pragma once

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <cmath>
#include "../Resources.h"

namespace Resources::detail {

/** @brief Paints the loading-bar light sweep inside a stationary progress fill. */
inline void paintProgressShine(QPainter& painter, const QRectF& fill,
                               const QPainterPath& fillPath, qreal phase, bool busy,
                               Qt::Orientation orientation = Qt::Horizontal)
{
    const qreal extent = orientation == Qt::Horizontal ? fill.width() : fill.height();
    if (extent <= 1.0) return;
    qreal localPhase = std::fmod(phase, 1.0);
    if (localPhase < 0.0) localPhase += 1.0;
    const qreal band = extent * (busy ? 0.25 : 0.18);
    const qreal offset = localPhase * (extent + band * 2.0) - band;
    const QPointF start = orientation == Qt::Horizontal
        ? QPointF(fill.left() + offset, fill.top()) : QPointF(fill.left(), fill.top() + offset);
    const QPointF end = orientation == Qt::Horizontal
        ? start + QPointF(band, 0) : start + QPointF(0, band);
    QLinearGradient shine(start, end);
    shine.setColorAt(0.0, QColor(255, 255, 255, 0));
    shine.setColorAt(0.5, QColor(255, 255, 255, busy ? 90 : 70));
    shine.setColorAt(1.0, QColor(255, 255, 255, 0));
    painter.save();
    painter.setClipPath(fillPath, Qt::IntersectClip);
    painter.setCompositionMode(QPainter::CompositionMode_Screen);
    painter.fillRect(fill, shine);
    painter.restore();
}

/** @brief Paints the complete loading-bar design and returns its fill path for optional text clipping. */
inline QPainterPath paintLoadingProgress(QPainter& painter, const QRectF& bounds, qreal fraction,
                                        qreal phase, bool busy, bool roundedEnd, bool enabled,
                                        const QColor& trackColor, const QColor& startColor,
                                        const QColor& endColor, const QColor& outlineColor)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QRectF track = bounds.adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal radius = track.height() / 2.0;
    painter.setPen(Qt::NoPen);
    const auto disabled = enabled ? Resources::ThemePalette{} : Resources::themePalette();
    painter.setBrush(enabled ? trackColor : disabled.value(QStringLiteral("disabledSurface")));
    painter.drawRoundedRect(track, radius, radius);
    QRectF fill = track;
    fill.setWidth(track.width() * (busy ? 1.0 : qBound(0.0, fraction, 1.0)));
    QPainterPath fillPath;
    if (fill.width() > 0.0 && roundedEnd) {
        if (fill.width() <= 2.0 * radius) {
            fillPath.addRoundedRect(fill, fill.width() / 2.0, fill.width() / 2.0);
        } else {
            fillPath.moveTo(fill.left(), fill.top());
            fillPath.lineTo(fill.right() - radius, fill.top());
            fillPath.arcTo(QRectF(fill.right() - 2.0 * radius, fill.top(), 2.0 * radius, 2.0 * radius), 90.0, -180.0);
            fillPath.lineTo(fill.left(), fill.bottom());
            fillPath.closeSubpath();
        }
    } else if (fill.width() > 0.0) {
        fillPath.addRect(fill);
    }
    QLinearGradient gradient(track.topLeft(), track.topRight());
    gradient.setColorAt(0.0, enabled ? startColor : disabled.value(QStringLiteral("disabledText")));
    gradient.setColorAt(1.0, enabled ? endColor : disabled.value(QStringLiteral("disabledText")));
    QPainterPath trackPath;
    trackPath.addRoundedRect(track, radius, radius);
    painter.save();
    painter.setClipPath(trackPath);
    painter.fillPath(fillPath, gradient);
    if (enabled) paintProgressShine(painter, fill, fillPath, phase, busy);
    painter.restore();
    painter.setPen(QPen(outlineColor, 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(track, radius, radius);
    painter.restore();
    return fillPath;
}

} // namespace Resources::detail
