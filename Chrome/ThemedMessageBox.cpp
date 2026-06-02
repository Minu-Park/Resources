#include "ThemedMessageBox.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

ThemedMessageBox::ThemedMessageBox(Icon icon, const QString& title, const QString& text, QWidget* parent)
    : ThemedDialog(title, parent)
{
    auto* body = new QHBoxLayout();
    body->setSpacing(12);

    auto* iconLabel = new QLabel(this);
    iconLabel->setObjectName(QStringLiteral("ThemedMessageBoxIcon"));
    QString iconPath;
    switch (icon) {
    case Warning:     iconPath = QStringLiteral(":/Resources/Icons/icons8-warning-96.png");     break;
    case Critical:    iconPath = QStringLiteral(":/Resources/Icons/icons8-error-96.png");       break;
    case Information: iconPath = QStringLiteral(":/Resources/Icons/icons8-information-96.png"); break;
    }
    QPixmap pix(iconPath);
    if (!pix.isNull()) {
        const qreal dpr = devicePixelRatio();
        QPixmap scaled = pix.scaled(QSize(32, 32) * dpr, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        scaled.setDevicePixelRatio(dpr);
        iconLabel->setPixmap(scaled);
    }
    iconLabel->setFixedSize(32, 32);
    body->addWidget(iconLabel, 0, Qt::AlignTop);

    auto* messageLabel = new QLabel(text, this);
    messageLabel->setObjectName(QStringLiteral("ThemedMessageBoxText"));
    messageLabel->setWordWrap(true);
    messageLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    messageLabel->setMinimumWidth(280);
    messageLabel->setMaximumWidth(520);
    body->addWidget(messageLabel, 1);

    contentLayout()->addLayout(body);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    auto* okButton = new QPushButton(tr("OK"), this);
    okButton->setObjectName(QStringLiteral("ThemedMessageBoxOkButton"));
    okButton->setDefault(true);
    okButton->setMinimumWidth(80);
    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(okButton);
    contentLayout()->addLayout(buttonLayout);

    setMinimumWidth(360);
    adjustSize();
}

void ThemedMessageBox::critical(QWidget* parent, const QString& title, const QString& text)
{
    ThemedMessageBox box(Critical, title, text, parent);
    box.exec();
}

void ThemedMessageBox::warning(QWidget* parent, const QString& title, const QString& text)
{
    ThemedMessageBox box(Warning, title, text, parent);
    box.exec();
}

void ThemedMessageBox::information(QWidget* parent, const QString& title, const QString& text)
{
    ThemedMessageBox box(Information, title, text, parent);
    box.exec();
}
