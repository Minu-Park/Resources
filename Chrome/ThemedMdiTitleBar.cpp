#include "ThemedMdiTitleBar.h"
#include <QMdiSubWindow>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMouseEvent>
#include <QStyle>
#include <QStyleOption>
#include <QPainter>
#include <QCoreApplication>
#include <QMenuBar>

ThemedMdiTitleBar::ThemedMdiTitleBar(QMdiSubWindow* subWindow, QMenuBar* menuBar, QWidget* parent)
    : QWidget(parent)
    , _subWindow(subWindow)
    , _menuBar(menuBar)
{
    setObjectName(QStringLiteral("ThemedMdiTitleBar"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedHeight(32);
    setCursor(Qt::ArrowCursor);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(11, 0, 12, 0);
    layout->setSpacing(6);
    layout->setAlignment(Qt::AlignVCenter);

    // Title label and icon are added to the layout directly
    if (!_subWindow->windowIcon().isNull()) {
        auto* iconLabel = new QLabel(this);
        iconLabel->setObjectName(QStringLiteral("ThemedMdiTitleBarIconLabel"));
        iconLabel->setPixmap(_subWindow->windowIcon().pixmap(18, 18));
        iconLabel->setFixedSize(18, 18);
        iconLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        layout->addWidget(iconLabel, 0, Qt::AlignVCenter);
        layout->addSpacing(6);
    }

    _titleLabel = new QLabel(_subWindow->windowTitle(), this);
    _titleLabel->setObjectName(QStringLiteral("ThemedMdiTitleBarLabel"));
    _titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(_titleLabel, 0, Qt::AlignVCenter);

    // Integrate the session's menu bar right next to the title label
    if (_menuBar) {
        _menuBar->setParent(this);
        _menuBar->setNativeMenuBar(false);
        _menuBar->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
        layout->addWidget(_menuBar, 0, Qt::AlignVCenter);
    }

    layout->addStretch();

    // System control buttons bundled in a tight layout
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(0);

    _minButton = new QPushButton(this);
    _minButton->setObjectName(QStringLiteral("TitleMinButton"));
    _minButton->setFocusPolicy(Qt::NoFocus);

    _maxButton = new QPushButton(this);
    _maxButton->setObjectName(QStringLiteral("TitleMaxButton"));
    _maxButton->setFocusPolicy(Qt::NoFocus);
    _maxButton->setProperty("maximized", false);

    _closeButton = new QPushButton(this);
    _closeButton->setObjectName(QStringLiteral("TitleCloseButton"));
    _closeButton->setFocusPolicy(Qt::NoFocus);

    buttonLayout->addWidget(_minButton, 0, Qt::AlignVCenter);
    buttonLayout->addWidget(_maxButton, 0, Qt::AlignVCenter);
    buttonLayout->addWidget(_closeButton, 0, Qt::AlignVCenter);
    layout->addLayout(buttonLayout);

    connect(_minButton, &QPushButton::clicked, [this]() {
        _subWindow->setProperty("wasMaximizedBeforeMinimize", _subWindow->isMaximized());
        _subWindow->setMinimumSize(QSize(0, 0));
        _subWindow->showMinimized();
    });
    connect(_maxButton, &QPushButton::clicked, [this]() {
        if (_subWindow->isMaximized()) {
            _subWindow->showNormal();
        } else {
            _subWindow->showMaximized();
        }
        updateMaximizeIcon();
    });
    connect(_closeButton, &QPushButton::clicked, _subWindow, &QWidget::close);

    connect(_subWindow, &QWidget::windowTitleChanged, this, [this](const QString& title) {
        if (_isMinimized) {
            QString elided = _titleLabel->fontMetrics().elidedText(title, Qt::ElideRight, 80);
            _titleLabel->setText(elided);
        } else {
            _titleLabel->setText(title);
        }
    });
    setMouseTracking(true);

    // Register event filters to handle mouse hover swap dynamically
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

void ThemedMdiTitleBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && !_subWindow->isMaximized()) {
        _dragPosition = event->globalPosition().toPoint() - _subWindow->frameGeometry().topLeft();
        _isDragging = true;
        event->accept();
    } else {
        QWidget::mousePressEvent(event);
    }
}

void ThemedMdiTitleBar::mouseMoveEvent(QMouseEvent* event)
{
    if (_isDragging && (event->buttons() & Qt::LeftButton)) {
        _subWindow->move(event->globalPosition().toPoint() - _dragPosition);
        event->accept();
    } else {
        if (parentWidget()) {
            QMouseEvent translatedEvent(
                event->type(),
                parentWidget()->mapFromGlobal(event->globalPosition().toPoint()),
                event->globalPosition(),
                event->button(),
                event->buttons(),
                event->modifiers()
            );
            QCoreApplication::sendEvent(parentWidget(), &translatedEvent);
        }
        QWidget::mouseMoveEvent(event);
    }
}

void ThemedMdiTitleBar::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        _isDragging = false;
        event->accept();
    } else {
        QWidget::mouseReleaseEvent(event);
    }
}

void ThemedMdiTitleBar::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        if (_subWindow->isMaximized()) {
            _subWindow->showNormal();
        } else {
            _subWindow->showMaximized();
        }
        updateMaximizeIcon();
        event->accept();
    } else {
        QWidget::mouseDoubleClickEvent(event);
    }
}

void ThemedMdiTitleBar::updateMaximizeIcon()
{
    bool isMax = _subWindow->isMaximized();
    _maxButton->setProperty("maximized", isMax);

    bool underMouse = _maxButton->underMouse();
    QString path = underMouse ? QStringLiteral(":/Resources/Icons/icons8-maximize-window-48-hover.png") : QStringLiteral(":/Resources/Icons/icons8-maximize-window-48.png");
    _maxButton->setIcon(createSmoothIcon(path, QSize(16, 16)));
}

bool ThemedMdiTitleBar::eventFilter(QObject* watched, QEvent* event)
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

void ThemedMdiTitleBar::paintEvent(QPaintEvent* event)
{
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
    QWidget::paintEvent(event);
}

QIcon ThemedMdiTitleBar::createSmoothIcon(const QString& path, const QSize& logicalSize) const
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

void ThemedMdiTitleBar::updateState(bool isMinimized)
{
    _isMinimized = isMinimized;
    
    if (_isMinimized) {
        _minButton->hide();
        if (_menuBar) {
            _menuBar->hide();
        }
        QString orig = _subWindow->windowTitle();
        QString elided = _titleLabel->fontMetrics().elidedText(orig, Qt::ElideRight, 80);
        _titleLabel->setText(elided);
    } else {
        _minButton->show();
        if (_menuBar) {
            _menuBar->show();
        }
        _titleLabel->setText(_subWindow->windowTitle());
    }
}

