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
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QTest>
#include <QTreeWidget>
#include <QVBoxLayout>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

// Check the visible silhouette rather than only the straight border midpoints.
// A quarter circle must have fractional coverage, with no opaque pixels outside
// its arc; logical-pixel binary masks cannot satisfy this at fractional DPR.
static QString comboCornerError(const QImage& image, const QSize& logicalSize, bool above)
{
    const qreal dpr = image.devicePixelRatio();
    const qreal radius = 8.5;
    const int extent = qCeil(9 * dpr);
    for (const bool right : {false, true}) {
        for (int y = 0; y < extent; ++y) {
            for (int x = 0; x < extent; ++x) {
                int covered = 0;
                for (int sy = 0; sy < 8; ++sy) {
                    for (int sx = 0; sx < 8; ++sx) {
                        // Fractional DPR can leave a partial last raster pixel.
                        const qreal px = right
                            ? logicalSize.width() - (image.width() - 1 - x + (sx + 0.5) / 8) / dpr
                            : (x + (sx + 0.5) / 8) / dpr;
                        const qreal py = above ? (y + (sy + 0.5) / 8) / dpr
                            : logicalSize.height() - (image.height() - 1 - y + (sy + 0.5) / 8) / dpr;
                        const qreal dx = px - radius;
                        const qreal dy = py - radius;
                        if (px >= 0 && py >= 0
                            && (dx >= 0 || dy >= 0 || dx * dx + dy * dy <= radius * radius)) ++covered;
                    }
                }
                const int expected = qRound(255.0 * covered / 64);
                const int actual = image.pixelColor(right ? image.width() - 1 - x : x,
                                                    above ? y : image.height() - 1 - y).alpha();
                if (qAbs(actual - expected) > 32) {
                    return QStringLiteral("%1 outer corner at (%2,%3), DPR %4: alpha %5, expected %6")
                        .arg(right ? QStringLiteral("right") : QStringLiteral("left"))
                        .arg(x).arg(y).arg(dpr).arg(actual).arg(expected);
                }
            }
        }
    }
    return {};
}

// Measure both joined side borders, including the control's white connecting
// edge. Checking popup geometry alone misses a one-row loss of border coverage.
static QString comboJoinError(const QImage& image, qreal joinY)
{
    const qreal dpr = image.devicePixelRatio();
    const int extent = qCeil(2 * dpr);
    const int top = qFloor((joinY - 2) * dpr);
    const int bottom = qCeil((joinY + 2) * dpr);
    for (const bool right : {false, true}) {
        const auto ink = [&](int y) {
            int coverage = 0;
            for (int x = 0; x < extent; ++x) {
                const QColor pixel = image.pixelColor(right ? image.width() - 1 - x : x, y);
                coverage += qBound(0, pixel.blue() - pixel.red(), 60);
            }
            return coverage;
        };
        const int reference = qMin(ink(top - 1), ink(bottom));
        if (reference < 25) return QStringLiteral("Missing side border next to the join.");
        for (int y = top; y < bottom; ++y) {
            // Channel quantization can round a 75% fractional row down by one.
            if (ink(y) + 1 < reference * 0.75) {
                return QStringLiteral("%1 join row %2, DPR %3: border coverage %4, reference %5")
                    .arg(right ? QStringLiteral("right") : QStringLiteral("left"))
                    .arg(y).arg(dpr).arg(ink(y)).arg(reference);
            }
        }
    }
    return {};
}

