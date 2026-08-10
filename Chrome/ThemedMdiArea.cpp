#include "ThemedMdiArea.h"

#include "ThemedMdiContainer.h"

#include <QColor>
#include <QChildEvent>
#include <QDebug>
#include <QElapsedTimer>
#include <QEvent>
#include <QImage>
#include <QMdiSubWindow>
#include <QPaintEvent>
#include <QPainter>
#include <QPixmap>
#include <QRegion>
#include <QSet>
#include <QStyle>
#include <QTimer>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

namespace {

constexpr int shadowProfileIntervalMs = 5000;

bool environmentFlagEnabled(const char* name)
{
    const QByteArray value = qgetenv(name).trimmed().toLower();
    return value == QByteArrayLiteral("1")
        || value == QByteArrayLiteral("true")
        || value == QByteArrayLiteral("on")
        || value == QByteArrayLiteral("yes");
}

struct TimingSamples {
    void add(qint64 nanoseconds)
    {
        nanoseconds = std::max<qint64>(nanoseconds, 0);
        ++count;
        totalNanoseconds += static_cast<quint64>(nanoseconds);
        maximumNanoseconds = std::max(maximumNanoseconds, nanoseconds);
        values.push_back(nanoseconds);
    }

    [[nodiscard]] qint64 percentile(double fraction) const
    {
        if (values.empty()) {
            return 0;
        }

        std::vector<qint64> ordered(values);
        const std::size_t index = std::min(
            ordered.size() - 1,
            static_cast<std::size_t>(std::ceil(fraction * ordered.size()) - 1));
        std::nth_element(ordered.begin(), ordered.begin() + index, ordered.end());
        return ordered[index];
    }

    void reset()
    {
        count = 0;
        totalNanoseconds = 0;
        maximumNanoseconds = 0;
        values.clear();
    }

    quint64 count = 0;
    quint64 totalNanoseconds = 0;
    qint64 maximumNanoseconds = 0;
    std::vector<qint64> values;
};

class ScopedTimingSample {
public:
    explicit ScopedTimingSample(TimingSamples* destination)
        : _destination(destination)
    {
        if (_destination) {
            _timer.start();
        }
    }

    ~ScopedTimingSample()
    {
        if (_destination) {
            _destination->add(_timer.nsecsElapsed());
        }
    }

private:
    TimingSamples* _destination = nullptr;
    QElapsedTimer _timer;
};

qreal signedDistanceToRoundedRect(const QPointF& point, const QRectF& rect, qreal radius)
{
    radius = std::clamp(radius, 0.0, std::min(rect.width(), rect.height()) / 2.0);
    const QPointF center = rect.center();
    const qreal halfWidth = rect.width() / 2.0;
    const qreal halfHeight = rect.height() / 2.0;
    const qreal dx = std::abs(point.x() - center.x()) - (halfWidth - radius);
    const qreal dy = std::abs(point.y() - center.y()) - (halfHeight - radius);
    const qreal outside = std::hypot(std::max(dx, 0.0), std::max(dy, 0.0));
    const qreal inside = std::min(std::max(dx, dy), 0.0);
    return outside + inside - radius;
}

QRgb premultipliedPixel(const QColor& color, int alpha)
{
    alpha = std::clamp(alpha, 0, 255);
    return qRgba((color.red() * alpha + 127) / 255,
                 (color.green() * alpha + 127) / 255,
                 (color.blue() * alpha + 127) / 255,
                 alpha);
}

} // namespace

class ThemedMdiArea::ShadowDiagnostics {
public:
    ShadowDiagnostics()
    {
        sampleTimer.start();
    }

    void resetWindow()
    {
        paints.reset();
        cacheBuilds.reset();
        synchronizations.reset();
        geometryChanges = 0;
        maskUpdates = 0;
        cacheInvalidations = 0;
        queuedSynchronizations = 0;
        coalescedSynchronizations = 0;
        stackUnderCalls = 0;
    }

    QElapsedTimer sampleTimer;
    TimingSamples paints;
    TimingSamples cacheBuilds;
    TimingSamples synchronizations;
    quint64 geometryChanges = 0;
    quint64 maskUpdates = 0;
    quint64 cacheInvalidations = 0;
    quint64 queuedSynchronizations = 0;
    quint64 coalescedSynchronizations = 0;
    quint64 stackUnderCalls = 0;
};

