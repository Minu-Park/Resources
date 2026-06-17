#pragma once

#include <QMainWindow>
#include <QPoint>
#include <QPointer>

class QMenuBar;
class ThemedMainTitleBar;

class ThemedMainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit ThemedMainWindow(QWidget* parent = nullptr);
    ~ThemedMainWindow() override;

    /**
     * @brief Accessor for the custom title bar widget.
     */
    ThemedMainTitleBar* titleBar() const { return _titleBar; }
    QMenuBar* themedMenuBar() const { return _menuBar; }

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
    QMenuBar* _menuBar = nullptr;
    ThemedMainTitleBar* _titleBar = nullptr;

    QPoint _dragStartPos;
    int _resizeMode = ResizeNone;
    bool _windowChromeMaximized = false;
    bool _forceRoundedChrome = false;
    QPointer<QWidget> _cursorOverriddenWidget = nullptr;
};
