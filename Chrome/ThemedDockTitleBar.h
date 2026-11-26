#pragma once

#include <QIcon>
#include <QFrame>

class QDockWidget;
class QLabel;
class QPushButton;

class ThemedDockTitleBar : public QFrame {
    Q_OBJECT
public:
    /** @brief Title bar height: compact for docks inside a window, window height for a dock that sits beside MDI windows. */
    enum class Height { Compact, Window };
    /** @brief Fixed height of a compact dock title bar. */
    static constexpr int kCompactHeight = 28;

    explicit ThemedDockTitleBar(QDockWidget* dockWidget, QWidget* parent = nullptr,
                                Height height = Height::Compact);
    static void setupDockWidget(QDockWidget* dockWidget, QWidget* contentWidget,
                                Height height = Height::Compact);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    bool eventFilter(QObject* watched, QEvent* event) override;
    QRect bottomRule() const;
    void applyFloatingChrome(bool floating);
    void refreshFloatingChromeStyle();
    void updateFloatingMask();
    QIcon createSmoothIcon(const QString& path, const QSize& logicalSize) const;

    QDockWidget* _dockWidget = nullptr;
    int _height = kCompactHeight;
    QLabel* _titleLabel = nullptr;
    QPushButton* _floatButton = nullptr;
    QPushButton* _closeButton = nullptr;
};

