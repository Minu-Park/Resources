#pragma once

#include <QWidget>

class QDockWidget;
class QLabel;
class QPushButton;

class DockTitleBar : public QWidget {
    Q_OBJECT
public:
    explicit DockTitleBar(QDockWidget* dockWidget, QWidget* parent = nullptr);
    static void setupDockWidget(QDockWidget* dockWidget, QWidget* contentWidget);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void applyFloatingChrome(bool floating);
    void refreshFloatingChromeStyle();
    void updateFloatingMask();

    QDockWidget* _dockWidget = nullptr;
    QLabel* _titleLabel = nullptr;
    QPushButton* _floatButton = nullptr;
    QPushButton* _closeButton = nullptr;
};