class ThemedMdiShadowWidget final : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QColor shadowColor READ shadowColor WRITE setShadowColor)
    Q_PROPERTY(int shadowExtent READ shadowExtent WRITE setShadowExtent)
    Q_PROPERTY(int cornerRadius READ cornerRadius WRITE setCornerRadius)
    Q_PROPERTY(int offsetX READ offsetX WRITE setOffsetX)
    Q_PROPERTY(int offsetY READ offsetY WRITE setOffsetY)
    Q_PROPERTY(QObject* targetWindow READ targetWindow)
    Q_PROPERTY(qulonglong cacheGeneration READ cacheGeneration)
    Q_PROPERTY(int cacheCornerAlpha READ cacheCornerAlpha)
    Q_PROPERTY(int cacheInteriorAlpha READ cacheInteriorAlpha)
    Q_PROPERTY(bool profilingEnabled READ profilingEnabled)
    Q_PROPERTY(qulonglong profilePaintCount READ profilePaintCount)
    Q_PROPERTY(qulonglong profileCacheBuildCount READ profileCacheBuildCount)

public:
    explicit ThemedMdiShadowWidget(QWidget* parent,
                                  QMdiSubWindow* target,
                                  ThemedMdiArea::ShadowDiagnostics* diagnostics)
        : QWidget(parent)
        , _target(target)
        , _diagnostics(diagnostics)
    {
        setObjectName(QStringLiteral("ThemedMdiShadow"));
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setFocusPolicy(Qt::NoFocus);
        setAcceptDrops(false);
        setAutoFillBackground(false);
        hide();
    }

    [[nodiscard]] QColor shadowColor() const noexcept { return _shadowColor; }
    [[nodiscard]] int shadowExtent() const noexcept { return _shadowExtent; }
    [[nodiscard]] int cornerRadius() const noexcept { return _cornerRadius; }
    [[nodiscard]] int offsetX() const noexcept { return _offsetX; }
    [[nodiscard]] int offsetY() const noexcept { return _offsetY; }
    [[nodiscard]] QObject* targetWindow() const noexcept { return _target; }
    [[nodiscard]] qulonglong cacheGeneration() const noexcept { return _cacheGeneration; }
    [[nodiscard]] int cacheCornerAlpha() const
    {
        const int padding = shadowPadding();
        return cacheAlphaAt(QPointF(padding, padding));
    }
    [[nodiscard]] int cacheInteriorAlpha() const
    {
        const int padding = shadowPadding();
        return cacheAlphaAt(QPointF(padding + _cornerRadius,
                                    padding + _cornerRadius));
    }
    [[nodiscard]] bool profilingEnabled() const noexcept { return _diagnostics != nullptr; }
    [[nodiscard]] qulonglong profilePaintCount() const noexcept
    {
        return _diagnostics ? _diagnostics->paints.count : 0;
    }
    [[nodiscard]] qulonglong profileCacheBuildCount() const noexcept
    {
        return _diagnostics ? _diagnostics->cacheBuilds.count : 0;
    }
    [[nodiscard]] quint64 cachePixelBytes() const noexcept
    {
        return static_cast<quint64>(_cache.width())
            * static_cast<quint64>(_cache.height()) * 4U;
    }

    void setShadowColor(const QColor& color)
    {
        if (!color.isValid() || _shadowColor == color) {
            return;
        }
        _shadowColor = color;
        invalidateCache();
    }

    void setShadowExtent(int extent)
    {
        extent = std::clamp(extent, 1, 64);
        if (_shadowExtent == extent) {
            return;
        }
        _shadowExtent = extent;
        invalidateCache(true);
    }

    void setCornerRadius(int radius)
    {
        radius = std::clamp(radius, 0, 64);
        if (_cornerRadius == radius) {
            return;
        }
        _cornerRadius = radius;
        invalidateCache(true);
    }

    void setOffsetX(int offset)
    {
        offset = std::clamp(offset, -32, 32);
        if (_offsetX == offset) {
            return;
        }
        _offsetX = offset;
        invalidateCache(true);
    }

    void setOffsetY(int offset)
    {
        offset = std::clamp(offset, -32, 32);
        if (_offsetY == offset) {
            return;
        }
        _offsetY = offset;
        invalidateCache(true);
    }

    [[nodiscard]] int shadowPadding() const noexcept
    {
        return _shadowExtent + std::max(std::abs(_offsetX), std::abs(_offsetY));
    }

    void setMetricsChangedCallback(std::function<void()> callback)
    {
        _metricsChanged = std::move(callback);
    }

    bool setTrackedGeometry(const QRect& targetRect,
                            const QRect& fullShadowRect,
                            const QRect& clippedShadowRect)
    {
        const QSize previousSize = size();
        if (geometry() != clippedShadowRect) {
            setGeometry(clippedShadowRect);
            if (_diagnostics) {
                ++_diagnostics->geometryChanges;
            }
        }

        const QPoint clippedOrigin = clippedShadowRect.topLeft();
        const QRect outputRect(fullShadowRect.topLeft() - clippedOrigin,
                               fullShadowRect.size());
        const QRect localTargetRect(targetRect.topLeft() - clippedOrigin,
                                    targetRect.size());
        if (_fullOutputRect != outputRect
            || _localTargetRect != localTargetRect
            || previousSize != size()
            || _maskedCornerRadius != _cornerRadius) {
            _fullOutputRect = outputRect;
            _localTargetRect = localTargetRect;
            _maskedCornerRadius = _cornerRadius;
            if (_diagnostics) {
                ++_diagnostics->maskUpdates;
            }
            QRegion shadowRegion(rect());
            shadowRegion -= QRegion(_localTargetRect);
            if (shadowRegion.isEmpty()) {
                clearMask();
                return false;
            }

            // Keep the broad target interior out of the backing-store damage
            // region, but admit small corner guards so the ARGB cache can draw
            // the target's rounded silhouette without a rectangular notch.
            const int effectiveRadius = std::clamp(
                _cornerRadius, 0,
                std::min(_localTargetRect.width(), _localTargetRect.height()) / 2);
            if (effectiveRadius > 0) {
                const int span = effectiveRadius + 1;
                const int right = _localTargetRect.right() - span + 1;
                const int bottom = _localTargetRect.bottom() - span + 1;
                shadowRegion += QRegion(QRect(_localTargetRect.topLeft(), QSize(span, span)));
                shadowRegion += QRegion(QRect(right, _localTargetRect.top(), span, span));
                shadowRegion += QRegion(QRect(_localTargetRect.left(), bottom, span, span));
                shadowRegion += QRegion(QRect(right, bottom, span, span));
                shadowRegion &= QRegion(rect());
            }
            setMask(shadowRegion);
            update();
        }
        return !mask().isEmpty();
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);
        ScopedTimingSample paintTiming(_diagnostics ? &_diagnostics->paints : nullptr);
        ensureCache();
        if (_cache.isNull() || !_fullOutputRect.isValid()) {
            return;
        }

        QPainter painter(this);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        drawNineSlice(painter, QRectF(_fullOutputRect));
    }

    void changeEvent(QEvent* event) override
    {
        QWidget::changeEvent(event);
        if (event->type() == QEvent::StyleChange
            || event->type() == QEvent::PaletteChange
            || event->type() == QEvent::ApplicationPaletteChange) {
            invalidateCache(true);
        }
    }

