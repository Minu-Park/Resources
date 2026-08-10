#pragma once

#include <QMdiArea>
#include <QMetaObject>
#include <QPointer>

#include <memory>
#include <vector>

class QEvent;
class QMdiSubWindow;
class ThemedMdiContainer;
class ThemedMdiShadowWidget;

class ThemedMdiArea : public QMdiArea {
public:
    enum class ShadowMode {
        Disabled,
        ActiveWindowOnly,
        AllVisibleWindows
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
    struct ShadowEntry;
    friend class ThemedMdiShadowWidget;

    void installShadowOnViewport(QWidget* viewport);
    void clearShadowEntries();
    void refreshShadowEntries();
    [[nodiscard]] ShadowEntry* shadowEntryFor(QMdiSubWindow* target) const;
    void connectEntryContainer(ShadowEntry& entry);
    void synchronizeShadow(ShadowEntry& entry);
    void synchronizeAllShadows(bool ensureStacking);
    void scheduleShadowSynchronization();
    void reportShadowDiagnostics();
    void hideShadows();
    void hideShadowFor(QMdiSubWindow* target);
    [[nodiscard]] bool canShowShadow(const ShadowEntry& entry) const;

    QPointer<QWidget> _trackedViewport;
    std::vector<std::unique_ptr<ShadowEntry>> _shadowEntries;
    std::unique_ptr<ShadowDiagnostics> _shadowDiagnostics;
    ShadowMode _shadowMode = ShadowMode::Disabled;
    bool _shadowSyncQueued = false;
};
