#include "ThemedDialog.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QStyle>
#include <QStyleOption>
#include <QVBoxLayout>
#include <QWindow>

class ThemedDialog::TitleBar final : public QWidget {
public:
    explicit TitleBar(ThemedDialog* dialog, const QString& title)
        : QWidget(dialog)
        , _dialog(dialog)
    {
        setObjectName(QStringLiteral("ThemedDialogTitleBar"));
        setFixedHeight(30);

        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(12, 0, 12, 0);
        layout->setSpacing(6);

        _titleLabel = new QLabel(title, this);
        _titleLabel->setObjectName(QStringLiteral("ThemedDialogTitleLabel"));
        _titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        layout->addWidget(_titleLabel, 0, Qt::AlignVCenter);

        layout->addStretch();

        _closeButton = new QPushButton(this);
        _closeButton->setObjectName(QStringLiteral("ThemedDialogCloseButton"));
        _closeButton->setFocusPolicy(Qt::NoFocus);
        _closeButton->setFixedSize(20, 20);
        _closeButton->setIconSize(QSize(16, 16));
        setCloseIcon(QStringLiteral(":/Resources/Icons/icons8-close-window-48.png"));

        layout->addWidget(_closeButton, 0, Qt::AlignVCenter);

        connect(_closeButton, &QPushButton::clicked, _dialog, &QDialog::close);
        _closeButton->installEventFilter(this);
    }

    void setTitleText(const QString& title)
    {
        _titleLabel->setText(title);
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == _closeButton) {
            if (event->type() == QEvent::Enter) {
                setCloseIcon(QStringLiteral(":/Resources/Icons/icons8-close-window-48-hover.png"));
            } else if (event->type() == QEvent::Leave) {
                setCloseIcon(QStringLiteral(":/Resources/Icons/icons8-close-window-48.png"));
            }
        }
        return QWidget::eventFilter(watched, event);
    }

    void paintEvent(QPaintEvent* event) override
    {
        QStyleOption opt;
        opt.initFrom(this);
        QPainter painter(this);
        style()->drawPrimitive(QStyle::PE_Widget, &opt, &painter, this);
        QWidget::paintEvent(event);
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() != Qt::LeftButton) {
            QWidget::mousePressEvent(event);
            return;
        }

        if (QWindow* window = _dialog->windowHandle()) {
            if (window->startSystemMove()) {
                event->accept();
                return;
            }
        }

        _dragPosition = event->globalPosition().toPoint() - _dialog->frameGeometry().topLeft();
        _isDragging = true;
        event->accept();
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (_isDragging && (event->buttons() & Qt::LeftButton)) {
            _dialog->move(event->globalPosition().toPoint() - _dragPosition);
            event->accept();
            return;
        }
        QWidget::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) {
            _isDragging = false;
            event->accept();
            return;
        }
        QWidget::mouseReleaseEvent(event);
    }

private:
    void setCloseIcon(const QString& path)
    {
        const QIcon icon = createSmoothIcon(path, QSize(16, 16));
        if (!icon.isNull()) {
            _closeButton->setIcon(icon);
        }
    }

    QIcon createSmoothIcon(const QString& path, const QSize& logicalSize) const
    {
        QPixmap pixmap(path);
        if (pixmap.isNull()) {
            return QIcon();
        }
        const double dpr = devicePixelRatio();
        QPixmap scaledPixmap = pixmap.scaled(
            logicalSize * dpr,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        );
        scaledPixmap.setDevicePixelRatio(dpr);
        return QIcon(scaledPixmap);
    }

    ThemedDialog* _dialog = nullptr;
    QLabel* _titleLabel = nullptr;
    QPushButton* _closeButton = nullptr;
    QPoint _dragPosition;
    bool _isDragging = false;
};

ThemedDialog::ThemedDialog(const QString& title, QWidget* parent)
    : QDialog(parent)
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setWindowTitle(title);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    auto* container = new QWidget(this);
    container->setObjectName(QStringLiteral("ThemedDialogContainer"));
    mainLayout->addWidget(container);

    auto* rootLayout = new QVBoxLayout(container);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    _titleBar = new TitleBar(this, title);
    rootLayout->addWidget(_titleBar);

    _contentWidget = new QWidget(container);
    _contentLayout = new QVBoxLayout(_contentWidget);
    _contentLayout->setContentsMargins(16, 12, 16, 12);
    _contentLayout->setSpacing(12);
    rootLayout->addWidget(_contentWidget);
}

QWidget* ThemedDialog::contentWidget() const
{
    return _contentWidget;
}

QVBoxLayout* ThemedDialog::contentLayout() const
{
    return _contentLayout;
}

void ThemedDialog::setTitleText(const QString& title)
{
    setWindowTitle(title);
    _titleBar->setTitleText(title);
}
