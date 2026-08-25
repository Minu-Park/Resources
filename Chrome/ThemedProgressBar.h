#ifndef THEMEDPROGRESSBAR_H
#define THEMEDPROGRESSBAR_H

#include <QWidget>
#include <QTimer>
#include <QPainter>
#include <QPainterPath>
#include <QtGlobal>
#include <cmath>

/**
 * @brief Paints a compact themed determinate or indeterminate progress bar.
 * @note The widget owns only its geometry and paint behavior; colors remain local
 *       fallback values because this custom-painted surface is not QSS-stylable.
 */
class ThemedProgressBar : public QWidget
{
    Q_OBJECT
public:
    /** @brief Creates a progress bar with an expanding width and compact height. */
    explicit ThemedProgressBar(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedHeight(8);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        _animationTimer.setInterval(16);
        QObject::connect(&_animationTimer, &QTimer::timeout, this, [this]{
            _phase += 0.005;
            if (_phase > 1000.) _phase = 0.;
            update();
        });
        _animationTimer.start();
    }

    /** @brief Enables or disables the animated busy presentation. */
    void setBusy(bool on) {
        _busy = on;
        update();
    }

    /**
     * @brief Sets the determinate progress value.
     * @param value Value in the inclusive 0-100 range; values outside it are clamped.
     */
    void setValue(int value) {
        _value = qBound(_min, value, _max);
        update();
    }

    /** @brief Returns the current clamped determinate progress value. */
    int getValue() const { return _value; }

    /**
     * @brief Configures a rounded trailing cap for the determinate fill.
     * @param enabled Whether the leading edge of the filled portion is rounded.
     * @note A narrow fill becomes a compact pill so the cap remains antialiased at
     *       the beginning of a download.
     */
    void setRoundedEnd(bool enabled) {
        _roundedEnd = enabled;
        update();
    }

protected:
    /** @brief Paints the track, progress fill, animated shine, and outline. */
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const QRectF r = rect().toRectF().adjusted(0.5, 0.5, -0.5, -0.5);
        const qreal radius = r.height() / 2.0;

        // Background
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 18));
        p.drawRoundedRect(r, radius, radius);

        // Progress ratio
        qreal t = 0.0;
        if (_max > _min) t = (_value - _min) / qreal(_max - _min);
        t = qBound<qreal>(0.0, t, 1.0);

        // Fill rect (always based on value)
        QRectF fill = r;
        fill.setWidth(r.width() * t);

        const QRectF paintRect = fill;

        QPainterPath progressPath;
        if (fill.width() > 0.0 && _roundedEnd) {
            if (fill.width() <= 2.0 * radius) {
                progressPath.addRoundedRect(fill, fill.width() / 2.0, fill.width() / 2.0);
            } else {
                const qreal capLeft = fill.right() - 2.0 * radius;
                progressPath.moveTo(fill.left(), fill.top());
                progressPath.lineTo(fill.right() - radius, fill.top());
                progressPath.arcTo(QRectF(capLeft, fill.top(), 2.0 * radius, 2.0 * radius),
                                   90.0,
                                   -180.0);
                progressPath.lineTo(fill.left(), fill.bottom());
                progressPath.closeSubpath();
            }
        } else {
            progressPath.addRect(fill);
        }

        // Fill gradient (mapped across full width for smooth transitions)
        QLinearGradient g(r.topLeft(), r.topRight());
        g.setColorAt(0.0, _startColor);
        g.setColorAt(1.0, _endColor);

        p.save();

        // Clip to the background track's rounded bounds
        QPainterPath clip;
        clip.addRoundedRect(r, radius, radius);
        p.setClipPath(clip);

        p.setBrush(g);
        p.drawPath(progressPath);

        const qreal w = paintRect.width();
        if (w > 1.0) {
            qreal localPhase = std::fmod(_phase, 1.0);
            if (localPhase < 0) localPhase += 1.0;

            const qreal bandW = w * (_busy ? 0.25 : 0.18);
            const qreal x = paintRect.left() + (localPhase * (w + bandW * 2) - bandW);

            QLinearGradient shine(QPointF(x, 0), QPointF(x + bandW, 0));
            shine.setColorAt(0.0, QColor(255, 255, 255, 0));
            shine.setColorAt(0.5, QColor(255, 255, 255, _busy ? 90 : 70));
            shine.setColorAt(1.0, QColor(255, 255, 255, 0));

            p.save();
            p.setClipPath(progressPath, Qt::IntersectClip);
            p.setCompositionMode(QPainter::CompositionMode_Screen);
            p.setBrush(shine);
            p.drawRect(paintRect);
            p.restore();
        }

        p.restore();

        p.setPen(QPen(QColor(0, 0, 0, 25), 1));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(r, radius, radius);
    }

private:
    int _min = 0;
    int _max = 100;
    int _value = 0;

    bool _busy = false;
    bool _roundedEnd = false;
    QTimer _animationTimer;
    QColor _startColor = QColor(0x1e5ea6);
    QColor _endColor   = QColor(0x0f3259);

    qreal _phase = 0.;
};

#endif // THEMEDPROGRESSBAR_H
