#pragma once

#include <QWidget>
#include <QPoint>
#include <QRect>

class QMdiSubWindow;
class CustomTitleBar;
class QEvent;
class QMenuBar;

class MdiSubWindowContainer : public QWidget {
    Q_OBJECT
public:
    MdiSubWindowContainer(QMdiSubWindow* subWin, QWidget* content, QMenuBar* menuBar, QWidget* parent = nullptr);

signals:
    void minimizeRequested(QMdiSubWindow* subWin);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void leaveEvent(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void handleWindowStateChange();
    enum ResizeDirection {
        ResizeNone = 0,
        ResizeLeft = 1,
        ResizeRight = 2,
        ResizeTop = 4,
        ResizeBottom = 8
    };

    int determineResizeMode(const QPoint& pos);
    void updateCursorShape(const QPoint& pos);

    QMdiSubWindow* _subWin;
    QWidget* _content;
    CustomTitleBar* _titleBar;
    QPoint _dragStartPos;
    QRect _dragStartGeometry;
    int _resizeMode = ResizeNone;
};
