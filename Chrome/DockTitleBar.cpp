#include "DockTitleBar.h"
#include <QDockWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMouseEvent>
#include <QStyle>
#include <QStyleOption>
#include <QPainter>

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
    _floatButton->setProperty("maximized", _dockWidget->isFloating());

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

    // Update icons on state change
    connect(_dockWidget, &QDockWidget::topLevelChanged, this, &DockTitleBar::updateFloatIcon);
    
    // Accept hover events to avoid parent intervention
    setMouseTracking(true);

    // Register event filters to handle mouse hover swap dynamically
    _closeButton->installEventFilter(this);
    _floatButton->installEventFilter(this);

    _closeButton->setIcon(QIcon(QStringLiteral(":/Resources/Icons/icons8-close-window-48.png")));
    _closeButton->setIconSize(QSize(12, 12));

    _floatButton->setIconSize(QSize(12, 12));
    updateFloatIcon();
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
    event->ignore(); // Let QDockWidget handle drag and undock natively
}

void DockTitleBar::mouseMoveEvent(QMouseEvent* event)
{
    event->ignore();
}

void DockTitleBar::mouseReleaseEvent(QMouseEvent* event)
{
    event->ignore();
}

void DockTitleBar::updateFloatIcon()
{
    bool isFloat = _dockWidget->isFloating();
    _floatButton->setProperty("maximized", isFloat);

    bool underMouse = _floatButton->underMouse();
    _floatButton->setIcon(QIcon(underMouse ? QStringLiteral(":/Resources/Icons/icons8-maximize-window-48-hover.png") : QStringLiteral(":/Resources/Icons/icons8-maximize-window-48.png")));
}

bool DockTitleBar::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == _closeButton) {
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

QSize DockTitleBar::sizeHint() const
{
    return QSize(QWidget::sizeHint().width(), 28);
}

QSize DockTitleBar::minimumSizeHint() const
{
    return QSize(QWidget::minimumSizeHint().width(), 28);
}
