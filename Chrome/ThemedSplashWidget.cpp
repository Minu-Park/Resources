#include "ThemedSplashWidget.h"

ThemedSplashWidget::ThemedSplashWidget(QWidget* parent, const QString& title, const QString& version)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog | Qt::WindowStaysOnTopHint | Qt::NoDropShadowWindowHint);
    setWindowModality(Qt::ApplicationModal);

    auto* layoutOuter = new QVBoxLayout(this);
    layoutOuter->setContentsMargins(18, 18, 18, 18);
    layoutOuter->setSpacing(0);

    _frame = new QFrame(this);
    _frame->setObjectName(QStringLiteral("SplashFrame"));
    _frame->setFixedSize(360, 180);

    auto* shadow = new QGraphicsDropShadowEffect(_frame);
    shadow->setBlurRadius(30);
    shadow->setOffset(0, 10);
    shadow->setColor(QColor(0, 0, 0, 55));
    _frame->setGraphicsEffect(shadow);
    layoutOuter->addWidget(_frame);

    auto* layoutInside = new QVBoxLayout(_frame);
    layoutInside->setSpacing(0);
    layoutInside->setContentsMargins(16, 18, 16, 18);

    _labelImage = new QLabel(_frame);
    _labelImage->setObjectName(QStringLiteral("SplashImage"));
    _labelImage->setAlignment(Qt::AlignRight);
    _labelImage->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    _labelImage->setFixedHeight(0);

    auto* layoutTitle = new QVBoxLayout;
    layoutTitle->setContentsMargins(0, 0, 0, 0);
    layoutTitle->setSpacing(0);

    _labelTitle = new QLabel(_frame);
    _labelTitle->setObjectName(QStringLiteral("SplashTitle"));
    _labelTitle->setAlignment(Qt::AlignRight);

    _labelVersion = new QLabel(_frame);
    _labelVersion->setObjectName(QStringLiteral("SplashVersion"));
    _labelVersion->setAlignment(Qt::AlignRight);

    layoutTitle->addWidget(_labelTitle);
    layoutTitle->addWidget(_labelVersion);

    auto* layoutProgressWrapper = new QVBoxLayout;
    layoutProgressWrapper->setContentsMargins(0, 0, 0, 0);
    layoutProgressWrapper->setSpacing(6);

    auto* layoutTextRow = new QHBoxLayout;
    layoutTextRow->setContentsMargins(0, 0, 0, 0);
    layoutTextRow->setSpacing(0);

    _labelText = new QLabel(_frame);
    _labelText->setObjectName(QStringLiteral("SplashText"));
    _labelText->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    _labelPercent = new QLabel(_frame);
    _labelPercent->setObjectName(QStringLiteral("SplashPercent"));
    _labelPercent->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    layoutTextRow->addWidget(_labelText, 1);
    layoutTextRow->addWidget(_labelPercent, 0);

    _progress = new ThemedProgressBar(_frame);
    _progress->setObjectName(QStringLiteral("SplashProgress"));

    layoutInside->addWidget(_labelImage, 0, Qt::AlignRight);
    layoutInside->addSpacing(15);
    layoutInside->addLayout(layoutTitle, 0);
    layoutInside->addStretch(1);

    layoutProgressWrapper->addLayout(layoutTextRow);
    layoutProgressWrapper->addWidget(_progress);
    layoutInside->addLayout(layoutProgressWrapper);

    setTitle(title);
    setVersion(version);
    setText(QStringLiteral("Loading..."));
    setValue(0);
}

void ThemedSplashWidget::setValue(int v)
{
    _progress->setBusy(false);
    _progress->setValue(v);

    int pct = qBound(0, v, 100);
    _labelPercent->setText(QStringLiteral(" %1%").arg(pct));
    _lastProgress = pct;
}

void ThemedSplashWidget::setText(const QString& text)
{
    _labelText->setText(text);
}

void ThemedSplashWidget::setBusy(bool on)
{
    _progress->setBusy(on);
    _labelPercent->setText(on ? QString() : QStringLiteral("%1%").arg(_lastProgress));
    _labelPercent->setVisible(!on);
    QTimer::singleShot(10, this, [this]{
        update();
        repaint();
    });
}

void ThemedSplashWidget::centerOn()
{
    const QRect r = QGuiApplication::primaryScreen()->availableGeometry();
    move(r.center() - rect().center());
}

void ThemedSplashWidget::setImage(const QPixmap& px, int imageHeight)
{
    _iconImage = px;
    _imageHeight = imageHeight;
    updateImage();
}

void ThemedSplashWidget::setTitle(const QString& title)
{
    _labelTitle->setText(title);
    setWindowTitle(title);
}

void ThemedSplashWidget::setVersion(const QString& version)
{
    _labelVersion->setText(version);
}

void ThemedSplashWidget::updateImage()
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

void ThemedSplashWidget::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    updateImage();
}

void ThemedSplashWidget::showEvent(QShowEvent* e)
{
    QWidget::showEvent(e);
    centerOn();
}

