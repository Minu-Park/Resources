#include "ThemedWindow.h"
#include "ThemedMainTitleBar.h"
#include "Resources.h"

#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>
#include <QMainWindow>
#include <QMenuBar>
#include <QWindow>
#include <QStyle>
#include <QScreen>
#include <QGuiApplication>
#include <QStatusBar>

ThemedWindow::ThemedWindow(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("ThemedWindow"));
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setContentsMargins(1, 1, 1, 1);
    setProperty("maximized", false);
    setMouseTracking(true);
    setCursor(Qt::ArrowCursor);

    _layout = new QVBoxLayout(this);
    _layout->setContentsMargins(0, 0, 0, 0);
    _layout->setSpacing(0);
}

void ThemedWindow::setCentralWidget(QWidget* widget) {
    if (!widget) return;

    if (_centralWidget) {
        _layout->removeWidget(_centralWidget);
        _centralWidget->deleteLater();
        _centralWidget = nullptr;
    }

    if (_titleBar) {
        _layout->removeWidget(_titleBar);
        _titleBar->deleteLater();
        _titleBar = nullptr;
    }

    _centralWidget = widget;

    // If it's a QMainWindow, hijack its QMenuBar to place it in the custom title bar
    if (auto* mainWindow = qobject_cast<QMainWindow*>(_centralWidget)) {
        QMenuBar* menuBar = mainWindow->menuBar();
        if (menuBar) {
            menuBar->setNativeMenuBar(false);
            _titleBar = new ThemedMainTitleBar(mainWindow, menuBar, this);
            _titleBar->setProperty("maximized", _windowChromeMaximized || isMaximized());
            _layout->addWidget(_titleBar);
            registerChildForResizeFilter(_titleBar);
        }
        
        // Ensure QMainWindow's default status bar works with our layout limits if needed.
        // Also remove the native title bar setup if it has one.
        mainWindow->setWindowFlags(Qt::Widget);
    }

    _layout->addWidget(_centralWidget, 1);
    registerChildForResizeFilter(_centralWidget);
    updateWindowChromeState();
}

void ThemedWindow::registerChildForResizeFilter(QWidget* widget) {
    if (!widget) return;
    widget->setMouseTracking(true);
    widget->installEventFilter(this);
    const auto children = widget->findChildren<QWidget*>();
    for (QWidget* child : children) {
        child->setMouseTracking(true);
        child->installEventFilter(this);
    }
}

void ThemedWindow::updateWindowMask() {
    clearMask();
}

void ThemedWindow::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    Resources::paintMainWindowBorder(this, painter, _windowChromeMaximized || isMaximized(), _forceRoundedChrome);
}

void ThemedWindow::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    updateWindowMask();
}

void ThemedWindow::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange) {
        updateWindowChromeState();
        updateWindowMask();
    }
}

void ThemedWindow::mousePressEvent(QMouseEvent* event) {
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
    QWidget::mousePressEvent(event);
}

void ThemedWindow::mouseMoveEvent(QMouseEvent* event) {
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
    QWidget::mouseMoveEvent(event);
}

void ThemedWindow::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        _resizeMode = ResizeNone;
        setCursor(Qt::ArrowCursor);
        event->accept();
    } else {
        QWidget::mouseReleaseEvent(event);
    }
}

void ThemedWindow::leaveEvent(QEvent* event) {
    Q_UNUSED(event);
    setCursor(Qt::ArrowCursor);
}

int ThemedWindow::determineResizeMode(const QPoint& pos) {
    if (isMaximized()) {
        return ResizeNone;
    }

    int mode = ResizeNone;
    const int border = 6;
    const int topBorder = 3;

    if (pos.x() < border) mode |= ResizeLeft;
    if (pos.x() > width() - border) mode |= ResizeRight;
    if (pos.y() < topBorder) mode |= ResizeTop;
    if (pos.y() > height() - border) mode |= ResizeBottom;

    return mode;
}

void ThemedWindow::updateCursorShape(const QPoint& pos) {
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

bool ThemedWindow::eventFilter(QObject* watched, QEvent* event) {
    auto* watchedWidget = qobject_cast<QWidget*>(watched);
    if (!watchedWidget) {
        return QWidget::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseMove) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        QPoint localPos = mapFromGlobal(mouseEvent->globalPosition().toPoint());

        if (_resizeMode != ResizeNone) {
            this->mouseMoveEvent(mouseEvent);
            return true;
        }

        int mode = determineResizeMode(localPos);
        if (mode != ResizeNone) {
            Qt::CursorShape shape = Qt::ArrowCursor;
            if ((mode & ResizeLeft && mode & ResizeTop) || (mode & ResizeRight && mode & ResizeBottom)) {
                shape = Qt::SizeFDiagCursor;
            } else if ((mode & ResizeRight && mode & ResizeTop) || (mode & ResizeLeft && mode & ResizeBottom)) {
                shape = Qt::SizeBDiagCursor;
            } else if (mode & ResizeLeft || mode & ResizeRight) {
                shape = Qt::SizeHorCursor;
            } else if (mode & ResizeTop || mode & ResizeBottom) {
                shape = Qt::SizeVerCursor;
            }

            this->setCursor(shape);
            watchedWidget->setCursor(shape);
            return true;
        } else {
            this->unsetCursor();
            watchedWidget->unsetCursor();
        }
    }
    else if (event->type() == QEvent::MouseButtonPress) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        QPoint localPos = mapFromGlobal(mouseEvent->globalPosition().toPoint());
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
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void ThemedWindow::updateWindowChromeState() {
    if (!isMaximized()) {
        _forceRoundedChrome = false;
    }
    applyWindowChromeState(isMaximized());
}

void ThemedWindow::applyWindowChromeState(bool maximized) {
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

    if (_centralWidget) {
        if (auto* mainWindow = qobject_cast<QMainWindow*>(_centralWidget)) {
            if (QStatusBar* bar = mainWindow->statusBar()) {
                bar->setProperty("maximized", maximized);
                bar->style()->unpolish(bar);
                bar->style()->polish(bar);
                bar->update();
            }
        }
    }
}

void ThemedWindow::prepareForMaximizeTransition() {
    _forceRoundedChrome = false;
    applyWindowChromeState(true);
    clearMask();
    repaint();
}

void ThemedWindow::prepareForRestoreTransition() {
    _forceRoundedChrome = true;
    applyWindowChromeState(false);
    updateWindowMask();
    update();
}
