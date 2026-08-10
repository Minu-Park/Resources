#pragma once

#include <QMdiArea>
#include <QMetaObject>
#include <QPointer>

#include <memory>

class QEvent;
class QMdiSubWindow;
class ThemedMdiContainer;
class ThemedMdiShadowWidget;

class ThemedMdiArea : public QMdiArea {
public:
    enum class ShadowMode {
        Disabled,
        ActiveWindowOnly
    };

    explicit ThemedMdiArea(QWidget* parent = nullptr);
    ~ThemedMdiArea() override;

    void setShadowMode(ShadowMode mode);
    [[nodiscard]] ShadowMode shadowMode() const noexcept;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void changeEvent(QEvent* event) override;
    void setupViewport(QWidget* viewport) override;

private:
    class ShadowDiagnostics;
    friend class ThemedMdiShadowWidget;

    void installShadowOnViewport(QWidget* viewport);
    void setShadowTarget(QMdiSubWindow* target);
    void connectTargetContainer();
    void synchronizeShadow(bool ensureStacking);
    void scheduleShadowSynchronization();
    void reportShadowDiagnostics();
    void hideShadow();
    [[nodiscard]] bool canShowShadow() const;

    QPointer<ThemedMdiShadowWidget> _shadow;
    QPointer<QWidget> _trackedViewport;
    QPointer<QMdiSubWindow> _shadowTarget;
    QPointer<ThemedMdiContainer> _connectedContainer;
    QMetaObject::Connection _contentAttachmentConnection;
    std::unique_ptr<ShadowDiagnostics> _shadowDiagnostics;
    ShadowMode _shadowMode = ShadowMode::Disabled;
    bool _shadowSyncQueued = false;
};