private:
    [[nodiscard]] int cacheAlphaAt(const QPointF& logicalPoint) const
    {
        if (_cache.isNull() || _cacheDpr <= 0.0) {
            return -1;
        }
        const QImage image = _cache.toImage();
        const int x = std::clamp(
            qFloor(logicalPoint.x() * _cacheDpr), 0, image.width() - 1);
        const int y = std::clamp(
            qFloor(logicalPoint.y() * _cacheDpr), 0, image.height() - 1);
        return qAlpha(image.pixel(x, y));
    }

    void invalidateCache(bool geometryChanged = false)
    {
        if (_diagnostics) {
            ++_diagnostics->cacheInvalidations;
        }
        _cache = QPixmap();
        _cacheDpr = 0.0;
        update();
        if (geometryChanged && _metricsChanged) {
            _metricsChanged();
        }
    }

    void ensureCache()
    {
        const qreal currentDpr = std::max(devicePixelRatioF(), 1.0);
        if (!_cache.isNull() && std::abs(_cacheDpr - currentDpr) < 0.001) {
            return;
        }

        ScopedTimingSample cacheTiming(
            _diagnostics ? &_diagnostics->cacheBuilds : nullptr);

        const int padding = shadowPadding();
        const int logicalMargin = padding + _cornerRadius;
        const int logicalSize = logicalMargin * 2 + 1;
        const int physicalSize = std::max(1, qCeil(logicalSize * currentDpr));
        QImage image(physicalSize, physicalSize, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);

        const QRectF coreRect(padding, padding,
                              logicalSize - padding * 2,
                              logicalSize - padding * 2);
        const QRectF shiftedCore = coreRect.translated(_offsetX, _offsetY);
        const qreal sigma = std::max(1.0, _shadowExtent / 3.0);
        const qreal maximumAlpha = _shadowColor.alphaF();

        for (int y = 0; y < physicalSize; ++y) {
            auto* scanLine = reinterpret_cast<QRgb*>(image.scanLine(y));
            for (int x = 0; x < physicalSize; ++x) {
                const QPointF point((x + 0.5) / currentDpr,
                                    (y + 0.5) / currentDpr);
                const qreal targetDistance = signedDistanceToRoundedRect(
                    point, coreRect, _cornerRadius);
                // Leave the one-physical-pixel boundary band intact. The
                // actual frame above this sibling owns edge antialiasing;
                // applying fractional coverage here as well would create a
                // bright seam at the rounded corners.
                if (targetDistance < (-0.5 / currentDpr)) {
                    continue;
                }
                const qreal signedDistance = signedDistanceToRoundedRect(
                    point, shiftedCore, _cornerRadius);
                const qreal distance = std::max(0.0, signedDistance);
                const qreal falloff = std::exp(-(distance * distance)
                                               / (2.0 * sigma * sigma));
                const int alpha = qRound(255.0 * maximumAlpha * falloff);
                if (alpha > 0) {
                    scanLine[x] = premultipliedPixel(_shadowColor, alpha);
                }
            }
        }

        image.setDevicePixelRatio(currentDpr);
        _cache = QPixmap::fromImage(image);
        _cache.setDevicePixelRatio(currentDpr);
        _cacheDpr = currentDpr;
        _sourceMarginPixels = std::clamp(
            qRound(logicalMargin * currentDpr), 0, physicalSize / 2);
        _targetMargin = logicalMargin;
        ++_cacheGeneration;
    }

    void drawNineSlice(QPainter& painter, const QRectF& targetRect)
    {
        const int sourceWidth = _cache.width();
        const int sourceHeight = _cache.height();
        const int sourceMarginX = std::min(_sourceMarginPixels, sourceWidth / 2);
        const int sourceMarginY = std::min(_sourceMarginPixels, sourceHeight / 2);
        const qreal targetMarginX = std::min<qreal>(_targetMargin, targetRect.width() / 2.0);
        const qreal targetMarginY = std::min<qreal>(_targetMargin, targetRect.height() / 2.0);

        const int sourceX[] = {0, sourceMarginX, sourceWidth - sourceMarginX, sourceWidth};
        const int sourceY[] = {0, sourceMarginY, sourceHeight - sourceMarginY, sourceHeight};
        const qreal targetX[] = {targetRect.x(),
                                 targetRect.x() + targetMarginX,
                                 targetRect.x() + targetRect.width() - targetMarginX,
                                 targetRect.x() + targetRect.width()};
        const qreal targetY[] = {targetRect.y(),
                                 targetRect.y() + targetMarginY,
                                 targetRect.y() + targetRect.height() - targetMarginY,
                                 targetRect.y() + targetRect.height()};

        for (int row = 0; row < 3; ++row) {
            for (int column = 0; column < 3; ++column) {
                if (row == 1 && column == 1) {
                    continue;
                }
                const QRect sourceRect(sourceX[column], sourceY[row],
                                       sourceX[column + 1] - sourceX[column],
                                       sourceY[row + 1] - sourceY[row]);
                const QRectF destinationRect(targetX[column], targetY[row],
                                             targetX[column + 1] - targetX[column],
                                             targetY[row + 1] - targetY[row]);
                if (sourceRect.isValid() && destinationRect.isValid()) {
                    painter.drawPixmap(destinationRect, _cache, QRectF(sourceRect));
                }
            }
        }
    }

    QColor _shadowColor = QColor(22, 32, 43, 24);
    int _shadowExtent = 8;
    int _cornerRadius = 12;
    int _offsetX = 0;
    int _offsetY = 1;
    QRect _fullOutputRect;
    QRect _localTargetRect;
    int _maskedCornerRadius = -1;
    QPixmap _cache;
    qreal _cacheDpr = 0.0;
    int _sourceMarginPixels = 0;
    int _targetMargin = 0;
    qulonglong _cacheGeneration = 0;
    std::function<void()> _metricsChanged;
    QPointer<QMdiSubWindow> _target;
    ThemedMdiArea::ShadowDiagnostics* _diagnostics = nullptr;
};

