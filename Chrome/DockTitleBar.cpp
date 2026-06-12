#include "DockTitleBar.h"
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

DockTitleBar::DockTitleBar(QDockWidget* dockWidget, QWidget* parent)
    : QWidget(parent)
    , _dockWidget(dockWidget)
{
    setObjectName(QStringLiteral("DockTitleBar"));
    setFixedHeight(28); // Standard compact height for dock widget title bars

    auto* layout = new QHBoxLayout(this);
    // Align title bar text and buttons, adding slight top margin to lower the font
    layout->setContentsMargins(12, 2, 12, 0);
    layout->setSpacing(6);
    layout->setAlignment(Qt::AlignVCenter);

    // Title label
    _titleLabel = new QLabel(_dockWidget->windowTitle(), this);
    _titleLabel->setObjectName(QStringLiteral("DockTitleBarLabel"));
    _titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(_titleLabel, 0, Qt::AlignVCenter);

    layout->addStretch();

    // System control buttons bundled in a tight layout
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(6); // 6px spacing to perfectly align centers with MainTitleBar 18px buttons (which use 4px spacing)

    // Floating/Restore button
    _floatButton = new QPushButton(this);
    _floatButton->setObjectName(QStringLiteral("DockMaxButton"));
    _floatButton->setFocusPolicy(Qt::NoFocus);

    // Close button
    _closeButton = new QPushButton(this);
    _closeButton->setObjectName(QStringLiteral("DockCloseButton"));
    _closeButton->setFocusPolicy(Qt::NoFocus);

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

    _closeButton->setIcon(QIcon(QStringLiteral(":/Resources/Icons/icons8-close-window-48.png")));
    _closeButton->setIconSize(QSize(16, 16));

    _floatButton->setIconSize(QSize(16, 16));
    _floatButton->setIcon(QIcon(QStringLiteral(":/Resources/Icons/icons8-maximize-window-48.png")));

    _dockWidget->installEventFilter(this);
    connect(_dockWidget, &QDockWidget::topLevelChanged, this, &DockTitleBar::applyFloatingChrome);
    applyFloatingChrome(_dockWidget->isFloating());
}

void DockTitleBar::paintEvent(QPaintEvent* event)
{
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
    QWidget::paintEvent(event);
}

void DockTitleBar::mousePressEvent(QMouseEvent* event)
{
    event->ignore(); // Let QDockWidget handle drag and undock natively at all times
}

void DockTitleBar::mouseMoveEvent(QMouseEvent* event)
{
    event->ignore();
}

void DockTitleBar::mouseReleaseEvent(QMouseEvent* event)
{
    event->ignore();
}

bool DockTitleBar::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == _dockWidget) {
        if (event->type() == QEvent::Resize || event->type() == QEvent::Show) {
            updateFloatingMask();
        }
    }
    else if (watched == _closeButton) {
        if (event->type() == QEvent::Enter) {
            _closeButton->setIcon(QIcon(QStringLiteral(":/Resources/Icons/icons8-close-window-48-hover.png")));
        } else if (event->type() == QEvent::Leave) {
            _closeButton->setIcon(QIcon(QStringLiteral(":/Resources/Icons/icons8-close-window-48.png")));
        }
    }
    else if (watched == _floatButton) {
        if (event->type() == QEvent::Enter) {
            _floatButton->setIcon(QIcon(QStringLiteral(":/Resources/Icons/icons8-maximize-window-48-hover.png")));
        } else if (event->type() == QEvent::Leave) {
            _floatButton->setIcon(QIcon(QStringLiteral(":/Resources/Icons/icons8-maximize-window-48.png")));
        }
    }
    return QWidget::eventFilter(watched, event);
}

void DockTitleBar::applyFloatingChrome(bool floating)
{
    _dockWidget->setProperty("floatingState", floating);
    setProperty("floatingState", floating);

    if (QWidget* container = _dockWidget->widget()) {
        container->setProperty("floatingState", floating);
    }

    _dockWidget->setAttribute(Qt::WA_TranslucentBackground, floating);
    _dockWidget->setAttribute(Qt::WA_NoSystemBackground, floating);
    _dockWidget->setWindowFlag(Qt::FramelessWindowHint, floating);

    if (floating && _dockWidget->layout()) {
        _dockWidget->layout()->setContentsMargins(0, 0, 0, 0);
        _dockWidget->layout()->setSpacing(0);
    }

    refreshFloatingChromeStyle();
    updateFloatingMask();

    if (_dockWidget->isVisible()) {
        QTimer::singleShot(0, _dockWidget, [this]() {
            _dockWidget->show();
            updateFloatingMask();
        });
    }
}

void DockTitleBar::refreshFloatingChromeStyle()
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

void DockTitleBar::updateFloatingMask()
{
    if (!_dockWidget->isFloating()) {
        _dockWidget->clearMask();
        return;
    }

    const QSize size = _dockWidget->size();
    if (size.isEmpty()) {
        return;
    }

    QBitmap mask(size);
    mask.fill(Qt::color0);

    QPainter painter(&mask);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setBrush(Qt::color1);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(_dockWidget->rect(), 12, 12);

    _dockWidget->setMask(mask);
}

QSize DockTitleBar::sizeHint() const
{
    return QSize(QWidget::sizeHint().width(), 28);
}

QSize DockTitleBar::minimumSizeHint() const
{
    return QSize(QWidget::minimumSizeHint().width(), 28);
}

void DockTitleBar::setupDockWidget(QDockWidget* dockWidget, QWidget* contentWidget)
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
    auto* titleBar = new DockTitleBar(dockWidget, dockWidget);
    dockWidget->setTitleBarWidget(titleBar);
}
