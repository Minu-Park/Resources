#include "Resources.h"
#include "Widgets/SearchableTreeWidget.h"
#include <QSignalSpy>
#include <QTest>
#include <QtWidgets>

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
        QTest::mouseMove(table.viewport(), QPoint(410, 150));
        const QColor base = sample(table.viewport()->grab().toImage(), 0, 0);
        for (int r = 0; r < 2; ++r)
        {
            QTest::mouseMove(table.viewport(), table.visualItemRect(table.item(r, 1)).center());
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
        QTest::mouseMove(&table, QPoint(1, 1));
        QCoreApplication::processEvents();
        const auto cleared = table.viewport()->grab().toImage();
        for (int r = 0; r < 2; ++r)
            QCOMPARE(sample(cleared, r, 1), base);
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
