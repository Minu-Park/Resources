#pragma once

#include <QFrame>
#include <QString>

class QStackedWidget;
class QTabBar;

/** @brief One card frame around a tab strip, a full-width separator and page stack. */
class DeviceScopeCard final : public QFrame {
    Q_OBJECT

public:
    /** @brief Creates the card; Resources owns its role presentation. */
    explicit DeviceScopeCard(QWidget* parent = nullptr);

    /** @brief Returns the in-card tab strip. */
    QTabBar* tabBar() const { return _tabs; }
    /** @brief Returns the page stack shown below the separator. */
    QStackedWidget* pages() const { return _pages; }

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

signals:
    /** @brief Reports a selected tab change; the page stack already follows it. */
    void currentChanged(int index);

private:
    QTabBar* _tabs = nullptr;
    QStackedWidget* _pages = nullptr;
};
