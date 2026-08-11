#pragma once

#include <QWidget>
#include <QPoint>
#include <QRect>
#include <QList>

class QMdiSubWindow;
class ThemedMdiTitleBar;
class QEvent;
class QMenuBar;

class ThemedMdiContainer : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int frameCornerRadius READ frameCornerRadius WRITE setFrameCornerRadius)
public:
    ThemedMdiContainer(QMdiSubWindow* subWin, QWidget* content, QMenuBar* menuBar, QWidget* parent = nullptr);
    [[nodiscard]] QWidget* content() const noexcept;
    [[nodiscard]] int frameCornerRadius() const noexcept;
    void setFrameCornerRadius(int radius);
    QWidget* takeContent();
    void restoreContent(QWidget* content);
    QSize minimumSizeHint() const override;

signals:
    void minimizeRequested(QMdiSubWindow* subWin);
    void contentAttachmentChanged(bool attached);
    void frameCornerRadiusChanged(int radius);

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
    void updateContentMask();
    void beginResize(int resizeMode, const QPoint& globalPosition);
    void resizeFromGlobalPosition(const QPoint& globalPosition);
    void finishResize();
    void updateResizeHandleGeometry();
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
    ThemedMdiTitleBar* _titleBar;
    QPoint _dragStartPos;
    QRect _dragStartGeometry;
    int _resizeMode = ResizeNone;
    int _frameCornerRadius = 12;
    QSize _contentMaskSize;
    int _contentMaskRadius = -1;
    bool _contentMaskApplied = false;
    QList<QWidget*> _resizeHandles;
};
