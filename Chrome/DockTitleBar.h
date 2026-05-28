#pragma once

#include <QWidget>

class QDockWidget;
class QLabel;
class QPushButton;

class DockTitleBar : public QWidget {
    Q_OBJECT
public:
    explicit DockTitleBar(QDockWidget* dockWidget, QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private slots:
    void updateFloatIcon();

private:
    QDockWidget* _dockWidget = nullptr;
    QLabel* _titleLabel = nullptr;
    QPushButton* _floatButton = nullptr;
    QPushButton* _closeButton = nullptr;
};
