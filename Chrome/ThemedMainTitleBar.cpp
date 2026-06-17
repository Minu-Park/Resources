#include "ThemedMainTitleBar.h"
#include <QMainWindow>
#include <QWindow>
#include <QMenuBar>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMouseEvent>
#include <QMetaObject>
#include <QPixmap>
#include <QStyle>
#include <QStyleOption>
#include <QPainter>

ThemedMainTitleBar::ThemedMainTitleBar(QMainWindow* mainWindow, QMenuBar* menuBar, QWidget* parent)
    : QWidget(parent)
    , _mainWindow(mainWindow)
{
    setObjectName(QStringLiteral("ThemedMainTitleBar"));
    setFixedHeight(40);
    setCursor(Qt::ArrowCursor);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 0, 13, 0);
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

    // App Title - Use parent's windowTitle (ThemedWindow)
    _titleLabel = new QLabel(parent ? parent->windowTitle() : QString(), this);
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
    buttonLayout->setSpacing(0);

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

    connect(_minButton, &QPushButton::clicked, this, [this]() {
        if (auto* topLevel = this->window()) {
            topLevel->showMinimized();
        }
    });
    connect(_maxButton, &QPushButton::clicked, [this]() {
        if (auto* topLevel = this->window()) {
            if (topLevel->isMaximized()) {
                topLevel->showNormal();
            } else {
                topLevel->showMaximized();
            }
            updateMaximizeIcon();
        }
    });
    connect(_closeButton, &QPushButton::clicked, this, [this]() {
        if (auto* topLevel = this->window()) {
            topLevel->close();
        }
    });

    if (parent) {
        connect(parent, &QWidget::windowTitleChanged, _titleLabel, &QLabel::setText);
    }

    _minButton->installEventFilter(this);
    _maxButton->installEventFilter(this);
    _closeButton->installEventFilter(this);

    _minButton->setIcon(createSmoothIcon(QStringLiteral(":/Resources/Icons/icons8-minimize-window-48.png"), QSize(16, 16)));
    _minButton->setIconSize(QSize(16, 16));

    _closeButton->setIcon(createSmoothIcon(QStringLiteral(":/Resources/Icons/icons8-close-window-48.png"), QSize(16, 16)));
    _closeButton->setIconSize(QSize(16, 16));

    _maxButton->setIconSize(QSize(16, 16));
    updateMaximizeIcon();
}

void ThemedMainTitleBar::mousePressEvent(QMouseEvent* event)
{
    auto* topLevel = this->window();
    if (event->button() == Qt::LeftButton && topLevel && !topLevel->isMaximized()) {
        if (auto* window = topLevel->windowHandle()) {
            if (window->startSystemMove()) {
                event->accept();
                return;
            }
        }
        _dragPosition = event->globalPosition().toPoint() - topLevel->frameGeometry().topLeft();
        _isDragging = true;
        event->accept();
    } else {
        QWidget::mousePressEvent(event);
    }
}

void ThemedMainTitleBar::mouseMoveEvent(QMouseEvent* event)
{
    auto* topLevel = this->window();
    if (_isDragging && topLevel && (event->buttons() & Qt::LeftButton)) {
        topLevel->move(event->globalPosition().toPoint() - _dragPosition);
        event->accept();
    } else {
        QWidget::mouseMoveEvent(event);
    }
}

void ThemedMainTitleBar::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        _isDragging = false;
        event->accept();
    } else {
        QWidget::mouseReleaseEvent(event);
    }
}

void ThemedMainTitleBar::mouseDoubleClickEvent(QMouseEvent* event)
{
    auto* topLevel = this->window();
    if (event->button() == Qt::LeftButton && topLevel) {
        if (topLevel->isMaximized()) {
            topLevel->showNormal();
        } else {
            topLevel->showMaximized();
        }
        updateMaximizeIcon();
        event->accept();
    } else {
        QWidget::mouseDoubleClickEvent(event);
    }
}

void ThemedMainTitleBar::updateMaximizeIcon()
{
    auto* topLevel = this->window();
    bool isMax = topLevel ? topLevel->isMaximized() : false;
    _maxButton->setProperty("maximized", isMax);

    bool underMouse = _maxButton->underMouse();
    QString path = underMouse ? QStringLiteral(":/Resources/Icons/icons8-maximize-window-48-hover.png") : QStringLiteral(":/Resources/Icons/icons8-maximize-window-48.png");
    _maxButton->setIcon(createSmoothIcon(path, QSize(16, 16)));
}

bool ThemedMainTitleBar::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == _minButton) {
        if (event->type() == QEvent::Enter) {
            _minButton->setIcon(createSmoothIcon(QStringLiteral(":/Resources/Icons/icons8-minimize-window-48-hover.png"), QSize(16, 16)));
        } else if (event->type() == QEvent::Leave) {
            _minButton->setIcon(createSmoothIcon(QStringLiteral(":/Resources/Icons/icons8-minimize-window-48.png"), QSize(16, 16)));
        }
    }
    else if (watched == _closeButton) {
        if (event->type() == QEvent::Enter) {
            _closeButton->setIcon(createSmoothIcon(QStringLiteral(":/Resources/Icons/icons8-close-window-48-hover.png"), QSize(16, 16)));
        } else if (event->type() == QEvent::Leave) {
            _closeButton->setIcon(createSmoothIcon(QStringLiteral(":/Resources/Icons/icons8-close-window-48.png"), QSize(16, 16)));
        }
    }
    else if (watched == _maxButton) {
        if (event->type() == QEvent::Enter) {
            _maxButton->setIcon(createSmoothIcon(QStringLiteral(":/Resources/Icons/icons8-maximize-window-48-hover.png"), QSize(16, 16)));
        } else if (event->type() == QEvent::Leave) {
            _maxButton->setIcon(createSmoothIcon(QStringLiteral(":/Resources/Icons/icons8-maximize-window-48.png"), QSize(16, 16)));
        }
    }
    return QWidget::eventFilter(watched, event);
}

void ThemedMainTitleBar::paintEvent(QPaintEvent* event)
{
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
    QWidget::paintEvent(event);
}

QIcon ThemedMainTitleBar::createSmoothIcon(const QString& path, const QSize& logicalSize) const
{
    const QPixmap source(path);
    if (source.isNull()) {
        return QIcon();
    }

    const qreal dpr = devicePixelRatioF();
    QPixmap target(logicalSize * dpr);
    target.fill(Qt::transparent);

    {
        QPainter painter(&target);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.drawPixmap(target.rect(), source);
    }

    target.setDevicePixelRatio(dpr);
    return QIcon(target);
}

