#include "Chrome/ThemedLoadingWidget.h"
#include "Chrome/ThemedDockTitleBar.h"
#include "Chrome/ThemedMainWindow.h"
#include "Chrome/ThemedTreeWidget.h"
#include "Resources.h"

#include <QApplication>
#include <QAbstractItemView>
#include <QComboBox>
#include <QDir>
#include <QDockWidget>
#include <QMenu>
#include <QListView>
#include <QPainter>
#include <QPushButton>
#include <QProcess>
#include <QScreen>
#include <QSignalSpy>
#include <QStyle>
#include <QTest>
#include <QTreeWidget>
#include <QVBoxLayout>

class ResourcesTransientWindowTests : public QObject
{
    Q_OBJECT
private slots:
    void treeBranchesSurviveRowPainting()
    {
        class BranchProbe : public ThemedTreeWidget {
        public:
            int branchPaints = 0;
        protected:
            void drawBranches(QPainter* painter, const QRect& rect, const QModelIndex& index) const override {
                if (rect.width() > 0) ++const_cast<BranchProbe*>(this)->branchPaints;
                ThemedTreeWidget::drawBranches(painter, rect, index);
            }
        };
        Resources::installResources(*qApp);
        for (const auto direction : {Qt::LeftToRight, Qt::RightToLeft}) {
            BranchProbe tree;
            tree.setLayoutDirection(direction);
            tree.setInteractionMode(ThemedTreeWidget::InteractionMode::Row);
            tree.setHeaderLabels({QStringLiteral("Source"), QStringLiteral("Path")});
            auto* root = new QTreeWidgetItem(&tree, {QStringLiteral("Root")});
            new QTreeWidgetItem(root, {QStringLiteral("Child")});
            tree.resize(400, 200);
            tree.show();
            tree.grab();
            QVERIFY(tree.branchPaints > 0);
            root->setExpanded(true);
            tree.branchPaints = 0;
            tree.grab();
            QVERIFY(tree.branchPaints > 0);
            QCOMPARE(tree.selectionBehavior(), QAbstractItemView::SelectRows);
        }
    }

    void treeBranchKeepsRoundedRowCorner()
    {
        Resources::installResources(*qApp);
        ThemedTreeWidget tree;
        tree.setInteractionMode(ThemedTreeWidget::InteractionMode::Row);
        tree.setHeaderLabels({QStringLiteral("Source"), QStringLiteral("Path")});
        auto* root = new QTreeWidgetItem(&tree, {QStringLiteral("Root"), QStringLiteral("/Library")});
        new QTreeWidgetItem(root, {QString(), QStringLiteral("Child.cti")});
        tree.resize(400, 200);
        tree.show();
        QVERIFY(QTest::qWaitForWindowExposed(&tree));
        tree.setCurrentItem(root);

        const QImage image = tree.viewport()->grab().toImage();
        const qreal ratio = image.devicePixelRatio();
        const QRect row = tree.visualItemRect(root);
        const auto pixel = [&](int x, int y) { return image.pixelColor(qRound(x * ratio), qRound(y * ratio)); };
        const QColor base = pixel(tree.viewport()->width() / 2, tree.viewport()->height() - 2);
        QCOMPARE(pixel(0, row.top()), base);
        QCOMPARE(pixel(0, row.top() + 2), base);
        QCOMPARE(pixel(0, row.bottom() - 2), base);
        QCOMPARE(tree.style()->styleHint(QStyle::SH_ItemView_ShowDecorationSelected, nullptr, &tree), 0);
    }

    void comboPopupLifecycle_data()
    {
        QTest::addColumn<bool>("compact");
        QTest::addColumn<bool>("editable");
        QTest::addColumn<int>("itemCount");
        QTest::addColumn<bool>("replaceView");
        QTest::addColumn<bool>("installLate");
        QTest::newRow("preexisting-popup") << false << false << 20 << false << true;
        QTest::newRow("standard") << false << false << 3 << false << false;
        QTest::newRow("standard-editable") << false << true << 3 << false << false;
        QTest::newRow("compact") << true << false << 3 << false << false;
        QTest::newRow("compact-editable") << true << true << 3 << false << false;
        QTest::newRow("device-scroll") << false << false << 20 << false << false;
        QTest::newRow("device-scroll-editable") << false << true << 20 << false << false;
        QTest::newRow("compact-scroll") << true << false << 20 << false << false;
        QTest::newRow("replacement-view") << false << false << 20 << true << false;
    }

