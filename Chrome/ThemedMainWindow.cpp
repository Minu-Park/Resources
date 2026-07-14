#include "ThemedMainWindow.h"
#include "ThemedMainTitleBar.h"
#include "Resources.h"

#include <QEvent>
#include <QPainterPath>
#include <QMouseEvent>
#include <QPainter>
#include <QMenuBar>
#include <QWindow>
#include <QStyle>
#include <QStatusBar>
#include <QApplication>
#include <QShowEvent>



ThemedMainWindow::ThemedMainWindow(QWidget* parent) : QMainWindow(parent) {
    setObjectName(QStringLiteral("ThemedMainWindow"));
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setContentsMargins(1, 1, 1, 1);
    setProperty("maximized", false);
    setMouseTracking(true);
    setCursor(Qt::ArrowCursor);

    _menuBar = new QMenuBar(this);
    _menuBar->setNativeMenuBar(false);
    _titleBar = new ThemedMainTitleBar(this, _menuBar, this);
    _titleBar->setProperty("maximized", false);
    setMenuWidget(_titleBar);

    qApp->installEventFilter(this);
}

ThemedMainWindow::~ThemedMainWindow() {
    qApp->removeEventFilter(this);
}

void ThemedMainWindow::setFullscreenTitleBarOverlayEnabled(const bool enabled)
{
    if (_fullscreenTitleBarOverlayEnabled == enabled)
    {
        return;
    }

    _fullscreenTitleBarOverlayEnabled = enabled;
    if (enabled)
    {
        _titleBar->setMenuBar(nullptr);
        _fullscreenTitleBarOverlay = new ThemedMainTitleBar(this, _menuBar, this);
        _fullscreenTitleBarOverlay->setProperty("fullscreenOverlay", true);
        _fullscreenTitleBarOverlay->setGeometry(0, 0, width(), _titleBar->height());
        _fullscreenTitleBarOverlay->hide();
        _titleBar->hide();
        return;
    }

    if (_fullscreenTitleBarOverlay)
    {
        _fullscreenTitleBarOverlay->hide();
        _fullscreenTitleBarOverlay->setMenuBar(nullptr);
        _titleBar->setMenuBar(_menuBar);
        delete _fullscreenTitleBarOverlay;
        _fullscreenTitleBarOverlay = nullptr;
    }
    _titleBar->show();
}

void ThemedMainWindow::setFullscreenTitleBarOverlayVisible(const bool visible)
{
    if (!_fullscreenTitleBarOverlayEnabled || !_fullscreenTitleBarOverlay)
    {
        return;
    }

    _fullscreenTitleBarOverlay->setVisible(visible);
    if (visible)
    {
        _fullscreenTitleBarOverlay->raise();
    }
}

void ThemedMainWindow::updateWindowMask() {
    clearMask();
}

void ThemedMainWindow::paintEvent(QPaintEvent* event) {
    QMainWindow::paintEvent(event);
    QPainter painter(this);
    Resources::paintMainWindowBorder(
        this,
        painter,
        _windowChromeMaximized || isMaximized() || isFullScreen(),
        !isFullScreen());
}

void ThemedMainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    if (_fullscreenTitleBarOverlay)
    {
        _fullscreenTitleBarOverlay->setGeometry(0, 0, width(), _fullscreenTitleBarOverlay->height());
    }
    updateWindowMask();
}

void ThemedMainWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);
    Resources::applyWindowPlatformAttributes(this);
}

void ThemedMainWindow::changeEvent(QEvent* event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange) {
        updateWindowChromeState();
        updateWindowMask();
    }
}

void ThemedMainWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        _dragStartPos = event->globalPosition().toPoint();
        QPoint localPos = mapFromGlobal(event->globalPosition().toPoint());
        int mode = determineResizeMode(localPos);
        if (mode != ResizeNone) {
            if (auto* window = this->windowHandle()) {
                Qt::Edges edges = Qt::Edges();
                if (mode & ResizeLeft) edges |= Qt::LeftEdge;
                if (mode & ResizeRight) edges |= Qt::RightEdge;
                if (mode & ResizeTop) edges |= Qt::TopEdge;
                if (mode & ResizeBottom) edges |= Qt::BottomEdge;

                window->startSystemResize(edges);
                event->accept();
                return;
            }
            _resizeMode = mode;
            event->accept();
            return;
        }
    }
    QMainWindow::mousePressEvent(event);
}

void ThemedMainWindow::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton && _resizeMode != ResizeNone) {
        QPoint currentGlobalPos = event->globalPosition().toPoint();
        QPoint delta = currentGlobalPos - _dragStartPos;
        QRect geom = geometry();

        int left = geom.left();
        int right = geom.x() + geom.width();
        int top = geom.top();
        int bottom = geom.y() + geom.height();

        if (_resizeMode & ResizeLeft) {
            left += delta.x();
            if (right - left < minimumWidth()) {
                left = right - minimumWidth();
            }
        }
        if (_resizeMode & ResizeRight) {
            right += delta.x();
            if (right - left < minimumWidth()) {
                right = left + minimumWidth();
            }
        }
        if (_resizeMode & ResizeTop) {
            top += delta.y();
            if (bottom - top < minimumHeight()) {
                top = bottom - minimumHeight();
            }
        }
        if (_resizeMode & ResizeBottom) {
            bottom += delta.y();
            if (bottom - top < minimumHeight()) {
                bottom = top + minimumHeight();
            }
        }

        setGeometry(left, top, right - left, bottom - top);
        _dragStartPos = currentGlobalPos;
        event->accept();
        return;
    } else {
        QPoint localPos = mapFromGlobal(event->globalPosition().toPoint());
        updateCursorShape(localPos);
    }
    QMainWindow::mouseMoveEvent(event);
}

