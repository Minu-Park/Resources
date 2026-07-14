#pragma once

#include <QWidget>
#include <QPoint>

class QMainWindow;
class QHBoxLayout;
class QLabel;
class QPushButton;
class QMenuBar;

class ThemedMainTitleBar : public QWidget {
    Q_OBJECT
public:
    explicit ThemedMainTitleBar(QMainWindow* mainWindow, QMenuBar* menuBar, QWidget* parent = nullptr);

    void setMenuBar(QMenuBar* menuBar);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    bool eventFilter(QObject* watched, QEvent* event) override;
    QIcon createSmoothIcon(const QString& path, const QSize& logicalSize) const;
    void updateMaximizeIcon();

    QMainWindow* _mainWindow;
    QHBoxLayout* _layout;
    QLabel* _logoLabel;
    QLabel* _titleLabel;
    QMenuBar* _menuBar = nullptr;
    QPushButton* _minButton;
    QPushButton* _maxButton;
    QPushButton* _closeButton;
    QPoint _dragPosition;
    bool _isDragging = false;
};