class ResourcesTransientWindowTests : public QObject
{
    Q_OBJECT
private slots:
    void rowTreeCheckIndicatorsAreVisible()
    {
        Resources::installResources(*qApp);
        ThemedTreeWidget tree;
        tree.setInteractionMode(ThemedTreeWidget::InteractionMode::Row);
        auto* item = new QTreeWidgetItem(&tree, {QString()});
        item->setCheckState(0, Qt::Unchecked);
        tree.resize(300, 160);
        tree.show();
        QVERIFY(QTest::qWaitForWindowExposed(&tree));
        QTest::mouseMove(tree.viewport(), QPoint(250, 120));
        struct Probe : QStyledItemDelegate {
            using QStyledItemDelegate::initStyleOption;
        } probe;
        QStyleOptionViewItem option;
        option.initFrom(&tree);
        option.widget = &tree;
        option.rect = tree.visualItemRect(item);
        probe.initStyleOption(&option, tree.indexFromItem(item));
        const QRect indicator = tree.style()->subElementRect(
            QStyle::SE_ItemViewItemCheckIndicator, &option, &tree);
        QVERIFY(!indicator.isEmpty());
        const auto capture = [&] {
            const QImage image = tree.viewport()->grab().toImage();
            const qreal ratio = image.devicePixelRatio();
            return image.copy(QRect(qRound(indicator.x() * ratio), qRound(indicator.y() * ratio),
                                    qRound(indicator.width() * ratio), qRound(indicator.height() * ratio)));
        };
        const QImage unchecked = capture();
        item->setCheckState(0, Qt::Checked);
        const QImage checked = capture();
        item->setCheckState(0, Qt::PartiallyChecked);
        const QImage partial = capture();
        QVERIFY(checked != unchecked);
        QVERIFY(partial != checked);
        QVERIFY(partial != unchecked);
        item->setData(0, Qt::CheckStateRole, QVariant());
        const QImage absent = capture();
        QVERIFY(unchecked != absent);
        QVERIFY(checked != absent);
        QVERIFY(partial != absent);
        item->setCheckState(0, Qt::Checked);
        QTest::mouseClick(tree.viewport(), Qt::LeftButton, Qt::NoModifier, indicator.center());
        QCOMPARE(item->checkState(0), Qt::Unchecked);
    }

