#include "ThemedMdiContainer.h"
#include "ThemedMdiTitleBar.h"
#include <QMdiSubWindow>
#include <QMdiArea>
#include <QMenuBar>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QCursor>
#include <QBitmap>
#include <QStyle>
#include <QStyleOption>
#include <QResizeEvent>

#include <algorithm>

ThemedMdiContainer::ThemedMdiContainer(QMdiSubWindow* subWin, QWidget* content, QMenuBar* menuBar, QWidget* parent)
    : QWidget(parent)
    , _subWin(subWin)
    , _content(content)
{
    setObjectName(QStringLiteral("ThemedMdiContainer"));
    
    // Keep a 1px inset for the normal rounded frame. Maximized MDI windows
    // remove their lateral frame, so they must not retain a white side strip.
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(1, 1, 1, 1);
    layout->setSpacing(0);

    _titleBar = new ThemedMdiTitleBar(subWin, menuBar, this);
    layout->addWidget(_titleBar);
    
    layout->addWidget(_content);

    const auto addResizeHandle = [this](const int resizeMode, const Qt::CursorShape cursor) {
        auto* handle = new QWidget(this);
        handle->setObjectName(QStringLiteral("ThemedMdiResizeHandle"));
        handle->setProperty("resizeMode", resizeMode);
        handle->setCursor(cursor);
        handle->installEventFilter(this);
        _resizeHandles.append(handle);
    };
    addResizeHandle(ResizeLeft, Qt::SizeHorCursor);
    addResizeHandle(ResizeRight, Qt::SizeHorCursor);
    addResizeHandle(ResizeTop, Qt::SizeVerCursor);
    addResizeHandle(ResizeBottom, Qt::SizeVerCursor);
    addResizeHandle(ResizeLeft | ResizeTop, Qt::SizeFDiagCursor);
    addResizeHandle(ResizeRight | ResizeTop, Qt::SizeBDiagCursor);
    addResizeHandle(ResizeLeft | ResizeBottom, Qt::SizeBDiagCursor);
    addResizeHandle(ResizeRight | ResizeBottom, Qt::SizeFDiagCursor);

    setMouseTracking(true);
    _subWin->installEventFilter(this);
    if (_content) {
        _content->installEventFilter(this);
    }
    if (_subWin) {
        updateSubWindowMinimumSize();
    }
}

QWidget* ThemedMdiContainer::content() const noexcept
{
    return _content;
}

int ThemedMdiContainer::frameCornerRadius() const noexcept
{
    return _frameCornerRadius;
}

void ThemedMdiContainer::setFrameCornerRadius(int radius)
{
    radius = std::clamp(radius, 0, 64);
    if (_frameCornerRadius == radius)
    {
        return;
    }

    _frameCornerRadius = radius;
    updateContentMask();
    emit frameCornerRadiusChanged(_frameCornerRadius);
}

QWidget* ThemedMdiContainer::takeContent()
{
    if (!_content)
    {
        return nullptr;
    }

    QWidget* content = _content;
    _content = nullptr;
    content->removeEventFilter(this);
    if (layout())
    {
        layout()->removeWidget(content);
    }
    // The rounded MDI mask belongs to this chrome container.  A detached
    // GraphicsEngine must not retain the old MDI-size clipping region when it
    // is reparented into an HMI wrapper.
    content->clearMask();
    _contentMaskSize = {};
    _contentMaskRadius = -1;
    _contentMaskApplied = false;
    content->setParent(nullptr);
    if (_subWin)
    {
        updateSubWindowMinimumSize();
    }
    emit contentAttachmentChanged(false);
    return content;
}

void ThemedMdiContainer::restoreContent(QWidget* content)
{
    if (!content || _content == content)
    {
        return;
    }
    if (_content)
    {
        return;
    }

    _content = content;
    _contentMaskSize = {};
    _contentMaskRadius = -1;
    _contentMaskApplied = false;
    content->setParent(this);
    content->installEventFilter(this);
    if (layout())
    {
        layout()->addWidget(content);
    }
    if (_subWin)
    {
        updateSubWindowMinimumSize();
    }
    updateContentMask();
    updateGeometry();
    emit contentAttachmentChanged(true);
}

