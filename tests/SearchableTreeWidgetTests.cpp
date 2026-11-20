#include "Widgets/SearchableTreeWidget.h"
#include "Widgets/SearchableTreeController.h"
#include "Widgets/DeviceScopeCard.h"
#include <QStyledItemDelegate>
#include <QSignalSpy>

#include <QApplication>
#include <QDir>
#include <QFrame>
#include <QHeaderView>
#include <QLineEdit>
#include <QPointer>
#include <QScrollBar>
#include <QSettings>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>

namespace {
void configure(SearchableTreeWidget& tree) {
  tree.setColumnCount(2);
  tree.setHeaderLabels({QStringLiteral("Name"), QStringLiteral("Value")});
  tree.resize(400, 320);
}

QTreeWidgetItem* item(SearchableTreeWidget& tree, QTreeWidgetItem* parent,
                      const QString& label, const QString& key, const QString& terms = {}) {
  auto* row = new QTreeWidgetItem(parent, {label, QStringLiteral("42")});
  // Consumer metadata must not collide with the browser roles.
  row->setData(0, Qt::UserRole, QStringLiteral("consumer metadata"));
  tree.registerItem(row, key, terms);
  return row;
}
}

class SearchableTreeTests final : public QObject {
  Q_OBJECT
private slots:
  void pendingLayoutPreservesScrollToBottom() {
    SearchableTreeWidget tree;
    configure(tree);
    tree.show();
    QTest::qWait(20);
    for (int i = 0; i < 60; ++i)
      new QTreeWidgetItem(&tree, {QString("Feature %1").arg(i), "42"});
    QCoreApplication::processEvents();
    tree.scrollToBottom();
    QTest::qWait(20);
    const auto last = tree.visualItemRect(tree.topLevelItem(59));
    QVERIFY(tree.viewport()->rect().contains(last));
    auto* surface = tree.findChild<QFrame*>("DeviceFeatureSearchBubble");
    QVERIFY(last.bottom() < tree.viewport()->mapFrom(&tree, surface->pos()).y());
  }

  void attachedTreeKeepsIntrinsicVisibilityAndDelegate() {
    QTreeWidget tree;
    tree.setColumnCount(2);
    class Delegate : public QStyledItemDelegate {
    public:
      explicit Delegate(QObject* parent) : QStyledItemDelegate(parent) {}
      QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override { return QSize(150, 37); }
    };
    auto* original = new Delegate(&tree);
    tree.setItemDelegateForColumn(0, original);
    auto* visible = new QTreeWidgetItem(&tree, {"Visible", "42"});
    auto* hidden = new QTreeWidgetItem(&tree, {"Unavailable", ""});
    hidden->setHidden(true);
    auto* controller = SearchableTreeController::attach(&tree, {});
    controller->refreshFilter();
    auto* proxy = tree.itemDelegateForColumn(0);
    QVERIFY(proxy != original);
    QCOMPARE(proxy->sizeHint({}, tree.model()->index(0, 0)).height(), 37);
    QTreeWidget plain;
    new QTreeWidgetItem(&plain, {"Default delegate"});
    SearchableTreeController::attach(&plain, {});
    plain.setItemDelegate(new Delegate(&plain));
    QCOMPARE(plain.itemDelegateForColumn(0)->sizeHint({}, plain.model()->index(0, 0)).height(), 37);
    QSignalSpy commits(proxy, &QAbstractItemDelegate::commitData);
    original->commitData(nullptr);
    QCOMPARE(commits.size(), 1);
    auto* input = tree.findChild<QLineEdit*>("DeviceFeatureSearchInput");
    input->setText("Unavailable"); controller->refreshFilter();
    QVERIFY(hidden->isHidden()); QVERIFY(visible->isHidden());
    input->clear(); controller->refreshFilter();
    QVERIFY(hidden->isHidden()); QVERIFY(!visible->isHidden());
    visible->setHidden(true); controller->refreshFilter();
    input->setText("Visible"); input->clear(); controller->refreshFilter();
    QVERIFY(visible->isHidden());
  }

