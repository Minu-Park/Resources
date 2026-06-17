#ifndef ThemedSplashWidget_H
#define ThemedSplashWidget_H

#include "ThemedProgressBar.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QLabel>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QScreen>
#include <QTimer>
#include <QPixmap>
#include <QResizeEvent>
#include <QShowEvent>

class ThemedSplashWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ThemedSplashWidget(QWidget* parent = nullptr, const QString& title = "", const QString& version = "");
    
    void setValue(int v);
    void setText(const QString& text);
    void setBusy(bool on);
    void centerOn();
    void setImage(const QPixmap& px, int imageHeight = 200);
    void setTitle(const QString& title);
    void setVersion(const QString& version);

protected:
    void resizeEvent(QResizeEvent* e) override;
    void showEvent(QShowEvent* e) override;

private:
    void updateImage();

    QFrame *_frame = nullptr;
    QLabel* _labelImage = nullptr;
    QLabel* _labelTitle = nullptr;
    QLabel* _labelVersion = nullptr;
    QLabel* _labelText = nullptr;
    QLabel* _labelPercent = nullptr;
    ThemedProgressBar *_progress = nullptr;
    int _lastProgress = 0;

    QPixmap _iconImage;
    int _imageHeight = 200;
};

#endif // ThemedSplashWidget_H