    void treeHoverIncludesBranches()
    {
        Resources::installResources(*qApp);
        for (const auto direction : {Qt::LeftToRight, Qt::RightToLeft}) {
            ThemedTreeWidget tree;
            tree.setLayoutDirection(direction);
            tree.setInteractionMode(ThemedTreeWidget::InteractionMode::Row);
            tree.setHeaderLabels({QStringLiteral("Source"), QStringLiteral("Status")});
            auto* root = new QTreeWidgetItem(&tree, {QStringLiteral("Root"), QStringLiteral("2 producers")});
            new QTreeWidgetItem(root, {QStringLiteral("Child.cti")});
            tree.resize(450, 200);
            tree.show();
            QVERIFY(QTest::qWaitForWindowExposed(&tree));
            for (const bool expanded : {false, true}) {
                root->setExpanded(expanded);
                const QRect row = tree.visualItemRect(root);
                const int edge = direction == Qt::LeftToRight ? 1 : tree.viewport()->width() - 2;
                for (const int x : {tree.viewport()->width() / 2, edge + (direction == Qt::LeftToRight ? 9 : -9)}) {
                    QTest::mouseMove(tree.viewport(), QPoint(x, row.center().y()));
                    const QImage image = tree.viewport()->grab().toImage();
                    const qreal ratio = image.devicePixelRatio();
                    QCOMPARE(image.pixelColor(qRound(edge * ratio), qRound(row.center().y() * ratio)),
                             tree.palette().highlight().color());
                }
            }
        }
    }

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
        QTest::addColumn<QString>("context");
        QTest::newRow("preexisting-popup") << false << false << 20 << false << true << QString();
        QTest::newRow("standard") << false << false << 3 << false << false << QString();
        QTest::newRow("single-device") << false << false << 1 << false << false << QString();
        QTest::newRow("single-device-native-phase") << false << false << 1 << false << false << QStringLiteral("native-phase");
        QTest::newRow("standard-editable") << false << true << 3 << false << false << QString();
        QTest::newRow("compact") << true << false << 3 << false << false << QString();
        QTest::newRow("compact-editable") << true << true << 3 << false << false << QString();
        QTest::newRow("device-scroll") << false << false << 20 << false << false << QString();
        QTest::newRow("device-scroll-editable") << false << true << 20 << false << false << QString();
        QTest::newRow("compact-scroll") << true << false << 20 << false << false << QString();
        QTest::newRow("replacement-view") << false << false << 20 << true << false << QString();
        QTest::newRow("script-selector") << false << false << 20 << false << false << QStringLiteral("script");
        QTest::newRow("right-to-left") << false << false << 20 << false << false << QStringLiteral("rtl");
    }

    void comboPopupLifecycle()
    {
        QFETCH(bool, compact);
        QFETCH(bool, editable);
        QFETCH(int, itemCount);
        QFETCH(bool, replaceView);
        QFETCH(bool, installLate);
        QFETCH(QString, context);
        if (!installLate) Resources::installResources(*qApp);
        QWidget owner;
        owner.setAutoFillBackground(true);
        if (context == QStringLiteral("rtl")) owner.setLayoutDirection(Qt::RightToLeft);
        if (compact) owner.setObjectName(QStringLiteral("PluginMarketplaceContent"));
        owner.resize(360, 240);
        QComboBox combo(&owner);
        if (context == QStringLiteral("script")) combo.setObjectName(QStringLiteral("ProcessingScriptComboBox"));
        if (installLate) {
            combo.ensurePolished();
            combo.view()->window()->ensurePolished();
            Resources::installResources(*qApp);
        }
        combo.setEditable(editable);
        for (int row = 0; row < itemCount; ++row) {
            combo.addItem(itemCount == 1 ? QStringLiteral("Basler acA1300-60gm (24070438)")
                                       : itemCount > 3 ? QStringLiteral("Euresys Playlink (Device%1)").arg(row)
                                       : QStringLiteral("1.0.%1").arg(2 - row));
        }
        combo.setMaxVisibleItems(10);
        const QRect screen = owner.screen()->availableGeometry();
        for (const bool above : {false, true}) {
            owner.move(screen.left() + 20, above ? screen.bottom() - owner.height() + 1 : screen.top() + 20);
            combo.setGeometry(12, above ? 208 : 12, itemCount == 1 ? 215 : itemCount > 3 && !compact ? 156 : 336, compact ? 20 : 24);
            owner.show();
            owner.raise();
            owner.activateWindow();
            QTest::qWait(30);
            if (above) {
                // QWidget::move locates a normal window's frame, not its client
                // origin. Keep the control itself on-screen beneath an upward popup.
                const int controlBottom = combo.mapToGlobal(QPoint(0, combo.height())).y();
                owner.move(owner.pos() + QPoint(0, screen.bottom() - 16 - controlBottom));
                QTest::qWait(30);
            }
#ifdef Q_OS_WIN
            if (context == QStringLiteral("native-phase") && QGuiApplication::platformName() == QStringLiteral("windows")) {
                // A native origin on an odd physical pixel is rounded by Qt's
                // integer global geometry. Reproduce frameless-host seam offsets.
                const HWND handle = reinterpret_cast<HWND>(owner.winId());
                RECT nativeRect;
                QVERIFY(GetWindowRect(handle, &nativeRect));
                QVERIFY(SetWindowPos(handle, nullptr, nativeRect.left + 1, nativeRect.top + 1, 0, 0,
                    SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE));
                QTest::qWait(30);
            }
#endif
            if (replaceView && !above) combo.setView(new QListView);
            combo.showPopup();
            auto* popup = combo.view()->window();
            QTRY_VERIFY(popup->isVisible());
            QTest::qWait(30);
            const QPoint origin = combo.mapToGlobal(QPoint());
            QCOMPARE(popup->x(), origin.x());
            QCOMPARE(popup->width(), combo.width());
            QCOMPARE(above ? popup->geometry().bottom() + 1 : popup->y(),
                     above ? origin.y() + 2 : origin.y() + combo.height() - 2);
            const QImage control = combo.grab().toImage();
            QCOMPARE(control.pixelColor(control.width() / 2, above ? 0 : control.height() - 1), QColor(Qt::white));
            QVERIFY(popup->mask().isEmpty());
            const QString captures = qEnvironmentVariable("PLAYGROUND_COMBO_SCREENSHOTS");
            const bool nativeCapture = qEnvironmentVariable("PLAYGROUND_COMBO_CAPTURE_MODE") != QStringLiteral("widget");
            const QRect joinBounds = QRect(origin, combo.size()).united(popup->geometry());
            QPixmap join(joinBounds.size() * combo.devicePixelRatioF());
            join.setDevicePixelRatio(combo.devicePixelRatioF());
            join.fill(Qt::white);
            {
                QPainter painter(&join);
                painter.drawImage(origin - joinBounds.topLeft(), control);
                painter.drawPixmap(popup->pos() - joinBounds.topLeft(), popup->grab());
            }
            const qreal joinY = origin.y() - joinBounds.top() + (above ? 0 : combo.height());
            if (!captures.isEmpty()) {
                QDir().mkpath(captures);
                QVERIFY(join.save(QDir(captures).filePath(QStringLiteral("join-%1-%2.png")
                    .arg(QString::fromLatin1(QTest::currentDataTag()), above ? "above" : "below"))));
            }
            const QString joinError = comboJoinError(join.toImage(), joinY);
            QVERIFY2(joinError.isEmpty(), qPrintable(joinError));
            if (QGuiApplication::platformName() == QStringLiteral("windows") && !captures.isEmpty() && nativeCapture) {
                const QPoint captureOrigin = joinBounds.topLeft() - popup->screen()->geometry().topLeft();
                const QPixmap nativeJoin = popup->screen()->grabWindow(0, captureOrigin.x(), captureOrigin.y(), joinBounds.width(), joinBounds.height());
                QVERIFY(nativeJoin.save(QDir(captures).filePath(QStringLiteral("native-join-%1-%2.png")
                    .arg(QString::fromLatin1(QTest::currentDataTag()), above ? "above" : "below"))));
                const QString nativeJoinError = comboJoinError(nativeJoin.toImage(), joinY);
                QVERIFY2(nativeJoinError.isEmpty(), qPrintable(nativeJoinError));
            }
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
            qInfo() << "Combo raster:" << QGuiApplication::platformName()
                    << shell.size() << "DPR" << shell.devicePixelRatio();
            QVERIFY(!popup->graphicsEffect());
            if (!captures.isEmpty()) {
                QVERIFY(shell.save(QDir(captures).filePath(QStringLiteral("alpha-%1-%2.png")
                    .arg(QString::fromLatin1(QTest::currentDataTag()), above ? QStringLiteral("above") : QStringLiteral("below")))));
            }
            const QString cornerError = comboCornerError(shell, popup->size(), above);
            QVERIFY2(cornerError.isEmpty(), qPrintable(cornerError));
            const QColor border(QStringLiteral("#8badc7"));
            const auto isBorder = [&](const QColor& color, qreal coverage = 1) {
                return qAbs(color.alpha() - qRound(255 * coverage)) <= 8 && qAbs(color.red() - border.red()) <= 12
                    && qAbs(color.green() - border.green()) <= 12
                    && qAbs(color.blue() - border.blue()) <= 12;
            };
            const qreal rightCoverage = qBound(qreal(0), popup->width() * shell.devicePixelRatio() - (shell.width() - 1), qreal(1));
            const qreal outerCoverage = above ? 1 : qBound(qreal(0), popup->height() * shell.devicePixelRatio() - (shell.height() - 1), qreal(1));
            QVERIFY2(isBorder(shell.pixelColor(0, shell.height() / 2)), "The item view covered the popup's left border.");
            QVERIFY2(isBorder(shell.pixelColor(shell.width() - 1, shell.height() / 2), rightCoverage), "The item view covered the popup's right border.");
            QVERIFY2(isBorder(shell.pixelColor(shell.width() / 2, above ? 0 : shell.height() - 1), outerCoverage), "The item view covered the popup's outer border.");
            if (QGuiApplication::platformName() == QStringLiteral("windows") && !captures.isEmpty() && nativeCapture) {
                const QPoint captureOrigin = popup->pos() - popup->screen()->geometry().topLeft();
                const QPixmap native = popup->screen()->grabWindow(0, captureOrigin.x(), captureOrigin.y(), popup->width(), popup->height());
                if (!captures.isEmpty()) QVERIFY(native.save(QDir(captures).filePath(QStringLiteral("native-%1-%2.png")
                    .arg(QString::fromLatin1(QTest::currentDataTag()), above ? QStringLiteral("above") : QStringLiteral("below")))));
                const QImage displayed = native.toImage();
                QVERIFY2(isBorder(displayed.pixelColor(0, displayed.height() / 2)), "The live viewport occluded the popup's left border.");
                if (rightCoverage == 1) QVERIFY2(isBorder(displayed.pixelColor(displayed.width() - 1, displayed.height() / 2)), "The live viewport occluded the popup's right border.");
                if (outerCoverage == 1) QVERIFY2(isBorder(displayed.pixelColor(displayed.width() / 2, above ? 0 : displayed.height() - 1)), "The live viewport occluded the popup's outer border.");
                // Verify desktop composition too: QWidget::grab does not exercise
                // the native window mask or Windows' translucent-window blending.
                combo.hidePopup();
                QTest::qWait(50);
                const QImage backdrop = popup->screen()->grabWindow(0, captureOrigin.x(), captureOrigin.y(),
                    popup->width(), popup->height()).toImage();
                QCOMPARE(displayed.size(), shell.size());
                QCOMPARE(backdrop.size(), shell.size());
                for (const QPoint& point : {QPoint(shell.width() - 1, shell.height() / 2),
                        QPoint(shell.width() / 2, above ? 0 : shell.height() - 1)}) {
                    const QColor front = shell.pixelColor(point), back = backdrop.pixelColor(point), shown = displayed.pixelColor(point);
                    const auto matches = [alpha = front.alphaF()](int actual, int foreground, int background) {
                        return qAbs(actual - qRound(foreground * alpha + background * (1 - alpha))) <= 8;
                    };
                    QVERIFY(matches(shown.red(), front.red(), back.red()) && matches(shown.green(), front.green(), back.green())
                        && matches(shown.blue(), front.blue(), back.blue()));
                }
                const int extent = qCeil(9 * shell.devicePixelRatio());
                for (const bool right : {false, true}) {
                    for (int y = 0; y < extent; ++y) {
                        for (int x = 0; x < extent; ++x) {
                            const QPoint point(right ? shell.width() - 1 - x : x,
                                               above ? y : shell.height() - 1 - y);
                            const QColor foreground = shell.pixelColor(point);
                            const QColor background = backdrop.pixelColor(point);
                            const QColor actual = displayed.pixelColor(point);
                            const qreal alpha = foreground.alphaF();
                            const auto matches = [alpha](int shown, int front, int back) {
                                return qAbs(shown - qRound(front * alpha + back * (1 - alpha))) <= 8;
                            };
                            QVERIFY2(matches(actual.red(), foreground.red(), background.red())
                                && matches(actual.green(), foreground.green(), background.green())
                                && matches(actual.blue(), foreground.blue(), background.blue()),
                                "The native compositor clipped or covered the antialiased corner.");
                        }
                    }
                }
                combo.showPopup();
                QTest::qWait(30);
            }
            QTest::keyClick(combo.view(), Qt::Key_Escape);
            QTRY_VERIFY(!popup->isVisible());
            QCOMPARE(combo.currentIndex(), 0);
            combo.showPopup();
            QTRY_VERIFY(popup->isVisible());
            QTest::keyClick(combo.view(), Qt::Key_Down);
            QTest::keyClick(combo.view(), Qt::Key_Return);
            QTRY_VERIFY(!popup->isVisible());
            QCOMPARE(combo.currentIndex(), qMin(1, itemCount - 1));
            combo.setCurrentIndex(0);
            combo.showPopup();
            QTRY_VERIFY(popup->isVisible());
            popup->resize(popup->width(), popup->height() + 12);
            QTest::qWait(30);
            QVERIFY(popup->mask().isEmpty());
            const QString resizedCornerError = comboCornerError(popup->grab().toImage(), popup->size(), above);
            QVERIFY2(resizedCornerError.isEmpty(), qPrintable(resizedCornerError));
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
        selector->addItems({QStringLiteral("Euresys Playlink (Device0)"), QStringLiteral("acA1300-60gm (4070438)"),
            QStringLiteral("Basler acA1920-40gm (a3e-1c-26)"), QStringLiteral("Euresys Playlink (Device0)"),
            QStringLiteral("Euresys Playlink (Example)"), QStringLiteral("iF-CXP12 (a698005f)"),
            QStringLiteral("Euresys GigE (Ethernet-10)"), QStringLiteral("Euresys GigE (Firewall)"), QStringLiteral("GigE Vision")});
        for (int row = 9; row < 20; ++row) selector->addItem(QStringLiteral("GenTL producer %1").arg(row));
        selector->setMaxVisibleItems(8);
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
            QCOMPARE(popup->pos(), combo->mapToGlobal(QPoint(0, combo->height() - 2)));
            QTest::mouseMove(combo->view()->viewport(), combo->view()->visualRect(combo->model()->index(0, 0)).center());
            QTest::qWait(30);
            const QImage shell = popup->grab().toImage();
            const QString cornerError = comboCornerError(shell, popup->size(), false);
            QVERIFY2(cornerError.isEmpty(), qPrintable(cornerError));
            const auto checkBorder = [](QColor color, qreal coverage) {
                QVERIFY(qAbs(color.alpha() - qRound(255 * coverage)) <= 2);
                color.setAlpha(255);
                QCOMPARE(color.rgba(), QColor(QStringLiteral("#8badc7")).rgba());
            };
            checkBorder(shell.pixelColor(0, shell.height() / 2), 1);
            checkBorder(shell.pixelColor(shell.width() - 1, shell.height() / 2),
                qBound(0.0, popup->width() * shell.devicePixelRatio() - shell.width() + 1, 1.0));
            checkBorder(shell.pixelColor(shell.width() / 2, shell.height() - 1),
                qBound(0.0, popup->height() * shell.devicePixelRatio() - shell.height() + 1, 1.0));
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
