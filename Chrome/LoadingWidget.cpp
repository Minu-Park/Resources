#include "LoadingWidget.h"

LoadingWidget::LoadingWidget(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog | Qt::WindowStaysOnTopHint | Qt::NoDropShadowWindowHint);
    setWindowModality(Qt::NonModal);

    auto* layoutOuter = new QVBoxLayout(this);
    layoutOuter->setContentsMargins(18, 18, 18, 18);
    layoutOuter->setSpacing(0);

    _frame = new QFrame(this);
    _frame->setObjectName(QStringLiteral("LoadingFrame"));
    _frame->setFixedSize(300, 150);

    auto* shadow = new QGraphicsDropShadowEffect(_frame);
    shadow->setBlurRadius(30);
    shadow->setOffset(0, 10);
    shadow->setColor(QColor(0, 0, 0, 55));
    _frame->setGraphicsEffect(shadow);
    layoutOuter->addWidget(_frame);

    auto* layoutInside = new QVBoxLayout(_frame);
    layoutInside->setContentsMargins(16, 16, 16, 16);
    layoutInside->setSpacing(12);

    _labelImage = new QLabel(_frame);
    _labelImage->setObjectName(QStringLiteral("LoadingImage"));
    _labelImage->setAlignment(Qt::AlignCenter);
    _labelImage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    _labelImage->setFixedHeight(0);

    _labelText = new QLabel(_frame);
    _labelText->setObjectName(QStringLiteral("LoadingText"));
    _labelText->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    _labelPercent = new QLabel(_frame);
    _labelPercent->setObjectName(QStringLiteral("LoadingPercent"));
    _labelPercent->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto* layoutTextRow = new QHBoxLayout;
    layoutTextRow->setContentsMargins(0, 0, 0, 0);
    layoutTextRow->setSpacing(0);
    layoutTextRow->addWidget(_labelText, 1);
    layoutTextRow->addWidget(_labelPercent, 0);

    _progress = new ProgressItem(_frame);
    _progress->setObjectName(QStringLiteral("LoadingProgress"));

    layoutInside->addWidget(_labelImage, 0, Qt::AlignHCenter);
    layoutInside->addStretch(1);
    layoutInside->addLayout(layoutTextRow);
    layoutInside->addSpacing(6);
    layoutInside->addWidget(_progress);

    setText(QStringLiteral("Loading..."));
    setImage(QPixmap(QStringLiteral(":/Resources/BASLER_Logo.png")), 80);
    setValue(100);
    setBusy(true);
}

void LoadingWidget::setValue(int v)
{
    _progress->setBusy(false);
    _progress->setValue(v);

    int pct = qBound(0, v, 100);
    _labelPercent->setText(QStringLiteral(" %1%").arg(pct));
    _lastProgress = pct;
}

void LoadingWidget::setText(const QString& text)
{
    _labelText->setText(text);
}

void LoadingWidget::setBusy(bool on)
{
    _progress->setBusy(on);
    _labelPercent->setText(on ? QString() : QStringLiteral("%1%").arg(_lastProgress));
    _labelPercent->setVisible(!on);

    QTimer::singleShot(10, this, [this]{
        update();
        repaint();
    });
}

void LoadingWidget::centerInParent()
{
    if (!parentWidget()) return;

    const QRect pr = parentWidget()->rect();
    const QSize s = sizeHint().isValid() ? sizeHint() : size();
    const QPoint parentGlobal = parentWidget()->mapToGlobal(QPoint(0, 0));

    move(parentGlobal.x() + (pr.width() - s.width()) / 2,
         parentGlobal.y() + (pr.height() - s.height()) / 2);
}

void LoadingWidget::setImage(const QPixmap& px, int imageHeight)
{
    _iconImage = px;
    _imageHeight = imageHeight;
    updateImage();
}

void LoadingWidget::updateImage()
{
    if (_iconImage.isNull()) {
        _labelImage->clear();
        return;
    }
    const int maxW = qMax(120, _frame->width() - 44);
    const int w = qMin(_imageHeight, maxW);

    const double dpr = devicePixelRatio();
    QPixmap scaled = _iconImage.scaled(w * dpr, 10000, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    scaled.setDevicePixelRatio(dpr);
    _labelImage->setPixmap(scaled);
    _labelImage->setFixedHeight(qMax(30, int(scaled.height() / dpr)));
}

void LoadingWidget::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    updateImage();
}

void LoadingWidget::showEvent(QShowEvent* e)
{
    QWidget::showEvent(e);
    centerInParent();
}