    void comboPopupLifecycle()
    {
        QFETCH(bool, compact);
        QFETCH(bool, editable);
        QFETCH(int, itemCount);
        QFETCH(bool, replaceView);
        QFETCH(bool, installLate);
        if (!installLate) Resources::installResources(*qApp);
        QWidget owner;
        if (compact) owner.setObjectName(QStringLiteral("PluginMarketplaceContent"));
        owner.resize(360, 240);
        QComboBox combo(&owner);
        if (installLate) {
            combo.ensurePolished();
            combo.view()->window()->ensurePolished();
            Resources::installResources(*qApp);
        }
        combo.setEditable(editable);
        for (int row = 0; row < itemCount; ++row) {
            combo.addItem(itemCount > 3 ? QStringLiteral("Euresys Playlink (Device%1)").arg(row)
                                       : QStringLiteral("1.0.%1").arg(2 - row));
        }
        combo.setMaxVisibleItems(10);
        const QRect screen = owner.screen()->availableGeometry();
        for (const bool above : {false, true}) {
            owner.move(screen.left() + 20, above ? screen.bottom() - owner.height() + 1 : screen.top() + 20);
            combo.setGeometry(12, above ? 208 : 12, itemCount > 3 && !compact ? 156 : 336, compact ? 20 : 24);
            owner.show();
            QTest::qWait(30);
            if (replaceView && !above) combo.setView(new QListView);
            combo.showPopup();
            auto* popup = combo.view()->window();
            QTRY_VERIFY(popup->isVisible());
            QTest::qWait(30);
            const QPoint origin = combo.mapToGlobal(QPoint());
            QCOMPARE(popup->x(), origin.x());
            QCOMPARE(popup->width(), combo.width());
            QCOMPARE(above ? popup->geometry().bottom() + 1 : popup->y(),
                     above ? origin.y() : origin.y() + combo.height());
            const QImage control = combo.grab().toImage();
            QCOMPARE(control.pixelColor(control.width() / 2, above ? 0 : control.height() - 1), QColor(Qt::white));
            const QPoint joinedCorner(0, above ? popup->height() - 1 : 0);
            const QPoint outerCorner(0, above ? 0 : popup->height() - 1);
            QVERIFY(popup->mask().contains(joinedCorner));
            QVERIFY(!popup->mask().contains(outerCorner));
            const QString captures = qEnvironmentVariable("PLAYGROUND_COMBO_SCREENSHOTS");
            if (!captures.isEmpty() && !editable) {
                QDir().mkpath(captures);
                const QRect bounds = QRect(owner.mapToGlobal(QPoint()), owner.size()).united(popup->geometry());
                QPixmap capture(bounds.size() * owner.devicePixelRatioF());
                capture.setDevicePixelRatio(owner.devicePixelRatioF());
                capture.fill(Qt::white);
                QPainter painter(&capture);
                painter.drawPixmap(owner.mapToGlobal(QPoint()) - bounds.topLeft(), owner.grab());
                painter.drawPixmap(popup->pos() - bounds.topLeft(), popup->grab());
                painter.end();
                QVERIFY(capture.save(QDir(captures).filePath(QStringLiteral("combo-%1-%2.png")
                    .arg(QString::fromLatin1(QTest::currentDataTag()),
                         above ? QStringLiteral("above") : QStringLiteral("below")))));
            }
            const QImage shell = popup->grab().toImage();
            const QColor border(QStringLiteral("#8badc7"));
            const auto isBorder = [&](const QColor& color) {
                return color.alpha() == 255 && qAbs(color.red() - border.red()) <= 12
                    && qAbs(color.green() - border.green()) <= 12
                    && qAbs(color.blue() - border.blue()) <= 12;
            };
            QVERIFY2(isBorder(shell.pixelColor(0, shell.height() / 2)), "The item view covered the popup's left border.");
            QVERIFY2(isBorder(shell.pixelColor(shell.width() - 1, shell.height() / 2)), "The item view covered the popup's right border.");
            QVERIFY2(isBorder(shell.pixelColor(shell.width() / 2, above ? 0 : shell.height() - 1)), "The item view covered the popup's outer border.");
            if (QGuiApplication::platformName() == QStringLiteral("windows") && !captures.isEmpty()) {
                const QPoint captureOrigin = popup->pos() - popup->screen()->geometry().topLeft();
                const QPixmap native = popup->screen()->grabWindow(0, captureOrigin.x(), captureOrigin.y(), popup->width(), popup->height());
                if (!captures.isEmpty()) QVERIFY(native.save(QDir(captures).filePath(QStringLiteral("native-%1-%2.png")
                    .arg(QString::fromLatin1(QTest::currentDataTag()), above ? QStringLiteral("above") : QStringLiteral("below")))));
                const QImage displayed = native.toImage();
                QVERIFY2(isBorder(displayed.pixelColor(0, displayed.height() / 2)), "The live viewport occluded the popup's left border.");
                QVERIFY2(isBorder(displayed.pixelColor(displayed.width() - 1, displayed.height() / 2)), "The live viewport occluded the popup's right border.");
                QVERIFY2(isBorder(displayed.pixelColor(displayed.width() / 2, above ? 0 : displayed.height() - 1)), "The live viewport occluded the popup's outer border.");
            }
            QTest::keyClick(combo.view(), Qt::Key_Escape);
            QTRY_VERIFY(!popup->isVisible());
            QCOMPARE(combo.currentIndex(), 0);
            combo.showPopup();
            QTRY_VERIFY(popup->isVisible());
            QTest::keyClick(combo.view(), Qt::Key_Down);
            QTest::keyClick(combo.view(), Qt::Key_Return);
            QTRY_VERIFY(!popup->isVisible());
            QCOMPARE(combo.currentIndex(), 1);
            combo.setCurrentIndex(0);
            combo.showPopup();
            QTRY_VERIFY(popup->isVisible());
            popup->resize(popup->width(), popup->height() + 12);
            QTest::qWait(30);
            QVERIFY(popup->mask().contains(QPoint(0, above ? popup->height() - 1 : 0)));
            QVERIFY(!popup->mask().contains(QPoint(0, above ? 0 : popup->height() - 1)));
            combo.hidePopup();
            QCOMPARE(combo.property("popupDirection").toString(), QString());
        }
    }