void ThemedMainWindow::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        _resizeMode = ResizeNone;
        setCursor(Qt::ArrowCursor);
        event->accept();
    } else {
        QMainWindow::mouseReleaseEvent(event);
    }
}

void ThemedMainWindow::leaveEvent(QEvent* event) {
    Q_UNUSED(event);
    setCursor(Qt::ArrowCursor);
}

int ThemedMainWindow::determineResizeMode(const QPoint& pos) {
    if (isMaximized() || isFullScreen()) {
        return ResizeNone;
    }

    int mode = ResizeNone;
    const int border = 6;
    const int topBorder = 6;

    if (pos.x() < border) mode |= ResizeLeft;
    if (pos.x() > width() - border) mode |= ResizeRight;
    if (pos.y() < topBorder) mode |= ResizeTop;
    if (pos.y() > height() - border) mode |= ResizeBottom;

    return mode;
}

void ThemedMainWindow::updateCursorShape(const QPoint& pos) {
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

void ThemedMainWindow::updateWindowChromeState() {
    const bool edgeToEdge = isMaximized() || isFullScreen();
    if (!edgeToEdge) {
        _forceRoundedChrome = false;
    }
    applyWindowChromeState(edgeToEdge);
}

void ThemedMainWindow::applyWindowChromeState(bool maximized) {
    _windowChromeMaximized = maximized;
    setContentsMargins(maximized ? QMargins(0, 0, 0, 0) : QMargins(1, 1, 1, 1));
    setProperty("maximized", maximized);
    style()->unpolish(this);
    style()->polish(this);
    this->update();

    if (_titleBar) {
        _titleBar->setProperty("maximized", maximized);
        _titleBar->style()->unpolish(_titleBar);
        _titleBar->style()->polish(_titleBar);
        _titleBar->update();
    }
    if (_fullscreenTitleBarOverlay) {
        _fullscreenTitleBarOverlay->setProperty("maximized", maximized);
        _fullscreenTitleBarOverlay->style()->unpolish(_fullscreenTitleBarOverlay);
        _fullscreenTitleBarOverlay->style()->polish(_fullscreenTitleBarOverlay);
        _fullscreenTitleBarOverlay->update();
    }

    if (QStatusBar* bar = statusBar()) {
        bar->setProperty("maximized", maximized);
        bar->style()->unpolish(bar);
        bar->style()->polish(bar);
        bar->update();
    }
}

void ThemedMainWindow::prepareForMaximizeTransition() {
    _forceRoundedChrome = false;
    applyWindowChromeState(true);
    clearMask();
    repaint();
}

void ThemedMainWindow::prepareForRestoreTransition() {
    _forceRoundedChrome = false;
    applyWindowChromeState(false);
    clearMask();
    update();
}

bool ThemedMainWindow::eventFilter(QObject* watched, QEvent* event) {
    auto* w = qobject_cast<QWidget*>(watched);
    if (w && (w == this || this->isAncestorOf(w))) {
        if (event->type() == QEvent::MouseMove) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            QPoint globalPos = mouseEvent->globalPosition().toPoint();
            QPoint localPos = mapFromGlobal(globalPos);

            if (_resizeMode != ResizeNone) {
                this->mouseMoveEvent(mouseEvent);
                return true;
            }

            int mode = determineResizeMode(localPos);
            if (mode != ResizeNone) {
                if (_cursorOverriddenWidget && _cursorOverriddenWidget != w) {
                    _cursorOverriddenWidget->unsetCursor();
                }
                updateCursorShape(localPos);
                w->setCursor(cursor());
                _cursorOverriddenWidget = w;
                return true;
            } else {
                if (_cursorOverriddenWidget) {
                    _cursorOverriddenWidget->unsetCursor();
                    _cursorOverriddenWidget = nullptr;
                }
                this->unsetCursor();
                this->setCursor(Qt::ArrowCursor);
            }
        }
        else if (event->type() == QEvent::MouseButtonPress) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            QPoint globalPos = mouseEvent->globalPosition().toPoint();
            QPoint localPos = mapFromGlobal(globalPos);
            int mode = determineResizeMode(localPos);
            if (mode != ResizeNone) {
                this->mousePressEvent(mouseEvent);
                return true;
            }
        }
        else if (event->type() == QEvent::MouseButtonRelease) {
            if (_resizeMode != ResizeNone) {
                auto* mouseEvent = static_cast<QMouseEvent*>(event);
                this->mouseReleaseEvent(mouseEvent);
                if (_cursorOverriddenWidget) {
                    _cursorOverriddenWidget->unsetCursor();
                    _cursorOverriddenWidget = nullptr;
                }
                this->unsetCursor();
                this->setCursor(Qt::ArrowCursor);
                return true;
            }
        }
        else if (event->type() == QEvent::Leave) {
            if (_cursorOverriddenWidget) {
                _cursorOverriddenWidget->unsetCursor();
                _cursorOverriddenWidget = nullptr;
            }
            this->unsetCursor();
            this->setCursor(Qt::ArrowCursor);
        }
    }
    return QMainWindow::eventFilter(watched, event);
}
