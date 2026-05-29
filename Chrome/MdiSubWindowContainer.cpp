#include "MdiSubWindowContainer.h"
#include "CustomTitleBar.h"
#include <QMdiSubWindow>
#include <QMenuBar>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QCursor>
#include <QBitmap>
#include <QStyle>
#include <QStyleOption>

MdiSubWindowContainer::MdiSubWindowContainer(QMdiSubWindow* subWin, QWidget* content, QMenuBar* menuBar, QWidget* parent)
    : QWidget(parent)
    , _subWin(subWin)
    , _content(content)
{
    setObjectName(QStringLiteral("MdiSubWindowContainer"));
    
    // 1px margin inside the container for border spacing and resize handles
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(1, 1, 1, 1);
    layout->setSpacing(0);

    _titleBar = new CustomTitleBar(subWin, menuBar, this);
    layout->addWidget(_titleBar);
    
    layout->addWidget(_content);

    setMouseTracking(true);
    _subWin->installEventFilter(this);
    if (_content) {
        _content->installEventFilter(this);
    }
}

void MdiSubWindowContainer::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        _dragStartPos = event->globalPosition().toPoint();
        _resizeMode = determineResizeMode(event->position().toPoint());
        if (_resizeMode != ResizeNone) {
            event->accept();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void MdiSubWindowContainer::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons() & Qt::LeftButton && _resizeMode != ResizeNone) {
        QPoint currentGlobalPos = event->globalPosition().toPoint();
        QPoint delta = currentGlobalPos - _dragStartPos;
        QRect geom = _subWin->geometry();

        int left = geom.left();
        int right = geom.x() + geom.width();
        int top = geom.top();
        int bottom = geom.y() + geom.height();

        QSize minSize = minimumSizeHint();
        int minW = qMax(250, minSize.width());
        int minH = qMax(150, minSize.height());

        if (_resizeMode & ResizeLeft) {
            left += delta.x();
            if (right - left < minW) {
                left = right - minW;
            }
        }
        if (_resizeMode & ResizeRight) {
            right += delta.x();
            if (right - left < minW) {
                right = left + minW;
            }
        }
        if (_resizeMode & ResizeTop) {
            top += delta.y();
            if (bottom - top < minH) {
                top = bottom - minH;
            }
        }
        if (_resizeMode & ResizeBottom) {
            bottom += delta.y();
            if (bottom - top < minH) {
                bottom = top + minH;
            }
        }

        _subWin->setGeometry(left, top, right - left, bottom - top);
        _dragStartPos = currentGlobalPos;
        event->accept();
        return;
    } else {
        updateCursorShape(event->position().toPoint());
    }
    QWidget::mouseMoveEvent(event);
}

void MdiSubWindowContainer::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        if (_resizeMode != ResizeNone) {
            QMetaObject::invokeMethod(_content, "notifyManualResizeFinished", Qt::AutoConnection);
        }
        _resizeMode = ResizeNone;
        setCursor(Qt::ArrowCursor);
        event->accept();
    } else {
        QWidget::mouseReleaseEvent(event);
    }
}

void MdiSubWindowContainer::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QStyleOption opt;
    opt.initFrom(this);
    QPainter painter(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &painter, this);
}

int MdiSubWindowContainer::determineResizeMode(const QPoint& pos)
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

void MdiSubWindowContainer::updateCursorShape(const QPoint& pos)
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

void MdiSubWindowContainer::leaveEvent(QEvent* event)
{
    Q_UNUSED(event);
    setCursor(Qt::ArrowCursor);
}

bool MdiSubWindowContainer::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == _subWin && event->type() == QEvent::WindowStateChange) {
        handleWindowStateChange();
    }
    return QWidget::eventFilter(watched, event);
}

void MdiSubWindowContainer::handleWindowStateChange()
{
    if (!_subWin || !_content || !_titleBar) return;

    bool maximized = _subWin->isMaximized();

    setProperty("maximized", maximized);
    style()->unpolish(this);
    style()->polish(this);
    _titleBar->setProperty("maximized", maximized);
    _titleBar->style()->unpolish(_titleBar);
    _titleBar->style()->polish(_titleBar);

    if (maximized) {
        _content->clearMask();
    }
    update();
}

void MdiSubWindowContainer::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
}

void MdiSubWindowContainer::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (_content && !_subWin->isMaximized() && !_subWin->isMinimized()) {
        QBitmap bmp(_content->size());
        if (!bmp.isNull()) {
            bmp.clear();
            QPainter p(&bmp);
            p.setRenderHint(QPainter::Antialiasing, true);
            p.setBrush(Qt::color1);
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(bmp.rect(), 11, 11);
            p.drawRect(0, 0, bmp.width(), 11);
            p.end();
            _content->setMask(bmp);
        }
    } else if (_content) {
        _content->clearMask();
    }
}
