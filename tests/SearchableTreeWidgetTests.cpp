#include "Widgets/SearchableTreeWidget.h"
#include "Widgets/SearchableTreeController.h"
#include <QStyledItemDelegate>
#include <QSignalSpy>

#include <QApplication>
#include <QDir>
#include <QFrame>
#include <QHeaderView>
#include <QLineEdit>
#include <QScrollBar>
#include <QSettings>
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
};

QTEST_MAIN(SearchableTreeTests)
#include "SearchableTreeWidgetTests.moc"