  void attachedTreeAnchorIgnoresScrollbarsAndRebuild() {
    QTreeWidget tree;
    tree.resize(400, 320);
    for (int i = 0; i < 60; ++i) new QTreeWidgetItem(&tree, {QString("Feature %1").arg(i)});
    auto* controller = SearchableTreeController::attach(&tree, {});
    tree.show(); QTest::qWait(20); controller->refreshFilter();
    auto* surface = tree.findChild<QFrame*>("DeviceFeatureSearchBubble");
    const QRect anchor = surface->geometry();
    auto* input = tree.findChild<QLineEdit*>("DeviceFeatureSearchInput");
    input->setText("Feature 59"); QTest::qWait(20);
    QCOMPARE(surface->geometry(), anchor);
    input->clear(); QTest::qWait(20);
    tree.scrollToBottom();
    controller->updateGeometry(); tree.scrollToBottom();
    const int previous = tree.verticalScrollBar()->value();
    controller->refreshFilter(); QTest::qWait(20);
    QCOMPARE(tree.verticalScrollBar()->value(), previous);
    QCOMPARE(surface->geometry(), anchor);
    input->setText("Feature 59"); QTest::qWait(20);
    input->clear(); QTest::qWait(20);
    QCOMPARE(tree.verticalScrollBar()->value(), previous);
    QVERIFY(tree.visualItemRect(tree.topLevelItem(59)).bottom()
        < tree.viewport()->mapFrom(&tree, QPoint(0, surface->y())).y());
    tree.clear(); new QTreeWidgetItem(&tree, {"Replacement"}); QTest::qWait(20);
    QCOMPARE(surface->geometry(), anchor);
  }


  void plainQtSearchKeepsEditorsAndRestoresState() {
    SearchableTreeWidget tree;
    configure(tree);
    auto* root = new QTreeWidgetItem(&tree, {QStringLiteral("Controls")});
    auto* group = new QTreeWidgetItem(root, {QStringLiteral("Acquisition")});
    auto* exposure = item(tree, group, QStringLiteral("Exposure"), QStringLiteral("sensor/exposure"),
                           QStringLiteral("ExposureTime"));
    auto* gain = item(tree, root, QStringLiteral("Gain"), QStringLiteral("sensor/gain"));
    auto* editor = new QLineEdit(QStringLiteral("selected value"));
    tree.setItemWidget(exposure, 1, editor);
    root->setExpanded(true);
    group->setExpanded(false);
    tree.setCurrentItem(gain);
    tree.show();
    auto* input = tree.findChild<QLineEdit*>(QStringLiteral("DeviceFeatureSearchInput"));
    input->setText(QStringLiteral("acquisition EXPOSURETIME"));
    QVERIFY(!exposure->isHidden());
    QVERIFY(gain->isHidden());
    QVERIFY(group->isExpanded());
    QCOMPARE(tree.itemWidget(exposure, 1), editor);
    QCOMPARE(editor->text(), QStringLiteral("selected value"));
    QCOMPARE(exposure->data(0, Qt::UserRole).toString(), QStringLiteral("consumer metadata"));
    input->clear();
    QVERIFY(!gain->isHidden());
    QVERIFY(!group->isExpanded());
    QCOMPARE(tree.currentItem(), gain);
  }

  void unregisteredLeavesAndRegisteredParentsAreSearchable() {
    SearchableTreeWidget tree;
    auto* root = new QTreeWidgetItem(&tree, {QStringLiteral("Root")});
    auto* parent = item(tree, root, QStringLiteral("Parent"), QStringLiteral("parent-id"));
    auto* child = new QTreeWidgetItem(parent, {QStringLiteral("Needle")});
    auto* other = new QTreeWidgetItem(root, {QStringLiteral("Other")});
    tree.registerItem(other, {}, QStringLiteral("alias"));
    auto* input = tree.findChild<QLineEdit*>(QStringLiteral("DeviceFeatureSearchInput"));
    input->setText(QStringLiteral("needle"));
    QVERIFY(!child->isHidden());
    QVERIFY(!parent->isHidden());
    QVERIFY(parent->isExpanded());
    QVERIFY(other->isHidden());
    input->setText(QStringLiteral("alias"));
    QVERIFY(!other->isHidden());
    QVERIFY(parent->isHidden());
    tree.toggleFavorite(other);
    QVERIFY(!tree.isFavorite(other));
    input->clear();
    tree.toggleFavorite(child);
    QVERIFY(!tree.isFavorite(child));
  }

