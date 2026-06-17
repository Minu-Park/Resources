#pragma once

#include <QWidget>
#include <QPoint>
#include <QPointer>

class QVBoxLayout;
class ThemedMainTitleBar;

class ThemedWindow : public QWidget {
    Q_OBJECT
public:
    explicit ThemedWindow(QWidget* parent = nullptr);
    ~ThemedWindow() override;

    /**
     * @brief Sets the central content widget of the frameless themed window.
     *        If it is a QMainWindow, it automatically extracts its QMenuBar to create the custom title bar.
     */
    void setCentralWidget(QWidget* widget);

    /**
     * @brief Accessor for the custom title bar widget.
     */
    ThemedMainTitleBar* titleBar() const { return _titleBar; }

    void prepareForMaximizeTransition();
    void prepareForRestoreTransition();

protected:
    void changeEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

    void updateWindowMask();
    void updateWindowChromeState();
    void applyWindowChromeState(bool maximized);

    enum ResizeDirection {
        ResizeNone = 0,
        ResizeLeft = 1,
        ResizeRight = 2,
        ResizeTop = 4,
        ResizeBottom = 8
    };

    int determineResizeMode(const QPoint& pos);
    void updateCursorShape(const QPoint& pos);

private:
    QVBoxLayout* _layout = nullptr;
    ThemedMainTitleBar* _titleBar = nullptr;
    QWidget* _centralWidget = nullptr;

    QPoint _dragStartPos;
    int _resizeMode = ResizeNone;
    bool _windowChromeMaximized = false;
    bool _forceRoundedChrome = false;
    QPointer<QWidget> _cursorOverriddenWidget = nullptr;
};
