#include "Chrome/ThemedLoadingWidget.h"
#include "Chrome/ThemedDockTitleBar.h"
#include "Chrome/ThemedMainWindow.h"
#include "Chrome/ThemedTreeWidget.h"
#include "Chrome/ThemedMessageBox.h"
#include "Resources.h"

#include <QApplication>
#include <QActionGroup>
#include <QAbstractItemView>
#include <QComboBox>
#include <QDir>
#include <QGraphicsDropShadowEffect>
#include <QHeaderView>
#include <QDockWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QListView>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QProcess>
#include <QScreen>
#include <QSignalSpy>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QSlider>
#include <QStyleOptionSlider>
#include <QDateEdit>
#include <QTimeEdit>
#include <QTabWidget>
#include <QTabBar>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QTest>
#include <QTreeWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

static QRect darkTextBounds(const QImage& image, const QRect& area, int threshold = 100)
{
    QRect result;
    for (int y = area.top(); y <= area.bottom(); ++y) {
        for (int x = area.left(); x <= area.right(); ++x) {
            const QColor pixel = image.pixelColor(x, y);
            if (pixel.alpha() > 200 && pixel.red() < threshold - 30 && pixel.green() < threshold - 15 && pixel.blue() < threshold)
                result = result.united(QRect(x, y, 1, 1));
        }
    }
    return result;
}

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
                coverage += qBound(0, 255 - qGray(pixel.rgb()), 60);
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
    void messageIconBalancesShortAndWrappedText()
    {
        Resources::installResources(*qApp);
        for (const auto& text : {QStringLiteral("Discard unsaved changes?"),
                QStringLiteral("The selected operation contains unsaved changes. Review the current settings before continuing. ").repeated(4)}) {
            ThemedMessageBox dialog(ThemedMessageBox::Information, "Confirmation", text, nullptr, true);
            dialog.resize(460, 220);
            dialog.show();
            QApplication::processEvents();
            const auto* icon = dialog.findChild<QLabel*>("ThemedMessageBoxIcon");
            const auto* message = dialog.findChild<QLabel*>("ThemedMessageBoxText");
            QVERIFY(icon && message);
            QVERIFY(qAbs(icon->geometry().center().y() - message->geometry().center().y()) <= 1);
        }
    }

    void menuShellCorners_data()
    {
        QTest::addColumn<bool>("rtl");
        QTest::addColumn<int>("selection");
        QTest::newRow("plain") << false << -1;
        QTest::newRow("first-selected") << false << 0;
        QTest::newRow("last-selected") << false << 2;
        QTest::newRow("rtl") << true << -1;
    }

    void menuShellCorners()
    {
        QFETCH(bool, rtl);
        QFETCH(int, selection);
        Resources::installResources(*qApp);
        QWidget owner;
        owner.resize(500, 300);
        owner.show();
        QMenu menu(&owner);
        menu.setLayoutDirection(rtl ? Qt::RightToLeft : Qt::LeftToRight);
        menu.addAction(QStringLiteral("Open image"));
        menu.addSeparator();
        menu.addAction(QStringLiteral("Live analysis"))->setCheckable(true);
        menu.actions().last()->setChecked(true);
        for (int opening = 0; opening != 2; ++opening) {
            menu.popup(owner.mapToGlobal(QPoint(20, 20)));
            QTRY_VERIFY(menu.isVisible());
            if (opening == 1) menu.resize(menu.width() + 37, menu.height() + 11);
            if (selection >= 0) menu.setActiveAction(menu.actions()[selection]);
            QCoreApplication::processEvents();
            QVERIFY(menu.mask().isEmpty());
            QVERIFY(menu.graphicsEffect()->property("resourcesMenuShadow").toBool());
            const QImage image = menu.grab().toImage();
            QCOMPARE(menu.graphicsEffect()->boundingRect(), QRectF(menu.rect()));
            const int extent = qCeil(8 * image.devicePixelRatio());
            const int gutter = qRound(12 * image.devicePixelRatio());
            QVERIFY(!qApp->styleSheet().contains(QStringLiteral("@MENU_SHADOW_GUTTER@")));
            QCOMPARE(qobject_cast<QGraphicsDropShadowEffect*>(menu.graphicsEffect())->blurRadius(), qreal(12));
            QVERIFY(image.pixelColor(gutter - 1, image.height() / 2).alpha() <= 36);
            QVERIFY(image.pixelColor(gutter + 1, image.height() / 2).alpha() > 180);
            QVERIFY(!menu.actionAt(QPoint(2, menu.height() / 2)));
            const int shadowAlpha = image.pixelColor(image.width() / 2, image.height() - gutter + 2).alpha();
            QVERIFY2(shadowAlpha > 0 && shadowAlpha <= 36, "Missing or overly dark external menu shadow.");
            for (const bool bottom : {false, true}) {
                for (const bool right : {false, true}) {
                    int partial = 0;
                    QCOMPARE(image.pixelColor(right ? image.width() - 1 : 0,
                                              bottom ? image.height() - 1 : 0).alpha(), 0);
                    for (int y = 0; y < extent; ++y) {
                        for (int x = 0; x < extent; ++x) {
                            const QColor pixel = image.pixelColor(right ? image.width() - 1 - gutter - x : gutter + x,
                                                                  bottom ? image.height() - 1 - gutter - y : gutter + y);
                            if (pixel.alpha() > 0 && pixel.alpha() < 255) ++partial;
                            if (pixel.alpha() > 36) {
                                // Fractional border coverage composites over at most 36/255 shadow alpha.
                                const int minimum = 217 - (217 - 32) * 36 / pixel.alpha() - 8;
                                QVERIFY2(pixel.red() >= minimum && pixel.green() >= minimum && pixel.blue() >= minimum,
                                         "A dark widget-shadow wedge contaminated the menu corner.");
                            }
                        }
                    }
                    QVERIFY2(partial > 0, "Menu corner lost its antialiased alpha coverage.");
                }
            }
            const QString captures = qEnvironmentVariable("RESOURCES_MENU_SCREENSHOTS");
            if (!captures.isEmpty()) {
                QVERIFY(QDir().mkpath(captures));
                QVERIFY(image.save(QDir(captures).filePath(QStringLiteral("%1-%2.png")
                    .arg(QString::fromLatin1(QTest::currentDataTag())).arg(opening))));
            }
            menu.close();
        }
    }

    void menuStyleAndKeyboardLifecycle_data()
    {
        QTest::addColumn<bool>("rtl");
        QTest::newRow("ltr") << false;
        QTest::newRow("rtl") << true;
    }

    void menuStyleAndKeyboardLifecycle()
    {
        QFETCH(bool, rtl);
        Resources::installResources(*qApp);
        QWidget owner;
        owner.resize(500, 300);
        owner.show();
        QMenu menu(&owner);
        menu.setLayoutDirection(rtl ? Qt::RightToLeft : Qt::LeftToRight);
        auto* submenu = menu.addMenu(QStringLiteral("Analysis"));
        QAction* action = submenu->addAction(QStringLiteral("Measure"));
        QSignalSpy triggered(action, &QAction::triggered);
        menu.ensurePolished();
        // Model the effect installed by a native style after the Resources polish filter.
        menu.setGraphicsEffect(new QGraphicsDropShadowEffect(&menu));
        menu.popup(owner.mapToGlobal(QPoint(20, 20)));
        QTRY_VERIFY(menu.isVisible());
        QVERIFY(menu.graphicsEffect()->property("resourcesMenuShadow").toBool());
        const QRect geometry = menu.geometry();
        menu.setGraphicsEffect(new QGraphicsDropShadowEffect(&menu));
        QEvent styleChange(QEvent::StyleChange);
        QApplication::sendEvent(&menu, &styleChange);
        QVERIFY(menu.graphicsEffect()->property("resourcesMenuShadow").toBool());
        QCOMPARE(menu.geometry(), geometry);
        QVERIFY(menu.isVisible());
        menu.setActiveAction(submenu->menuAction());
        QTest::keyClick(&menu, rtl ? Qt::Key_Left : Qt::Key_Right);
        QTRY_VERIFY(submenu->isVisible());
        QVERIFY(submenu->mask().isEmpty());
        QVERIFY(submenu->graphicsEffect()->property("resourcesMenuShadow").toBool());
        const int parentActionY = menu.mapToGlobal(menu.actionGeometry(submenu->menuAction()).topLeft()).y();
        const int childActionY = submenu->mapToGlobal(submenu->actionGeometry(action).topLeft()).y();
        QVERIFY2(qAbs(parentActionY - childActionY) <= 1, "Shadow gutter shifted the submenu's action anchor.");
        const QString captures = qEnvironmentVariable("RESOURCES_MENU_SCREENSHOTS");
        if (!captures.isEmpty()) {
            QVERIFY(QDir().mkpath(captures));
            QVERIFY(submenu->grab().save(QDir(captures).filePath(QStringLiteral("%1-submenu.png")
                .arg(QString::fromLatin1(QTest::currentDataTag())))));
        }
        submenu->setActiveAction(action);
        QTest::keyClick(submenu, Qt::Key_Return);
        QCOMPARE(triggered.count(), 1);
        QTRY_VERIFY(!menu.isVisible());
        QTRY_VERIFY(!submenu->isVisible());
        menu.popup(owner.mapToGlobal(QPoint(20, 20)));
        QTRY_VERIFY(menu.isVisible());
        QTest::mouseClick(&menu, Qt::LeftButton, Qt::NoModifier, QPoint(2, 2));
        QTRY_VERIFY(!menu.isVisible());
        QCOMPARE(triggered.count(), 1);
    }

    void menuCheckIndicatorsKeepToolbarIcons()
    {
        Resources::installResources(*qApp);
        QWidget owner;
        owner.resize(400, 250);
        owner.show();
        QMenu menu(&owner);
        const QIcon icon(QStringLiteral(":/Resources/icon/image-48.png"));
        QVERIFY(!icon.isNull());
        auto* action = menu.addAction(icon, QStringLiteral("Checkable action"));
        action->setIconVisibleInMenu(true);
        const qint64 key = action->icon().cacheKey();
        action->setCheckable(true);
        action->setChecked(true);
        auto* command = menu.addAction(icon, QStringLiteral("Normal command"));
        command->setIconVisibleInMenu(true);
        menu.popup(owner.mapToGlobal(QPoint(20, 20)));
        QTRY_VERIFY(menu.isVisible());
        QVERIFY(!action->isIconVisibleInMenu());
        QCOMPARE(action->icon().cacheKey(), key);
        QVERIFY(command->isIconVisibleInMenu());
        QSignalSpy triggered(action, &QAction::triggered);
        menu.setActiveAction(action);
        QTest::keyClick(&menu, Qt::Key_Return);
        QCOMPARE(triggered.count(), 1);
        QVERIFY(!action->isChecked());
        QTRY_VERIFY(!menu.isVisible());
        action->setCheckable(false);
        QVERIFY(action->isIconVisibleInMenu());
        action->setIconVisibleInMenu(false);
        action->setCheckable(true);
        action->setCheckable(false);
        QVERIFY(!action->isIconVisibleInMenu());
        QCOMPARE(action->icon().cacheKey(), key);
        menu.close();
    }

    void iconMenusRetainExclusiveActionGlyphs()
    {
        Resources::installResources(*qApp);
        QMenu menu;
        menu.setObjectName(QStringLiteral("EngineModeMenu"));
        menu.setProperty("menuPresentation", QStringLiteral("icons"));
        QActionGroup group(&menu);
        group.setExclusive(true);
        auto* image = menu.addAction(QIcon(QStringLiteral(":/Resources/icon/image-view-48.png")), QStringLiteral("Image"));
        auto* surface = menu.addAction(QIcon(QStringLiteral(":/Resources/icon/orthogonal-view-48.png")), QStringLiteral("Surface"));
        for (auto* action : {image, surface}) {
            QVERIFY(!action->icon().isNull());
            action->setCheckable(true);
            group.addAction(action);
        }
        image->setChecked(true);
        menu.popup(QPoint(60, 60));
        QTRY_VERIFY(menu.isVisible());
        QVERIFY(image->isIconVisibleInMenu());
        QVERIFY(surface->isIconVisibleInMenu());
        QSignalSpy triggered(surface, &QAction::triggered);
        menu.setActiveAction(surface);
        QTest::keyClick(&menu, Qt::Key_Return);
        QCOMPARE(triggered.count(), 1);
        QVERIFY(surface->isChecked());
        QVERIFY(!image->isChecked());
        QTRY_VERIFY(!menu.isVisible());
    }

    void menuShadowScreenEdges()
    {
        Resources::installResources(*qApp);
        const QRect bounds = QGuiApplication::primaryScreen()->availableGeometry();
        const QPoint oldCursor = QCursor::pos();
        for (bool rtl : {false, true}) {
            for (const QPoint anchor : {bounds.topLeft(), bounds.topRight(), bounds.bottomLeft(), bounds.bottomRight()}) {
                QMenu menu;
                menu.setLayoutDirection(rtl ? Qt::RightToLeft : Qt::LeftToRight);
                auto* submenu = menu.addMenu(QStringLiteral("Analysis"));
                auto* action = submenu->addAction(QStringLiteral("Measure"));
                QSignalSpy triggered(action, &QAction::triggered);
                QCursor::setPos(anchor);
                QCoreApplication::processEvents();
                menu.popup(anchor);
                QTRY_VERIFY(menu.isVisible());
                const QRect shell = menu.geometry().adjusted(12, 12, -12, -12);
                QVERIFY(bounds.contains(shell));
                const int tolerance = menu.style()->pixelMetric(QStyle::PM_MenuDesktopFrameWidth, nullptr, &menu) + 2;
                QVERIFY2(qMin(qAbs(shell.left() - anchor.x()), qAbs(shell.right() + 1 - anchor.x())) <= tolerance,
                         qPrintable(QStringLiteral("Horizontal shell/anchor gap: %1..%2 / %3")
                                        .arg(shell.left()).arg(shell.right()).arg(anchor.x())));
                QVERIFY2(qMin(qAbs(shell.top() - anchor.y()), qAbs(shell.bottom() + 1 - anchor.y())) <= tolerance,
                         qPrintable(QStringLiteral("Vertical shell/anchor gap: %1..%2 / %3")
                                        .arg(shell.top()).arg(shell.bottom()).arg(anchor.y())));
                menu.setActiveAction(submenu->menuAction());
                QTest::keyClick(&menu, rtl ? Qt::Key_Left : Qt::Key_Right);
                QTRY_VERIFY(submenu->isVisible());
                QVERIFY(bounds.contains(submenu->geometry().adjusted(12, 12, -12, -12)));
                QTest::mouseClick(submenu, Qt::LeftButton, Qt::NoModifier,
                                  submenu->actionGeometry(action).center());
                QCOMPARE(triggered.count(), 1);
                QTRY_VERIFY(!menu.isVisible());
            }
        }
        QCursor::setPos(oldCursor);
    }

    void titleBarMenuTextHitArea()
    {
        Resources::installResources(*qApp);
        for (bool maximized : {false, true}) {
            ThemedMainWindow window;
            window.setWindowTitle(QStringLiteral("Menu hit test"));
            window.resize(720, 360);
            auto* bar = window.themedMenuBar();
            auto* menu = bar->addMenu(QStringLiteral("System"));
            menu->addAction(QStringLiteral("Settings"));
            if (maximized) window.showMaximized(); else window.show();
            QVERIFY(QTest::qWaitForWindowExposed(&window));
            QCoreApplication::processEvents();
            const QRect action = bar->actionGeometry(menu->menuAction());
            const QImage image = bar->grab().toImage();
            const qreal dpr = image.devicePixelRatio();
            QRect ink;
            for (int y = 0; y < image.height(); ++y) {
                for (int x = qCeil((action.left() + 4) * dpr);
                     x < qFloor((action.right() - 4) * dpr); ++x) {
                    const QColor color = image.pixelColor(x, y);
                    if (color.alpha() > 200 && qGray(color.rgb()) < 150)
                        ink |= QRect(QPoint(qFloor(x / dpr), qFloor(y / dpr)), QSize(1, 1));
                }
            }
            qInfo() << "Menu action / text" << action << ink << "bar" << bar->geometry();
            QVERIFY(!ink.isEmpty());
            QVERIFY2(action.contains(ink), "Painted menu text extends beyond its clickable action");
            for (const QPoint point : {ink.center(), QPoint(ink.center().x(), ink.bottom())}) {
                QCOMPARE(bar->actionAt(point), menu->menuAction());
                QTest::mouseClick(bar, Qt::LeftButton, Qt::NoModifier, point);
                QTRY_VERIFY(menu->isVisible());
                menu->close();
            }
        }
    }

    void menuBarShellAnchor()
    {
        Resources::installResources(*qApp);
        for (bool rtl : {false, true}) {
            QWidget window;
            window.setLayoutDirection(rtl ? Qt::RightToLeft : Qt::LeftToRight);
            auto* layout = new QVBoxLayout(&window);
            auto* bar = new QMenuBar;
            bar->setNativeMenuBar(false);
            layout->setMenuBar(bar);
            auto* menu = bar->addMenu(QStringLiteral("Analysis"));
            menu->addAction(QStringLiteral("Measure"));
            const QRect bounds = QGuiApplication::primaryScreen()->availableGeometry();
            const int width = qMin(500, bounds.width() - 40);
            window.resize(width, 180);
            window.move(bounds.center() - QPoint(width / 2, 90));
            window.show();
            QVERIFY(QTest::qWaitForWindowExposed(&window));
            const QRect action = bar->actionGeometry(menu->menuAction());
            QTest::mouseClick(bar, Qt::LeftButton, Qt::NoModifier, action.center());
            QTRY_VERIFY(menu->isVisible());
            const QRect shell = menu->geometry().adjusted(12, 12, -12, -12);
            const int edge = rtl ? shell.right() + 1 : shell.left();
            const int anchor = bar->mapToGlobal(rtl ? action.topRight() + QPoint(1, 0) : action.topLeft()).x();
            QVERIFY2(qAbs(edge - anchor) <= 3, qPrintable(QStringLiteral("Menu-bar shell/anchor: %1 / %2")
                                                             .arg(edge).arg(anchor)));
            menu->close();
        }
    }

    void horizontalSliderCentersInLabeledInputRow()
    {
        Resources::installResources(*qApp);
        QWidget panel;
        auto* form = new QFormLayout(&panel);
        auto* label = new QLabel(QStringLiteral("Threshold"));
        auto* slider = new QSlider(Qt::Horizontal);
        form->addRow(label, slider);
        panel.resize(360, 100);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        QVERIFY(qAbs(label->geometry().center().y() - slider->geometry().center().y()) <= 1);
        for (const int value : {0, 50, 100}) {
            slider->setRange(0, 100);
            slider->setValue(value);
            QStyleOptionSlider option;
            option.initFrom(slider);
            option.orientation = Qt::Horizontal;
            option.minimum = 0;
            option.maximum = 100;
            option.sliderPosition = value;
            option.sliderValue = value;
            const QRect groove = slider->style()->subControlRect(QStyle::CC_Slider, &option,
                QStyle::SC_SliderGroove, slider);
            const QRect handle = slider->style()->subControlRect(QStyle::CC_Slider, &option,
                QStyle::SC_SliderHandle, slider);
            QVERIFY(qAbs(groove.center().y() - slider->rect().center().y()) <= 1);
            QVERIFY(qAbs(handle.center().y() - groove.center().y()) <= 1);
        }
    }

    void exclusiveMenuIndicatorsRemainRadioButtons()
    {
        Resources::installResources(*qApp);
        QMenu menu;
        QActionGroup group(&menu);
        auto* first = menu.addAction(QStringLiteral("First mode"));
        auto* second = menu.addAction(QStringLiteral("Second mode"));
        for (auto* action : {first, second}) {
            action->setCheckable(true);
            group.addAction(action);
        }
        first->setChecked(true);
        auto* toggle = menu.addAction(QStringLiteral("Independent option"));
        toggle->setCheckable(true);
        menu.popup(QPoint(180, 180));
        QTRY_VERIFY(menu.isVisible());
        const auto capture = [&] {
            QCoreApplication::processEvents();
            return menu.grab().toImage();
        };
        const QImage exclusive = capture();
        const qreal dpr = exclusive.devicePixelRatio();
        const QRect firstRow = menu.actionGeometry(first);
        const auto indicatorOutline = [&](const QRect& row) {
            QRect result;
            for (int y = qFloor(row.top() * dpr); y < qCeil((row.bottom() + 1) * dpr); ++y) {
                for (int x = qFloor(row.left() * dpr);
                     x < qCeil((row.left() + 35) * dpr) && x < exclusive.width(); ++x) {
                    const QColor pixel = exclusive.pixelColor(x, y);
                    if (pixel.red() > 180 && pixel.red() < 249
                        && qAbs(pixel.red() - pixel.green()) <= 2 && qAbs(pixel.green() - pixel.blue()) <= 2)
                        result = result.united(QRect(x, y, 1, 1));
                }
            }
            return result;
        };
        const QRect outline = indicatorOutline(firstRow);
        QVERIFY(!outline.isEmpty());
        // The supplied 48px radio has a 40px visible circle inside its 18px menu slot.
        constexpr qreal radioInkSize = 18.0 * 40.0 / 48.0;
        QVERIFY2(qAbs(outline.width() - qRound(radioInkSize * dpr)) <= 1,
                 qPrintable(QStringLiteral("Radio outline width %1 at DPR %2").arg(outline.width()).arg(dpr)));
        QVERIFY(qAbs(outline.height() - qRound(radioInkSize * dpr)) <= 1);
        for (const QPoint corner : {outline.topLeft(), outline.topRight(), outline.bottomLeft(), outline.bottomRight()}) {
            const QColor pixel = exclusive.pixelColor(corner);
            QVERIFY(pixel.red() > 245 && pixel.green() > 245 && pixel.blue() > 245);
        }
        const QRect toggleRow = menu.actionGeometry(toggle);
        toggle->setChecked(true);
        const QImage checkedMenu = capture();
        QRect checkInk;
        for (int y = qFloor(toggleRow.top() * dpr); y < qCeil((toggleRow.bottom() + 1) * dpr); ++y)
            for (int x = qFloor(toggleRow.left() * dpr); x < qCeil((toggleRow.left() + 28) * dpr); ++x) {
                const QColor pixel = checkedMenu.pixelColor(x, y);
                if (pixel.alpha() > 128 && qGray(pixel.rgb()) < 110)
                    checkInk = checkInk.united(QRect(x, y, 1, 1));
            }
        QVERIFY(!checkInk.isEmpty());
        QVERIFY(checkInk.width() <= qCeil(16 * dpr));
        QVERIFY(checkInk.height() <= qCeil(16 * dpr));
        toggle->setChecked(false);
        const QString captures = qEnvironmentVariable("RESOURCES_MENU_SCREENSHOTS");
        if (!captures.isEmpty()) {
            QVERIFY(QDir().mkpath(captures));
            QVERIFY(exclusive.save(QDir(captures).filePath(QStringLiteral("exclusive-menu-indicators.png"))));
        }
        group.removeAction(first);
        const QImage independent = capture();
        QVERIFY(exclusive != independent);
        group.addAction(first);
        first->setChecked(true);
        menu.setActiveAction(second);
        QTest::keyClick(&menu, Qt::Key_Return);
        QVERIFY(second->isChecked());
        QVERIFY(!first->isChecked());
        QVERIFY(!toggle->isChecked());
    }

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

    void diagnosticsDividersStayInsideColumns()
    {
        Resources::installResources(*qApp);
        QTableWidget table(3, 2);
        table.setObjectName(QStringLiteral("GraphicsSettingsPresentationStatsTable"));
        table.setHorizontalHeaderLabels({QStringLiteral("Counter"), QStringLiteral("Value")});
        table.verticalHeader()->hide();
        table.horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table.setSelectionMode(QAbstractItemView::NoSelection);
        for (int row = 0; row < table.rowCount(); ++row)
            for (int column = 0; column < table.columnCount(); ++column)
                table.setItem(row, column, new QTableWidgetItem);
        for (int width : {320, 321, 480}) {
            table.resize(width, 180);
            table.show();
            QVERIFY(QTest::qWaitForWindowExposed(&table));
            QCoreApplication::processEvents();
            auto* header = table.horizontalHeader();
            QVERIFY(!table.showGrid());
            const QImage headerImage = header->grab().toImage();
            bool bubbleBorderVisible = false;
            for (int y = 0; y < qCeil(2 * headerImage.devicePixelRatio()); ++y) {
                const QColor pixel = headerImage.pixelColor(headerImage.width() / 4, y);
                bubbleBorderVisible |= pixel.alpha() > 128 && qGray(pixel.rgb()) < 230;
            }
            QVERIFY2(bubbleBorderVisible, "The shared bubble header outline must remain visible");
            QCOMPARE(header->sectionViewportPosition(1), table.columnViewportPosition(1));
            const QImage image = table.viewport()->grab().toImage();
            const qreal dpr = image.devicePixelRatio();
            const auto hasDivider = [&](const QImage& pixels, int logicalX, int logicalY) {
                const int y = qRound(logicalY * dpr);
                for (int x = qFloor(logicalX * dpr) - 1;
                     x <= qCeil((logicalX + 1) * dpr); ++x) {
                    if (x < 0 || x >= image.width()) continue;
                    const QColor pixel = pixels.pixelColor(x, y);
                    if (pixel.alpha() > 128 && qGray(pixel.rgb()) < 250)
                        return true;
                }
                return false;
            };
            const QRect first = table.visualItemRect(table.item(0, 0));
            const QRect last = table.visualItemRect(table.item(0, 1));
            QVERIFY(hasDivider(image, first.right(), first.top() + 4));
            QVERIFY(!hasDivider(image, last.right(), last.top() + 4));
            // Repaint each cell independently into the existing backing image. A full grab
            // would repaint both columns and conceal a divider drawn by the wrong cell.
            for (int column : {0, 1}) {
                const QRect dirty(table.columnViewportPosition(column), table.rowViewportPosition(0),
                                  table.columnWidth(column), table.rowHeight(0));
                QImage partial = image;
                QPainter painter(&partial);
                painter.fillRect(dirty, Qt::white);
                table.viewport()->render(&painter, dirty.topLeft(), QRegion(dirty));
                painter.end();
                QVERIFY(hasDivider(partial, first.right(), first.top() + 4));
                QVERIFY(!hasDivider(partial, last.right(), last.top() + 4));
            }
            const QImage whole = table.grab().toImage();
            const int dividerX = table.viewport()->mapTo(&table, first.topRight()).x();
            const auto dividerCenter = [&](int logicalY) {
                qreal weightedX = 0;
                int weight = 0;
                for (int x = qFloor((dividerX - 2) * dpr);
                     x <= qCeil((dividerX + 3) * dpr); ++x) {
                    const QColor pixel = whole.pixelColor(x, qRound(logicalY * dpr));
                    const int ink = qMax(0, 255 - qGray(pixel.rgb()));
                    weightedX += x * ink;
                    weight += ink;
                }
                return weight ? weightedX / weight : qreal(-1);
            };
            const qreal headerLine = dividerCenter(header->geometry().center().y());
            const qreal bodyLine = dividerCenter(table.viewport()->mapTo(
                &table, QPoint(0, first.top() + 4)).y());
            QVERIFY(headerLine >= 0);
            QVERIFY(bodyLine >= 0);
            QVERIFY2(qAbs(headerLine - bodyLine) <= 0.5,
                     qPrintable(QStringLiteral("Header/body divider raster: %1 / %2")
                                    .arg(headerLine).arg(bodyLine)));
        }
        QTableWidget ordinary(1, 2);
        ordinary.ensurePolished();
        QVERIFY(ordinary.showGrid());
    }

    void rowTableCheckIndicatorsAreVisible()
    {
        Resources::installResources(*qApp);
        QTableWidget table(1, 1);
        table.setProperty("interactionMode", QStringLiteral("row"));
        auto* item = new QTableWidgetItem;
        item->setCheckState(Qt::Unchecked);
        table.setItem(0, 0, item);
        table.resize(320, 160);
        table.show();
        QVERIFY(QTest::qWaitForWindowExposed(&table));
        QTest::mouseMove(table.viewport(), QPoint(260, 120));
        struct Probe : QStyledItemDelegate { using QStyledItemDelegate::initStyleOption; } probe;
        QStyleOptionViewItem option;
        option.initFrom(&table);
        option.widget = &table;
        option.rect = table.visualItemRect(item);
        probe.initStyleOption(&option, table.model()->index(0, 0));
        const QRect indicator = table.style()->subElementRect(
            QStyle::SE_ItemViewItemCheckIndicator, &option, &table);
        QVERIFY(indicator.width() >= 32);
        QVERIFY(indicator.height() >= 18);
        const auto capture = [&] {
            QCoreApplication::processEvents();
            const QImage image = table.viewport()->grab().toImage();
            const qreal dpr = image.devicePixelRatio();
            return image.copy(QRect(qRound(indicator.x() * dpr), qRound(indicator.y() * dpr),
                                    qRound(indicator.width() * dpr), qRound(indicator.height() * dpr)));
        };
        const QImage unchecked = capture();
        item->setCheckState(Qt::Checked);
        const QImage checked = capture();
        item->setCheckState(Qt::PartiallyChecked);
        const QImage partial = capture();
        item->setData(Qt::CheckStateRole, QVariant());
        const QImage absent = capture();
        QVERIFY(unchecked != checked);
        QVERIFY(unchecked != partial);
        QVERIFY(checked != partial);
        QVERIFY(unchecked != absent);
        QVERIFY(checked != absent);
        QVERIFY(partial != absent);
        item->setCheckState(Qt::Checked);
        QTest::mouseClick(table.viewport(), Qt::LeftButton, Qt::NoModifier, indicator.center());
        QCOMPARE(item->checkState(), Qt::Unchecked);
    }

    void treeHoverPreservesIndentation()
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
                             tree.palette().base().color());
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

    void singleLineFieldsShareTextInsets()
    {
        Resources::installResources(*qApp);
        QWidget owner;
        owner.setLayoutDirection(Qt::LeftToRight);
        owner.setFocusPolicy(Qt::StrongFocus);
        auto* layout = new QVBoxLayout(&owner);
        auto* plain = new QLineEdit("MMMMMMMM 120");
        auto* integer = new QSpinBox; integer->setRange(0, 1000); integer->setPrefix("MMMMMMMM "); integer->setValue(120);
        auto* number = new QDoubleSpinBox; number->setRange(0, 1000); number->setPrefix("MMMMMMMM "); number->setValue(120);
        auto* combo = new QComboBox; combo->addItem("MMMMMMMM 120");
        auto* editable = new QComboBox; editable->setEditable(true); editable->addItem("MMMMMMMM 120");
        auto* date = new QDateEdit(QDate(2026, 10, 19)); date->setDisplayFormat("'MMMMMMMM' yyyy-MM-dd");
        auto* time = new QTimeEdit(QTime(12, 34)); time->setDisplayFormat("'MMMMMMMM' HH:mm:ss");
        const QList<QWidget*> fields{plain, integer, number, combo, editable, date, time};
        for (auto* field : fields) { field->setFixedSize(280, 24); layout->addWidget(field); }
        owner.show(); QVERIFY(QTest::qWaitForWindowExposed(&owner)); owner.setFocus();
        for (auto* editor : owner.findChildren<QLineEdit*>()) editor->deselect();
        QCoreApplication::processEvents();
        int reference = -1;
        QStringList differences;
        for (auto* field : fields) {
            const QImage image = field->grab().toImage(); const qreal dpr = image.devicePixelRatio();
            const QRect scan(qRound(2 * dpr), qRound(3 * dpr), qRound(250 * dpr), qRound(18 * dpr));
            const QRect text = darkTextBounds(image, scan.intersected(image.rect()), 160);
            QVERIFY(!text.isEmpty());
            const int start = text.left();
            if (reference < 0) reference = start;
            if (qAbs(start - reference) > qCeil(dpr)) differences.append(QString("%1 inset %2 vs LineEdit %3 (DPR %4)")
                .arg(field->metaObject()->className()).arg(start).arg(reference).arg(dpr));
        }
        QVERIFY2(differences.isEmpty(), qPrintable(differences.join("; ")));
    }

    void tabPaneDividerSpansPastTabLabels()
    {
        Resources::installResources(*qApp);
        QTabWidget tabs; tabs.addTab(new QWidget, "First"); tabs.addTab(new QWidget, "Second");
        tabs.resize(440, 180); tabs.show(); QVERIFY(QTest::qWaitForWindowExposed(&tabs));
        const QImage image = tabs.grab().toImage(); const qreal dpr = image.devicePixelRatio();
        const int baseline = tabs.tabBar()->geometry().bottom();
        bool continuous = false;
        for (int y = qRound((baseline - 2) * dpr); y <= qRound((baseline + 3) * dpr); ++y) {
            int border = 0, total = 0;
            for (int x = qRound(4 * dpr); x < image.width() - qRound(4 * dpr); ++x) {
                const QColor pixel = image.pixelColor(x, y); const int gray = qGray(pixel.rgb());
                border += gray > 170 && gray < 235; ++total;
            }
            continuous |= border > total * 0.95;
        }
        QVERIFY2(continuous, "Tab header divider does not span the full content width.");
    }

    void editableComboTextInsets()
    {
        Resources::installResources(*qApp);
        for (bool compact : {false, true}) {
            for (bool rtl : {false, true}) {
                QWidget owner;
                if (compact) owner.setObjectName(QStringLiteral("PluginMarketplaceContent"));
                owner.setLayoutDirection(rtl ? Qt::RightToLeft : Qt::LeftToRight);
                QComboBox plain(&owner), editable(&owner);
                plain.addItem(QStringLiteral("MMMMMMMM"));
                editable.addItem(QStringLiteral("MMMMMMMM"));
                editable.setEditable(true);
                const int height = compact ? 20 : 24;
                plain.setGeometry(10, 10, 280, height);
                editable.setGeometry(10, 50, 280, height);
                owner.resize(320, 110);owner.show();
                QVERIFY(QTest::qWaitForWindowExposed(&owner));
                editable.lineEdit()->deselect();editable.clearFocus();plain.clearFocus();
                QCoreApplication::processEvents();
                const QImage a = plain.grab().toImage(), b = editable.grab().toImage();
                const qreal dpr = a.devicePixelRatio();
                const QRect scan(qRound((rtl ? 24 : 2) * dpr), qRound(2 * dpr),
                                 qRound(252 * dpr), qRound((height - 4) * dpr));
                const QRect plainText = darkTextBounds(a, scan.intersected(a.rect()), 160);
                const QRect editableText = darkTextBounds(b, scan.intersected(b.rect()), 160);
                QVERIFY(!plainText.isEmpty());QVERIFY(!editableText.isEmpty());
                const int difference = rtl ? plainText.right() - editableText.right()
                                           : plainText.left() - editableText.left();
                QVERIFY2(qAbs(difference) <= qCeil(dpr),
                         qPrintable(QStringLiteral("Text difference %1 DPR %2 compact %3 RTL %4: %5/%6")
                                        .arg(difference).arg(dpr).arg(compact).arg(rtl)
                                        .arg(plainText.left()).arg(editableText.left())));
                QCOMPARE(editable.size(), plain.size());
                QVERIFY(editable.lineEdit()->height() <= editable.height());
                QSignalSpy edited(editable.lineEdit(), &QLineEdit::textEdited);
                editable.lineEdit()->setFocus();editable.lineEdit()->selectAll();
                QTest::keyClicks(editable.lineEdit(), "Custom");
                QCOMPARE(editable.currentText(), QStringLiteral("Custom"));
                QVERIFY(!edited.isEmpty());
            }
        }
    }

    void comboPopupAcceptsPixelFonts()
    {
        Resources::installResources(*qApp);
        QTest::failOnWarning(QRegularExpression(QStringLiteral("QFont::setPointSize.*")));
        QComboBox combo;
        combo.addItems({QStringLiteral("Camera A"), QStringLiteral("Camera B")});
        combo.resize(280, 24);
        combo.show();
        QTest::qWait(30);
        QVERIFY(combo.font().pixelSize() > 0);
        QCOMPARE(combo.font().pointSize(), -1);
        for (int i = 0; i < 2; ++i) {
            combo.showPopup();
            QTRY_VERIFY(combo.view()->isVisible());
            QTest::keyClick(combo.view(), Qt::Key_Down);
            QTest::keyClick(combo.view(), Qt::Key_Return);
            QTRY_VERIFY(!combo.view()->isVisible());
        }
    }

    void comboPopupUsesNeutralRowsWithoutLeadingMarkers()
    {
        Resources::installResources(*qApp);
        for (bool nested : {false, true}) for (bool editable : {false, true}) {
            QTreeWidget tree;
            tree.setProperty("treeRole", "DeviceFeatureTree");
            QComboBox combo(nested ? tree.viewport() : nullptr);
            if (nested) { tree.resize(400, 240); tree.show(); }
            combo.setEditable(editable);
            combo.addItems({QStringLiteral("First option"), QStringLiteral("Second option"), QStringLiteral("Third option")});
            combo.setCurrentIndex(1);combo.resize(280, 24);combo.show();
            QVERIFY(QTest::qWaitForWindowExposed(&combo));
            combo.showPopup();QTRY_VERIFY(combo.view()->isVisible());
            auto* view = combo.view();
            const int rowHeight = view->visualRect(combo.model()->index(0, 0)).height();
            QVERIFY2(rowHeight <= combo.height(), qPrintable(QString("Popup row %1 exceeds field %2 (nested %3, editable %4)")
                .arg(rowHeight).arg(combo.height()).arg(nested).arg(editable)));
            const auto verifyRow = [&](int row) {
                QCoreApplication::processEvents();
                const QRect logical = view->visualRect(combo.model()->index(row, 0));
                const QImage image = view->viewport()->grab().toImage();
                const qreal dpr = image.devicePixelRatio();
                bool marker = false;
                for (int y = qCeil(logical.top() * dpr); y < qFloor((logical.bottom() + 1) * dpr); ++y)
                    for (int x = qFloor(logical.left() * dpr); x < qCeil((logical.left() + 6) * dpr); ++x) {
                        const QColor pixel = image.pixelColor(x, y);
                        marker |= pixel.alpha() > 128 && qGray(pixel.rgb()) < 120;
                    }
                const QColor background = image.pixelColor(qRound((logical.right() - 16) * dpr),
                                                           qRound(logical.center().y() * dpr));
                return !marker && background == Resources::themePalette().value(QStringLiteral("selection"));
            };
            QVERIFY(verifyRow(1));
            const QPoint hover = view->visualRect(combo.model()->index(2, 0)).center();
            QMouseEvent move(QEvent::MouseMove, hover, view->viewport()->mapToGlobal(hover),
                             Qt::NoButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(view->viewport(), &move);
            QCOMPARE(view->currentIndex().row(), 2);
            QVERIFY(verifyRow(2));
            QSignalSpy activated(&combo, qOverload<int>(&QComboBox::activated));
            QTest::keyClick(view, Qt::Key_Return);
            QTRY_VERIFY(!view->isVisible());
            QCOMPARE(combo.currentIndex(), 2);
            QCOMPARE(activated.count(), 1);
        }
    }

    void mixedMenuLabelsShareOneColumn()
    {
        Resources::installResources(*qApp);
        for (bool icons : {false, true}) for (bool rtl : {false, true}) {
            QMenu menu;
            menu.setLayoutDirection(rtl ? Qt::RightToLeft : Qt::LeftToRight);
            QList<QAction*> actions;
            if (icons) actions << menu.addAction(QIcon(QStringLiteral(":/Resources/icon/setting-48.png")), QStringLiteral("MMMM command"));
            actions << menu.addAction(QStringLiteral("MMMM command"));
            auto* check = menu.addAction(QStringLiteral("MMMM command"));
            check->setCheckable(true);check->setChecked(true);actions << check;
            QActionGroup group(&menu);
            auto* radio = menu.addAction(QStringLiteral("MMMM command"));
            radio->setCheckable(true);group.addAction(radio);radio->setChecked(true);actions << radio;
            auto* child = menu.addMenu(QStringLiteral("MMMM command"));
            child->addAction(QStringLiteral("MMMM child"));actions << child->menuAction();
            menu.popup(QPoint(180, 180));QTRY_VERIFY(menu.isVisible());
            QVERIFY(menu.property("resourcesMenuHasLeadingColumn").toBool());
            QCOMPARE(menu.property("resourcesMenuDirection").toString(), rtl ? QStringLiteral("rtl") : QStringLiteral("ltr"));
            const QImage image = menu.grab().toImage();
            const qreal dpr = image.devicePixelRatio();
            const QRect labelRow = menu.actionGeometry(actions.at(icons ? 1 : 0));
            const QRect labelInk = darkTextBounds(image, QRect(qRound(labelRow.left() * dpr),
                qRound(labelRow.top() * dpr), qRound(labelRow.width() * dpr), qRound(labelRow.height() * dpr)));
            QVERIFY(!labelInk.isEmpty());
            const int first = labelInk.left();
            for (auto* action : actions) {
                const QRect row = menu.actionGeometry(action);
                const QRect scan(qRound(row.left() * dpr), qRound(row.top() * dpr),
                                 qRound(row.width() * dpr), qRound(row.height() * dpr));
                // Match label glyphs independently of the equally dark check/radio/action icons.
                const QRect labelColumn(labelInk.left() - qCeil(dpr), scan.top(),
                    labelInk.width() + 2 * qCeil(dpr), scan.height());
                const QRect ink = darkTextBounds(image, scan.intersected(labelColumn).intersected(image.rect()));
                QVERIFY(!ink.isEmpty());
                QVERIFY(qAbs(ink.width() - labelInk.width()) <= qCeil(dpr));
                QVERIFY2(qAbs(ink.left() - first) <= qCeil(dpr), qPrintable(QStringLiteral("%1 icons=%2 rtl=%3 left=%4 first=%5").arg(action->text()).arg(icons).arg(rtl).arg(ink.left()).arg(first)));
            }
            menu.setActiveAction(child->menuAction());QTest::keyClick(&menu, rtl ? Qt::Key_Left : Qt::Key_Right);
            QTRY_VERIFY(child->isVisible());
            QVERIFY(!child->property("resourcesMenuHasLeadingColumn").toBool());
            child->close();menu.close();
        }
    }

    void menuColumnTracksVisibleActions()
    {
        Resources::installResources(*qApp);
        QMenu menu;
        auto* command = menu.addAction(QStringLiteral("MMMM command"));
        menu.popup(QPoint(180, 180));QTRY_VERIFY(menu.isVisible());
        const auto ownsPopupShell = [&menu] {
            auto* shadow = qobject_cast<QGraphicsDropShadowEffect*>(menu.graphicsEffect());
            return menu.isVisible() && shadow && shadow->property("resourcesMenuShadow").toBool()
                && shadow->blurRadius() == 12 && shadow->offset() == QPointF(0, 2)
                && menu.windowFlags().testFlag(Qt::FramelessWindowHint)
                && menu.windowFlags().testFlag(Qt::NoDropShadowWindowHint)
                && menu.mask().isEmpty() && menu.testAttribute(Qt::WA_TranslucentBackground);
        };
        QVERIFY(ownsPopupShell());
        QVERIFY(!menu.property("resourcesMenuHasLeadingColumn").toBool());
        auto* check = menu.addAction(QStringLiteral("MMMM choice"));check->setCheckable(true);
        QVERIFY(menu.property("resourcesMenuHasLeadingColumn").toBool());QVERIFY(ownsPopupShell());
        check->setVisible(false);
        QVERIFY(!menu.property("resourcesMenuHasLeadingColumn").toBool());QVERIFY(ownsPopupShell());
        check->setVisible(true);
        QVERIFY(menu.property("resourcesMenuHasLeadingColumn").toBool());QVERIFY(ownsPopupShell());
        check->setCheckable(false);
        QVERIFY(!menu.property("resourcesMenuHasLeadingColumn").toBool());QVERIFY(ownsPopupShell());
        check->setCheckable(true);
        QVERIFY(menu.property("resourcesMenuHasLeadingColumn").toBool());QVERIFY(ownsPopupShell());
        menu.removeAction(check);
        QVERIFY(!menu.property("resourcesMenuHasLeadingColumn").toBool());QVERIFY(ownsPopupShell());
        delete check;
        command->setIconVisibleInMenu(true);
        command->setIcon(QIcon(QStringLiteral(":/Resources/icon/setting-48.png")));
        QVERIFY(menu.property("resourcesMenuHasLeadingColumn").toBool());QVERIFY(ownsPopupShell());
        command->setIcon(QIcon());
        QVERIFY(!menu.property("resourcesMenuHasLeadingColumn").toBool());QVERIFY(ownsPopupShell());
        menu.setLayoutDirection(Qt::RightToLeft);
        QCOMPARE(menu.property("resourcesMenuDirection").toString(), QStringLiteral("rtl"));
        QVERIFY(ownsPopupShell());
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
            const QColor border(QStringLiteral("#8a8a89"));
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
                const QColor border(QStringLiteral("#8a8a89"));
                // Fractional coverage can round an unpremultiplied channel by one.
                QVERIFY(qAbs(color.red() - border.red()) <= 1);
                QVERIFY(qAbs(color.green() - border.green()) <= 1);
                QVERIFY(qAbs(color.blue() - border.blue()) <= 1);
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