struct ThemedMdiArea::ShadowEntry {
    QPointer<QMdiSubWindow> target;
    QPointer<ThemedMdiShadowWidget> shadow;
    QPointer<ThemedMdiContainer> container;
    QMetaObject::Connection targetDestroyedConnection;
    QMetaObject::Connection contentAttachmentConnection;
    QMetaObject::Connection cornerRadiusConnection;
};

ThemedMdiArea::ThemedMdiArea(QWidget* parent)
    : QMdiArea(parent)
{
    if (environmentFlagEnabled("RESOURCES_MDI_SHADOW_PROFILE")) {
        _shadowDiagnostics = std::make_unique<ShadowDiagnostics>();
        auto* reportTimer = new QTimer(this);
        reportTimer->setInterval(shadowProfileIntervalMs);
        reportTimer->setTimerType(Qt::CoarseTimer);
        connect(reportTimer, &QTimer::timeout,
                this, &ThemedMdiArea::reportShadowDiagnostics);
        reportTimer->start();
        qInfo().noquote() << QStringLiteral(
            "[MdiShadowProfile] enabled sampleIntervalMs=%1")
                                 .arg(shadowProfileIntervalMs);
    }

    installShadowOnViewport(viewport());

    connect(this, &QMdiArea::subWindowActivated, this,
            [this](QMdiSubWindow*) {
                if (_shadowMode == ShadowMode::Disabled) {
                    hideShadows();
                    return;
                }
                synchronizeAllShadows(true);
            });
}