    void comboPopupInDeviceDock()
    {
        Resources::installResources(*qApp);
        ThemedMainWindow owner;
        owner.resize(600, 420);
        auto* dock = new QDockWidget(QStringLiteral("Device Controls"), &owner);
        auto* controls = new QWidget;
        auto* layout = new QVBoxLayout(controls);
        auto* selector = new QComboBox;
        selector->setMinimumWidth(120);
        selector->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        selector->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        for (int row = 0; row < 20; ++row) selector->addItem(QStringLiteral("Euresys Playlink (Device%1)").arg(row));
        layout->addWidget(selector);
        auto* tree = new QTreeWidget;
        tree->setProperty("treeRole", QStringLiteral("DeviceFeatureTree"));
        tree->setColumnCount(2);
        tree->setHeaderLabels({QStringLiteral("Feature"), QStringLiteral("Value")});
        layout->addWidget(tree);
        auto* item = new QTreeWidgetItem(tree, {QStringLiteral("Pixel format"), QString()});
        auto* editor = new QComboBox;
        editor->addItems({QStringLiteral("Mono8"), QStringLiteral("Mono12"), QStringLiteral("RGB8")});
        tree->setItemWidget(item, 1, editor);
        ThemedDockTitleBar::setupDockWidget(dock, controls);
        owner.addDockWidget(Qt::LeftDockWidgetArea, dock);
        owner.show();
        dock->setFloating(true);
        dock->resize(300, 350);
        dock->move(owner.screen()->availableGeometry().topLeft() + QPoint(30, 30));
        QTest::qWait(50);
        for (auto* combo : {selector, editor}) {
            // A module may transfer an existing selector between control panels.
            combo->setParent(controls);
            if (combo == selector) layout->insertWidget(0, combo);
            else tree->setItemWidget(item, 1, combo);
            combo->show();
            QTest::qWait(30);
            combo->showPopup();
            auto* popup = combo->view()->window();
            QTRY_VERIFY(popup->isVisible());
            QTest::qWait(30);
            QCOMPARE(popup->width(), combo->width());
            QCOMPARE(popup->pos(), combo->mapToGlobal(QPoint(0, combo->height())));
            const QImage shell = popup->grab().toImage();
            QCOMPARE(shell.pixelColor(0, shell.height() / 2), QColor(QStringLiteral("#8badc7")));
            QCOMPARE(shell.pixelColor(shell.width() - 1, shell.height() / 2), QColor(QStringLiteral("#8badc7")));
            QCOMPARE(shell.pixelColor(shell.width() / 2, shell.height() - 1), QColor(QStringLiteral("#8badc7")));
            const QString captures = qEnvironmentVariable("PLAYGROUND_COMBO_SCREENSHOTS");
            if (!captures.isEmpty() && QGuiApplication::platformName() == QStringLiteral("windows")) {
                const QRect bounds = dock->geometry().united(popup->geometry());
                const QPoint origin = bounds.topLeft() - dock->screen()->geometry().topLeft();
                const auto capture = dock->screen()->grabWindow(0, origin.x(), origin.y(), bounds.width(), bounds.height());
                QVERIFY(capture.save(QDir(captures).filePath(combo == selector ? QStringLiteral("device-dock-selector.png")
                                                                                             : QStringLiteral("device-dock-editor.png"))));
            }
            QTest::mouseClick(combo->view()->viewport(), Qt::LeftButton, Qt::NoModifier,
                              combo->view()->visualRect(combo->model()->index(1, 0)).center());
            QTRY_VERIFY(!popup->isVisible());
            QCOMPARE(combo->currentIndex(), 1);
        }
    }

