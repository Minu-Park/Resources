#pragma once

#include <QIcon>
#include <QWidget>

class QDockWidget;
class QLabel;
class QPushButton;

class ThemedDockTitleBar : public QWidget {
    Q_OBJECT
public:
    explicit ThemedDockTitleBar(QDockWidget* dockWidget, QWidget* parent = nullptr);
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
    QIcon createSmoothIcon(const QString& path, const QSize& logicalSize) const;

    QDockWidget* _dockWidget = nullptr;
    QLabel* _titleLabel = nullptr;
    QPushButton* _floatButton = nullptr;
    QPushButton* _closeButton = nullptr;
};

