#include "CustomTitleBar.h"
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

CustomTitleBar::CustomTitleBar(QMdiSubWindow* subWindow, QMenuBar* menuBar, QWidget* parent)
    : QWidget(parent)
    , _subWindow(subWindow)
    , _menuBar(menuBar)
{
    setObjectName(QStringLiteral("CustomTitleBar"));
    setFixedHeight(32);
    setCursor(Qt::ArrowCursor);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 0, 12, 0);
    layout->setSpacing(6);
    layout->setAlignment(Qt::AlignVCenter);

    // Title label is added to the layout directly
    _titleLabel = new QLabel(_subWindow->windowTitle(), this);
    _titleLabel->setObjectName(QStringLiteral("CustomTitleBarLabel"));
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
    buttonLayout->setSpacing(6); // 6px spacing to perfectly align centers with MainTitleBar 18px buttons (which use 4px spacing)

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

    connect(_minButton, &QPushButton::clicked, _subWindow, &QWidget::showMinimized);
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
}

void CustomTitleBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && !_subWindow->isMaximized()) {
        _dragPosition = event->globalPosition().toPoint() - _subWindow->frameGeometry().topLeft();
        _isDragging = true;
        event->accept();
    } else {
        QWidget::mousePressEvent(event);
    }
}

void CustomTitleBar::mouseMoveEvent(QMouseEvent* event)
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

void CustomTitleBar::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        _isDragging = false;
        event->accept();
    } else {
        QWidget::mouseReleaseEvent(event);
    }
}

void CustomTitleBar::mouseDoubleClickEvent(QMouseEvent* event)
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

void CustomTitleBar::updateMaximizeIcon()
{
    bool isMax = _subWindow->isMaximized();
    if (_maxButton->property("maximized").toBool() != isMax) {
        _maxButton->setProperty("maximized", isMax);
        _maxButton->style()->unpolish(_maxButton);
        _maxButton->style()->polish(_maxButton);
    }
}

void CustomTitleBar::paintEvent(QPaintEvent* event)
{
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
    QWidget::paintEvent(event);
}

void CustomTitleBar::updateState(bool isMinimized)
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
