#pragma once

#include <QFrame>
#include <QPointer>
#include <QString>

class QHBoxLayout;
class QStackedWidget;
class QTabBar;
class QWidget;

/** @brief One card frame around the page stack and optional tab strip. */
class DeviceScopeCard final : public QFrame {
    Q_OBJECT

public:
    /** @brief Creates the card; Resources owns its role presentation. */
    explicit DeviceScopeCard(QWidget* parent = nullptr);
    ~DeviceScopeCard() override;

    /** @brief Returns the in-card tab strip. */
    QTabBar* tabBar() const { return _tabs; }
    /** @brief Returns the page stack shown below the separator. */
    QStackedWidget* pages() const { return _pages; }
    /** @brief Returns the card-owned bottom search strip, or nullptr. */
    QWidget* searchStrip() const { return _strip; }

    /** @brief Returns the number of scope pages. */
    int count() const;
    /** @brief Returns the page at `index`, or nullptr. */
    QWidget* widget(int index) const;
    /** @brief Appends one page and its tab label; returns the new index. */
    int addTab(QWidget* page, const QString& label);
    /** @brief Removes one page and its tab without deleting the page. */
    void removeTab(int index);
    /** @brief Returns the current tab index, or -1. */
    int currentIndex() const;
    /** @brief Selects one tab and shows its page. */
    void setCurrentIndex(int index);
    /** @brief Sets the tooltip for one tab. */
    void setTabToolTip(int index, const QString& tip);
    /** @brief Shows or hides the tab strip and its separator together. */
    void setTabStripVisible(bool visible);

    /** @brief Hosts the active page's real search surface in one card-owned strip. */
    void setSearchStripShown(bool shown);
    /** @brief Reports whether the card owns a bottom search strip. */
    bool isSearchStripShown() const { return _stripShown; }

signals:
    /** @brief Reports a selected tab change; the page stack already follows it. */
    void currentChanged(int index);

private:
    void dockActiveSearch();
    void undockActiveSearch();
    void onDockedOwnerDestroyed();
    void onPageRemoved(int index);
    QWidget* searchSurfaceFor(int index) const;

    QTabBar* _tabs = nullptr;
    QFrame* _tabSeparator = nullptr;
    QStackedWidget* _pages = nullptr;
    QFrame* _strip = nullptr;
    QFrame* _stripSeparator = nullptr;
    QHBoxLayout* _stripRow = nullptr;
    QPointer<QWidget> _dockedSurface;
    QPointer<QWidget> _dockedOwner;
    QMetaObject::Connection _ownerDestroyed;
    bool _stripShown = false;
};
