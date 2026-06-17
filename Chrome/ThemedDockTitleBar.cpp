#include "ThemedDockTitleBar.h"
#include "Resources.h"
#include <QDockWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMouseEvent>
#include <QStyle>
#include <QStyleOption>
#include <QPainter>
#include <QBitmap>
#include <QFrame>
#include <QVBoxLayout>
#include <QTimer>

ThemedDockTitleBar::ThemedDockTitleBar(QDockWidget* dockWidget, QWidget* parent)
    : QFrame(parent)
    , _dockWidget(dockWidget)
{
    setObjectName(QStringLiteral("ThemedDockTitleBar"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedHeight(28); // Standard compact height for dock widget title bars

    auto* layout = new QHBoxLayout(this);
    // Align title bar text and buttons, adding slight top margin to lower the font
    layout->setContentsMargins(12, 2, 12, 0);
    layout->setSpacing(6);
    layout->setAlignment(Qt::AlignVCenter);

    // Title label
    _titleLabel = new QLabel(_dockWidget->windowTitle(), this);
    _titleLabel->setObjectName(QStringLiteral("ThemedDockTitleBarLabel"));
    _titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(_titleLabel, 0, Qt::AlignVCenter);

    layout->addStretch();

    // System control buttons bundled in a tight layout
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(0);

    // Floating/Restore button
    _floatButton = new QPushButton(this);
    _floatButton->setObjectName(QStringLiteral("DockMaxButton"));
    _floatButton->setFocusPolicy(Qt::NoFocus);
    _floatButton->setVisible(_dockWidget->features().testFlag(QDockWidget::DockWidgetFloatable));

    // Close button
    _closeButton = new QPushButton(this);
    _closeButton->setObjectName(QStringLiteral("DockCloseButton"));
    _closeButton->setFocusPolicy(Qt::NoFocus);
    _closeButton->setVisible(_dockWidget->features().testFlag(QDockWidget::DockWidgetClosable));

    buttonLayout->addWidget(_floatButton, 0, Qt::AlignVCenter);
    buttonLayout->addWidget(_closeButton, 0, Qt::AlignVCenter);
    layout->addLayout(buttonLayout);

    // Connect window title changes
    connect(_dockWidget, &QWidget::windowTitleChanged, _titleLabel, &QLabel::setText);

    // Connect button click events
    connect(_closeButton, &QPushButton::clicked, _dockWidget, &QWidget::close);
    connect(_floatButton, &QPushButton::clicked, [this]() {
        _dockWidget->setFloating(!_dockWidget->isFloating());
    });

    // Register event filters to handle mouse hover swap dynamically
    _closeButton->installEventFilter(this);
    _floatButton->installEventFilter(this);

    _closeButton->setIcon(createSmoothIcon(QStringLiteral(":/Resources/Icons/icons8-close-window-48.png"), QSize(16, 16)));
    _closeButton->setIconSize(QSize(16, 16));

    _floatButton->setIconSize(QSize(16, 16));
    _floatButton->setIcon(createSmoothIcon(QStringLiteral(":/Resources/Icons/icons8-maximize-window-48.png"), QSize(16, 16)));

    _dockWidget->installEventFilter(this);
    connect(_dockWidget, &QDockWidget::topLevelChanged, this, &ThemedDockTitleBar::applyFloatingChrome);
    applyFloatingChrome(_dockWidget->isFloating());
}

void ThemedDockTitleBar::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    bool floating = _dockWidget ? _dockWidget->isFloating() : false;

    if (floating) {
        const qreal radius = PLATFORM_RADIUS(11.0, 8.0);
        // Clip drawing to our widget rect
        p.setClipRect(rect());
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        // Draw a larger rounded rect so the bottom corners remain square
        p.drawRoundedRect(QRectF(0, 0, width(), height() + radius), radius, radius);

        // Draw the outer border line (left, top, right) matching the rounded top corners
        p.setPen(QPen(QColor(0xd9, 0xe1, 0xea), 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(0.5, 0.5, width() - 1.0, height() + radius), radius, radius);

        // Draw the bottom border line (always straight at the bottom of the titlebar)
        p.setPen(QColor(0xd9, 0xe1, 0xea));
        p.drawLine(0, height() - 1, width(), height() - 1);
    } else {
        // Docked state: plain white background and bottom border only
        p.fillRect(rect(), Qt::white);
        p.setPen(QColor(0xd9, 0xe1, 0xea));
        p.drawLine(rect().bottomLeft(), rect().bottomRight());
    }
}

void ThemedDockTitleBar::mousePressEvent(QMouseEvent* event)
{
    event->ignore(); // Let QDockWidget handle drag and undock natively at all times
}

void ThemedDockTitleBar::mouseMoveEvent(QMouseEvent* event)
{
    event->ignore();
}

void ThemedDockTitleBar::mouseReleaseEvent(QMouseEvent* event)
{
    event->ignore();
}

bool ThemedDockTitleBar::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == _dockWidget) {
#if !defined(Q_OS_MAC) && !defined(Q_OS_LINUX)
        if (event->type() == QEvent::Resize || event->type() == QEvent::Show) {
            updateFloatingMask();
        }
#endif
    }
    else if (watched == _closeButton) {
        if (event->type() == QEvent::Enter) {
            _closeButton->setIcon(createSmoothIcon(QStringLiteral(":/Resources/Icons/icons8-close-window-48-hover.png"), QSize(16, 16)));
        } else if (event->type() == QEvent::Leave) {
            _closeButton->setIcon(createSmoothIcon(QStringLiteral(":/Resources/Icons/icons8-close-window-48.png"), QSize(16, 16)));
        }
    }
    else if (watched == _floatButton) {
        if (event->type() == QEvent::Enter) {
            _floatButton->setIcon(createSmoothIcon(QStringLiteral(":/Resources/Icons/icons8-maximize-window-48-hover.png"), QSize(16, 16)));
        } else if (event->type() == QEvent::Leave) {
            _floatButton->setIcon(createSmoothIcon(QStringLiteral(":/Resources/Icons/icons8-maximize-window-48.png"), QSize(16, 16)));
        }
    }
    return QWidget::eventFilter(watched, event);
}

