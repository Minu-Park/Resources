#include "MainTitleBar.h"
#include <QMainWindow>
#include <QWindow>
#include <QMenuBar>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMouseEvent>
#include <QPixmap>
#include <QStyle>
#include <QStyleOption>
#include <QPainter>

MainTitleBar::MainTitleBar(QMainWindow* mainWindow, QMenuBar* menuBar, QWidget* parent)
    : QWidget(parent)
    , _mainWindow(mainWindow)
{
    setObjectName(QStringLiteral("MainTitleBar"));
    setFixedHeight(34);
    setCursor(Qt::ArrowCursor);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 0, 12, 0);
    layout->setSpacing(12);
    layout->setAlignment(Qt::AlignVCenter);

    // App Logo
    _logoLabel = new QLabel(this);
    _logoLabel->setObjectName(QStringLiteral("MainTitleLogo"));
    _logoLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    QPixmap logoPixmap(QStringLiteral(":/Resources/Icon.png"));
    if (!logoPixmap.isNull()) {
        const double dpr = this->devicePixelRatio();
        QPixmap scaledLogo = logoPixmap.scaled(
            QSize(18, 18) * dpr,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        );
        scaledLogo.setDevicePixelRatio(dpr);
        _logoLabel->setPixmap(scaledLogo);
    }
    _logoLabel->setFixedSize(18, 18);
    layout->addWidget(_logoLabel, 0, Qt::AlignVCenter);

    // App Title
    _titleLabel = new QLabel(_mainWindow->windowTitle(), this);
    _titleLabel->setObjectName(QStringLiteral("MainTitleLabel"));
    _titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(_titleLabel, 0, Qt::AlignVCenter);

    // Menubar integration directly into the single row
    if (menuBar) {
        menuBar->setParent(this);
        menuBar->setNativeMenuBar(false);
        menuBar->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
        layout->addWidget(menuBar, 0, Qt::AlignVCenter);
    }

    layout->addStretch();

    // System Window Control Buttons bundled in a tight layout
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(6);

    _minButton = new QPushButton(this);
    _minButton->setObjectName(QStringLiteral("MainMinButton"));
    _minButton->setFocusPolicy(Qt::NoFocus);

    _maxButton = new QPushButton(this);
    _maxButton->setObjectName(QStringLiteral("MainMaxButton"));
    _maxButton->setFocusPolicy(Qt::NoFocus);
    _maxButton->setProperty("maximized", false);

    _closeButton = new QPushButton(this);
    _closeButton->setObjectName(QStringLiteral("MainCloseButton"));
    _closeButton->setFocusPolicy(Qt::NoFocus);

    buttonLayout->addWidget(_minButton, 0, Qt::AlignVCenter);
    buttonLayout->addWidget(_maxButton, 0, Qt::AlignVCenter);
    buttonLayout->addWidget(_closeButton, 0, Qt::AlignVCenter);
    layout->addLayout(buttonLayout);

    connect(_minButton, &QPushButton::clicked, _mainWindow, &QWidget::showMinimized);
    connect(_maxButton, &QPushButton::clicked, [this]() {
        if (_mainWindow->isMaximized()) {
            _mainWindow->showNormal();
        } else {
            _mainWindow->showMaximized();
        }
        updateMaximizeIcon();
    });
    connect(_closeButton, &QPushButton::clicked, _mainWindow, &QWidget::close);

    connect(_mainWindow, &QWidget::windowTitleChanged, _titleLabel, &QLabel::setText);

    _minButton->installEventFilter(this);
    _maxButton->installEventFilter(this);
    _closeButton->installEventFilter(this);

    _minButton->setIcon(QIcon(QStringLiteral(":/Resources/Icons/icons8-minimize-window-48.png")));
    _minButton->setIconSize(QSize(16, 16));

    _closeButton->setIcon(QIcon(QStringLiteral(":/Resources/Icons/icons8-close-window-48.png")));
    _closeButton->setIconSize(QSize(16, 16));

    _maxButton->setIconSize(QSize(16, 16));
    updateMaximizeIcon();
}

void MainTitleBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && !_mainWindow->isMaximized()) {
        if (auto* window = _mainWindow->windowHandle()) {
            if (window->startSystemMove()) {
                event->accept();
                return;
            }
        }
        _dragPosition = event->globalPosition().toPoint() - _mainWindow->frameGeometry().topLeft();
        _isDragging = true;
        event->accept();
    } else {
        QWidget::mousePressEvent(event);
    }
}

void MainTitleBar::mouseMoveEvent(QMouseEvent* event)
{
    if (_isDragging && (event->buttons() & Qt::LeftButton)) {
        _mainWindow->move(event->globalPosition().toPoint() - _dragPosition);
        event->accept();
    } else {
        QWidget::mouseMoveEvent(event);
    }
}

void MainTitleBar::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        _isDragging = false;
        event->accept();
    } else {
        QWidget::mouseReleaseEvent(event);
    }
}

void MainTitleBar::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        if (_mainWindow->isMaximized()) {
            _mainWindow->showNormal();
        } else {
            _mainWindow->showMaximized();
        }
        updateMaximizeIcon();
        event->accept();
    } else {
        QWidget::mouseDoubleClickEvent(event);
    }
}

void MainTitleBar::updateMaximizeIcon()
{
    bool isMax = _mainWindow->isMaximized();
    _maxButton->setProperty("maximized", isMax);

    bool underMouse = _maxButton->underMouse();
    _maxButton->setIcon(QIcon(underMouse ? QStringLiteral(":/Resources/Icons/icons8-maximize-window-48-hover.png") : QStringLiteral(":/Resources/Icons/icons8-maximize-window-48.png")));
}

bool MainTitleBar::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == _minButton) {
        if (event->type() == QEvent::Enter) {
            _minButton->setIcon(QIcon(QStringLiteral(":/Resources/Icons/icons8-minimize-window-48-hover.png")));
        } else if (event->type() == QEvent::Leave) {
            _minButton->setIcon(QIcon(QStringLiteral(":/Resources/Icons/icons8-minimize-window-48.png")));
        }
    }
    else if (watched == _closeButton) {
        if (event->type() == QEvent::Enter) {
            _closeButton->setIcon(QIcon(QStringLiteral(":/Resources/Icons/icons8-close-window-48-hover.png")));
        } else if (event->type() == QEvent::Leave) {
            _closeButton->setIcon(QIcon(QStringLiteral(":/Resources/Icons/icons8-close-window-48.png")));
        }
    }
    else if (watched == _maxButton) {
        if (event->type() == QEvent::Enter) {
            _maxButton->setIcon(QIcon(QStringLiteral(":/Resources/Icons/icons8-maximize-window-48-hover.png")));
        } else if (event->type() == QEvent::Leave) {
            _maxButton->setIcon(QIcon(QStringLiteral(":/Resources/Icons/icons8-maximize-window-48.png")));
        }
    }
    return QWidget::eventFilter(watched, event);
}

void MainTitleBar::paintEvent(QPaintEvent* event)
{
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
    QWidget::paintEvent(event);
}
