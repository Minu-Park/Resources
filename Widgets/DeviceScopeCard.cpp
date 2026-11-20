#include "Widgets/DeviceScopeCard.h"

#include "Widgets/SearchableTreeController.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTabBar>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {
constexpr char separatorName[] = "DeviceScopeCardSeparator";
constexpr char stripName[] = "DeviceScopeCardSearchStrip";
constexpr char stripRuleName[] = "DeviceScopeCardSearchSeparator";
constexpr char stripLayoutName[] = "DeviceScopeCardSearchLayout";
}

DeviceScopeCard::DeviceScopeCard(QWidget* parent)
    : QFrame(parent)
{
    setProperty("tabRole", QStringLiteral("DeviceScopeCard"));
    setFrameShape(QFrame::StyledPanel);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    _tabs = new QTabBar(this);
    _tabs->setObjectName(QStringLiteral("DeviceScopeCardTabs"));
    _tabs->setExpanding(false);
    _tabs->setDrawBase(false);
    _tabs->setUsesScrollButtons(true);
    layout->addWidget(_tabs);

    _tabSeparator = new QFrame(this);
    _tabSeparator->setObjectName(QString::fromLatin1(separatorName));
    _tabSeparator->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(_tabSeparator);

    _pages = new QStackedWidget(this);
    _pages->setObjectName(QStringLiteral("DeviceScopeCardPages"));
    layout->addWidget(_pages, 1);

    connect(_tabs, &QTabBar::currentChanged, _pages, &QStackedWidget::setCurrentIndex);
    connect(_tabs, &QTabBar::currentChanged, this, [this] { dockActiveSearch(); });
    connect(_tabs, &QTabBar::currentChanged, this, &DeviceScopeCard::currentChanged);
    connect(_pages, &QStackedWidget::widgetRemoved, this, &DeviceScopeCard::onPageRemoved);
}

DeviceScopeCard::~DeviceScopeCard()
{
    // Stop observing child teardown while this card is still whole. Otherwise a page
    // destroyed with the stack would re-enter onPageRemoved with an already dying tab bar.
    disconnect(_pages, &QStackedWidget::widgetRemoved, this, &DeviceScopeCard::onPageRemoved);
    // Release the borrowed surface and its destroyed watcher while every widget
    // is still whole; running it from a child's destructor re-polishes dying style state.
    undockActiveSearch();
}

int DeviceScopeCard::count() const
{
    return _tabs->count();
}

QWidget* DeviceScopeCard::widget(int index) const
{
    return _pages->widget(index);
}

int DeviceScopeCard::addTab(QWidget* page, const QString& label)
{
    if (!page) return -1;
    const int pageIndex = _pages->addWidget(page);
    const int tabIndex = _tabs->addTab(label);
    Q_ASSERT(pageIndex == tabIndex);
    if (_stripShown && currentIndex() == tabIndex) dockActiveSearch();
    return tabIndex;
}

void DeviceScopeCard::removeTab(int index)
{
    if (index < 0 || index >= _tabs->count()) return;
    QWidget* page = _pages->widget(index);
    if (!page) return;
    // The docked owner is still alive here: return its surface before the page leaves
    // the stack. External deletion is handled by the owner's destroyed watcher instead.
    if (page == _dockedOwner.data()) undockActiveSearch();
    // One removal path: QStackedWidget::widgetRemoved aligns the tab bar, search dock
    // and currentChanged signal for both explicit and external page removal.
    _pages->removeWidget(page);
}

int DeviceScopeCard::currentIndex() const
{
    return _tabs->currentIndex();
}

void DeviceScopeCard::setCurrentIndex(int index)
{
    _tabs->setCurrentIndex(index);
}

void DeviceScopeCard::setTabToolTip(int index, const QString& tip)
{
    _tabs->setTabToolTip(index, tip);
}

void DeviceScopeCard::setTabStripVisible(bool visible)
{
    _tabs->setVisible(visible);
    _tabSeparator->setVisible(visible);
}

QWidget* DeviceScopeCard::searchSurfaceFor(int index) const
{
    auto* tree = qobject_cast<QTreeWidget*>(widget(index));
    if (!tree) return nullptr;
    auto* controller = SearchableTreeController::attached(tree);
    return controller ? controller->searchSurface() : nullptr;
}