void ThemedDockTitleBar::applyFloatingChrome(bool floating)
{
    const QString stateStr = floating ? QStringLiteral("true") : QStringLiteral("false");
    _dockWidget->setProperty("floatingState", stateStr);
    setProperty("floatingState", stateStr);

    if (QWidget* container = _dockWidget->widget()) {
        container->setProperty("floatingState", stateStr);
    }

    _dockWidget->setAttribute(Qt::WA_TranslucentBackground, floating);
    _dockWidget->setAttribute(Qt::WA_NoSystemBackground, floating);
    _dockWidget->setWindowFlag(Qt::FramelessWindowHint, floating);

    if (floating) {
        Resources::applyWindowPlatformAttributes(_dockWidget);
    }

    if (floating && _dockWidget->layout()) {
        _dockWidget->layout()->setContentsMargins(0, 0, 0, 0);
        _dockWidget->layout()->setSpacing(0);
    }

    refreshFloatingChromeStyle();
    updateFloatingMask();

    if (_dockWidget->isVisible()) {
        QTimer::singleShot(0, _dockWidget, [this]() {
            _dockWidget->show();
            if (_dockWidget->isFloating()) {
                Resources::applyWindowPlatformAttributes(_dockWidget);
            }
            updateFloatingMask();
        });
    }
}

void ThemedDockTitleBar::refreshFloatingChromeStyle()
{
    _dockWidget->style()->unpolish(_dockWidget);
    _dockWidget->style()->polish(_dockWidget);

    if (QWidget* container = _dockWidget->widget()) {
        container->style()->unpolish(container);
        container->style()->polish(container);
    }

    style()->unpolish(this);
    style()->polish(this);
    _dockWidget->update();
}

void ThemedDockTitleBar::updateFloatingMask()
{
    _dockWidget->clearMask();
}

QIcon ThemedDockTitleBar::createSmoothIcon(const QString& path, const QSize& logicalSize) const
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

QSize ThemedDockTitleBar::sizeHint() const
{
    return QSize(QWidget::sizeHint().width(), 28);
}

QSize ThemedDockTitleBar::minimumSizeHint() const
{
    return QSize(QWidget::minimumSizeHint().width(), 28);
}

void ThemedDockTitleBar::setupDockWidget(QDockWidget* dockWidget, QWidget* contentWidget)
{
    if (!dockWidget || !contentWidget) return;

    // Create container frame
    QFrame* container = new QFrame(dockWidget);
    container->setObjectName(QStringLiteral("DockContainerWidget"));

    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addWidget(contentWidget, 1);

    // Set container to dock widget
    dockWidget->setWidget(container);

    // Install the custom title bar as the real dock title bar so ignored
    // mouse events still reach QDockWidget's native drag/dock handler.
    auto* titleBar = new ThemedDockTitleBar(dockWidget, dockWidget);
    dockWidget->setTitleBarWidget(titleBar);
}