ThemedMdiArea::~ThemedMdiArea()
{
    clearShadowEntries();
    if (_trackedViewport) {
        _trackedViewport->removeEventFilter(this);
    }
}

void ThemedMdiArea::setShadowMode(ShadowMode mode)
{
    if (_shadowMode == mode) {
        return;
    }

    _shadowMode = mode;
    if (_shadowMode == ShadowMode::Disabled) {
        clearShadowEntries();
        return;
    }

    synchronizeAllShadows(true);
}

ThemedMdiArea::ShadowMode ThemedMdiArea::shadowMode() const noexcept
{
    return _shadowMode;
}

bool ThemedMdiArea::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == _trackedViewport) {
        switch (event->type()) {
        case QEvent::Hide:
            hideShadows();
            break;
        case QEvent::Close:
            hideShadows();
            scheduleShadowSynchronization();
            break;
        case QEvent::Resize:
            synchronizeAllShadows(false);
            break;
        case QEvent::Show:
            scheduleShadowSynchronization();
            break;
        case QEvent::ChildAdded:
        case QEvent::ChildRemoved: {
            auto* childEvent = static_cast<QChildEvent*>(event);
            if (qobject_cast<QMdiSubWindow*>(childEvent->child())) {
                scheduleShadowSynchronization();
            }
            break;
        }
        default:
            break;
        }
    }

    if (auto* target = qobject_cast<QMdiSubWindow*>(watched)) {
        if (ShadowEntry* entry = shadowEntryFor(target)) {
            switch (event->type()) {
            case QEvent::Move:
            case QEvent::Resize: {
                ScopedTimingSample synchronizationTiming(
                    _shadowDiagnostics ? &_shadowDiagnostics->synchronizations : nullptr);
                synchronizeShadow(*entry);
                break;
            }
            case QEvent::Hide:
                hideShadowFor(target);
                break;
            case QEvent::Destroy:
                hideShadowFor(target);
                scheduleShadowSynchronization();
                break;
            case QEvent::Close:
                hideShadowFor(target);
                scheduleShadowSynchronization();
                break;
            case QEvent::WindowStateChange:
            case QEvent::ParentChange:
            case QEvent::WinIdChange:
                hideShadowFor(target);
                scheduleShadowSynchronization();
                break;
            case QEvent::Show:
            case QEvent::ZOrderChange:
                scheduleShadowSynchronization();
                break;
            default:
                break;
            }
        }
    }

    return QMdiArea::eventFilter(watched, event);
}

void ThemedMdiArea::changeEvent(QEvent* event)
{
    QMdiArea::changeEvent(event);
    if (event->type() == QEvent::StyleChange
        || event->type() == QEvent::PaletteChange
        || event->type() == QEvent::ApplicationPaletteChange) {
        scheduleShadowSynchronization();
    }
}

void ThemedMdiArea::setupViewport(QWidget* newViewport)
{
    if (_trackedViewport) {
        _trackedViewport->removeEventFilter(this);
    }
    clearShadowEntries();

    QMdiArea::setupViewport(newViewport);
    installShadowOnViewport(newViewport);
    scheduleShadowSynchronization();
}

void ThemedMdiArea::installShadowOnViewport(QWidget* newViewport)
{
    _trackedViewport = newViewport;
    if (_trackedViewport) {
        _trackedViewport->installEventFilter(this);
    }
}