  void favoritesRemainLocalWithoutAStore() {
    SearchableTreeWidget first, second;
    auto* firstRoot = new QTreeWidgetItem(&first, {QStringLiteral("First")});
    auto* secondRoot = new QTreeWidgetItem(&second, {QStringLiteral("Second")});
    auto* a = item(first, firstRoot, QStringLiteral("First name"), QStringLiteral("shared-id"));
    auto* b = item(second, secondRoot, QStringLiteral("Second name"), QStringLiteral("shared-id"));
    first.toggleFavorite(a);
    QVERIFY(first.isFavorite(a));
    QVERIFY(!second.isFavorite(b));
    first.toggleFavorite(b);
    QVERIFY(!second.isFavorite(b));
    auto* favorites = first.findChild<QToolButton*>(QStringLiteral("DeviceFeatureFavoritesOnly"));
    favorites->setChecked(true);
    QVERIFY(!a->isHidden());
    auto* input = first.findChild<QLineEdit*>(QStringLiteral("DeviceFeatureSearchInput"));
    input->setText(QStringLiteral("missing"));
    QVERIFY(a->isHidden());
    input->clear();
    QVERIFY(!a->isHidden());
  }

  void favoriteStoresSynchronizeWithoutLosingMissingItems() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto file = directory.filePath(QStringLiteral("nested/browser.ini"));
    SearchableTreeWidget first(nullptr, file, QStringLiteral("A/Items"));
    SearchableTreeWidget second(nullptr, file, QStringLiteral("A/Items"));
    SearchableTreeWidget otherKey(nullptr, file, QStringLiteral("B/Items"));
    auto* a = item(first, new QTreeWidgetItem(&first, {QStringLiteral("First")}),
                   QStringLiteral("Old label"), QStringLiteral("stable-key"));
    auto* b = item(second, new QTreeWidgetItem(&second, {QStringLiteral("Second")}),
                   QStringLiteral("New label"), QStringLiteral("stable-key"));
    auto* isolated = item(otherKey, new QTreeWidgetItem(&otherKey, {QStringLiteral("Isolated")}),
                          QStringLiteral("Same label"), QStringLiteral("stable-key"));
    first.toggleFavorite(a);
    QVERIFY(second.isFavorite(b));
    QVERIFY(b->data(0, SearchableTreeWidget::FavoriteRole).toBool());
    QVERIFY(!otherKey.isFavorite(isolated));
    QSettings settings(file, QSettings::IniFormat);
    settings.setValue(QStringLiteral("Window/Value"), 42);
    settings.sync();
    second.clear();
    second.refreshFilter();
    SearchableTreeWidget reopened(nullptr, file, QStringLiteral("A/Items"));
    auto* restored = item(reopened, new QTreeWidgetItem(&reopened, {QStringLiteral("Reopened")}),
                          QStringLiteral("Another name"), QStringLiteral("stable-key"));
    QVERIFY(reopened.isFavorite(restored));
    reopened.toggleFavorite(restored);
    QVERIFY(!first.isFavorite(a));
    settings.sync();
    QCOMPARE(settings.value(QStringLiteral("Window/Value")).toInt(), 42);
    QVERIFY(settings.value(QStringLiteral("A/Items")).toStringList().isEmpty());
  }

  void overlayIgnoresScrollbarVisibility_data() {
    QTest::addColumn<QSize>("size");
    QTest::newRow("narrow") << QSize(276, 240);
    QTest::newRow("normal") << QSize(400, 400);
    QTest::newRow("wide") << QSize(800, 700);
  }

  void overlayIgnoresScrollbarVisibility() {
    QFETCH(QSize, size);
    SearchableTreeWidget tree;
    configure(tree);
    tree.resize(size);
    auto* root = new QTreeWidgetItem(&tree, {QStringLiteral("Controls")});
    QTreeWidgetItem* last = nullptr;
    for (int i = 0; i < 80; ++i)
      last = item(tree, root, QStringLiteral("Item %1").arg(i), QStringLiteral("key/%1").arg(i));
    root->setExpanded(true);
    tree.header()->setStretchLastSection(false);
    tree.header()->resizeSection(0, 100);
    tree.header()->resizeSection(1, 100);
    tree.show();
    auto* bubble = tree.findChild<QFrame*>(QStringLiteral("DeviceFeatureSearchBubble"));
    auto* input = tree.findChild<QLineEdit*>(QStringLiteral("DeviceFeatureSearchInput"));
    QTRY_VERIFY(tree.verticalScrollBar()->isVisible());
    QTRY_VERIFY(!tree.horizontalScrollBar()->isVisible());
    const QRect stable = bubble->geometry();
    QCOMPARE(bubble->parentWidget(), static_cast<QWidget*>(&tree));
    tree.header()->resizeSection(1, size.width() * 2);
    QTRY_VERIFY(tree.horizontalScrollBar()->isVisible());
    QTRY_COMPARE(bubble->geometry(), stable);
    tree.scrollToBottom();
    const auto bubbleTop = tree.viewport()->mapFrom(&tree, bubble->pos()).y();
    QVERIFY(tree.visualItemRect(last).bottom() < bubbleTop);

    input->setText(QStringLiteral("Item 79"));
    QTRY_VERIFY(!tree.verticalScrollBar()->isVisible());
    QTRY_COMPARE(bubble->geometry(), stable);
    tree.header()->resizeSection(1, 100);
    QTRY_VERIFY(!tree.horizontalScrollBar()->isVisible());
    QTRY_COMPARE(bubble->geometry(), stable);
    input->clear();
    QTRY_VERIFY(tree.verticalScrollBar()->isVisible());
    QTRY_COMPARE(bubble->geometry(), stable);
  }

  void dockedStripUsesOneRealSurfacePerActivePage() {
    DeviceScopeCard card;
    auto* first = new SearchableTreeWidget(&card, QString(), QStringLiteral("A/Items"));
    configure(*first);
    auto* firstRoot = new QTreeWidgetItem(first);
    auto* alpha = item(*first, firstRoot, QStringLiteral("Alpha"), QStringLiteral("a"));
    auto* gain = item(*first, firstRoot, QStringLiteral("Gain"), QStringLiteral("g"));
    auto* second = new SearchableTreeWidget(&card, QString(), QStringLiteral("B/Items"));
    configure(*second);
    item(*second, new QTreeWidgetItem(second), QStringLiteral("Beta"), QStringLiteral("b"));
    card.addTab(first, QStringLiteral("First"));
    card.addTab(second, QStringLiteral("Second"));
    card.setSearchStripShown(true);
    card.resize(360, 420);
    card.show();
    QApplication::processEvents();

    auto* strip = card.searchStrip();
    QVERIFY(strip);
    auto* firstController = SearchableTreeController::attached(first);
    auto* secondController = SearchableTreeController::attached(second);
    QVERIFY(firstController);
    QVERIFY(secondController);
    QCOMPARE(firstController->searchPlacement(), SearchableTreeController::SearchPlacement::Strip);
    QCOMPARE(firstController->searchSurface()->parentWidget(), strip);
    QCOMPARE(strip->findChildren<QLineEdit*>(QStringLiteral("DeviceFeatureSearchInput")).size(), 1);
    QCOMPARE(strip->findChildren<QToolButton*>(QStringLiteral("DeviceFeatureFavoritesOnly")).size(), 1);
    QVERIFY(!first->findChild<QFrame*>(QStringLiteral("DeviceFeatureSearchBubble")));
    QCOMPARE(strip->property("objectName").toString(), QStringLiteral("DeviceScopeCardSearchStrip"));

    auto* rule = strip->findChild<QFrame*>(QStringLiteral("DeviceScopeCardSearchSeparator"));
    QVERIFY(rule);
    QCOMPARE(rule->height(), 1);
    QVERIFY(rule->geometry().top() >= 0);
    QVERIFY(strip->geometry().top() >= card.pages()->geometry().top());
    QVERIFY(strip->geometry().top() >= card.pages()->geometry().bottom());
    QCOMPARE(strip->geometry().bottom(), card.contentsRect().bottom());

    firstController->searchInput()->setText(QStringLiteral("Gain"));
    firstController->refreshFilter();
    QTRY_VERIFY(alpha->isHidden());
    QVERIFY(!gain->isHidden());
    firstController->toggleFavorite(gain);
    firstController->favoritesOnlyButton()->setChecked(true);
    firstController->refreshFilter();
    QVERIFY(firstController->isFavorite(gain));
    QVERIFY(alpha->isHidden());
    firstController->searchInput()->clear();
    firstController->refreshFilter();
    QTRY_VERIFY(alpha->isHidden());
    QVERIFY(!gain->isHidden());
    firstController->searchInput()->setText(QStringLiteral("Gain"));

    card.setCurrentIndex(1);
    QApplication::processEvents();
    QCOMPARE(secondController->searchSurface()->parentWidget(), strip);
    QCOMPARE(firstController->searchSurface()->parentWidget(), static_cast<QWidget*>(first));
    QVERIFY(firstController->searchSurface()->isHidden());
    QCOMPARE(secondController->searchInput()->text(), QString());

    card.setCurrentIndex(0);
    QApplication::processEvents();
    QCOMPARE(firstController->searchInput()->text(), QStringLiteral("Gain"));
    QVERIFY(firstController->favoritesOnlyButton()->isChecked());
    QCOMPARE(strip->findChildren<QLineEdit*>(QStringLiteral("DeviceFeatureSearchInput")).size(), 1);
  }

  void dockedStripNarrowAndFractionalWidths_data() {
    QTest::addColumn<int>("width");
    QTest::newRow("narrow") << 360;
    QTest::newRow("narrow-fractional") << 361;
    QTest::newRow("wide") << 728;
  }

  void dockedStripNarrowAndFractionalWidths() {
    QFETCH(int, width);
    DeviceScopeCard card;
    auto* tree = new SearchableTreeWidget(&card, QString(), QStringLiteral("C/Items"));
    configure(*tree);
    auto* root = new QTreeWidgetItem(tree);
    for (int i = 0; i < 60; ++i)
      item(*tree, root, QString("Feature %1").arg(i), QString("key/%1").arg(i));
    root->setExpanded(true);
    card.addTab(tree, QStringLiteral("Only"));
    card.setSearchStripShown(true);
    card.setFixedSize(width, 420);
    card.show();
    QApplication::processEvents();

    auto* strip = card.searchStrip();
    auto* controller = SearchableTreeController::attached(tree);
    QVERIFY(strip);
    QVERIFY(controller);
    QCOMPARE(strip->width(), card.contentsRect().width());
    QVERIFY(strip->height() >= 24);
    QVERIFY(strip->geometry().top() >= card.pages()->geometry().bottom());
    auto* input = controller->searchInput();
    QVERIFY(input->width() > 0);
    QVERIFY(strip->rect().contains(input->mapTo(strip, QPoint(0, 0))));
    QTRY_VERIFY(tree->verticalScrollBar()->maximum() > 0);
    const int lastRowBottom = tree->visualItemRect(root->child(59)).bottom();
    QVERIFY(lastRowBottom > 0);
    QVERIFY(strip->geometry().top() >= card.pages()->geometry().top());
  }

  void searchShortcutsUseOneScopePerPlacement()
  {
    DeviceScopeCard card;
    auto* tree = new SearchableTreeWidget(&card, QString(), QStringLiteral("S/Items"));
    configure(*tree);
    auto* leaf = item(*tree, new QTreeWidgetItem(tree),
                      QStringLiteral("Leaf"), QStringLiteral("leaf-key"));
    tree->setCurrentItem(leaf);
    card.addTab(tree, QStringLiteral("Scope"));
    card.resize(360, 420);
    card.show();
    QApplication::processEvents();

    auto* controller = SearchableTreeController::attached(tree);
    QVERIFY(controller);
    QCOMPARE(controller->searchPlacement(), SearchableTreeController::SearchPlacement::Overlay);
    auto* input = controller->searchInput();
    auto* star = controller->favoritesOnlyButton();

    // Overlaid surface is already a child of the tree. The tree scope alone owns
    // both keys; duplicate surface shortcuts make Qt drop the key as ambiguous.
    input->setFocus();
    QVERIFY(input->hasFocus());
    QTest::keyClick(input, Qt::Key_D, Qt::ControlModifier);
    QVERIFY(controller->isFavorite(leaf));
    QTest::keyClick(input, Qt::Key_D, Qt::ControlModifier);
    QVERIFY(!controller->isFavorite(leaf));
    star->setFocus();
    QVERIFY(star->hasFocus());
    QTest::keyClick(star, Qt::Key_D, Qt::ControlModifier);
    QVERIFY(controller->isFavorite(leaf));

    input->setText(QStringLiteral("leaf"));
    tree->setFocus();
    QTest::keyClick(tree, Qt::Key_F, Qt::ControlModifier);
    QVERIFY(input->hasFocus());
    QCOMPARE(input->selectedText(), QStringLiteral("leaf"));
    QTest::keyClick(input, Qt::Key_Escape);
    QCOMPARE(input->text(), QString());

    // Docked surface leaves the tree: its own scope takes over for input focus
    // while the tree scope keeps working when the tree itself has focus.
    card.setSearchStripShown(true);
    QApplication::processEvents();
    QCOMPARE(controller->searchPlacement(), SearchableTreeController::SearchPlacement::Strip);
    QVERIFY(controller->isFavorite(leaf));

    tree->setFocus();
    QTest::keyClick(tree, Qt::Key_D, Qt::ControlModifier);
    QVERIFY(!controller->isFavorite(leaf));
    input->setFocus();
    QTest::keyClick(input, Qt::Key_D, Qt::ControlModifier);
    QVERIFY(controller->isFavorite(leaf));

    input->setText(QStringLiteral("leaf"));
    tree->setFocus();
    QTest::keyClick(tree, Qt::Key_F, Qt::ControlModifier);
    QVERIFY(input->hasFocus());
    QCOMPARE(input->selectedText(), QStringLiteral("leaf"));
    QTest::keyClick(input, Qt::Key_Escape);
    QCOMPARE(input->text(), QString());
  }

  void dockedStripTeardownAndPlaceholderVisibility()
  {
    DeviceScopeCard card;
    auto* first = new SearchableTreeWidget(&card, QString(), QStringLiteral("T/Items"));
    configure(*first);
    item(*first, new QTreeWidgetItem(first), QStringLiteral("Alpha"), QStringLiteral("a"));
    card.addTab(first, QStringLiteral("First"));
    auto* placeholder = new QWidget(&card);
    placeholder->setObjectName(QStringLiteral("NoSearchPage"));
    card.addTab(placeholder, QStringLiteral("No search"));
    card.setSearchStripShown(true);
    card.resize(360, 420);
    card.show();
    QApplication::processEvents();

    auto* strip = card.searchStrip();
    auto* controller = SearchableTreeController::attached(first);
    QVERIFY(strip && controller);
    auto* rule = strip->findChild<QFrame*>(QStringLiteral("DeviceScopeCardSearchSeparator"));
    QVERIFY(rule && rule->isVisible());
    auto* surface = controller->searchSurface();
    QCOMPARE(surface->parentWidget(), strip);

    card.setCurrentIndex(1);
    QApplication::processEvents();
    QVERIFY(strip->isVisible());
    QVERIFY(rule->isHidden());
    QCOMPARE(strip->findChildren<QLineEdit*>(QStringLiteral("DeviceFeatureSearchInput")).size(), 0);
    QCOMPARE(surface->parentWidget(), static_cast<QWidget*>(first));
    QVERIFY(surface->isHidden());
    QCOMPARE(controller->searchPlacement(), SearchableTreeController::SearchPlacement::Overlay);

    card.setCurrentIndex(0);
    QApplication::processEvents();
    QVERIFY(rule->isVisible());
    QCOMPARE(surface->parentWidget(), strip);
    QCOMPARE(controller->searchPlacement(), SearchableTreeController::SearchPlacement::Strip);

    QPointer<QWidget> guardedSurface = surface;
    delete first;
    QApplication::processEvents();
    QVERIFY(guardedSurface.isNull());
    QVERIFY(strip->isVisible());
    QVERIFY(rule->isHidden());
    QCOMPARE(strip->findChildren<QLineEdit*>(QStringLiteral("DeviceFeatureSearchInput")).size(), 0);

    auto* replacement = new SearchableTreeWidget(&card, QString(), QStringLiteral("T/Items"));
    configure(*replacement);
    item(*replacement, new QTreeWidgetItem(replacement), QStringLiteral("Beta"), QStringLiteral("b"));
    card.addTab(replacement, QStringLiteral("Replacement"));
    card.setCurrentIndex(card.count() - 1);
    QApplication::processEvents();
    auto* replacementController = SearchableTreeController::attached(replacement);
    QVERIFY(replacementController);
    auto* replacementSurface = replacementController->searchSurface();
    QCOMPARE(replacementSurface->parentWidget(), strip);
    QCOMPARE(strip->findChildren<QLineEdit*>(QStringLiteral("DeviceFeatureSearchInput")).size(), 1);

    QPointer<QWidget> guardedReplacement = replacementSurface;
    card.removeTab(card.currentIndex());
    QApplication::processEvents();
    QVERIFY(guardedReplacement);
    QCOMPARE(guardedReplacement->parentWidget(), static_cast<QWidget*>(replacement));
    QVERIFY(guardedReplacement->isHidden());
    QVERIFY(strip->isVisible());
    QCOMPARE(strip->findChildren<QLineEdit*>(QStringLiteral("DeviceFeatureSearchInput")).size(), 0);
  }

  void removalAndExternalDeletionKeepTabsPagesAndDockAligned()
  {
    DeviceScopeCard card;
    auto* first = new SearchableTreeWidget(&card, QString(), QStringLiteral("A/Items"));
    configure(*first);
    item(*first, new QTreeWidgetItem(first), QStringLiteral("Alpha"), QStringLiteral("a"));
    auto* middle = new SearchableTreeWidget(&card, QString(), QStringLiteral("B/Items"));
    configure(*middle);
    item(*middle, new QTreeWidgetItem(middle), QStringLiteral("Beta"), QStringLiteral("b"));
    auto* last = new SearchableTreeWidget(&card, QString(), QStringLiteral("C/Items"));
    configure(*last);
    item(*last, new QTreeWidgetItem(last), QStringLiteral("Gamma"), QStringLiteral("c"));
    card.addTab(first, QStringLiteral("First"));
    card.addTab(middle, QStringLiteral("Middle"));
    card.addTab(last, QStringLiteral("Last"));
    card.setSearchStripShown(true);
    card.setCurrentIndex(1);
    card.resize(360, 420);
    card.show();
    QApplication::processEvents();

    auto* strip = card.searchStrip();
    auto* middleController = SearchableTreeController::attached(middle);
    auto* lastController = SearchableTreeController::attached(last);
    QVERIFY(strip && middleController && lastController);
    QCOMPARE(middleController->searchSurface()->parentWidget(), strip);
    QCOMPARE(middleController->searchPlacement(), SearchableTreeController::SearchPlacement::Strip);

    QSignalSpy changes(&card, &DeviceScopeCard::currentChanged);
    card.removeTab(0);
    QCOMPARE(card.count(), 2);
    QCOMPARE(card.pages()->count(), 2);
    QCOMPARE(card.tabBar()->count(), 2);
    QCOMPARE(card.currentIndex(), 0);
    QCOMPARE(card.widget(0), middle);
    QCOMPARE(middleController->searchSurface()->parentWidget(), strip);
    QCOMPARE(middleController->searchPlacement(), SearchableTreeController::SearchPlacement::Strip);
    QCOMPARE(changes.count(), 0);

    QPointer<QWidget> guardedMiddle = middle;
    delete middle;
    QApplication::processEvents();
    QVERIFY(guardedMiddle.isNull());
    QCOMPARE(card.count(), 1);
    QCOMPARE(card.pages()->count(), 1);
    QCOMPARE(card.tabBar()->count(), 1);
    QCOMPARE(card.currentIndex(), 0);
    QCOMPARE(card.widget(0), last);
    QCOMPARE(lastController->searchSurface()->parentWidget(), strip);
    QCOMPARE(lastController->searchPlacement(), SearchableTreeController::SearchPlacement::Strip);
    QCOMPARE(strip->findChildren<QLineEdit*>(QStringLiteral("DeviceFeatureSearchInput")).size(), 1);
    QCOMPARE(changes.count(), 1);
    QCOMPARE(changes.takeFirst().at(0).toInt(), 0);
  }

  void externalInactivePageDeletionKeepsSelectedPage()
  {
    DeviceScopeCard card;
    auto* first = new SearchableTreeWidget(&card, QString(), QStringLiteral("A/Items"));
    configure(*first);
    item(*first, new QTreeWidgetItem(first), QStringLiteral("Alpha"), QStringLiteral("a"));
    auto* middle = new SearchableTreeWidget(&card, QString(), QStringLiteral("B/Items"));
    configure(*middle);
    item(*middle, new QTreeWidgetItem(middle), QStringLiteral("Beta"), QStringLiteral("b"));
    auto* last = new SearchableTreeWidget(&card, QString(), QStringLiteral("C/Items"));
    configure(*last);
    item(*last, new QTreeWidgetItem(last), QStringLiteral("Gamma"), QStringLiteral("c"));
    card.addTab(first, QStringLiteral("First"));
    card.addTab(middle, QStringLiteral("Middle"));
    card.addTab(last, QStringLiteral("Last"));
    card.setSearchStripShown(true);
    card.setCurrentIndex(1);
    card.resize(360, 420);
    card.show();
    QApplication::processEvents();

    auto* strip = card.searchStrip();
    auto* middleController = SearchableTreeController::attached(middle);
    QVERIFY(strip && middleController);
    QCOMPARE(middleController->searchSurface()->parentWidget(), strip);

    QSignalSpy changes(&card, &DeviceScopeCard::currentChanged);
    QPointer<QWidget> guardedFirst = first;
    delete first;
    QApplication::processEvents();
    QVERIFY(guardedFirst.isNull());
    QCOMPARE(card.count(), 2);
    QCOMPARE(card.pages()->count(), 2);
    QCOMPARE(card.tabBar()->count(), 2);
    QCOMPARE(card.currentIndex(), 0);
    QCOMPARE(card.widget(card.currentIndex()), middle);
    QCOMPARE(middleController->searchSurface()->parentWidget(), strip);
    QCOMPARE(middleController->searchPlacement(), SearchableTreeController::SearchPlacement::Strip);
    QCOMPARE(strip->findChildren<QLineEdit*>(QStringLiteral("DeviceFeatureSearchInput")).size(), 1);
    QCOMPARE(changes.count(), 0);
  }
};

QTEST_MAIN(SearchableTreeTests)
#include "SearchableTreeWidgetTests.moc"
