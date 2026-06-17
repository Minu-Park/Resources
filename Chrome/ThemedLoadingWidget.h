#ifndef ThemedLoadingWidget_H
#define ThemedLoadingWidget_H

#include "ThemedProgressBar.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QLabel>
#include <QGraphicsDropShadowEffect>
#include <QTimer>
#include <QPixmap>
#include <QResizeEvent>
#include <QShowEvent>

class ThemedLoadingWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ThemedLoadingWidget(QWidget* parent = nullptr);

    void setValue(int v);
    void setText(const QString& text);
    void setBusy(bool on);
    void centerInParent();
    void setImage(const QPixmap& px, int imageHeight = 200);

protected:
    void resizeEvent(QResizeEvent* e) override;
    void showEvent(QShowEvent* e) override;

private:
    void updateImage();

    QFrame *_frame = nullptr;
    QLabel* _labelImage = nullptr;
    QLabel* _labelText = nullptr;
    QLabel* _labelPercent = nullptr;
    ThemedProgressBar *_progress = nullptr;
    int _lastProgress = 0;

    QPixmap _iconImage;
    int _imageHeight = 200;
};

#endif // ThemedLoadingWidget_H