void ThemedMdiArea::clearShadowEntries()
{
    for (const std::unique_ptr<ShadowEntry>& entry : _shadowEntries) {
        QObject::disconnect(entry->targetDestroyedConnection);
        QObject::disconnect(entry->contentAttachmentConnection);
        QObject::disconnect(entry->cornerRadiusConnection);
        if (entry->shadow) {
            entry->shadow->setMetricsChangedCallback({});
            delete entry->shadow;
        }
    }
    _shadowEntries.clear();
}

void ThemedMdiArea::refreshShadowEntries()
{
    if (_shadowMode == ShadowMode::Disabled || !_trackedViewport) {
        clearShadowEntries();
        return;
    }

    const QList<QMdiSubWindow*> windows = subWindowList(QMdiArea::StackingOrder);
    QSet<QMdiSubWindow*> targets;
    for (QMdiSubWindow* target : windows) {
        if (target && target->parentWidget() == _trackedViewport) {
            targets.insert(target);
        }
    }

    _shadowEntries.erase(
        std::remove_if(
            _shadowEntries.begin(), _shadowEntries.end(),
            [this, &targets](const std::unique_ptr<ShadowEntry>& entry) {
                if (entry->target && targets.contains(entry->target)) {
                    return false;
                }
                QObject::disconnect(entry->targetDestroyedConnection);
                QObject::disconnect(entry->contentAttachmentConnection);
                QObject::disconnect(entry->cornerRadiusConnection);
                if (entry->shadow) {
                    entry->shadow->setMetricsChangedCallback({});
                    delete entry->shadow;
                }
                return true;
            }),
        _shadowEntries.end());

    for (QMdiSubWindow* target : windows) {
        if (!targets.contains(target)) {
            continue;
        }
        ShadowEntry* entry = shadowEntryFor(target);
        if (!entry) {
            auto newEntry = std::make_unique<ShadowEntry>();
            newEntry->target = target;
            newEntry->shadow = new ThemedMdiShadowWidget(
                _trackedViewport, target, _shadowDiagnostics.get());
            newEntry->shadow->setMetricsChangedCallback([this]() {
                scheduleShadowSynchronization();
            });
            const QPointer<ThemedMdiShadowWidget> shadow = newEntry->shadow;
            newEntry->targetDestroyedConnection = connect(
                target, &QObject::destroyed, this, [this, shadow]() {
                    if (shadow) {
                        shadow->hide();
                    }
                    scheduleShadowSynchronization();
                });
            entry = newEntry.get();
            _shadowEntries.push_back(std::move(newEntry));
        }
        connectEntryContainer(*entry);
    }
}

ThemedMdiArea::ShadowEntry* ThemedMdiArea::shadowEntryFor(QMdiSubWindow* target) const
{
    const auto iterator = std::find_if(
        _shadowEntries.begin(), _shadowEntries.end(),
        [target](const std::unique_ptr<ShadowEntry>& entry) {
            return entry->target == target;
        });
    return iterator == _shadowEntries.end() ? nullptr : iterator->get();
}

void ThemedMdiArea::connectEntryContainer(ShadowEntry& entry)
{
    ThemedMdiContainer* container = entry.target
        ? qobject_cast<ThemedMdiContainer*>(entry.target->widget())
        : nullptr;
    if (entry.container == container) {
        return;
    }

    QObject::disconnect(entry.contentAttachmentConnection);
    entry.contentAttachmentConnection = {};
    QObject::disconnect(entry.cornerRadiusConnection);
    entry.cornerRadiusConnection = {};
    entry.container = container;
    if (!entry.container) {
        return;
    }

    const QPointer<QMdiSubWindow> target = entry.target;
    entry.contentAttachmentConnection = connect(
        entry.container, &ThemedMdiContainer::contentAttachmentChanged,
        this, [this, target](bool attached) {
            if (!target) {
                return;
            }
            if (!attached) {
                hideShadowFor(target);
            } else {
                scheduleShadowSynchronization();
            }
        });
    entry.cornerRadiusConnection = connect(
        entry.container, &ThemedMdiContainer::frameCornerRadiusChanged,
        this, [this]() {
            scheduleShadowSynchronization();
        });
}