void DeviceScopeCard::undockActiveSearch()
{
    if (!_dockedSurface && !_dockedOwner) return;
    QObject::disconnect(_ownerDestroyed);
    _ownerDestroyed = QMetaObject::Connection();
    if (_dockedSurface) {
        _stripRow->removeWidget(_dockedSurface);
        if (auto* tree = _dockedOwner ? qobject_cast<QTreeWidget*>(_dockedOwner.data()) : nullptr) {
            if (auto* controller = SearchableTreeController::attached(tree))
                controller->setSearchPlacement(SearchableTreeController::SearchPlacement::Overlay);
        }
        _dockedSurface->setParent(_dockedOwner.data());
        _dockedSurface->hide();
    }
    _dockedSurface = nullptr;
    _dockedOwner = nullptr;
}

void DeviceScopeCard::onDockedOwnerDestroyed()
{
    // The owner is mid-destruction: its controller and tree are already gone, but the
    // real surface was reparented into the strip and would otherwise linger as an
    // orphaned control whose shortcut callbacks still capture the dead controller.
    // Do not dereference the dying owner or use a cached index; QStackedWidget emits
    // widgetRemoved next, and that is the single tab/page alignment and signal path.
    if (_dockedSurface) {
        _stripRow->removeWidget(_dockedSurface);
        delete _dockedSurface;
    }
    _dockedSurface = nullptr;
    _dockedOwner = nullptr;
    _ownerDestroyed = QMetaObject::Connection();
}

void DeviceScopeCard::onPageRemoved(int index)
{
    if (index < 0 || index >= _tabs->count()) return;
    const bool removedCurrent = index == _tabs->currentIndex();

    // QStackedWidget has already dropped the page (explicitly or through an external
    // delete), so drop the matching tab with its signals blocked, realign the stack and
    // only then refresh the dock and report the selected page.
    {
        const QSignalBlocker blocker(_tabs);
        _tabs->removeTab(index);
    }
    const int current = _tabs->currentIndex();
    _pages->setCurrentIndex(current);
    dockActiveSearch();
    if (removedCurrent) emit currentChanged(current);
}

void DeviceScopeCard::dockActiveSearch()
{
    if (!_stripShown || !_strip) return;
    QWidget* owner = widget(currentIndex());
    QWidget* surface = searchSurfaceFor(currentIndex());
    if (owner && surface && owner == _dockedOwner && surface == _dockedSurface) return;
    undockActiveSearch();
    if (!owner || !surface) return;
    if (auto* tree = qobject_cast<QTreeWidget*>(owner)) {
        if (auto* controller = SearchableTreeController::attached(tree))
            controller->setSearchPlacement(SearchableTreeController::SearchPlacement::Strip);
    }
    surface->setParent(_strip);
    _stripRow->addWidget(surface);
    surface->show();
    _dockedOwner = owner;
    _dockedSurface = surface;
    _ownerDestroyed = connect(owner, &QObject::destroyed, this, [this] { onDockedOwnerDestroyed(); });
}

void DeviceScopeCard::setSearchStripShown(bool shown)
{
    if (shown == _stripShown) return;
    _stripShown = shown;
    if (!shown) {
        undockActiveSearch();
        for (int index = 0; index < count(); ++index) {
            auto* tree = qobject_cast<QTreeWidget*>(widget(index));
            if (!tree) continue;
            if (auto* controller = SearchableTreeController::attached(tree))
                controller->setSearchPlacement(SearchableTreeController::SearchPlacement::Overlay);
        }
        if (_strip) _strip->hide();
        return;
    }

    if (!_strip) {
        _strip = new QFrame(this);
        _strip->setObjectName(QString::fromLatin1(stripName));
        _strip->setFrameShape(QFrame::NoFrame);
        auto* outer = new QVBoxLayout(_strip);
        outer->setContentsMargins(0, 0, 0, 0);
        outer->setSpacing(0);
        auto* rule = new QFrame(_strip);
        rule->setObjectName(QString::fromLatin1(stripRuleName));
        rule->setFrameShape(QFrame::HLine);
        rule->setFixedHeight(1);
        outer->addWidget(rule);
        _stripRow = new QHBoxLayout;
        _stripRow->setObjectName(QString::fromLatin1(stripLayoutName));
        _stripRow->setProperty("layoutRole", QStringLiteral("search"));
        outer->addLayout(_stripRow, 1);
        layout()->addWidget(_strip);
    }
    _strip->show();
    dockActiveSearch();
}