QSize ThemedMdiContainer::minimumSizeHint() const
{
    if (_content) {
        // Always query the dynamic layout constraints from minimumSizeHint() in real time
        QSize contentMin = _content->minimumSizeHint();
        int titleHeight = _titleBar ? _titleBar->height() : 0;
        if (titleHeight <= 0) {
            titleHeight = 30; // Fallback height for custom title bar if not fully initialized
        }
        return QSize(contentMin.width() + 2, contentMin.height() + titleHeight + 2);
    }
    return QWidget::minimumSizeHint();
}

void ThemedMdiContainer::updateSubWindowMinimumSize()
{
    if (!_subWin)
    {
        return;
    }

    if (_subWin->isMinimized() || _subWin->isMaximized())
    {
        if (_subWin->minimumSize() != QSize(0, 0))
        {
            _subWin->setMinimumSize(QSize(0, 0));
        }
        return;
    }

    QSize targetMin = minimumSizeHint();
    targetMin.setWidth(qMax(0, targetMin.width()));
    targetMin.setHeight(qMax(0, targetMin.height()));

    if (auto* mdi = _subWin->mdiArea())
    {
        if (auto* viewport = mdi->viewport())
        {
            const QSize viewportSize = viewport->size();
            if (viewportSize.width() >= 0)
            {
                targetMin.setWidth(qMin(targetMin.width(), viewportSize.width()));
            }
            if (viewportSize.height() >= 0)
            {
                targetMin.setHeight(qMin(targetMin.height(), viewportSize.height()));
            }
        }
    }

    if (_subWin->minimumSize() != targetMin)
    {
        _subWin->setMinimumSize(targetMin);
    }
}

void ThemedMdiContainer::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        beginResize(determineResizeMode(event->position().toPoint()), event->globalPosition().toPoint());
        if (_resizeMode != ResizeNone) {
            event->accept();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void ThemedMdiContainer::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons() & Qt::LeftButton && _resizeMode != ResizeNone) {
        resizeFromGlobalPosition(event->globalPosition().toPoint());
        event->accept();
        return;
    } else {
        updateCursorShape(event->position().toPoint());
    }
    QWidget::mouseMoveEvent(event);
}

void ThemedMdiContainer::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        if (_resizeMode != ResizeNone) {
            finishResize();
        }
        setCursor(Qt::ArrowCursor);
        event->accept();
    } else {
        QWidget::mouseReleaseEvent(event);
    }
}

void ThemedMdiContainer::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QStyleOption opt;
    opt.initFrom(this);
    QPainter painter(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &painter, this);
}

int ThemedMdiContainer::determineResizeMode(const QPoint& pos)
{
    int mode = ResizeNone;
    const int border = 6; // detection boundary size

    if (pos.x() < border) mode |= ResizeLeft;
    if (pos.x() > width() - border) mode |= ResizeRight;
    if (pos.y() < border) mode |= ResizeTop;
    if (pos.y() > height() - border) mode |= ResizeBottom;

    // Disable top resize when maximized to prevent strange movements
    if (_subWin->isMaximized()) {
        return ResizeNone;
    }

    return mode;
}

void ThemedMdiContainer::updateCursorShape(const QPoint& pos)
{
    int mode = determineResizeMode(pos);

    if (mode == ResizeNone) {
        setCursor(Qt::ArrowCursor);
    } else if ((mode & ResizeLeft && mode & ResizeTop) || (mode & ResizeRight && mode & ResizeBottom)) {
        setCursor(Qt::SizeFDiagCursor);
    } else if ((mode & ResizeRight && mode & ResizeTop) || (mode & ResizeLeft && mode & ResizeBottom)) {
        setCursor(Qt::SizeBDiagCursor);
    } else if (mode & ResizeLeft || mode & ResizeRight) {
        setCursor(Qt::SizeHorCursor);
    } else if (mode & ResizeTop || mode & ResizeBottom) {
        setCursor(Qt::SizeVerCursor);
    }
}

void ThemedMdiContainer::leaveEvent(QEvent* event)
{
    Q_UNUSED(event);
    setCursor(Qt::ArrowCursor);
}

