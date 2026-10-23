#ifndef THEMEDPROGRESSBAR_H
#define THEMEDPROGRESSBAR_H

#include <QWidget>
#include <QTimer>
#include <QPainter>
#include <QPainterPath>
#include <QtGlobal>
#include <cmath>
#include "../Resources.h"
#include "ProgressShine.h"

/**
 * @brief Paints a compact themed determinate or indeterminate progress bar.
 * @note Resources palette roles provide defaults; surface-specific QSS may override colors.
 */
class ThemedProgressBar : public QWidget
{
    Q_OBJECT
    /** @brief Optional surface-specific track color supplied by the installed theme. */
    Q_PROPERTY(QColor trackColor MEMBER _trackColor)
    /** @brief Optional surface-specific gradient start supplied by the installed theme. */
    Q_PROPERTY(QColor startColor MEMBER _startColor)
    /** @brief Optional surface-specific gradient end supplied by the installed theme. */
    Q_PROPERTY(QColor endColor MEMBER _endColor)
    /** @brief Optional surface-specific outline supplied by the installed theme. */
    Q_PROPERTY(QColor outlineColor MEMBER _outlineColor)
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

    /** @brief Enables a full stationary busy fill with the shared loading light sweep. */
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
        const auto colors = Resources::themePalette();
        auto outline = colors.value(QStringLiteral("shadow"));
        outline.setAlpha(25);
        const qreal fraction = (_value - _min) / qreal(_max - _min);
        Resources::detail::paintLoadingProgress(p, QRectF(rect()), fraction, _phase, _busy,
            _roundedEnd, isEnabled(),
            _trackColor.isValid() ? _trackColor : colors.value(QStringLiteral("progressTrack")),
            _startColor.isValid() ? _startColor : colors.value(QStringLiteral("progressFill")),
            _endColor.isValid() ? _endColor : colors.value(QStringLiteral("progressFill")),
            _outlineColor.isValid() ? _outlineColor : outline);
    }

private:
    int _min = 0;
    int _max = 100;
    int _value = 0;

    bool _busy = false;
    bool _roundedEnd = true;
    QTimer _animationTimer;
    QColor _trackColor;
    QColor _startColor;
    QColor _endColor;
    QColor _outlineColor;

    qreal _phase = 0.;
};

#endif // THEMEDPROGRESSBAR_H
