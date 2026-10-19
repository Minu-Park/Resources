#include "Resources.h"
#include "ItemViewTheme.h"
#include "Widgets/SearchableTreeWidget.h"
#include <QSignalSpy>
#include <QTest>
#include <QScopeGuard>
#include <QtWidgets>
#include <memory>

namespace
{
void movePointer(QWidget *target, const QPoint &position)
{
    static QPointer<QWidget> previous;
    if (previous && previous != target)
    {
        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(previous, &leave);
    }
    QCursor::setPos(target->mapToGlobal(position));
    QTest::mouseMove(target->window()->windowHandle(), target->mapTo(target->window(), position));
    // Replay platform enter/hover delivery for headless and unattended window systems.
    QEnterEvent enter(position, target->mapTo(target->window(), position), target->mapToGlobal(position));
    QApplication::sendEvent(target, &enter);
    QHoverEvent hover(QEvent::HoverMove, position, target->mapToGlobal(position), QPointF(-1, -1));
    QApplication::sendEvent(target, &hover);
    previous = target;
}
}

class TrackingDelegate final : public QStyledItemDelegate
{
  public:
    using QStyledItemDelegate::QStyledItemDelegate;
    mutable int paints = 0;
    mutable int editors = 0;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        ++paints;
        QStyledItemDelegate::paint(painter, option, index);
    }
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override
    {
        ++editors;
        return QStyledItemDelegate::createEditor(parent, option, index);
    }
};
class ResourcesItemViewTests : public QObject
{
    Q_OBJECT
  private slots:
    void initTestCase() { Resources::installResources(*qApp); }
    void selectedTreeRowsDoNotPaintLeadingRails()
    {
        QTreeWidget tree;
        tree.setHeaderLabels({"Feature", "Value"});
        tree.setSelectionBehavior(QAbstractItemView::SelectRows);
        auto* row = new QTreeWidgetItem(&tree, {"Exposure", "120"});
        tree.resize(360, 150); tree.show();
        QVERIFY(QTest::qWaitForWindowExposed(&tree));
        tree.setCurrentItem(row);
        const auto image = tree.viewport()->grab().toImage();
        const qreal dpr = image.devicePixelRatio();
        const int y = qRound(tree.visualItemRect(row).center().y() * dpr);
        for (int x = qRound(2 * dpr); x < qRound(16 * dpr); ++x)
            QVERIFY2(qGray(image.pixelColor(x, y).rgb()) > 200, "A native selection rail remains outside the row capsule.");
    }
    void incompleteThemeReloadIsAtomic()
    {
        const auto installed = qApp->styleSheet();
        const auto colors = Resources::themePalette();
        QTemporaryDir incomplete;
        QString error;
        QVERIFY(!Resources::reloadThemeFromDirectory(incomplete.path(), &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(qApp->styleSheet(), installed);
        QCOMPARE(Resources::themePalette(), colors);
    }
    void palettePatchesAreAtomicAndRefreshExistingWidgets()
    {
        const auto original = Resources::themePalette();
        const auto restore = qScopeGuard([&] { Resources::setThemePalette(original); });
        QLineEdit editor("Existing control");
        editor.resize(220, 24);
        editor.show();
        QVERIFY(QTest::qWaitForWindowExposed(&editor));
        QVERIFY(Resources::setThemePalette({{"text", QColor("#234567")}, {"selection", QColor("#ddeeff")}}));
        QCOMPARE(editor.palette().color(QPalette::Text), QColor("#234567"));
        QCOMPARE(Resources::themePalette().value("selection"), QColor("#ddeeff"));
        const auto accepted = Resources::themePalette();
        QVERIFY(!Resources::setThemePalette({{"text", QColor("#abcdef")}, {"unknown", QColor("#112233")}}));
        QCOMPARE(Resources::themePalette(), accepted);
        QVERIFY(!Resources::setThemePalette({{"text", QColor()}}));
        QVERIFY(!Resources::setThemePalette({}));
        QCOMPARE(Resources::themePalette(), accepted);
        QVERIFY(Resources::setFontColor(QColor(20, 40, 60, 10)));
        QCOMPARE(Resources::fontColor(), QColor(20, 40, 60));
        QCOMPARE(Resources::themePalette().value("text"), Resources::fontColor());
        Resources::resetThemePalette();
        QCOMPARE(Resources::themePalette(), Resources::defaultThemePalette());
        QCOMPARE(editor.palette().color(QPalette::Text), Resources::defaultThemePalette().value("text"));
        QVERIFY(!qApp->styleSheet().contains(QRegularExpression("@[A-Z_]+@")));
    }
    void joinedOnlyKeepsAdjacentCellsContinuous()
    {
        QTableWidget table(2, 3);
        table.setShowGrid(false);
        table.setSelectionMode(QAbstractItemView::ExtendedSelection);
        table.resize(450, 180);
        table.show();
        QVERIFY(QTest::qWaitForWindowExposed(&table));
        table.activateWindow();
        table.setFocus();
        table.selectAll();
        table.selectionModel()->setCurrentIndex(table.model()->index(0, 1), QItemSelectionModel::NoUpdate);
        Resources::setItemSelectionAppearance(&table, Resources::ItemSelectionAppearance::JoinedOnly);
        const auto sample = [](const QImage& image, QPoint p) {
            return image.pixelColor(qRound(p.x() * image.devicePixelRatio()),
                                    qRound(p.y() * image.devicePixelRatio()));
        };
        auto image = table.viewport()->grab().toImage();
        for (int column = 0; column < 3; ++column)
        {
            const QRect rect = table.visualRect(table.model()->index(0, column));
            QCOMPARE(sample(image, rect.center()), table.palette().highlight().color());
            if (column < 2)
                QCOMPARE(sample(image, QPoint(rect.right(), rect.center().y())), table.palette().highlight().color());
        }
        Resources::setItemSelectionAppearance(&table, Resources::ItemSelectionAppearance::CurrentCell);
        image = table.viewport()->grab().toImage();
        QCOMPARE(sample(image, table.visualRect(table.currentIndex()).center()), QColor("#d4d4d3"));
        QCOMPARE(table.selectionModel()->selectedIndexes().size(), 6);
    }
    void unselectedKeyboardCurrentRemainsVisible_data()
    {
        QTest::addColumn<bool>("ctrlClick");
        QTest::newRow("ctrl-arrow") << false;
        QTest::newRow("ctrl-click-deselect") << true;
    }
    void joinedOnlyTreeAndList_data()
    {
        QTest::addColumn<bool>("tree");
        QTest::newRow("tree") << true;
        QTest::newRow("list") << false;
    }
    void joinedOnlyTreeAndList()
    {
        QFETCH(bool, tree);
        QStandardItemModel model(2, 3);
        std::unique_ptr<QAbstractItemView> view(tree
            ? static_cast<QAbstractItemView*>(new QTreeView)
            : static_cast<QAbstractItemView*>(new QListView));
        view->setModel(&model);
        view->setSelectionMode(QAbstractItemView::ExtendedSelection);
        if (auto* treeView = qobject_cast<QTreeView*>(view.get()))
            treeView->setRootIsDecorated(false);
        view->resize(450, 180);
        view->show();
        QVERIFY(QTest::qWaitForWindowExposed(view.get()));
        view->activateWindow();
        view->setFocus();
        view->selectAll();
        view->selectionModel()->setCurrentIndex(model.index(0, 0), QItemSelectionModel::NoUpdate);
        Resources::setItemSelectionAppearance(view.get(), Resources::ItemSelectionAppearance::JoinedOnly);
        const auto image = view->viewport()->grab().toImage();
        for (int row = 0; row < 2; ++row)
        {
            const QPoint p = view->visualRect(model.index(row, 0)).center();
            QCOMPARE(image.pixelColor(qRound(p.x() * image.devicePixelRatio()),
                                      qRound(p.y() * image.devicePixelRatio())),
                     view->palette().highlight().color());
        }
    }
    void unselectedKeyboardCurrentRemainsVisible()
    {
        QFETCH(bool, ctrlClick);
        QTableWidget table(1, 3);
        table.setShowGrid(false);
        table.setSelectionMode(QAbstractItemView::ExtendedSelection);
        table.resize(450, 160);
        table.show();
        QVERIFY(QTest::qWaitForWindowExposed(&table));
        table.activateWindow();
        table.setFocus();
        const auto first = table.model()->index(0, 0);
        table.selectionModel()->setCurrentIndex(first, QItemSelectionModel::ClearAndSelect);
        if (ctrlClick)
            QTest::mouseClick(table.viewport(), Qt::LeftButton, Qt::ControlModifier, table.visualRect(first).center());
        else
            QTest::keyClick(&table, Qt::Key_Right, Qt::ControlModifier);
        QVERIFY(table.currentIndex().isValid());
        QVERIFY(!table.selectionModel()->isSelected(table.currentIndex()));
        const QRect rect = table.visualRect(table.currentIndex());
        const auto image = table.viewport()->grab().toImage();
        const qreal dpr = image.devicePixelRatio();
        const QColor edge = image.pixelColor(qRound(rect.center().x() * dpr), qRound((rect.top() + 1) * dpr));
        const QColor center = image.pixelColor(qRound(rect.center().x() * dpr), qRound(rect.center().y() * dpr));
        QVERIFY2(qGray(center.rgb()) - qGray(edge.rgb()) >= 20, qPrintable(edge.name()));
        QVERIFY(center != QColor("#d4d4d3"));
    }
    void currentCellIsFilledWithinJoinedSelection_data()
    {
        QTest::addColumn<bool>("treeView");
        QTest::addColumn<bool>("rtl");
        for (bool tree : {false, true})
            for (bool rtl : {false, true})
                QTest::newRow(qPrintable(QString("%1-%2").arg(tree).arg(rtl))) << tree << rtl;
    }
    void currentCellIsFilledWithinJoinedSelection()
    {
        QFETCH(bool, treeView);
        QFETCH(bool, rtl);
        std::unique_ptr<QAbstractItemView> view(treeView
            ? static_cast<QAbstractItemView*>(new QTreeView)
            : static_cast<QAbstractItemView*>(new QTableView));
        QStandardItemModel model(2, 3);
        view->setModel(&model);
        view->setLayoutDirection(rtl ? Qt::RightToLeft : Qt::LeftToRight);
        view->setSelectionBehavior(QAbstractItemView::SelectRows);
        if (auto* tree = qobject_cast<QTreeView*>(view.get())) tree->setRootIsDecorated(false);
        if (auto* table = qobject_cast<QTableView*>(view.get())) table->setShowGrid(false);
        view->resize(470, 180);
        view->show();
        QVERIFY(QTest::qWaitForWindowExposed(view.get()));
        view->activateWindow();
        view->setFocus();
        view->selectionModel()->select(model.index(0, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        view->selectionModel()->select(model.index(1, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        const auto sample = [&](const QImage& image, QPoint point) {
            return image.pixelColor(qRound(point.x() * image.devicePixelRatio()),
                                    qRound(point.y() * image.devicePixelRatio()));
        };
        for (int column : {0, 1, 2})
        {
            const auto current = model.index(0, column);
            view->selectionModel()->setCurrentIndex(current, QItemSelectionModel::NoUpdate);
            QCoreApplication::processEvents();
            const auto image = view->viewport()->grab().toImage();
            const QRect rect = view->visualRect(current);
            QCOMPARE(sample(image, rect.center()), QColor("#d4d4d3"));
            // Current and selection fills share the same top/bottom edges.
            QCOMPARE(sample(image, QPoint(rect.center().x(), rect.top() + 1)), QColor("#d4d4d3"));
            QCOMPARE(sample(image, QPoint(rect.center().x(), rect.bottom() - 1)), QColor("#d4d4d3"));
            // Later columns/rows must not paint their seam overlap over the current capsule.
            for (const QPoint edge : {QPoint(rect.right(), rect.center().y()),
                                     QPoint(rect.center().x(), rect.bottom())})
            {
                const QColor color = sample(image, edge);
                const QColor currentColor("#d4d4d3");
                // Fractional DPR can blend the boundary pixel; a neighbor repaint
                // replaces it with the much lighter selection color instead.
                QVERIFY2(qAbs(color.red() - currentColor.red()) <= 8 &&
                             qAbs(color.green() - currentColor.green()) <= 8 &&
                             qAbs(color.blue() - currentColor.blue()) <= 8,
                         qPrintable(QString("Current-cell edge overwritten at %1,%2: %3")
                             .arg(edge.x()).arg(edge.y()).arg(color.name())));
            }
            for (int other = 0; other < 3; ++other)
                if (other != column)
                    QCOMPARE(sample(image, view->visualRect(model.index(0, other)).center()),
                             view->palette().highlight().color());
        }
    }
    void itemSwitchUsesLeadingEdgeAndMatchingHitRect_data()
    {
        QTest::addColumn<bool>("rtl");
        QTest::newRow("ltr") << false;
        QTest::newRow("rtl") << true;
    }
    void listCurrentCellUsesSharedFill()
    {
        QListWidget list;
        list.setSelectionMode(QAbstractItemView::ExtendedSelection);
        list.addItems({QString(), QString()});
        list.resize(320, 150);
        list.show();
        QVERIFY(QTest::qWaitForWindowExposed(&list));
        list.activateWindow();
        list.setFocus();
        list.selectAll();
        list.setCurrentRow(1, QItemSelectionModel::NoUpdate);
        const QImage image = list.viewport()->grab().toImage();
        for (int row = 0; row < 2; ++row)
        {
            const QPoint point = list.visualItemRect(list.item(row)).center();
            QCOMPARE(image.pixelColor(qRound(point.x() * image.devicePixelRatio()),
                                      qRound(point.y() * image.devicePixelRatio())),
                     row == 1 ? QColor("#d4d4d3") : list.palette().highlight().color());
        }
    }
    void itemSwitchUsesLeadingEdgeAndMatchingHitRect()
    {
        QFETCH(bool, rtl);
        QTreeWidget tree;
        tree.setRootIsDecorated(false);
        tree.setLayoutDirection(rtl ? Qt::RightToLeft : Qt::LeftToRight);
        auto* item = new QTreeWidgetItem(&tree, {QStringLiteral("Producer.cti")});
        item->setCheckState(0, Qt::Unchecked);
        tree.resize(400, 160);
        tree.show();
        QVERIFY(QTest::qWaitForWindowExposed(&tree));
        struct Probe : QStyledItemDelegate { using QStyledItemDelegate::initStyleOption; } probe;
        QStyleOptionViewItem option;
        option.initFrom(&tree);
        option.widget = &tree;
        option.rect = tree.visualItemRect(item);
        probe.initStyleOption(&option, tree.indexFromItem(item));
        const QRect check = tree.style()->subElementRect(QStyle::SE_ItemViewItemCheckIndicator, &option, &tree);
        const QRect text = tree.style()->subElementRect(QStyle::SE_ItemViewItemText, &option, &tree);
        const int inset = rtl ? option.rect.right() - check.right() : check.left() - option.rect.left();
        QVERIFY2(inset >= 0 && inset <= 5, qPrintable(QString("leading inset=%1 direction=%2")
            .arg(inset).arg(tree.property("layoutDirection").toString())));
        QVERIFY(!check.intersects(text));
        const int gap = rtl ? check.left() - text.right() - 1 : text.left() - check.right() - 1;
        QVERIFY2(gap >= 0 && gap <= 10, qPrintable(QString("vacant switch/text gutter=%1").arg(gap)));
        QTest::mouseClick(tree.viewport(), Qt::LeftButton, Qt::NoModifier, check.center());
        QCOMPARE(item->checkState(0), Qt::Checked);
        tree.setCurrentItem(item);
        QTest::keyClick(&tree, Qt::Key_Space);
        QCOMPARE(item->checkState(0), Qt::Unchecked);
    }
    void adjoiningSelectionsHaveNoInternalCorners_data()
    {
        QTest::addColumn<bool>("rtl");
        QTest::addColumn<bool>("reordered");
        QTest::newRow("ltr") << false << false;
        QTest::newRow("rtl") << true << false;
        QTest::newRow("moved-hidden") << false << true;
        QTest::newRow("rtl-moved-hidden") << true << true;
    }
    void adjoiningSelectionsHaveNoInternalCorners()
    {
        QFETCH(bool, rtl);
        QFETCH(bool, reordered);
        QTableWidget table(3, 3);
        table.setLayoutDirection(rtl ? Qt::RightToLeft : Qt::LeftToRight);
        table.setShowGrid(false);
        table.setSelectionMode(QAbstractItemView::ExtendedSelection);
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 3; ++c)
                table.setItem(r, c, new QTableWidgetItem);
        table.resize(430, 200);
        table.show();
        QVERIFY(QTest::qWaitForWindowExposed(&table));
        if (reordered)
        {
            table.horizontalHeader()->moveSection(0, 2);
            table.hideColumn(1);
        }
        table.selectAll();
        table.clearFocus();
        QTest::mouseMove(table.viewport(), QPoint(400, 170));
        const QImage image = table.viewport()->grab().toImage();
        const qreal dpr = image.devicePixelRatio();
        const auto pixel = [&](QPoint p)
        { return image.pixelColor(qRound(p.x() * dpr), qRound(p.y() * dpr)); };
        // Every touching selected edge must retain the selection fill, including near its corners.
        for (int r = 0; r < 2; ++r)
            for (int c = 0; c < 3; ++c)
            {
                if (table.isColumnHidden(c))
                    continue;
                const QRect cell = table.visualItemRect(table.item(r, c));
                const auto below = table.indexAt(QPoint(cell.center().x(), cell.bottom() + 1));
                QVERIFY(below.isValid());
                QCOMPARE(pixel(QPoint(cell.left() + 1, cell.bottom())), table.palette().highlight().color());
                const auto right = table.indexAt(QPoint(cell.right() + 1, cell.center().y()));
                if (right.isValid())
                    QCOMPARE(pixel(QPoint(cell.right(), cell.top() + 1)),
                             table.palette().highlight().color());
            }
    }
    void originalDelegateStillEditsAndCommits()
    {
        QTableWidget table(1, 1);
        table.setItem(0, 0, new QTableWidgetItem("Before"));
        auto *source = new TrackingDelegate(&table);
        table.setItemDelegate(source);
        QSignalSpy commits(source, &QAbstractItemDelegate::commitData);
        table.resize(320, 150);
        table.show();
        QVERIFY(QTest::qWaitForWindowExposed(&table));
        table.grab();
        QVERIFY(source->paints > 0);
        QVERIFY(table.itemDelegate() != source);
        table.editItem(table.item(0, 0));
        auto *editor = table.findChild<QLineEdit *>();
        QVERIFY(editor);
        QCOMPARE(source->editors, 1);
        editor->setText("After");
        QTest::keyClick(editor, Qt::Key_Return);
        QTRY_COMPARE(table.item(0, 0)->text(), QString("After"));
        QCOMPARE(commits.count(), 1);
    }
    void tableHoverSpansOnlyTheHoveredRow_data()
    {
        QTest::addColumn<bool>("rtl");
        QTest::newRow("ltr") << false;
        QTest::newRow("rtl") << true;
    }
    void tableHoverSpansOnlyTheHoveredRow()
    {
        QFETCH(bool, rtl);
        QTableWidget table(2, 3);
        table.setLayoutDirection(rtl ? Qt::RightToLeft : Qt::LeftToRight);
        table.setShowGrid(false);
        table.setSelectionBehavior(QAbstractItemView::SelectRows);
        for (int r = 0; r < 2; ++r)
            for (int c = 0; c < 3; ++c)
                table.setItem(r, c, new QTableWidgetItem);
        table.resize(430, 180);
        table.show();
        QVERIFY(QTest::qWaitForWindowExposed(&table));
        table.clearFocus();
        const auto sample = [&](const QImage &image, int r, int c)
        {
            const QPoint p = table.visualItemRect(table.item(r, c)).center();
            return image.pixelColor(qRound(p.x() * image.devicePixelRatio()),
                                    qRound(p.y() * image.devicePixelRatio()));
        };
        movePointer(table.viewport(), QPoint(table.viewport()->width() - 8, table.viewport()->height() - 8));
        const QColor base = sample(table.viewport()->grab().toImage(), 0, 0);
        for (int r = 0; r < 2; ++r)
        {
            movePointer(table.viewport(), table.visualItemRect(table.item(r, 1)).center());
            QCoreApplication::processEvents();
            const auto image = table.viewport()->grab().toImage();
            const QColor hover = sample(image, r, 1);
            QVERIFY(hover != base);
            for (int c = 0; c < 3; ++c)
            {
                QCOMPARE(sample(image, r, c), hover);
                QCOMPARE(sample(image, 1 - r, c), base);
            }
        }
        movePointer(&table, QPoint(1, 1));
        QCoreApplication::processEvents();
        const auto cleared = table.viewport()->grab().toImage();
        for (int r = 0; r < 2; ++r)
            QCOMPARE(sample(cleared, r, 1), base);
    }
    void rowHoverClearsWhenCovered_data()
    {
        QTest::addColumn<bool>("popup");
        QTest::newRow("window") << false;
        QTest::newRow("popup") << true;
    }
    void rowHoverClearsWhenCovered()
    {
        QFETCH(bool, popup);
        QTableWidget table(1, 3);
        table.setSelectionBehavior(QAbstractItemView::SelectRows);
        table.setShowGrid(false);
        for (int c = 0; c < 3; ++c)
            table.setItem(0, c, new QTableWidgetItem);
        table.resize(430, 180);
        table.show();
        QVERIFY(QTest::qWaitForWindowExposed(&table));
        table.clearFocus();
        const QPoint sample = table.visualItemRect(table.item(0, 0)).center();
        const auto color = [&]
        {
            const auto image = table.viewport()->grab().toImage();
            return image.pixelColor(qRound(sample.x() * image.devicePixelRatio()),
                                    qRound(sample.y() * image.devicePixelRatio()));
        };
        movePointer(table.viewport(), QPoint(table.viewport()->width() - 8, table.viewport()->height() - 8));
        const QColor base = color();
        const QPoint hover = table.visualItemRect(table.item(0, 1)).center();
        movePointer(table.viewport(), hover);
        QTRY_VERIFY(table.viewport()->underMouse());
        QVERIFY(color() != base);
        const QPoint cursor = QCursor::pos();
        QWidget cover(nullptr, popup ? Qt::Popup : Qt::Window | Qt::FramelessWindowHint);
        cover.resize(100, 80);
        cover.move(cursor - QPoint(40, 30));
        cover.show();
        QVERIFY(QTest::qWaitForWindowExposed(&cover));
        movePointer(&cover, cover.mapFromGlobal(cursor));
        // Replay the window-system leave even on headless platforms without native occlusion events.
        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(table.viewport(), &leave);
        QTRY_VERIFY(!table.viewport()->underMouse());
        QCOMPARE(QCursor::pos(), cursor);
        QVERIFY(table.viewport()->rect().contains(table.viewport()->mapFromGlobal(cursor)));
        QCOMPARE(color(), base);
        cover.hide();
        movePointer(table.viewport(), QPoint(table.viewport()->width() - 8, table.viewport()->height() - 8));
        movePointer(table.viewport(), hover);
        QTRY_VERIFY(table.viewport()->underMouse());
        QVERIFY(color() != base);
    }
    void themePreservesMouseTracking_data()
    {
        QTest::addColumn<int>("kind");
        QTest::addColumn<bool>("tracking");
        for (int kind = 0; kind < 3; ++kind)
            for (bool tracking : {false, true})
                QTest::newRow(qPrintable(QString("%1-%2").arg(kind).arg(tracking))) << kind << tracking;
    }
    void themePreservesMouseTracking()
    {
        QFETCH(int, kind);
        QFETCH(bool, tracking);
        std::unique_ptr<QAbstractItemView> view;
        if (kind == 0)
            view = std::make_unique<QTableView>();
        else if (kind == 1)
            view = std::make_unique<QTreeView>();
        else
            view = std::make_unique<QListView>();
        QStandardItemModel model(2, 2);
        model.setData(model.index(0, 0), "Status", Qt::StatusTipRole);
        view->setModel(&model);
        view->setMouseTracking(tracking);
        QSignalSpy entered(view.get(), &QAbstractItemView::entered);
        view->resize(320, 180);
        view->show();
        QVERIFY(QTest::qWaitForWindowExposed(view.get()));
        // Isolate delegate installation from QSS polish, which has its own native tracking policy.
        auto *source = new QStyledItemDelegate(view.get());
        view->setItemDelegate(source);
        view->setMouseTracking(tracking);
        view->viewport()->setMouseTracking(tracking);
        Resources::applyItemViewTheme(view.get());
        QVERIFY(view->itemDelegate() != source);
        QCOMPARE(view->hasMouseTracking(), tracking);
        QCOMPARE(view->viewport()->hasMouseTracking(), tracking);
        movePointer(view->viewport(), QPoint(view->viewport()->width() - 8, view->viewport()->height() - 8));
        entered.clear();
        movePointer(view->viewport(), view->visualRect(model.index(0, 0)).center());
        QCoreApplication::processEvents();
        if (!tracking)
            QCOMPARE(entered.count(), 0);
    }
    void nestedFavoriteDelegateCommitsOnce_data()
    {
        QTest::addColumn<int>("key");
        QTest::newRow("enter") << int(Qt::Key_Return);
        QTest::newRow("tab") << int(Qt::Key_Tab);
        QTest::newRow("focus-out") << 0;
    }
    void nestedFavoriteDelegateCommitsOnce()
    {
        QFETCH(int, key);
        SearchableTreeWidget tree;
        tree.setHeaderLabels({"Name", "Value"});
        auto *item = new QTreeWidgetItem(&tree, {"Before", "42"});
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        tree.resize(420, 220);
        tree.show();
        QVERIFY(QTest::qWaitForWindowExposed(&tree));
        auto *active = tree.itemDelegateForColumn(0);
        QVERIFY(active);
        QSignalSpy commits(active, &QAbstractItemDelegate::commitData);
        QSignalSpy closes(active, &QAbstractItemDelegate::closeEditor);
        tree.setCurrentItem(item, 0);
        tree.editItem(item, 0);
        auto *editor = tree.viewport()->findChild<QLineEdit *>();
        QVERIFY(editor);
        editor->setText("After");
        if (key)
            QTest::keyClick(editor, Qt::Key(key));
        else
            tree.setFocus();
        QTRY_COMPARE(item->text(0), QString("After"));
        QCoreApplication::processEvents();
        QCOMPARE(commits.count(), 1);
        QCOMPARE(closes.count(), 1);
    }
    void featureFavoritesAndEmbeddedEditorsSurvive()
    {
        SearchableTreeWidget tree;
        tree.setProperty("treeRole", QStringLiteral("DeviceFeatureTree"));
        tree.setHeaderLabels({"Feature", "Value"});
        auto *item = new QTreeWidgetItem(&tree, {"Gain", ""});
        tree.registerItem(item, QStringLiteral("gain"));
        auto *editor = new QLineEdit("2.0");
        tree.setItemWidget(item, 1, editor);
        tree.resize(420, 200);
        tree.show();
        QVERIFY(QTest::qWaitForWindowExposed(&tree));
        editor->setFocus();
        editor->selectAll();
        const QRect rect = tree.visualItemRect(item);
        const int extent = tree.style()->pixelMetric(QStyle::PM_SmallIconSize);
        const QPoint star(tree.header()->sectionViewportPosition(0) + tree.header()->sectionSize(0) -
                              extent / 2 - 3,
                          rect.center().y());
        QTest::mouseMove(tree.viewport(), star);
        QTest::mouseClick(tree.viewport(), Qt::LeftButton, Qt::NoModifier, star);
        QVERIFY(tree.isFavorite(item));
        QCOMPARE(editor->text(), QString("2.0"));
        QTest::mouseClick(tree.viewport(), Qt::LeftButton, Qt::NoModifier, star);
        QVERIFY(!tree.isFavorite(item));
    }
    void popupsKeepTheirOriginalDelegates()
    {
        QComboBox combo;
        combo.addItems({"One", "Two"});
        auto *original = combo.view()->itemDelegate();
        combo.show();
        combo.showPopup();
        QCoreApplication::processEvents();
        QCOMPARE(combo.view()->itemDelegate(), original);
        combo.hidePopup();
    }
    void enabledAndDisabledSwitchesHaveIdenticalGeometry()
    {
        QWidget host;
        auto *layout = new QVBoxLayout(&host);
        QCheckBox active("Enable"), inactive("Enable");
        inactive.setEnabled(false);
        layout->addWidget(&active);
        layout->addWidget(&inactive);
        host.show();
        host.setFocusPolicy(Qt::StrongFocus);
        host.setFocus();
        for (auto state : {Qt::Unchecked, Qt::Checked, Qt::PartiallyChecked})
        {
            active.setCheckState(state);
            inactive.setCheckState(state);
            QStyleOptionButton a, b;
            a.initFrom(&active);
            b.initFrom(&inactive);
            a.rect = active.rect();
            b.rect = inactive.rect();
            QCOMPARE(active.style()->subElementRect(QStyle::SE_CheckBoxIndicator, &a, &active),
                     inactive.style()->subElementRect(QStyle::SE_CheckBoxIndicator, &b, &inactive));
            for (QCheckBox* control : {&active, &inactive}) {
                QStyleOptionButton option;
                option.initFrom(control); option.rect = control->rect();
                const QRect indicator = control->style()->subElementRect(QStyle::SE_CheckBoxIndicator, &option, control);
                const QImage image = control->grab().toImage();
                const qreal dpr = image.devicePixelRatio();
                const QRect content(qRound((indicator.left() + 2) * dpr), qRound((indicator.top() + 2) * dpr),
                                    qRound((indicator.width() - 4) * dpr), qRound((indicator.height() - 4) * dpr));
                QRect visible;
                for (int y = content.top(); y <= content.bottom(); ++y)
                    for (int x = content.left(); x <= content.right(); ++x)
                        if (qGray(image.pixelColor(x, y).rgb()) < 248) visible |= QRect(x, y, 1, 1);
                QVERIFY2(visible.width() >= qFloor(32 * dpr) && visible.height() >= qFloor(18 * dpr),
                         "Switch artwork shrank inside its indicator slot.");
            }
        }
        // Stack widgets must not be destroyed by their temporary layout parent.
        active.setParent(nullptr);
        inactive.setParent(nullptr);
    }
    void completedProgressTextRetainsContrast()
    {
        QProgressBar bar;
        bar.resize(240, 24);
        bar.show();
        QVERIFY(QTest::qWaitForWindowExposed(&bar));
        const auto captionInk = [&](bool filled)
        {
            const QImage image = bar.grab().toImage();
            const qreal dpr = image.devicePixelRatio();
            int count = 0;
            for (int y = qRound(5 * dpr); y < qRound(19 * dpr); ++y)
                for (int x = qRound(85 * dpr); x < qRound(155 * dpr); ++x)
                {
                    const QColor c = image.pixelColor(x, y);
                    // Accept antialiased glyph coverage on both native and headless font rasterizers.
                    if (filled ? c.red() > 150 && c.green() > 180 && c.blue() > 210
                               : c.red() < 150 && c.green() < 150 && c.blue() < 150)
                        ++count;
                }
            return count;
        };
        bar.setValue(0);
        QVERIFY(captionInk(false) > 5);
        bar.setValue(100);
        QVERIFY(captionInk(true) > 5);
    }

    void compactRowsPreserveExplicitConsumerDensity()
    {
        QTableWidget standard(2, 1), custom(2, 1);
        custom.verticalHeader()->setDefaultSectionSize(42);
        standard.show();
        custom.show();
        QCoreApplication::processEvents();
        QCOMPARE(custom.verticalHeader()->defaultSectionSize(), 42);
        QVERIFY(standard.verticalHeader()->defaultSectionSize() >= 30);
    }
};
QTEST_MAIN(ResourcesItemViewTests)
#include "ResourcesItemViewTests.moc"