bool ThemedMdiContainer::eventFilter(QObject* watched, QEvent* event)
{
    if (auto* handle = qobject_cast<QWidget*>(watched); _resizeHandles.contains(handle)) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                beginResize(handle->property("resizeMode").toInt(), mouseEvent->globalPosition().toPoint());
                return _resizeMode != ResizeNone;
            }
        }
        if (event->type() == QEvent::MouseMove && _resizeMode != ResizeNone) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            resizeFromGlobalPosition(mouseEvent->globalPosition().toPoint());
            return true;
        }
        if (event->type() == QEvent::MouseButtonRelease && _resizeMode != ResizeNone) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                finishResize();
                return true;
            }
        }
    }

    if (watched == _subWin) {
        if (event->type() == QEvent::WindowStateChange) {
            handleWindowStateChange();
        }
        else if (event->type() == QEvent::Resize) {
            updateSubWindowMinimumSize();
        }
    }
    if (watched == _content && (event->type() == QEvent::LayoutRequest || event->type() == QEvent::Resize)) {
        if (_subWin) {
            updateSubWindowMinimumSize();
        }
        if (event->type() == QEvent::Resize) {
            updateContentMask();
        }
    }
    return QWidget::eventFilter(watched, event);
}

void ThemedMdiContainer::handleWindowStateChange()
{
    if (!_subWin || !_content || !_titleBar) return;

    if (_subWin->isMinimized()) {
        updateContentMask();
        emit minimizeRequested(_subWin);
        return;
    }

    bool maximized = _subWin->isMaximized();
    if (!_subWin->isMinimized() && _subWin->isVisible()) {
        _subWin->setProperty("wasMaximizedBeforeMinimize", maximized);
    }

    setProperty("maximized", maximized);
    if (auto* containerLayout = qobject_cast<QVBoxLayout*>(layout())) {
        const int lateralInset = maximized ? 0 : 1;
        containerLayout->setContentsMargins(lateralInset, 1, lateralInset, 1);
    }
    style()->unpolish(this);
    style()->polish(this);
    _titleBar->setProperty("maximized", maximized);
    _titleBar->style()->unpolish(_titleBar);
    _titleBar->style()->polish(_titleBar);
    updateContentMask();
    update();

    if (QMdiArea* mdi = _subWin->mdiArea()) {
        for (QObject* owner = mdi->parent(); owner; owner = owner->parent()) {
            if (owner->metaObject()->indexOfMethod("updateMdiMinimumSize()") >= 0) {
                QMetaObject::invokeMethod(owner, "updateMdiMinimumSize", Qt::QueuedConnection);
                break;
            }
        }
    }
}

void ThemedMdiContainer::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
}

void ThemedMdiContainer::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateContentMask();
    updateResizeHandleGeometry();
}