    void nativeApplicationSwitch()
    {
#ifdef Q_OS_MACOS
        if (QGuiApplication::platformName() != "cocoa") QSKIP("Requires native macOS windows");
        Resources::installResources(*qApp);
        QWidget owner;
        owner.setWindowTitle("Resources application-switch check");
        owner.resize(640, 480);
        owner.show();
        owner.raise();
        owner.activateWindow();
        QTRY_COMPARE(qApp->applicationState(), Qt::ApplicationActive);
        ThemedLoadingWidget loading(&owner);
        loading.show();
        QTRY_VERIFY(loading.isVisible());
        QMenu menu(&owner);
        menu.addAction("Application-switch check");
        menu.popup(owner.mapToGlobal(QPoint(20, 20)));
        QTRY_VERIFY(menu.isVisible());
        QCOMPARE(QProcess::execute("/usr/bin/osascript",
                 {"-e", "tell application \"Finder\" to activate"}), 0);
        QTRY_VERIFY(qApp->applicationState() != Qt::ApplicationActive);
        QTRY_VERIFY(!loading.isVisible());
        QVERIFY(!menu.isVisible());
        owner.raise();
        owner.activateWindow();
        QTRY_COMPARE(qApp->applicationState(), Qt::ApplicationActive);
        QTRY_VERIFY(loading.isVisible());
        QVERIFY(!menu.isVisible());
#else
        QSKIP("Native desktop switching is exercised on macOS");
#endif
    }

    void loadingAndMenuLifecycle()
    {
        Resources::installResources(*qApp);
        qApp->applicationStateChanged(Qt::ApplicationActive);
        QWidget owner;
        QPushButton button("Other window");
        owner.show();
        button.show();
        ThemedLoadingWidget loading(&owner);
        // The constructor reads the real state; explicitly start the test active.
        qApp->applicationStateChanged(Qt::ApplicationActive);
        loading.show();
        QVERIFY(loading.isVisible());
        QCOMPARE(loading.windowModality(), Qt::NonModal);
        QVERIFY(loading.windowFlags().testFlag(Qt::WindowStaysOnTopHint));
        QSignalSpy clicks(&button, &QPushButton::clicked);
        QTest::mouseClick(&button, Qt::LeftButton);
        QCOMPARE(clicks.count(), 1);
        QVERIFY(loading.isVisible());

        QMenu menu(&owner);
        menu.addAction("Action");
        menu.popup(QPoint(10, 10));
        QVERIFY(menu.isVisible());
        qApp->applicationStateChanged(Qt::ApplicationInactive);
        QVERIFY(!loading.isVisible());
        QVERIFY(!menu.isVisible());
        qApp->applicationStateChanged(Qt::ApplicationActive);
        QVERIFY(loading.isVisible());
        QVERIFY(!menu.isVisible());

        qApp->applicationStateChanged(Qt::ApplicationInactive);
        loading.close(); // Work may finish while its window is already hidden.
        qApp->applicationStateChanged(Qt::ApplicationActive);
        QVERIFY(!loading.isVisible());
        qApp->applicationStateChanged(Qt::ApplicationInactive);
        loading.show(); // Work may start while another application is active.
        QVERIFY(!loading.isVisible());
        qApp->applicationStateChanged(Qt::ApplicationActive);
        QVERIFY(loading.isVisible());
        qApp->applicationStateChanged(Qt::ApplicationInactive);
        loading.hide();
        qApp->applicationStateChanged(Qt::ApplicationActive);
        QVERIFY(!loading.isVisible());
    }
};

QTEST_MAIN(ResourcesTransientWindowTests)
#include "ResourcesTransientWindowTests.moc"