void ThemedMdiArea::synchronizeShadow(ShadowEntry& entry)
{
    connectEntryContainer(entry);
    if (!canShowShadow(entry)) {
        if (entry.shadow) {
            entry.shadow->hide();
        }
        return;
    }

    entry.shadow->ensurePolished();
    if (entry.container) {
        entry.container->ensurePolished();
        entry.shadow->setCornerRadius(entry.container->frameCornerRadius());
    }
    const int padding = entry.shadow->shadowPadding();
    const QRect targetRect = entry.target->geometry();
    const QRect fullShadowRect = targetRect.adjusted(-padding, -padding, padding, padding);
    const QRect clippedShadowRect = fullShadowRect.intersected(viewport()->rect());
    if (!clippedShadowRect.isValid()
        || !entry.shadow->setTrackedGeometry(targetRect, fullShadowRect, clippedShadowRect)) {
        entry.shadow->hide();
        return;
    }

    if (!entry.shadow->isVisible()) {
        entry.shadow->show();
        if (entry.shadow->parentWidget() == entry.target->parentWidget()) {
            entry.shadow->stackUnder(entry.target);
            if (_shadowDiagnostics) {
                ++_shadowDiagnostics->stackUnderCalls;
            }
        }
    }
}

void ThemedMdiArea::synchronizeAllShadows(bool ensureStacking)
{
    ScopedTimingSample synchronizationTiming(
        _shadowDiagnostics ? &_shadowDiagnostics->synchronizations : nullptr);
    if (_shadowMode == ShadowMode::Disabled) {
        clearShadowEntries();
        return;
    }

    refreshShadowEntries();
    const QList<QMdiSubWindow*> windows = subWindowList(QMdiArea::StackingOrder);
    for (QMdiSubWindow* target : windows) {
        if (ShadowEntry* entry = shadowEntryFor(target)) {
            synchronizeShadow(*entry);
        }
    }

    if (!ensureStacking) {
        return;
    }
    for (QMdiSubWindow* target : windows) {
        ShadowEntry* entry = shadowEntryFor(target);
        if (!entry || !entry->shadow || !entry->shadow->isVisible()) {
            continue;
        }
        if (entry->shadow->parentWidget() == target->parentWidget()) {
            entry->shadow->stackUnder(target);
            if (_shadowDiagnostics) {
                ++_shadowDiagnostics->stackUnderCalls;
            }
        }
    }
}

void ThemedMdiArea::scheduleShadowSynchronization()
{
    if (_shadowSyncQueued) {
        if (_shadowDiagnostics) {
            ++_shadowDiagnostics->coalescedSynchronizations;
        }
        return;
    }
    if (_shadowDiagnostics) {
        ++_shadowDiagnostics->queuedSynchronizations;
    }
    _shadowSyncQueued = true;
    QTimer::singleShot(0, this, [this]() {
        _shadowSyncQueued = false;
        synchronizeAllShadows(true);
    });
}

