#include "DockTitleBar.h"
#include <QDockWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QWindow>
#include <QMouseEvent>
#include <QStyle>
#include <QStyleOption>
#include <QPainter>
#include <QBitmap>
#include <QFrame>
#include <QVBoxLayout>

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
    _closeButton->setIconSize(QSize(16, 16));

    _floatButton->setIconSize(QSize(16, 16));
    updateFloatIcon();

    _dockWidget->installEventFilter(this);

    connect(_dockWidget, &QDockWidget::topLevelChanged, this, &DockTitleBar::handleTopLevelChanged);
    handleTopLevelChanged(_dockWidget->isFloating());
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
    if (event->button() == Qt::LeftButton && _dockWidget->isFloating()) {
        if (auto* window = _dockWidget->windowHandle()) {
            if (window->startSystemMove()) {
                event->accept();
                return;
            }
        }
        _dragPosition = event->globalPosition().toPoint() - _dockWidget->frameGeometry().topLeft();
        _isDragging = true;
        event->accept();
    } else {
        event->ignore(); // Let QDockWidget handle drag and undock natively
    }
}

void DockTitleBar::mouseMoveEvent(QMouseEvent* event)
{
    if (_isDragging && (event->buttons() & Qt::LeftButton)) {
        _dockWidget->move(event->globalPosition().toPoint() - _dragPosition);
        event->accept();
    } else {
        event->ignore();
    }
}

void DockTitleBar::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        _isDragging = false;
        event->accept();
    } else {
        event->ignore();
    }
}

void DockTitleBar::handleTopLevelChanged(bool topLevel)
{
    _dockWidget->setProperty("floatingState", topLevel);
    this->setProperty("floatingState", topLevel);

    QWidget* container = _dockWidget->widget();
    if (container)
    {
        container->setProperty("floatingState", topLevel);
    }

    if (topLevel)
    {
        // Save current position and apply frameless hint
        QPoint pos = _dockWidget->pos();
        _dockWidget->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
        _dockWidget->setAttribute(Qt::WA_TranslucentBackground, true);
        _dockWidget->setAttribute(Qt::WA_NoSystemBackground, true);
        _dockWidget->winId(); // Force platform window creation
        _dockWidget->move(pos);
        _dockWidget->show();

        if (_dockWidget->layout())
        {
            _dockWidget->layout()->setContentsMargins(0, 0, 0, 0);
            _dockWidget->layout()->setSpacing(0);
        }
    }
    else
    {
        _dockWidget->setWindowFlags(Qt::Widget);
        _dockWidget->setAttribute(Qt::WA_TranslucentBackground, false);
        _dockWidget->setAttribute(Qt::WA_NoSystemBackground, false);
    }

    updateDockMask();

    _dockWidget->style()->unpolish(_dockWidget);
    _dockWidget->style()->polish(_dockWidget);

    if (container)
    {
        container->style()->unpolish(container);
        container->style()->polish(container);
        for (QObject* child : container->children())
        {
            if (QWidget* childWidget = qobject_cast<QWidget*>(child))
            {
                childWidget->style()->unpolish(childWidget);
                childWidget->style()->polish(childWidget);
            }
        }
    }

    this->style()->unpolish(this);
    this->style()->polish(this);
    _dockWidget->update();
}

void DockTitleBar::updateDockMask()
{
    if (_dockWidget)
    {
        _dockWidget->clearMask();
    }
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
    if (watched == _dockWidget) {
        if (event->type() == QEvent::Resize || event->type() == QEvent::Show) {
            updateDockMask();
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

    // Pre-enable translucent background to ensure smooth frameless rounding works when undocked
    dockWidget->setAttribute(Qt::WA_TranslucentBackground, true);

    // Create container frame
    QFrame* container = new QFrame(dockWidget);
    container->setObjectName(QStringLiteral("DockContainerWidget"));

    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Create title bar inside container
    auto* titleBar = new DockTitleBar(dockWidget, container);

    layout->addWidget(titleBar);
    layout->addWidget(contentWidget, 1);

    // Set container to dock widget
    dockWidget->setWidget(container);

    // Hide native titlebar using dummy widget
    auto* dummyTitle = new QWidget(dockWidget);
    dummyTitle->setFixedHeight(0);
    dummyTitle->setVisible(false);
    dockWidget->setTitleBarWidget(dummyTitle);
}
