#pragma once

#include <QWidget>
#include <QPoint>

class QMainWindow;
class QLabel;
class QPushButton;
class QMenuBar;

class MainTitleBar : public QWidget {
    Q_OBJECT
public:
    explicit MainTitleBar(QMainWindow* mainWindow, QMenuBar* menuBar, QWidget* parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void updateMaximizeIcon();

    QMainWindow* _mainWindow;
    QLabel* _logoLabel;
    QLabel* _titleLabel;
    QPushButton* _minButton;
    QPushButton* _maxButton;
    QPushButton* _closeButton;
    QPoint _dragPosition;
    bool _isDragging = false;
};
