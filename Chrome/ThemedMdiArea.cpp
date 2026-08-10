#include "ThemedMdiArea.h"

#include "ThemedMdiContainer.h"

#include <QColor>
#include <QEvent>
#include <QImage>
#include <QMdiSubWindow>
#include <QPaintEvent>
#include <QPainter>
#include <QPixmap>
#include <QRegion>
#include <QStyle>
#include <QTimer>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <functional>

namespace {

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

class ThemedMdiShadowWidget final : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QColor shadowColor READ shadowColor WRITE setShadowColor)
    Q_PROPERTY(int shadowExtent READ shadowExtent WRITE setShadowExtent)
    Q_PROPERTY(int cornerRadius READ cornerRadius WRITE setCornerRadius)
    Q_PROPERTY(int offsetX READ offsetX WRITE setOffsetX)
    Q_PROPERTY(int offsetY READ offsetY WRITE setOffsetY)
    Q_PROPERTY(qulonglong cacheGeneration READ cacheGeneration)

public:
    explicit ThemedMdiShadowWidget(QWidget* parent)
        : QWidget(parent)
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
    [[nodiscard]] qulonglong cacheGeneration() const noexcept { return _cacheGeneration; }

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
        }

        const QPoint clippedOrigin = clippedShadowRect.topLeft();
        const QRect outputRect(fullShadowRect.topLeft() - clippedOrigin,
                               fullShadowRect.size());
        const QRect localTargetRect(targetRect.topLeft() - clippedOrigin,
                                    targetRect.size());
        if (_fullOutputRect != outputRect
            || _localTargetRect != localTargetRect
            || previousSize != size()) {
            _fullOutputRect = outputRect;
            _localTargetRect = localTargetRect;
            QRegion shadowRegion(rect());
            shadowRegion -= QRegion(_localTargetRect);
            if (shadowRegion.isEmpty()) {
                clearMask();
                return false;
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
    void invalidateCache(bool geometryChanged = false)
    {
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

    QColor _shadowColor = QColor(8, 20, 35, 54);
    int _shadowExtent = 14;
    int _cornerRadius = 12;
    int _offsetX = 0;
    int _offsetY = 3;
    QRect _fullOutputRect;
    QRect _localTargetRect;
    QPixmap _cache;
    qreal _cacheDpr = 0.0;
    int _sourceMarginPixels = 0;
    int _targetMargin = 0;
    qulonglong _cacheGeneration = 0;
    std::function<void()> _metricsChanged;
};

ThemedMdiArea::ThemedMdiArea(QWidget* parent)
    : QMdiArea(parent)
{
    // QMdiArea already routes viewport child events through eventFilter().
    // Construct the proxy only after every derived member is initialized.
    installShadowOnViewport(viewport());

    connect(this, &QMdiArea::subWindowActivated, this,
            [this](QMdiSubWindow* subWindow) {
                if (_shadowMode == ShadowMode::Disabled) {
                    hideShadow();
                    return;
                }
                setShadowTarget(subWindow);
                synchronizeShadow(true);
            });
}

ThemedMdiArea::~ThemedMdiArea()
{
    if (_shadow) {
        _shadow->setMetricsChangedCallback({});
    }
    setShadowTarget(nullptr);
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
        setShadowTarget(nullptr);
        hideShadow();
        return;
    }

    setShadowTarget(activeSubWindow());
    synchronizeShadow(true);
}

ThemedMdiArea::ShadowMode ThemedMdiArea::shadowMode() const noexcept
{
    return _shadowMode;
}

bool ThemedMdiArea::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == _trackedViewport) {
        if (event->type() == QEvent::Hide) {
            hideShadow();
        } else if (event->type() == QEvent::Close) {
            hideShadow();
            scheduleShadowSynchronization();
        } else if (event->type() == QEvent::Resize) {
            synchronizeShadow(false);
        } else if (event->type() == QEvent::Show) {
            scheduleShadowSynchronization();
        }
    }

    if (watched == _shadowTarget) {
        switch (event->type()) {
        case QEvent::Move:
        case QEvent::Resize:
            synchronizeShadow(false);
            break;
        case QEvent::Hide:
        case QEvent::Destroy:
            hideShadow();
            break;
        case QEvent::Close:
            hideShadow();
            scheduleShadowSynchronization();
            break;
        case QEvent::WindowStateChange:
        case QEvent::ParentChange:
        case QEvent::WinIdChange:
            hideShadow();
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
    if (_shadow) {
        _shadow->setMetricsChangedCallback({});
        delete _shadow;
        _shadow.clear();
    }

    QMdiArea::setupViewport(newViewport);
    installShadowOnViewport(newViewport);
    hideShadow();
    scheduleShadowSynchronization();
}

void ThemedMdiArea::installShadowOnViewport(QWidget* newViewport)
{
    _trackedViewport = newViewport;
    if (!_trackedViewport) {
        return;
    }

    _shadow = new ThemedMdiShadowWidget(_trackedViewport);
    _trackedViewport->installEventFilter(this);
    _shadow->setMetricsChangedCallback([this]() {
        scheduleShadowSynchronization();
    });
}

void ThemedMdiArea::setShadowTarget(QMdiSubWindow* target)
{
    if (_shadowTarget == target) {
        connectTargetContainer();
        return;
    }

    if (_shadowTarget) {
        _shadowTarget->removeEventFilter(this);
    }
    QObject::disconnect(_contentAttachmentConnection);
    _contentAttachmentConnection = {};
    _connectedContainer.clear();

    _shadowTarget = target;
    if (_shadowTarget) {
        _shadowTarget->installEventFilter(this);
    }
    connectTargetContainer();
}

void ThemedMdiArea::connectTargetContainer()
{
    ThemedMdiContainer* container = nullptr;
    if (_shadowTarget) {
        container = qobject_cast<ThemedMdiContainer*>(_shadowTarget->widget());
    }
    if (_connectedContainer == container) {
        return;
    }

    QObject::disconnect(_contentAttachmentConnection);
    _contentAttachmentConnection = {};
    _connectedContainer = container;
    if (_connectedContainer) {
        _contentAttachmentConnection = connect(
            _connectedContainer, &ThemedMdiContainer::contentAttachmentChanged,
            this, [this](bool attached) {
                if (!attached) {
                    hideShadow();
                } else {
                    scheduleShadowSynchronization();
                }
            });
    }
}

void ThemedMdiArea::synchronizeShadow(bool ensureStacking)
{
    if (_shadowMode == ShadowMode::Disabled) {
        hideShadow();
        return;
    }

    if (_shadowTarget != activeSubWindow()) {
        setShadowTarget(activeSubWindow());
        ensureStacking = true;
    } else {
        connectTargetContainer();
    }

    if (!canShowShadow()) {
        hideShadow();
        return;
    }

    _shadow->ensurePolished();
    const int padding = _shadow->shadowPadding();
    const QRect targetRect = _shadowTarget->geometry();
    const QRect fullShadowRect = targetRect.adjusted(-padding, -padding, padding, padding);
    const QRect clippedShadowRect = fullShadowRect.intersected(viewport()->rect());
    if (!clippedShadowRect.isValid()) {
        hideShadow();
        return;
    }

    const bool wasVisible = _shadow->isVisible();
    if (!_shadow->setTrackedGeometry(targetRect, fullShadowRect, clippedShadowRect)) {
        hideShadow();
        return;
    }
    if (!wasVisible) {
        _shadow->show();
    }
    if ((ensureStacking || !wasVisible)
        && _shadow->parentWidget() == _shadowTarget->parentWidget()) {
        _shadow->stackUnder(_shadowTarget);
    }
}

void ThemedMdiArea::scheduleShadowSynchronization()
{
    if (_shadowSyncQueued) {
        return;
    }
    _shadowSyncQueued = true;
    QTimer::singleShot(0, this, [this]() {
        _shadowSyncQueued = false;
        synchronizeShadow(true);
    });
}

void ThemedMdiArea::hideShadow()
{
    if (_shadow) {
        _shadow->hide();
    }
}

bool ThemedMdiArea::canShowShadow() const
{
    if (!_shadow || !_shadowTarget || !viewport()) {
        return false;
    }
    if (viewMode() != QMdiArea::SubWindowView
        || activeSubWindow() != _shadowTarget
        || _shadowTarget->parentWidget() != viewport()) {
        return false;
    }
    if (!isVisible() || !viewport()->isVisible() || !_shadowTarget->isVisible()
        || _shadowTarget->isMinimized() || _shadowTarget->isMaximized()) {
        return false;
    }
    if (_shadowTarget->testAttribute(Qt::WA_NativeWindow)
        || _shadowTarget->testAttribute(Qt::WA_AlwaysStackOnTop)) {
        return false;
    }
    if (_connectedContainer && !_connectedContainer->content()) {
        return false;
    }
    return _shadowTarget->geometry().isValid();
}

#include "ThemedMdiArea.moc"