void ThemedMdiArea::reportShadowDiagnostics()
{
    if (!_shadowDiagnostics) {
        return;
    }

    const qint64 sampleMilliseconds = std::max<qint64>(
        _shadowDiagnostics->sampleTimer.restart(), 1);
    const TimingSamples& paints = _shadowDiagnostics->paints;
    const TimingSamples& cacheBuilds = _shadowDiagnostics->cacheBuilds;
    const TimingSamples& synchronizations = _shadowDiagnostics->synchronizations;
    const double seconds = sampleMilliseconds / 1000.0;
    const double paintHz = paints.count / seconds;
    const double paintTotalMilliseconds = paints.totalNanoseconds / 1000000.0;
    const double paintGuiBusyPercent = 100.0 * paintTotalMilliseconds / sampleMilliseconds;
    const double paintAverageMicroseconds = paints.count == 0
        ? 0.0
        : paints.totalNanoseconds / 1000.0 / paints.count;
    const double cacheBuildTotalMilliseconds = cacheBuilds.totalNanoseconds / 1000000.0;
    const double cacheBuildAverageMicroseconds = cacheBuilds.count == 0
        ? 0.0
        : cacheBuilds.totalNanoseconds / 1000.0 / cacheBuilds.count;
    const double synchronizationAverageMicroseconds = synchronizations.count == 0
        ? 0.0
        : synchronizations.totalNanoseconds / 1000.0 / synchronizations.count;

    int shadowCount = 0;
    int visibleCount = 0;
    quint64 cachePixelBytes = 0;
    qulonglong cacheGenerationSum = 0;
    QSize maximumShadowSize;
    for (const std::unique_ptr<ShadowEntry>& entry : _shadowEntries) {
        if (!entry->shadow) {
            continue;
        }
        ++shadowCount;
        visibleCount += entry->shadow->isVisible() ? 1 : 0;
        cachePixelBytes += entry->shadow->cachePixelBytes();
        cacheGenerationSum += entry->shadow->cacheGeneration();
        maximumShadowSize.setWidth(
            std::max(maximumShadowSize.width(), entry->shadow->width()));
        maximumShadowSize.setHeight(
            std::max(maximumShadowSize.height(), entry->shadow->height()));
    }
    const qreal dpr = _trackedViewport
        ? _trackedViewport->devicePixelRatioF()
        : devicePixelRatioF();

    qInfo().noquote() << QStringLiteral(
        "[MdiShadowProfile] sampleMs=%1 shadowCountNow=%2 visibleCountNow=%3 "
        "paints=%4 paintHz=%5 paintTotalMs=%6 paintGuiBusyPct=%7 paintAvgUs=%8 "
        "paintP50Us=%9 paintP95Us=%10 paintP99Us=%11 paintMaxUs=%12 "
        "cacheBuilds=%13 cacheBuildTotalMs=%14 cacheBuildAvgUs=%15 cachePixelKiB=%16 "
        "syncs=%17 syncAvgUs=%18 syncP95Us=%19 geometryChanges=%20 maskUpdates=%21 "
        "invalidations=%22 queuedSyncs=%23 coalescedSyncs=%24 stackUnderCalls=%25 "
        "cacheGenerationSumNow=%26 dprNow=%27 maxShadowSizeNow=%28x%29")
                             .arg(sampleMilliseconds)
                             .arg(shadowCount)
                             .arg(visibleCount)
                             .arg(paints.count)
                             .arg(paintHz, 0, 'f', 2)
                             .arg(paintTotalMilliseconds, 0, 'f', 3)
                             .arg(paintGuiBusyPercent, 0, 'f', 4)
                             .arg(paintAverageMicroseconds, 0, 'f', 2)
                             .arg(paints.percentile(0.50) / 1000.0, 0, 'f', 2)
                             .arg(paints.percentile(0.95) / 1000.0, 0, 'f', 2)
                             .arg(paints.percentile(0.99) / 1000.0, 0, 'f', 2)
                             .arg(paints.maximumNanoseconds / 1000.0, 0, 'f', 2)
                             .arg(cacheBuilds.count)
                             .arg(cacheBuildTotalMilliseconds, 0, 'f', 3)
                             .arg(cacheBuildAverageMicroseconds, 0, 'f', 2)
                             .arg(cachePixelBytes / 1024.0, 0, 'f', 2)
                             .arg(synchronizations.count)
                             .arg(synchronizationAverageMicroseconds, 0, 'f', 2)
                             .arg(synchronizations.percentile(0.95) / 1000.0, 0, 'f', 2)
                             .arg(_shadowDiagnostics->geometryChanges)
                             .arg(_shadowDiagnostics->maskUpdates)
                             .arg(_shadowDiagnostics->cacheInvalidations)
                             .arg(_shadowDiagnostics->queuedSynchronizations)
                             .arg(_shadowDiagnostics->coalescedSynchronizations)
                             .arg(_shadowDiagnostics->stackUnderCalls)
                             .arg(cacheGenerationSum)
                             .arg(dpr, 0, 'f', 2)
                             .arg(maximumShadowSize.width())
                             .arg(maximumShadowSize.height());

    _shadowDiagnostics->resetWindow();
}

void ThemedMdiArea::hideShadows()
{
    for (const std::unique_ptr<ShadowEntry>& entry : _shadowEntries) {
        if (entry->shadow) {
            entry->shadow->hide();
        }
    }
}

void ThemedMdiArea::hideShadowFor(QMdiSubWindow* target)
{
    if (ShadowEntry* entry = shadowEntryFor(target)) {
        if (entry->shadow) {
            entry->shadow->hide();
        }
    }
}

bool ThemedMdiArea::canShowShadow(const ShadowEntry& entry) const
{
    if (_shadowMode == ShadowMode::Disabled
        || !entry.shadow || !entry.target || !viewport()) {
        return false;
    }
    if (_shadowMode == ShadowMode::ActiveWindowOnly
        && activeSubWindow() != entry.target) {
        return false;
    }
    if (viewMode() != QMdiArea::SubWindowView
        || entry.target->parentWidget() != viewport()) {
        return false;
    }
    if (!isVisible() || !viewport()->isVisible() || !entry.target->isVisible()
        || entry.target->isMinimized() || entry.target->isMaximized()
        || entry.target->isFullScreen()) {
        return false;
    }
    if (entry.target->testAttribute(Qt::WA_NativeWindow)
        || entry.target->testAttribute(Qt::WA_AlwaysStackOnTop)) {
        return false;
    }
    if (entry.container && !entry.container->content()) {
        return false;
    }
    return entry.target->geometry().isValid();
}

#include "ThemedMdiArea.moc"