void ThemedMdiContainer::updateContentMask()
{
    if (!_content)
    {
        return;
    }
    if (!_subWin || _subWin->isMaximized() || _subWin->isMinimized())
    {
        if (_contentMaskApplied || !_content->mask().isEmpty()) {
            _content->clearMask();
        }
        _contentMaskSize = {};
        _contentMaskRadius = -1;
        _contentMaskApplied = false;
        return;
    }

    // The content sits one pixel inside the framed container, so its bottom
    // curve follows the same silhouette with the border inset removed.
    const int innerRadius = std::max(_frameCornerRadius - 1, 0);
    if (innerRadius == 0 || _content->size().isEmpty())
    {
        if (_contentMaskApplied || !_content->mask().isEmpty()) {
            _content->clearMask();
        }
        _contentMaskSize = {};
        _contentMaskRadius = -1;
        _contentMaskApplied = false;
        return;
    }

    if (_contentMaskApplied
        && _contentMaskSize == _content->size()
        && _contentMaskRadius == innerRadius
        && !_content->mask().isEmpty())
    {
        return;
    }

    QBitmap bitmap(_content->size());
    if (bitmap.isNull())
    {
        _content->clearMask();
        _contentMaskSize = {};
        _contentMaskRadius = -1;
        _contentMaskApplied = false;
        return;
    }

    bitmap.clear();
    QPainter painter(&bitmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(Qt::color1);
    painter.setPen(Qt::NoPen);

    // QWidget::setMask(QBitmap) becomes a binary QRegion.  If the rounded
    // content reaches the exact inner QSS curve, those whole logical pixels
    // can cover the parent's antialiased border.  Keep a one-DIP guard only
    // in the bottom corner arcs; the parent frame then owns the visible AA
    // edge while the straight content area remains flush with the frame.
    const int guard = innerRadius > 1 ? 1 : 0;
    const int guardedRadius = innerRadius - guard;
    const QRect guardedRect = bitmap.rect().adjusted(
        guard, guard, -guard, -guard);
    painter.drawRoundedRect(guardedRect, guardedRadius, guardedRadius);
    painter.fillRect(QRect(0, 0, bitmap.width(),
                           qMax(0, bitmap.height() - innerRadius)),
                     Qt::color1);
    const int straightWidth = bitmap.width() - 2 * innerRadius;
    if (straightWidth > 0) {
        painter.fillRect(QRect(innerRadius,
                               qMax(0, bitmap.height() - innerRadius),
                               straightWidth, innerRadius),
                         Qt::color1);
    }
    painter.end();
    _content->setMask(bitmap);
    _contentMaskSize = _content->size();
    _contentMaskRadius = innerRadius;
    _contentMaskApplied = true;
}

void ThemedMdiContainer::beginResize(const int resizeMode, const QPoint& globalPosition)
{
    if (!_subWin || _subWin->isMaximized()) {
        _resizeMode = ResizeNone;
        return;
    }
    _resizeMode = resizeMode;
    _dragStartPos = globalPosition;
    _dragStartGeometry = _subWin->geometry();
}

void ThemedMdiContainer::resizeFromGlobalPosition(const QPoint& globalPosition)
{
    if (!_subWin || _resizeMode == ResizeNone) {
        return;
    }

    const QPoint delta = globalPosition - _dragStartPos;
    const QRect geom = _dragStartGeometry;
    int left = geom.left();
    int right = geom.x() + geom.width();
    int top = geom.top();
    int bottom = geom.y() + geom.height();
    const QSize minSize = minimumSizeHint();
    const int minW = qMax(250, minSize.width());
    const int minH = qMax(150, minSize.height());

    if (_resizeMode & ResizeLeft) left = qMin(left + delta.x(), right - minW);
    if (_resizeMode & ResizeRight) right = qMax(right + delta.x(), left + minW);
    if (_resizeMode & ResizeTop) top = qMin(top + delta.y(), bottom - minH);
    if (_resizeMode & ResizeBottom) bottom = qMax(bottom + delta.y(), top + minH);

    _subWin->setGeometry(left, top, right - left, bottom - top);
}

void ThemedMdiContainer::finishResize()
{
    if (_content && _content->metaObject()->indexOfMethod("notifyManualResizeFinished()") >= 0) {
        QMetaObject::invokeMethod(_content, "notifyManualResizeFinished", Qt::AutoConnection);
    }
    _resizeMode = ResizeNone;
}

void ThemedMdiContainer::updateResizeHandleGeometry()
{
    constexpr int border = 6;
    const int containerWidth = width();
    const int containerHeight = height();
    for (QWidget* handle : _resizeHandles) {
        const int mode = handle->property("resizeMode").toInt();
        if (mode == (ResizeLeft | ResizeTop)) handle->setGeometry(0, 0, border, border);
        else if (mode == (ResizeRight | ResizeTop)) handle->setGeometry(qMax(0, containerWidth - border), 0, border, border);
        else if (mode == (ResizeLeft | ResizeBottom)) handle->setGeometry(0, qMax(0, containerHeight - border), border, border);
        else if (mode == (ResizeRight | ResizeBottom)) handle->setGeometry(qMax(0, containerWidth - border), qMax(0, containerHeight - border), border, border);
        else if (mode == ResizeLeft) handle->setGeometry(0, border, border, qMax(0, containerHeight - 2 * border));
        else if (mode == ResizeRight) handle->setGeometry(qMax(0, containerWidth - border), border, border, qMax(0, containerHeight - 2 * border));
        else if (mode == ResizeTop) handle->setGeometry(border, 0, qMax(0, containerWidth - 2 * border), border);
        else if (mode == ResizeBottom) handle->setGeometry(border, qMax(0, containerHeight - border), qMax(0, containerWidth - 2 * border), border);
        handle->raise();
    }
}
