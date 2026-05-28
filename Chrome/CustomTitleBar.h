#pragma once

#include <QWidget>
#include <QPoint>

class QMdiSubWindow;
class QLabel;
class QPushButton;
class QMenuBar;

class CustomTitleBar : public QWidget {
    Q_OBJECT
public:
    explicit CustomTitleBar(QMdiSubWindow* subWindow, QMenuBar* menuBar, QWidget* parent = nullptr);
    void updateState(bool isMinimized);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void updateMaximizeIcon();

    QMdiSubWindow* _subWindow;
    QLabel* _titleLabel;
    QMenuBar* _menuBar;
    QPushButton* _minButton;
    QPushButton* _maxButton;
    QPushButton* _closeButton;
    QPoint _dragPosition;
    bool _isDragging = false;
    bool _isMinimized = false;
};
