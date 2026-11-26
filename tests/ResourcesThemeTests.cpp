#include "Resources.h"
#include "Chrome/ThemedMdiTitleBar.h"
#include "Chrome/ThemedMainTitleBar.h"
#include "Chrome/ThemedDockTitleBar.h"
#include "Chrome/ThemedDialog.h"
#include "Widgets/DeviceScopeCard.h"
#include "Widgets/SearchableTreeWidget.h"
#include <QMainWindow>
#include <QDockWidget>
#include <QMdiSubWindow>
#include <QPushButton>
#include <QMenu>
#include <QPainter>
#include "theme/ResourcesTheme.h"
#include "theme/ThemeStyleSheet.h"
#include <QApplication>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QRegularExpression>
#include <QSpinBox>
#include <QStackedWidget>
#include <QSplitter>
#include <QStatusBar>
#include <QStyle>
#include <QTabBar>
#include <QTemporaryDir>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWindow>
#include <QGridLayout>
#include <QFormLayout>
#include <QLabel>
#include <QCheckBox>
#include <QRadioButton>
#include <QStyleOptionButton>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QtTest>

// Unrelated consumer identities must opt into density using the Qt-only contract.
class CompactConsumerWidget : public QWidget {
    Q_OBJECT
public:
    explicit CompactConsumerWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setProperty("actionDensity", "compact");
        setProperty("controlRole", "devicePanel");
    }
};

using Resources::ResourcesTheme;

class ResourcesThemeTests : public QObject {
    Q_OBJECT
    QTemporaryDir factory_;

    static bool write(const QString& path, const QByteArray& bytes)
    {
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
    }
    static bool copyFactory(const QString& root)
    {
        if (!QDir().mkpath(root + "/qss")) return false;
        const QDir embedded(":/Resources/theme/qss");
        for (const auto& name : embedded.entryList({"*.qss"}, QDir::Files)) {
            QFile file(embedded.filePath(name));
            if (!file.open(QIODevice::ReadOnly) || !write(root + "/qss/" + name, file.readAll())) return false;
        }
        QFile file(":/Resources/theme/metrics.json");
        return file.open(QIODevice::ReadOnly) && write(root + "/metrics.json", file.readAll());
    }
    static QJsonObject metricSource(const QString& root)
    {
        QFile file(root + "/metrics.json");
        if (!file.open(QIODevice::ReadOnly)) return {};
        return QJsonDocument::fromJson(file.readAll()).object();
    }

private slots:
    void initTestCase() { QVERIFY(factory_.isValid()); QVERIFY(copyFactory(factory_.path())); }
    void cleanup()
    {
        Resources::resetThemePalette();
        QString error;
        QVERIFY2(Resources::reloadThemeFromDirectory(factory_.path() + "/qss", &error), qPrintable(error));
    }
    void dockSeparatorHitMeasurement()
    {
        QMainWindow window;
        window.setDockOptions(QMainWindow::AllowTabbedDocks);
        window.setCentralWidget(new QWidget);
        QDockWidget first("First", &window), second("Second", &window);
        auto* content = new QWidget;
        content->setStyleSheet("background: #208060;");
        first.setWidget(content); second.setWidget(new QWidget);
        window.addDockWidget(Qt::RightDockWidgetArea, &first);
        window.splitDockWidget(&first, &second, Qt::Vertical);
        window.resize(800, 600); window.show();
        QTest::qWait(100);
        const QRect dock = first.geometry();
        QCOMPARE(dock.left() - window.centralWidget()->geometry().right() - 1, 1);
        QCOMPARE(second.geometry().top() - dock.bottom() - 1, 1);
        const QImage image = window.grab().toImage();
        const QPoint contentPoint = content->mapTo(&window, QPoint(0, content->height() / 2));
        QCOMPARE(image.pixelColor(contentPoint.x(), contentPoint.y()), QColor("#208060"));
        QCOMPARE(image.pixelColor(contentPoint.x() + 2, contentPoint.y()), QColor("#208060"));
        bool central = false, between = false;
        const QPoint centralLine(dock.left() - 1, dock.center().y());
        const QPoint dockLine(dock.center().x(), dock.bottom() + 1);
        QCOMPARE(image.pixelColor(centralLine), Resources::themePalette().value("border"));
        QCOMPARE(image.pixelColor(dockLine), Resources::themePalette().value("border"));
        for (QWidget* splitter : window.findChildren<QWidget*>(QStringLiteral("qt_qmainwindow_extended_splitter"))) {
            if (!splitter->isVisible()) continue;
            if (splitter->geometry().contains(centralLine)) { central = true; QVERIFY(splitter->width() >= 5); }
            if (splitter->geometry().contains(dockLine)) { between = true; QVERIFY(splitter->height() >= 5); }
        }
        QVERIFY(central);
        QVERIFY(between);
    }

    void dockTabsProvideCloseControls()
    {
        QMainWindow window;
        QDockWidget first("First", &window), second("Second", &window);
        first.setWidget(new QWidget); second.setWidget(new QWidget);
        window.addDockWidget(Qt::RightDockWidgetArea, &first);
        window.addDockWidget(Qt::RightDockWidgetArea, &second);
        window.tabifyDockWidget(&first, &second);
        first.raise();
        window.statusBar(); window.resize(640, 480); window.show();
        QTabBar* bar = nullptr;
        QTRY_VERIFY((bar = window.findChild<QTabBar*>(QString(), Qt::FindDirectChildrenOnly)) != nullptr);
        QTRY_COMPARE(bar->count(), 2);
        const QImage tabImage = bar->grab().toImage();
        QCOMPARE(tabImage.pixelColor(0, 0), Resources::themePalette().value("selectionAccent"));
        QCOMPARE(tabImage.pixelColor(0, 1), Resources::themePalette().value("selectionAccent"));
        QCOMPARE(bar->geometry().left(), first.geometry().left());
        QTRY_VERIFY(bar->tabButton(0, QTabBar::RightSide));
        QTRY_VERIFY(bar->tabButton(1, QTabBar::RightSide));
        first.setWindowTitle("Renamed");
        QTRY_COMPARE(bar->tabButton(0, QTabBar::RightSide)->accessibleName(), QStringLiteral("Close Renamed"));
        QCOMPARE(bar->tabButton(0, QTabBar::RightSide)->toolTip(), QStringLiteral("Close Renamed"));
        first.setFeatures(first.features() & ~QDockWidget::DockWidgetClosable);
        QTRY_VERIFY(!bar->tabButton(0, QTabBar::RightSide));
        first.setFeatures(first.features() | QDockWidget::DockWidgetClosable);
        QTRY_VERIFY(bar->tabButton(0, QTabBar::RightSide));
        QTRY_COMPARE(bar->tabButton(0, QTabBar::RightSide)->accessibleName(), QStringLiteral("Close Renamed"));
        for (int i = 0; i < bar->count(); ++i) {
            auto* button = qobject_cast<QToolButton*>(bar->tabButton(i, QTabBar::RightSide));
            QVERIFY(button);
            QCOMPARE(button->accessibleName(), QStringLiteral("Close ") + bar->tabText(i));
            QVERIFY(!button->isCheckable());
            QCOMPARE(button->property("tabSelected").toBool(), i == bar->currentIndex());
        }
        for (int i = 0; i < 2; ++i) {
            bar->setCurrentIndex(i);
            QApplication::processEvents();
            for (int j = 0; j < 2; ++j) {
                auto* button = qobject_cast<QToolButton*>(bar->tabButton(j, QTabBar::RightSide));
                button->setAttribute(Qt::WA_UnderMouse, false);
                button->grab();
                const QImage icon = button->icon().pixmap(QSize(12, 12)).toImage();
                const QColor expected = Resources::themePalette().value(i == j ? "selectionAccent" : "text");
                bool found = false;
                for (int y = 0; y < icon.height(); ++y)
                    for (int x = 0; x < icon.width(); ++x) {
                        const QColor pixel = icon.pixelColor(x, y);
                        if (pixel.alpha() > 100 && qAbs(pixel.red() - expected.red()) <= 2 && qAbs(pixel.green() - expected.green()) <= 2 && qAbs(pixel.blue() - expected.blue()) <= 2) found = true;
                    }
                QVERIFY(found);
            }
        }
        bar->setCurrentIndex(0);
        QTRY_COMPARE(bar->tabButton(0, QTabBar::RightSide)->property("tabSelected").toBool(), true);
        QCOMPARE(bar->tabButton(1, QTabBar::RightSide)->property("tabSelected").toBool(), false);
        const QString closed = bar->tabText(0);
        qobject_cast<QToolButton*>(bar->tabButton(0, QTabBar::RightSide))->click();
        QDockWidget* target = closed == first.windowTitle() ? &first : &second;
        QTRY_VERIFY(!target->isVisible());
        target->show();
        QTRY_COMPARE(bar->count(), 2);
        QTRY_VERIFY(bar->tabButton(0, QTabBar::RightSide));
        QTRY_VERIFY(bar->tabButton(1, QTabBar::RightSide));
        QCOMPARE(bar->height(), window.statusBar()->height());
    }

    void singleDockRemovesTabBarSpace()
    {
        QMainWindow window;
        QDockWidget first("First", &window), second("Second", &window);
        first.setWidget(new QWidget); second.setWidget(new QWidget);
        window.addDockWidget(Qt::RightDockWidgetArea, &first);
        window.addDockWidget(Qt::RightDockWidgetArea, &second);
        window.tabifyDockWidget(&first, &second);
        window.resize(640, 480); window.show();
        QTabBar* bar = nullptr;
        QTRY_VERIFY((bar = window.findChild<QTabBar*>(QString(), Qt::FindDirectChildrenOnly)) != nullptr);
        QTRY_VERIFY(bar->isVisible());
        const int areaBottom = bar->geometry().bottom();
        second.hide();
        QTest::qWait(100);
        QVERIFY(!bar->geometry().intersects(window.rect()));
        QCOMPARE(first.geometry().bottom(), areaBottom);
        second.show();
        QTRY_COMPARE(bar->count(), 2);
        QTRY_VERIFY(bar->geometry().intersects(window.rect()));
        QTRY_VERIFY(bar->tabButton(0, QTabBar::RightSide));
        QTRY_VERIFY(bar->tabButton(1, QTabBar::RightSide));
    }

    void dockTabRemapsCloseButton()
    {
        QMainWindow window;
        QDockWidget first("First", &window), second("Second", &window), third("Third", &window);
        window.addDockWidget(Qt::RightDockWidgetArea, &first);
        window.addDockWidget(Qt::RightDockWidgetArea, &second);
        window.addDockWidget(Qt::LeftDockWidgetArea, &third);
        window.tabifyDockWidget(&first, &second);
        window.show();
        QTabBar* bar = nullptr;
        QTRY_VERIFY((bar = window.findChild<QTabBar*>(QString(), Qt::FindDirectChildrenOnly)) != nullptr);
        QTRY_COMPARE(bar->count(), 2);
        QTRY_VERIFY(bar->tabButton(0, QTabBar::RightSide));
        window.removeDockWidget(&second);
        second.hide();
        window.tabifyDockWidget(&first, &third);
        third.show();
        QTRY_COMPARE(bar->count(), 2);
        int index = -1;
        QTRY_VERIFY((index = bar->tabText(0) == "Third" ? 0 : (bar->tabText(1) == "Third" ? 1 : -1)) >= 0);
        QTRY_VERIFY(bar->tabButton(index, QTabBar::RightSide));
        QTRY_COMPARE(bar->tabButton(index, QTabBar::RightSide)->accessibleName(), QStringLiteral("Close Third"));
        for (int i = 0; i < bar->count(); ++i)
            QVERIFY(bar->tabButton(i, QTabBar::RightSide)->accessibleName() != QStringLiteral("Close Second"));
    }

    void windowWithoutDockTabsHasNoCloseButtons()
    {
        QMainWindow window;
        QDockWidget dock("Only", &window);
        window.addDockWidget(Qt::RightDockWidgetArea, &dock);
        window.show(); QApplication::processEvents();
        QVERIFY(window.findChildren<QToolButton*>("ResourcesDockTabClose").isEmpty());
    }

    void installIsIdempotentAndFacadeUsesSameOwner()
    {
        auto& theme = ResourcesTheme::instance();
        QCOMPARE(theme.parent(), qApp);
        const auto children = theme.children();
        QVERIFY(theme.setThemePalette({{"text", QColor("#345678")}}));
        Resources::installResources(*qApp);
        Resources::installResources(*qApp);
        QCOMPARE(&ResourcesTheme::instance(), &theme);
        QCOMPARE(theme.children(), children);
        QCOMPARE(Resources::fontColor(), QColor("#345678"));
        QCOMPARE(Resources::themePalette(), theme.themePalette());
        QVERIFY(!qApp->styleSheet().contains('@'));
    }
    void defaultRadioInkTracksInteractionWithoutChangingSelection()
    {
        class Radio : public QRadioButton {
        public:
            QImage indicator(QStyle::State state) {
                QStyleOptionButton option;
                initStyleOption(&option);
                option.state = state;
                QImage image(size(), QImage::Format_ARGB32_Premultiplied);
                image.fill(Qt::transparent);
                QPainter painter(&image);
                style()->drawControl(QStyle::CE_RadioButton, &option, &painter, this);
                painter.end();
                return image.copy(style()->subElementRect(QStyle::SE_RadioButtonIndicator, &option, this));
            }
        } radio;
        radio.setAutoExclusive(false);
        radio.ensurePolished();
        radio.resize(160, 24);
        const auto hasInk = [](const QImage& image, const QColor& ink) {
            int pixels = 0;
            for (int y = 0; y < image.height(); ++y)
                for (int x = 0; x < image.width(); ++x) {
                    const QColor pixel = image.pixelColor(x, y);
                    if (pixel.alpha() > 200 && qAbs(pixel.red() - ink.red()) < 8
                        && qAbs(pixel.green() - ink.green()) < 8
                        && qAbs(pixel.blue() - ink.blue()) < 8) ++pixels;
                }
            return pixels > 5;
        };
        for (bool checked : {false, true}) {
            radio.setChecked(checked);
            const auto state = QStyle::State_Enabled | (checked ? QStyle::State_On : QStyle::State_Off);
            QVERIFY(hasInk(radio.indicator(state), QColor(checked ? "#15487f" : "#3c3c3b")));
            QVERIFY(hasInk(radio.indicator(state | QStyle::State_MouseOver), QColor("#8eafd1")));
            QVERIFY(hasInk(radio.indicator(state | QStyle::State_MouseOver | QStyle::State_Sunken), QColor("#15487f")));
            QCOMPARE(radio.isChecked(), checked);
            const auto disabled = radio.indicator(checked ? QStyle::State_On : QStyle::State_Off);
            QVERIFY(disabled != radio.indicator(state));
        }
        radio.show();
        QTest::mouseClick(&radio, Qt::LeftButton, Qt::NoModifier, QPoint(10, 10));
        QVERIFY(!radio.isChecked());
        QTest::keyClick(&radio, Qt::Key_Space);
        QVERIFY(radio.isChecked());
    }
    void itemViewChecksUseCompactCheckboxAndStandaloneKeepsSwitch()
    {
        QWidget host;
        auto *layout = new QVBoxLayout(&host);
        QTreeWidget tree;
        tree.setHeaderHidden(true);
        tree.setRootIsDecorated(false);
        auto *unchecked = new QTreeWidgetItem(&tree, {QStringLiteral("Unchecked")});
        auto *checked = new QTreeWidgetItem(&tree, {QStringLiteral("Checked")});
        auto *partial = new QTreeWidgetItem(&tree, {QStringLiteral("Partial")});
        auto *absent = new QTreeWidgetItem(&tree, {QStringLiteral("No check")});
        unchecked->setCheckState(0, Qt::Unchecked);
        checked->setCheckState(0, Qt::Checked);
        partial->setCheckState(0, Qt::PartiallyChecked);
        QCheckBox standalone(QStringLiteral("Standalone switch"));
        standalone.setChecked(true);
        layout->addWidget(&tree);
        layout->addWidget(&standalone);
        host.resize(360, 180);
        host.show();
        QVERIFY(QTest::qWaitForWindowExposed(&host));
        QCoreApplication::processEvents();

        struct Probe : QStyledItemDelegate { using QStyledItemDelegate::initStyleOption; } probe;
        const auto indicatorRect = [&](QTreeWidgetItem *item) {
            QStyleOptionViewItem option;
            option.initFrom(&tree);
            option.widget = &tree;
            option.rect = tree.visualItemRect(item);
            probe.initStyleOption(&option, tree.indexFromItem(item));
            return tree.style()->subElementRect(QStyle::SE_ItemViewItemCheckIndicator, &option, &tree);
        };
        const int rowHeight = tree.visualItemRect(absent).height();
        for (auto *item : {unchecked, checked, partial}) {
            const QRect indicator = indicatorRect(item);
            QCOMPARE(indicator.size(), QSize(14, 14));
            QCOMPARE(indicator.center().y(), tree.visualItemRect(item).center().y());
            QCOMPARE(tree.visualItemRect(item).height(), rowHeight);
        }
        QStyleOptionButton buttonOption;
        buttonOption.initFrom(&standalone);
        const QRect standaloneIndicator = standalone.style()->subElementRect(
            QStyle::SE_CheckBoxIndicator, &buttonOption, &standalone);
        QCOMPARE(standaloneIndicator.size(), QSize(40, 24));

        const auto capture = [&](QTreeWidgetItem *item) {
            const QRect indicator = indicatorRect(item);
            const QImage image = tree.viewport()->grab().toImage();
            const qreal dpr = image.devicePixelRatio();
            return image.copy(QRect(QPoint(qRound(indicator.left() * dpr), qRound(indicator.top() * dpr)),
                                    indicator.size() * dpr));
        };
        const QImage uncheckedImage = capture(unchecked);
        const QImage checkedImage = capture(checked);
        const QImage partialImage = capture(partial);
        QVERIFY(checkedImage != uncheckedImage);
        QVERIFY(partialImage != checkedImage);
        QVERIFY(partialImage != uncheckedImage);
    }

    void fieldsResolveOneGeometryAcrossParents()
    {
        CompactConsumerWidget device;
        CompactConsumerWidget source;
        QSpinBox integer(&device);
        QDoubleSpinBox floating(&source);
        QLineEdit field;
        for (QWidget* widget : {static_cast<QWidget*>(&integer), static_cast<QWidget*>(&floating), static_cast<QWidget*>(&field)}) {
            widget->ensurePolished();
            widget->resize(180, 100);
        }
        const auto metrics = ResourcesTheme::instance().metrics(ResourcesTheme::ControlKind::Field);
        QCOMPARE(integer.height(), metrics.contentHeight + 2 * metrics.borderWidth);
        QCOMPARE(floating.height(), integer.height());
        QCOMPARE(field.height(), integer.height());
    }
    void compactActionsShareDensityAndAllowExplicitOverride()
    {
        CompactConsumerWidget device;
        CompactConsumerWidget source;
        QToolButton first(&device), second(&source);
        device.show(); source.show();
        QApplication::processEvents();
        QCOMPARE(first.property("resourcesDensity").toString(), QString("compact"));
        QCOMPARE(second.property("resourcesDensity").toString(), QString("compact"));
        first.resize(100, 100); second.resize(100, 100);
        const auto metrics = ResourcesTheme::instance().metrics(ResourcesTheme::ControlKind::ToolButton, ResourcesTheme::Density::Compact);
        QCOMPARE(first.size(), QSize(metrics.contentWidth + 2 * metrics.borderWidth, metrics.contentHeight + 2 * metrics.borderWidth));
        QCOMPARE(second.size(), first.size());
        ResourcesTheme::instance().setDensity(first, ResourcesTheme::Density::Default);
        first.resize(100, 100);
        QVERIFY(first.width() > second.width());
        first.setParent(&source); first.show(); QApplication::processEvents();
        QCOMPARE(first.property("resourcesDensity").toString(), QString("default"));
        // Automatic admission follows reparenting; explicit density remains consumer-selected.
        QWidget plain;
        second.setParent(&plain); second.show(); QApplication::processEvents();
        QCOMPARE(second.property("resourcesDensity").toString(), QString("default"));
        QWidget wrapper(&device);
        QToolButton nested(&wrapper);
        wrapper.show(); nested.show(); QApplication::processEvents();
        QCOMPARE(nested.property("resourcesDensity").toString(), QString("compact"));
        wrapper.setParent(&plain); wrapper.show(); QApplication::processEvents();
        QCOMPARE(nested.property("resourcesDensity").toString(), QString("default"));
        first.setParent(&device); second.setParent(&source);
    }
    void densityChangesRefreshExistingSpinGeometry()
    {
        QSpinBox spin;
        spin.show(); QApplication::processEvents();
        auto& theme = ResourcesTheme::instance();
        const int normalHeight = spin.height();
        theme.setDensity(spin, ResourcesTheme::Density::Compact);
        spin.resize(180, 100);
        const auto compact = theme.metrics(ResourcesTheme::ControlKind::Field, ResourcesTheme::Density::Compact);
        QCOMPARE(spin.height(), compact.contentHeight + 2 * compact.borderWidth);
        QVERIFY(spin.height() < normalHeight);
        theme.setDensity(spin, ResourcesTheme::Density::Default);
        spin.resize(180, 100);
        QCOMPARE(spin.height(), normalHeight);
    }
    void actionDensityScopesAreExplicitAndFollowLiveChanges()
    {
        QWidget root;
        root.setObjectName("UnrelatedFutureConsumer");
        QToolButton action(&root);
        root.show(); action.show(); QApplication::processEvents();
        QCOMPARE(action.property("resourcesDensity").toString(), QString("default"));
        root.setProperty("actionDensity", "compact");
        QCOMPARE(action.property("resourcesDensity").toString(), QString("compact"));
        QWidget boundary(&root);
        boundary.setProperty("actionDensity", "default");
        QToolButton nested(&boundary);
        nested.ensurePolished();
        QCOMPARE(nested.property("resourcesDensity").toString(), QString("default"));
        ResourcesTheme::instance().setDensity(action, ResourcesTheme::Density::Default);
        root.setProperty("actionDensity", "default");
        root.setProperty("actionDensity", "compact");
        QCOMPARE(action.property("resourcesDensity").toString(), QString("default"));
    }
    void layoutRolesIgnoreNamesAndFollowReloadedMetrics()
    {
        QWidget root;
        auto* layout = new QVBoxLayout(&root);
        layout->setObjectName("UnrelatedFutureLayout");
        layout->setProperty("layoutRole", "content");
        root.show(); QApplication::processEvents();
        QCOMPARE(layout->contentsMargins(), QMargins(12, 12, 12, 12));
        QCOMPARE(layout->spacing(), 8);
        layout->setProperty("layoutRole", "actions");
        QCOMPARE(layout->contentsMargins(), QMargins());
        QCOMPARE(layout->spacing(), 6);
        auto* header = new QHBoxLayout;
        layout->addLayout(header);
        header->setProperty("layoutRole", "console-header");
        QCOMPARE(header->contentsMargins(), QMargins(8, 0, 8, 0));
        QCOMPARE(header->spacing(), 4);
        auto* compactStack = new QVBoxLayout;
        layout->addLayout(compactStack);
        compactStack->setProperty("layoutRole", "compact-form");
        QCOMPARE(compactStack->contentsMargins(), QMargins());
        QCOMPARE(compactStack->spacing(), 6);
        auto* compactForm = new QFormLayout;
        layout->addLayout(compactForm);
        compactForm->setProperty("layoutRole", "compact-form");
        QCOMPARE(compactForm->contentsMargins(), QMargins());
        QCOMPARE(compactForm->horizontalSpacing(), 8);
        QCOMPARE(compactForm->verticalSpacing(), 6);
        QTemporaryDir candidate; QVERIFY(copyFactory(candidate.path()));
        auto values = metricSource(candidate.path());
        values["LAYOUT_COMPACT_SPACING"] = 10;
        QVERIFY(write(candidate.path() + "/metrics.json", QJsonDocument(values).toJson()));
        QString error;
        QVERIFY2(Resources::reloadThemeFromDirectory(candidate.path() + "/qss", &error), qPrintable(error));
        QApplication::processEvents();
        QCOMPARE(layout->spacing(), 10);
        auto* grid = new QGridLayout;
        layout->addLayout(grid);
        grid->setProperty("layoutRole", "field-grid");
        QCOMPARE(grid->horizontalSpacing(), 12);
        QCOMPARE(grid->verticalSpacing(), 8);
        auto* form = new QFormLayout;
        layout->addLayout(form);
        form->setProperty("layoutRole", "wide-form");
        QCOMPARE(form->horizontalSpacing(), 24);
        QCOMPARE(form->verticalSpacing(), 12);
    }
    void platformCompositionPreservesDeclarationsAndResolvesOverrides()
    {
        const QString source = "QLabel, QToolButton { padding: 2px; border-radius: 7px; }\n"
                               "QToolButton:enabled:hover { margin: 1px; }\n";
        const QString patch = "QToolButton { border-radius: 18px; }\n"
                              "QToolButton:hover:enabled { margin: 3px; }\n";
        QString composed, error;
        QVERIFY2(Resources::detail::composePlatformVariant(source, {}, composed, &error), qPrintable(error));
        QCOMPARE(composed, source);
        QVERIFY2(Resources::detail::composePlatformVariant(source, patch, composed, &error), qPrintable(error));
        QVERIFY(composed.contains("QLabel{padding:2px;border-radius:7px;}"));
        QVERIFY(composed.contains("QToolButton{padding:2px;}"));
        QVERIFY(composed.contains("QToolButton{border-radius:18px;}"));
        QVERIFY(!composed.contains("margin:1px"));
        QCOMPARE(composed.count("margin:3px;"), 1);
        QCOMPARE(composed.count("border-radius:18px;"), 1);
        // Run the same composition path even on platforms without a native patch.
        const auto original = composed;
        QVERIFY2(Resources::detail::composePlatformVariant(original, patch, composed, &error), qPrintable(error));
        QCOMPARE(composed, original);
    }
    void malformedPlatformCompositionPreservesOutput_data()
    {
        QTest::addColumn<QString>("source");
        QTest::addColumn<QString>("patch");
        QTest::newRow("patch-missing-semicolon") << QString("QLabel { padding: 2px; }")
            << QString("QLabel { padding: 4px }");
        QTest::newRow("patch-stray-text") << QString("QLabel { padding: 2px; }")
            << QString("QLabel { invalid; padding: 4px; }");
        QTest::newRow("source-missing-semicolon") << QString("QLabel { padding: 2px }")
            << QString("QLabel { padding: 4px; }");
        QTest::newRow("source-without-platform") << QString("QLabel { padding: 2px }") << QString();
        QTest::newRow("patch-unclosed-nested") << QString("QLabel { padding: 2px; }")
            << QString("QLabel { padding: 2px;\nQToolButton { margin: 1px; }");
        QTest::newRow("patch-unclosed-tail") << QString("QLabel { padding: 2px; }")
            << QString("QLabel { padding: 4px; } QToolButton { margin: 1px;");
        QTest::newRow("patch-extra-close") << QString("QLabel { padding: 2px; }")
            << QString("QLabel { padding: 4px; }}");
        QTest::newRow("source-unclosed-without-platform") << QString("QLabel { padding: 2px;") << QString();
        QTest::newRow("source-unclosed-before-platform") << QString("QLabel { padding: 2px;")
            << QString("QToolButton { margin: 1px; }");
    }
    void malformedPlatformCompositionPreservesOutput()
    {
        QFETCH(QString, source);
        QFETCH(QString, patch);
        const QString installed("QLabel { padding: 7px; }");
        QString composed = installed, error;
        QVERIFY(!Resources::detail::composePlatformVariant(source, patch, composed, &error));
        QVERIFY2(error.contains("Unparsed theme"), qPrintable(error));
        QCOMPARE(composed, installed);
    }
    void tabStripsStartAtLeadingEdge()
    {
        QWidget host;
        QTabBar tabs(&host);
        tabs.setExpanding(false);
        tabs.addTab("Only tab");
        tabs.setGeometry(0, 0, 400, 32);
        host.resize(400, 40); host.show();
        QApplication::processEvents();
        QCOMPARE(tabs.style()->styleHint(QStyle::SH_TabBar_Alignment, nullptr, &tabs), int(Qt::AlignLeft));
        QCOMPARE(tabs.tabRect(0).left(), 0);
    }
    void deviceTabScrollButtonsRetainBorderlessSlots()
    {
        CompactConsumerWidget device;
        QTabBar tabs(&device);
        tabs.setUsesScrollButtons(true);
        tabs.setExpanding(false);
        tabs.setGeometry(0, 0, 180, 32);
        for (int i = 0; i < 12; ++i) tabs.addTab(QString("Device tab %1").arg(i));
        device.resize(200, 70); device.show();
        QApplication::processEvents();
        const auto buttons = tabs.findChildren<QToolButton*>();
        QCOMPARE(buttons.size(), 2);
        for (auto* button : buttons) {
            QVERIFY(button->isVisible());
            QVERIFY(button->property("resourcesDensity").toString() != "compact");
            QCOMPARE(button->size(), QSize(20, 20));
        }
    }
    void deviceCounterRoleResolvesMonospaceBubbleAndWarning()
    {
        QStatusBar bar;
        bar.setSizeGripEnabled(false);
        auto* state = new QLabel("Live", &bar);
        state->setAlignment(Qt::AlignCenter);
        bar.addWidget(state);
        auto* counter = new QLabel("3 failed", &bar);
        counter->setProperty("statusRole", "deviceCounter");
        counter->setAlignment(Qt::AlignCenter);
        bar.addPermanentWidget(counter);
        bar.setFixedHeight(32);
        bar.show();
        QApplication::processEvents();

        QCOMPARE(counter->font().family(), QStringLiteral("JetBrains Mono"));
        QVERIFY(state->font().family() != QStringLiteral("JetBrains Mono"));
        QCOMPARE(counter->contentsRect().height(), 24);
        const QImage normal = counter->grab().toImage();
        QCOMPARE(normal.pixelColor(normal.width() / 2, 0), Resources::themePalette().value("controlBorder"));

        counter->setProperty("failedState", "warning");
        counter->style()->unpolish(counter);
        counter->style()->polish(counter);
        QApplication::processEvents();
        const QImage warning = counter->grab().toImage();
        QCOMPARE(warning.pixelColor(warning.width() / 2, 0), Resources::themePalette().value("warning"));
        QCOMPARE(counter->palette().color(QPalette::WindowText), Resources::themePalette().value("warningText"));
    }
    void graphicsSaveBubbleRoleUsesStatusSurface()
    {
        QStatusBar bar;
        bar.setSizeGripEnabled(false);
        auto* bubble = new QWidget(&bar);
        bubble->setObjectName(QStringLiteral("EngineStatusSave"));
        bubble->setProperty("statusRole", "save");
        bubble->setAttribute(Qt::WA_StyledBackground);
        auto* layout = new QHBoxLayout(bubble);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(new QLabel(QStringLiteral("Saving frame.png…"), bubble));
        bar.addWidget(bubble);
        bar.setFixedHeight(32);
        bar.show();
        QApplication::processEvents();

        QVERIFY(bubble->height() >= 24);
        const QImage surface = bubble->grab().toImage();
        QVERIFY(!surface.isNull());
        QCOMPARE(surface.pixelColor(surface.width() / 2, 0), Resources::themePalette().value("controlBorder"));
    }

    void deviceCounterRoleResolvesInDevicePanelRow()
    {
        QWidget panel;
        panel.setProperty("controlRole", "devicePanel");
        auto* layout = new QVBoxLayout(&panel);
        auto* counter = new QLabel("3 failed", &panel);
        counter->setProperty("statusRole", "deviceCounter");
        counter->setAlignment(Qt::AlignCenter);
        layout->addWidget(counter);
        panel.show();
        QApplication::processEvents();

        QCOMPARE(counter->font().family(), QStringLiteral("JetBrains Mono"));
        QCOMPARE(counter->contentsRect().height(), 24);
        const QImage normal = counter->grab().toImage();
        QCOMPARE(normal.pixelColor(normal.width() / 2, 0), Resources::themePalette().value("controlBorder"));

        counter->setProperty("failedState", "warning");
        counter->style()->unpolish(counter);
        counter->style()->polish(counter);
        QApplication::processEvents();
        const QImage warning = counter->grab().toImage();
        QCOMPARE(warning.pixelColor(warning.width() / 2, 0), Resources::themePalette().value("warning"));
        QCOMPARE(counter->palette().color(QPalette::WindowText), Resources::themePalette().value("warningText"));
    }
    void deviceStateTokenResolvesCompactBoldInk()
    {
        QWidget header;
        auto* token = new QLabel("Connected", &header);
        token->setProperty("stateRole", "deviceState");
        auto* plain = new QLabel("Connected", &header);
        header.show();
        QApplication::processEvents();
        QVERIFY(token->font().bold());
        QVERIFY(!plain->font().bold());
        QCOMPARE(token->palette().color(QPalette::WindowText), Resources::themePalette().value("text"));
        QVERIFY(token->font().family() != QStringLiteral("JetBrains Mono"));
    }
    void deviceScopeCardOwnsTheInCardFrame()
    {
        QWidget host;
        auto* layout = new QVBoxLayout(&host);
        auto* card = new DeviceScopeCard(&host);
        auto* devicePage = new QTreeWidget(card);
        card->addTab(devicePage, "Remote Device");
        auto* featurePage = new QTreeWidget(card);
        featurePage->setProperty("treeRole", "DeviceFeatureTree");
        featurePage->setHeaderLabels({"Feature", "Value"});
        card->addTab(featurePage, "Stream");
        layout->addWidget(card);
        host.resize(460, 360);
        host.show();
        QApplication::processEvents();

        QCOMPARE(card->property("tabRole").toString(), QStringLiteral("DeviceScopeCard"));
        auto* separator = card->findChild<QFrame*>(QStringLiteral("DeviceScopeCardSeparator"));
        QVERIFY(separator);
        QCOMPARE(separator->height(), 1);
        QCOMPARE(separator->width(), card->contentsRect().width());
        QVERIFY(card->tabBar()->geometry().top() >= card->contentsRect().top());
        QCOMPARE(card->pages()->geometry().top(), separator->geometry().bottom() + 1);

        card->setCurrentIndex(1);
        QCOMPARE(card->pages()->currentIndex(), 1);
        QCOMPARE(card->pages()->currentWidget(), featurePage);

        const QColor border = Resources::themePalette().value("border");
        const QImage image = card->grab().toImage();
        QCOMPARE(image.pixelColor(image.width() / 2, 0), border);
        QCOMPARE(image.pixelColor(0, image.height() / 2), border);
        QCOMPARE(image.pixelColor(image.width() / 2, separator->geometry().center().y()), border);
        const QPoint featureEdge = featurePage->mapTo(
            card, QPoint(0, featurePage->height() / 2));
        QCOMPARE(image.pixelColor(featureEdge), Resources::themePalette().value("surface"));

        card->removeTab(0);
        QCOMPARE(card->count(), 1);
        QCOMPARE(card->pages()->count(), 1);
        QCOMPARE(card->pages()->currentWidget(), featurePage);
    }
    void deviceTargetsAndFeaturesShareTreeGeometry()
    {
        QTreeWidget targets;
        QTreeWidget features;
        targets.setProperty("treeRole", "DeviceTargetTree");
        features.setProperty("treeRole", "DeviceFeatureTree");
        const auto populate = [](QTreeWidget& tree) {
            auto* parent = new QTreeWidgetItem(&tree, {QStringLiteral("Parent")});
            new QTreeWidgetItem(parent, {QStringLiteral("Child")});
            tree.expandAll();
            tree.resize(360, 180);
            tree.show();
            return parent;
        };
        auto* target = populate(targets);
        auto* feature = populate(features);
        QApplication::processEvents();
        QCOMPARE(targets.indentation(), 18);
        QCOMPARE(targets.indentation(), features.indentation());
        const auto rowGeometry = [](QTreeWidget& tree, QTreeWidgetItem* item) {
            const auto row = tree.visualItemRect(item);
            return QRect(row.topLeft(), QSize(1, row.height()));
        };
        QCOMPARE(rowGeometry(targets, target), rowGeometry(features, feature));
        QCOMPARE(rowGeometry(targets, target->child(0)), rowGeometry(features, feature->child(0)));
        const auto branch = [](QTreeWidget& tree, QTreeWidgetItem* parent) {
            const auto image = tree.viewport()->grab().toImage();
            const qreal scale = image.devicePixelRatio();
            const auto row = tree.visualItemRect(parent);
            return image.copy(0, qRound(row.y() * scale), qRound(row.left() * scale),
                              qRound(row.height() * scale));
        };
        const auto targetArrow = branch(targets, target);
        QVERIFY(!targetArrow.isNull());
        bool hasVisibleArrow = false;
        for (int y = 0; y < targetArrow.height(); ++y)
            for (int x = 0; x < targetArrow.width(); ++x)
                hasVisibleArrow |= qGray(targetArrow.pixel(x, y)) < 150;
        QVERIFY(hasVisibleArrow);
        QCOMPARE(targetArrow, branch(features, feature));
    }
    void dockedFavoritesStarCentersWithinItsStrip()
    {
        DeviceScopeCard card;
        auto* tree = new SearchableTreeWidget(&card);
        card.addTab(tree, QStringLiteral("Device"));
        card.setSearchStripShown(true);
        card.resize(360, 260);
        card.show();
        card.activateWindow();
        QApplication::processEvents();

        auto* strip = card.searchStrip();
        QVERIFY(strip);
        auto* star = strip->findChild<QToolButton*>(QStringLiteral("DeviceFeatureFavoritesOnly"));
        auto* separator = strip->findChild<QFrame*>(QStringLiteral("DeviceScopeCardSearchSeparator"));
        QVERIFY(star && separator);
        const auto inkCenter = [strip, star] {
            const auto image = strip->grab().toImage();
            const auto dpr = image.devicePixelRatio();
            const QRect area(star->mapTo(strip, QPoint()), star->size());
            const auto interior = area.adjusted(2, 2, -2, -2);
            int top = image.height(), bottom = -1;
            for (int y = qRound(interior.top() * dpr); y < qRound((interior.bottom() + 1) * dpr); ++y) {
                for (int x = qRound(interior.left() * dpr); x < qRound((interior.right() + 1) * dpr); ++x) {
                    const auto pixel = image.pixel(x, y);
                    if (qAlpha(pixel) < 128 || qGray(pixel) >= 180) continue;
                    top = qMin(top, y);
                    bottom = qMax(bottom, y);
                }
            }
            return bottom >= top ? (top + bottom + 1) / (2 * dpr) : -1.0;
        };
        const double stripCenter = (strip->height() + separator->height()) / 2.0;
        // Native styles can position the icon canvas one pixel apart before rasterization.
        constexpr double rasterTolerance = 2.0;
        QCOMPARE(star->size(), QSize(20, 20));
        const double restingInk = inkCenter();
        QVERIFY2(qAbs(restingInk - stripCenter) <= rasterTolerance,
                 qPrintable(QStringLiteral("star center %1, strip center %2").arg(restingInk).arg(stripCenter)));

        tree->setFavoriteColor(QColor(QStringLiteral("#245080")));
        const double recoloredInk = inkCenter();
        QCOMPARE(recoloredInk, restingInk);

        star->clearFocus();
        star->setFocus(Qt::ShortcutFocusReason);
        QApplication::processEvents();
        QCOMPARE(star->property("resourcesKeyboardFocus"), QVariant(true));
        QCOMPARE(star->size(), QSize(20, 20));
        const double focusedInk = inkCenter();
        QVERIFY2(qAbs(focusedInk - stripCenter) <= rasterTolerance,
                 qPrintable(QStringLiteral("star center %1, strip center %2").arg(focusedInk).arg(stripCenter)));
    }
    void engineToolbarAndPlotActionsTrackReloadedMetrics()
    {
        QToolBar toolbar;
        toolbar.setObjectName("EngineToolBar");
        auto* action = toolbar.addAction("Action");
        toolbar.show(); QApplication::processEvents();
        auto* button = qobject_cast<QToolButton*>(toolbar.widgetForAction(action));
        QVERIFY(button);
        QToolButton plot, namedPlot, compactAction;
        plot.setProperty("plotActionRole", "action");
        namedPlot.setObjectName("PlotViewActionButton");
        compactAction.setProperty("controlRole", "compactAction");
        plot.show(); namedPlot.show(); compactAction.show(); QApplication::processEvents();
        QCOMPARE(plot.minimumSize(), QSize(20, 20));
        QCOMPARE(namedPlot.minimumSize(), plot.minimumSize());
        QCOMPARE(compactAction.minimumSize(), plot.minimumSize());
        const auto original = button->minimumSize();
        QTemporaryDir candidate; QVERIFY(copyFactory(candidate.path()));
        auto values = metricSource(candidate.path());
        values["COMPACT_ACTION_CONTENT_SIZE"] = 26;
        values["TOOLBAR_ACTION_PADDING"] = 4;
        QVERIFY(write(candidate.path() + "/metrics.json", QJsonDocument(values).toJson()));
        QString error;
        QVERIFY2(Resources::reloadThemeFromDirectory(candidate.path() + "/qss", &error), qPrintable(error));
        QApplication::processEvents();
        QCOMPARE(button->minimumSize(), QSize(36, 36));
        QVERIFY(button->minimumSize().width() > original.width());
        QCOMPARE(plot.minimumSize(), QSize(28, 28));
        QCOMPARE(namedPlot.minimumSize(), plot.minimumSize());
        QCOMPARE(compactAction.minimumSize(), plot.minimumSize());
    }
    void reloadUpdatesQssAndRuntimeMaskTogether()
    {
        QTreeWidget tree;
        tree.setProperty("treeRole", "DeviceFeatureTree");
        tree.resize(320, 200); tree.show(); QApplication::processEvents();
        const auto oldMask = tree.viewport()->mask();
        QVERIFY(!oldMask.isEmpty());
        QSpinBox spin; spin.show(); QApplication::processEvents();
        QTemporaryDir candidate; QVERIFY(copyFactory(candidate.path()));
        auto values = metricSource(candidate.path());
        values["TREE_FRAME_RADIUS"] = 18;
        values["TREE_FRAME_BORDER_WIDTH"] = 2;
        values["FIELD_CONTENT_HEIGHT"] = 28;
        QVERIFY(write(candidate.path() + "/metrics.json", QJsonDocument(values).toJson()));
        QVERIFY(Resources::setThemePalette({{"text", QColor("#234567")}}));
        QString error;
        QVERIFY2(Resources::reloadThemeFromDirectory(candidate.path() + "/qss", &error), qPrintable(error));
        QApplication::processEvents();
        const auto resolved = ResourcesTheme::instance().metrics(ResourcesTheme::ControlKind::TreeFrame);
        QCOMPARE(resolved.radius, 18);
        QCOMPARE(resolved.borderWidth, 2);
        QVERIFY(tree.viewport()->mask() != oldMask);
        QVERIFY(qApp->styleSheet().contains(QRegularExpression("border-radius:\\s*18px")));
        spin.resize(180, 100);
        QCOMPARE(spin.height(), 30);
        QCOMPARE(Resources::fontColor(), QColor("#234567"));
    }
    void invalidSnapshotPreservesPaletteMetricsAndStyle_data()
    {
        QTest::addColumn<QString>("fault");
        for (const auto& fault : {"missing-fragment", "unknown-token", "duplicate-definition", "reordered-state", "subtype-template", "unterminated-declaration", "stray-text", "unclosed-block", "unclosed-tail", "extra-block-close", "unclosed-owned-block", "stray-block-text", "missing-metrics", "unknown-metric", "fractional-metric", "negative-metric"})
            QTest::newRow(fault) << QString::fromLatin1(fault);
    }
    void invalidSnapshotPreservesPaletteMetricsAndStyle()
    {
        QFETCH(QString, fault);
        QTemporaryDir candidate; QVERIFY(copyFactory(candidate.path()));
        if (fault == "missing-fragment") QVERIFY(QFile::remove(candidate.path() + "/qss/40_chrome.qss"));
        else if (fault == "unknown-token") {
            QFile file(candidate.path() + "/qss/00_base.qss");
            QVERIFY(file.open(QIODevice::Append));
            file.write("\nQWidget { color: @UNKNOWN_ROLE@; }\n");
        } else if (fault == "duplicate-definition" || fault == "reordered-state" || fault == "subtype-template") {
            QFile file(candidate.path() + "/qss/70_plugin_manager.qss");
            QVERIFY(file.open(QIODevice::Append));
            file.write(fault == "duplicate-definition"
                ? "\nQAbstractSpinBox { background: @SURFACE@; }\n"
                : fault == "reordered-state" ? "\nQToolButton:hover:enabled { border-color: @HOVERBORDER@; }\n"
                : "\nQSpinBox { min-height: 26px; }\n");
        } else if (fault == "unterminated-declaration" || fault == "stray-text") {
            QFile file(candidate.path() + "/qss/00_base.qss");
            QVERIFY(file.open(QIODevice::Append));
            file.write(fault == "unterminated-declaration"
                ? "\nQLabel#InvalidCandidate { padding: 2px; border-radius: 7px }\n"
                : "\nQLabel#InvalidCandidate { invalid; padding: 2px; }\n");
        } else if (fault == "unclosed-block" || fault == "unclosed-tail" || fault == "extra-block-close"
                   || fault == "unclosed-owned-block" || fault == "stray-block-text") {
            QFile file(candidate.path() + "/qss/" + (fault == "unclosed-owned-block" ? "70_plugin_manager.qss" : "00_base.qss"));
            QVERIFY(file.open(QIODevice::Append));
            file.write(fault == "unclosed-block" || fault == "unclosed-owned-block"
                ? "\nQLabel { padding: 2px;\nQToolButton { margin: 1px; }\n"
                : fault == "unclosed-tail" ? "\nQLabel#BrokenTail { padding: 2px;\n"
                : fault == "extra-block-close" ? "\nQLabel#ExtraClose { padding: 2px; }}\n"
                : "\nUnparsedTrailingText\n");
        } else if (fault == "missing-metrics") QVERIFY(QFile::remove(candidate.path() + "/metrics.json"));
        else {
            auto values = metricSource(candidate.path());
            if (fault == "unknown-metric") values["UNKNOWN"] = 1;
            else values["FIELD_CONTENT_HEIGHT"] = fault == "fractional-metric" ? 22.5 : -1;
            QVERIFY(write(candidate.path() + "/metrics.json", QJsonDocument(values).toJson()));
        }
        const auto colors = Resources::themePalette();
        const auto style = qApp->styleSheet();
        const auto height = ResourcesTheme::instance().metric("FIELD_CONTENT_HEIGHT");
        QString error;
        QVERIFY(!Resources::reloadThemeFromDirectory(candidate.path() + "/qss", &error));
        QVERIFY(!error.isEmpty());
        if (fault == "unterminated-declaration" || fault == "stray-text")
            QVERIFY2(error.contains("Unparsed theme declaration"), qPrintable(error));
        if (fault == "unclosed-block" || fault == "unclosed-tail" || fault == "extra-block-close"
            || fault == "unclosed-owned-block" || fault == "stray-block-text")
            QVERIFY2(error.contains("Unparsed theme block"), qPrintable(error));
        QCOMPARE(Resources::themePalette(), colors);
        QCOMPARE(qApp->styleSheet(), style);
        QCOMPARE(ResourcesTheme::instance().metric("FIELD_CONTENT_HEIGHT"), height);
    }

    void mouseFocusDoesNotLeaveASecondHoveredCommand()
    {
        Resources::resetThemePalette();
        QWidget root;
        QHBoxLayout layout(&root);
        QToolButton live, single;
        QIcon liveIcon;
        liveIcon.addFile(":/Resources/icon/cameras-48.png", {}, QIcon::Normal, QIcon::Off);
        liveIcon.addFile(":/Resources/icon/stop-48.png", {}, QIcon::Normal, QIcon::On);
        live.setIcon(liveIcon);
        live.setCheckable(true);
        live.setProperty("iconStateRole", "action");
        single.setIcon(QIcon(":/Resources/icon/camera-48.png"));
        for (auto* button : {&live, &single}) {
            button->setFocusPolicy(Qt::StrongFocus);
            layout.addWidget(button);
        }
        root.show(); root.activateWindow();
        QApplication::processEvents();
        const auto ink = [](QToolButton& button) {
            const auto state = button.isChecked() ? QIcon::On : QIcon::Off;
            const QImage image = button.icon().pixmap(QSize(16, 16), QIcon::Normal, state).toImage();
            for (int y = 0; y < image.height(); ++y)
                for (int x = 0; x < image.width(); ++x)
                    if (image.pixelColor(x, y).alpha() == 255) return image.pixelColor(x, y).rgb();
            return QRgb(0);
        };
        QTest::mouseClick(&live, Qt::LeftButton);
        QTest::mouseClick(&live, Qt::LeftButton);
        QVERIFY(!live.isChecked());
        live.setFocus(Qt::MouseFocusReason);
        QVERIFY(live.hasFocus());
        QTest::mouseMove(&single, single.rect().center());
        live.setAttribute(Qt::WA_UnderMouse, false);
        single.setAttribute(Qt::WA_UnderMouse, true);
        QApplication::processEvents();
        QCOMPARE(ink(live), Resources::themePalette().value("text").rgb());
        QCOMPARE(ink(single), Resources::themePalette().value("hoverBorder").rgb());
        // Tab navigation still receives visible focus even with the pointer elsewhere.
        single.setAttribute(Qt::WA_UnderMouse, false);
        QTest::keyClick(&live, Qt::Key_Tab);
        QTRY_VERIFY(single.hasFocus());
        QVERIFY(root.testAttribute(Qt::WA_KeyboardFocusChange));
        QCOMPARE(single.property("resourcesKeyboardFocus"), QVariant(true));
        QCOMPARE(live.property("resourcesKeyboardFocus"), QVariant(false));
        QCOMPARE(ink(single), Resources::themePalette().value("text").rgb());
        QCOMPARE(ink(live), Resources::themePalette().value("text").rgb());
    }

    void keyboardOutlineFollowsAvailabilityAndPopupRestoration()
    {
        QWidget root;
        QHBoxLayout layout(&root);
        QToolButton first;
        QPushButton second("Run");
        first.setIcon(QIcon(":/Resources/icon/camera-48.png"));
        first.setFocusPolicy(Qt::StrongFocus);
        second.setFocusPolicy(Qt::StrongFocus);
        layout.addWidget(&first); layout.addWidget(&second);
        root.show(); root.activateWindow();
        QApplication::processEvents();
        first.setFocus(Qt::MouseFocusReason);
        QTest::keyClick(&first, Qt::Key_Tab);
        QTRY_VERIFY(second.hasFocus());
        QCOMPARE(second.property("resourcesKeyboardFocus"), QVariant(true));
        QCOMPARE(first.property("resourcesKeyboardFocus"), QVariant(false));
        const QImage keyboard = second.grab().toImage();
        QCOMPARE(keyboard.pixelColor(keyboard.width() / 2, 0), Resources::themePalette().value("focusBorder"));
        // Disabling the focused action keeps keyboard navigation visible on its successor.
        second.setEnabled(false);
        QTRY_VERIFY(first.hasFocus());
        QCOMPARE(second.property("resourcesKeyboardFocus"), QVariant(false));
        QCOMPARE(first.property("resourcesKeyboardFocus"), QVariant(true));
        const QImage glyph = first.grab().toImage();
        QCOMPARE(glyph.pixelColor(glyph.width() / 2, 0), Resources::themePalette().value("focusBorder"));
        // Clicking the same focused control clears the ring without clearing real focus.
        QTest::mouseClick(&first, Qt::LeftButton);
        QVERIFY(first.hasFocus());
        QCOMPARE(first.property("resourcesKeyboardFocus"), QVariant(false));
        second.setEnabled(true);
        QTest::keyClick(&first, Qt::Key_Tab);
        QTRY_VERIFY(second.hasFocus());
        QMenu menu;
        menu.addAction("Example command");
        menu.popup(root.mapToGlobal(QPoint(0, root.height())));
        QApplication::processEvents();
        menu.close();
        root.activateWindow();
        QTRY_VERIFY(second.hasFocus());
        QCOMPARE(second.property("resourcesKeyboardFocus"), QVariant(true));
        root.hide();
        QApplication::processEvents();
        QCOMPARE(second.property("resourcesKeyboardFocus"), QVariant(false));
        QVERIFY(qApp->property("_resourcesKeyboardNavigation").toBool());
    }

    void borderlessCommandsKeepGeometry_data()
    {
        QTest::addColumn<QString>("name");
        for (const auto& name : {"DeviceFeatureFavoritesOnly", "ProcessingConsoleClear", "ProcessingConsoleClose"})
            QTest::newRow(name) << QString::fromLatin1(name);
    }
    void borderlessCommandsKeepGeometry()
    {
        QFETCH(QString, name);
        QWidget root;
        QHBoxLayout layout(&root);
        QToolButton button;
        button.setObjectName(name);
        if (name == "DeviceFeatureFavoritesOnly") button.setIcon(QIcon(":/Resources/icon/camera-48.png"));
        else button.setText("Clear");
        button.setFocusPolicy(Qt::StrongFocus);
        layout.addWidget(&button);
        root.show(); root.activateWindow();
        button.setFocus(Qt::MouseFocusReason);
        QApplication::processEvents();
        const QSize initial = button.size();
        button.clearFocus();
        button.setFocus(Qt::ShortcutFocusReason);
        QApplication::processEvents();
        QCOMPARE(button.property("resourcesKeyboardFocus"), QVariant(true));
        QCOMPARE(button.size(), initial);
        const QImage image = button.grab().toImage();
        QCOMPARE(image.pixelColor(image.width() / 2, 0), Resources::themePalette().value("focusBorder"));
    }

    void windowControlsKeepTheirOriginalInk()
    {
        QMdiSubWindow window;
        QMainWindow mainWindow;
        QDockWidget dockWindow;
        ThemedMdiTitleBar title(&window, nullptr);
        ThemedMainTitleBar mainTitle(&mainWindow, nullptr);
        ThemedDockTitleBar dockTitle(&dockWindow);
        ThemedDialog dialog("Neutral window controls");
        for (QWidget* surface : {static_cast<QWidget*>(&title), static_cast<QWidget*>(&mainTitle),
                                 static_cast<QWidget*>(&dockTitle), static_cast<QWidget*>(&dialog)}) {
            surface->show();
            QApplication::processEvents();
            const auto buttons = surface->findChildren<QPushButton*>();
            QVERIFY(!buttons.isEmpty());
            for (auto* button : buttons) {
                QVERIFY(!button->property("glyphStateTint").isValid());
                for (bool hover : {false, true}) {
                    button->setAttribute(Qt::WA_UnderMouse, hover);
                    if (hover) { QEvent enter(QEvent::Enter); QApplication::sendEvent(button, &enter); }
                    else { QEvent leave(QEvent::Leave); QApplication::sendEvent(button, &leave); }
                    button->setDown(hover);
                    QVERIFY(!button->grab().isNull());
                    QApplication::processEvents();
                    for (const auto mode : {QIcon::Normal, QIcon::Active, QIcon::Selected}) {
                        const QImage image = button->icon().pixmap(QSize(16, 16), mode).toImage();
                        bool hasInk = false;
                        for (int y = 0; y < image.height(); ++y)
                            for (int x = 0; x < image.width(); ++x) {
                                const QColor ink = image.pixelColor(x, y);
                                if (ink.alpha() < 240) continue;
                                hasInk = true;
                                QVERIFY2(qAbs(ink.red() - ink.green()) <= 1, qPrintable(button->objectName() + QStringLiteral(" mode=%1 hover=%2 visible=%3").arg(mode).arg(hover).arg(button->isVisible())));
                                QVERIFY(qAbs(ink.green() - ink.blue()) <= 1);
                                QVERIFY(ink.red() <= 60);
                            }
                        QVERIFY(hasInk);
                    }
                    button->setDown(false);
                }
            }
        }
    }

    void buttonGlyphClassificationAndInteraction()
    {
        const QSize size(16, 16);
        QPixmap neutral(size); neutral.fill(QColor("#3c3c3b"));
        QPixmap multicolor(size); multicolor.fill(Qt::red);
        { QPainter painter(&multicolor); painter.fillRect(0, 0, 8, 16, Qt::green); }
        QWidget root;
        QVBoxLayout layout(&root);
        QToolButton glyph, artwork, text, optedOut;
        for (auto* button : {&glyph, &artwork, &text, &optedOut}) {
            button->setIcon(QIcon(neutral));
            button->setIconSize(size);
            button->setFocusPolicy(Qt::NoFocus);
            layout.addWidget(button);
        }
        glyph.setCheckable(true);
        artwork.setIcon(QIcon(multicolor));
        text.setText("Run"); text.setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        optedOut.setProperty("glyphStateTint", false);
        root.show();
        QApplication::processEvents();
        const auto colors = Resources::themePalette();
        const auto shown = [&](QToolButton& button) {
            return button.icon().pixmap(size, QIcon::Normal,
                button.isChecked() ? QIcon::On : QIcon::Off).toImage();
        };
        const auto outline = [](QToolButton& button) {
            const QImage image = button.grab().toImage();
            return image.pixelColor(image.width() / 2, 0);
        };
        for (auto* button : {&glyph, &artwork, &text, &optedOut}) {
            button->setAttribute(Qt::WA_UnderMouse, true);
            button->update();
        }
        QApplication::processEvents();
        QCOMPARE(glyph.property("resourcesGlyphButton"), QVariant(true));
        QCOMPARE(shown(glyph).pixelColor(8, 8), colors.value("hoverBorder"));
        QVERIFY(outline(glyph) != colors.value("hoverBorder"));
        glyph.setDown(true);
        QCOMPARE(shown(glyph).pixelColor(8, 8), colors.value("focusBorder"));
        QVERIFY(outline(glyph) != colors.value("focusBorder"));
        glyph.setDown(false); glyph.setChecked(true);
        QCOMPARE(shown(glyph).pixelColor(8, 8), colors.value("focusBorder"));
        QVERIFY(outline(glyph) != colors.value("focusBorder"));
        for (auto* button : {&artwork, &text, &optedOut}) {
            QCOMPARE(button->property("resourcesGlyphButton"), QVariant(false));
            QCOMPARE(outline(*button), colors.value("hoverBorder"));
        }
        QCOMPARE(shown(artwork), multicolor.toImage());
        QCOMPARE(shown(optedOut), neutral.toImage());
        // Neutral icons beside text share the ink rule while retaining the button outline.
        QCOMPARE(shown(text).pixelColor(8, 8), colors.value("hoverBorder"));
        glyph.setChecked(false);
        glyph.setIcon(QIcon(multicolor));
        QVERIFY(!glyph.grab().isNull());
        QCOMPARE(glyph.property("resourcesGlyphButton"), QVariant(false));
        QCOMPARE(shown(glyph), multicolor.toImage());
        QCOMPARE(outline(glyph), colors.value("hoverBorder"));
        glyph.setIcon(QIcon(neutral));
        QVERIFY(!glyph.grab().isNull());
        QCOMPARE(glyph.property("resourcesGlyphButton"), QVariant(true));
        QCOMPARE(shown(glyph).pixelColor(8, 8), colors.value("hoverBorder"));
        QVERIFY(outline(glyph) != colors.value("hoverBorder"));
    }

    // Checkable buttons with distinct Off/On artwork show their state and preview the other one.
    void stateTogglePreviewsTheOppositeArtwork()
    {
        Resources::resetThemePalette();
        const auto palette = Resources::themePalette();
        const QString offFile = QStringLiteral(":/Resources/icon/layout-tabs-48.png");
        const QString onFile = QStringLiteral(":/Resources/icon/layout-grid-48.png");
        const QSize size(18, 18);
        // Alpha identifies the artwork; the first opaque pixel identifies its ink.
        const auto alphaOf = [](const QPixmap& pixmap) {
            return pixmap.toImage().convertToFormat(QImage::Format_Alpha8);
        };
        const auto inkOf = [](const QPixmap& pixmap) {
            const QImage image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
            for (int y = 0; y < image.height(); ++y)
                for (int x = 0; x < image.width(); ++x)
                    if (qAlpha(image.pixel(x, y)) == 255) return QColor(image.pixel(x, y)).rgb();
            return QRgb(0);
        };
        const QImage offAlpha = alphaOf(QIcon(offFile).pixmap(size));
        const QImage onAlpha = alphaOf(QIcon(onFile).pixmap(size));
        QVERIFY(offAlpha != onAlpha);
        QWidget root;
        QVBoxLayout layout(&root);
        auto* toggle = new QToolButton;
        QIcon icon;
        icon.addFile(offFile, QSize(), QIcon::Normal, QIcon::Off);
        icon.addFile(onFile, QSize(), QIcon::Normal, QIcon::On);
        toggle->setIcon(icon);
        toggle->setIconSize(size);
        toggle->setCheckable(true);
        toggle->setFocusPolicy(Qt::NoFocus);
        auto* single = new QToolButton;
        single->setIcon(QIcon(offFile));
        single->setIconSize(size);
        single->setCheckable(true);
        single->setFocusPolicy(Qt::NoFocus);
        layout.addWidget(toggle);
        layout.addWidget(single);
        root.show();
        QApplication::processEvents();
        const auto shown = [&](QToolButton* button) {
            return button->icon().pixmap(size, QIcon::Normal, button->isChecked() ? QIcon::On : QIcon::Off);
        };
        const auto hover = [](QToolButton* button, bool on) {
            button->setAttribute(Qt::WA_UnderMouse, on);
            button->update();
            QApplication::processEvents();
        };
        const QRgb neutral = QColor("#3c3c3b").rgb();
        const QRgb hoverInk = palette.value("hoverBorder").rgb();
        const QRgb activeInk = palette.value("focusBorder").rgb();

        // A preceding test may leave the native pointer over this newly shown button.
        hover(toggle, false);
        QCOMPARE(alphaOf(shown(toggle)), offAlpha);
        QCOMPARE(inkOf(shown(toggle)), neutral);
        hover(toggle, true);
        QCOMPARE(alphaOf(shown(toggle)), onAlpha);
        QCOMPARE(inkOf(shown(toggle)), hoverInk);
        toggle->setDown(true);
        QCOMPARE(alphaOf(shown(toggle)), onAlpha);
        QCOMPARE(inkOf(shown(toggle)), activeInk);
        toggle->setDown(false);
        // A state change under the pointer shows the new state, not the opposite preview.
        toggle->setChecked(true);
        QApplication::processEvents();
        QCOMPARE(alphaOf(shown(toggle)), onAlpha);
        QCOMPARE(inkOf(shown(toggle)), activeInk);
        hover(toggle, false);
        QCOMPARE(alphaOf(shown(toggle)), onAlpha);
        QCOMPARE(inkOf(shown(toggle)), activeInk);
        hover(toggle, true);
        QCOMPARE(alphaOf(shown(toggle)), offAlpha);
        QCOMPARE(inkOf(shown(toggle)), hoverInk);
        hover(toggle, false);
        toggle->setEnabled(false);
        QCOMPARE(alphaOf(shown(toggle)), onAlpha);
        toggle->setEnabled(true);

        // Command artwork remains the next action even during hover, press and blocked updates.
        toggle->setProperty("iconStateRole", "action");
        for (bool checked : {false, true}) {
            { const QSignalBlocker blocker(toggle); toggle->setChecked(checked); }
            const QImage expected = checked ? onAlpha : offAlpha;
            hover(toggle, true);
            QCOMPARE(alphaOf(shown(toggle)), expected);
            toggle->setDown(true);
            QCOMPARE(alphaOf(shown(toggle)), expected);
            QCOMPARE(inkOf(shown(toggle)), activeInk);
            toggle->setDown(false);
            hover(toggle, false);
            toggle->setFocusPolicy(Qt::StrongFocus);
            toggle->setFocus();
            QApplication::processEvents();
            QCOMPARE(alphaOf(shown(toggle)), expected);
            toggle->clearFocus();
        }

        // Single-artwork toggles keep their glyph and only change ink.
        hover(single, true);
        QCOMPARE(alphaOf(shown(single)), offAlpha);
        QCOMPARE(inkOf(shown(single)), hoverInk);
        single->setChecked(true);
        QCOMPARE(alphaOf(shown(single)), offAlpha);
        QCOMPARE(inkOf(shown(single)), activeInk);
    }

    void resizeGripRoleIsOptInAndDirectionNeutral()
    {
        const QString sheet = qApp->styleSheet();
        QVERIFY(sheet.contains(QStringLiteral("controlRole=\"resizeGrip\"]::handle:vertical")));
        QVERIFY(sheet.contains(QStringLiteral("controlRole=\"resizeGrip\"]::handle:horizontal")));
        // Hover reveal is the only presentation; no selector keeps the grip visible at rest.
        QVERIFY(!sheet.contains(QStringLiteral("gripVisibility")));

        QWidget host;
        auto* layout = new QVBoxLayout(&host);
        auto* vertical = new QSplitter(Qt::Vertical, &host);
        vertical->addWidget(new QWidget(vertical));
        vertical->addWidget(new QWidget(vertical));
        layout->addWidget(vertical);
        host.resize(360, 300);
        host.show();
        QApplication::processEvents();
        QVERIFY(vertical->handle(1));
        const bool nativeMetrics = QGuiApplication::platformName() != QLatin1String("offscreen");
        const int nativeHeight = vertical->handle(1)->height();
        if (nativeMetrics) QVERIFY(nativeHeight != 9);
        vertical->setProperty("controlRole", QStringLiteral("resizeGrip"));
        vertical->style()->unpolish(vertical);
        vertical->style()->polish(vertical);
        auto* verticalHandle = vertical->handle(1);
        verticalHandle->style()->unpolish(verticalHandle);
        verticalHandle->style()->polish(verticalHandle);
        vertical->update();
        QApplication::processEvents();
        if (nativeMetrics) QCOMPARE(verticalHandle->height(), 9);

        auto* horizontal = new QSplitter(Qt::Horizontal, &host);
        horizontal->setProperty("controlRole", QStringLiteral("resizeGrip"));
        horizontal->addWidget(new QWidget(horizontal));
        horizontal->addWidget(new QWidget(horizontal));
        horizontal->resize(300, 80);
        layout->addWidget(horizontal);
        horizontal->show();
        QApplication::processEvents();
        QVERIFY(horizontal->handle(1));
        if (nativeMetrics) QCOMPARE(horizontal->handle(1)->width(), 9);

        auto* window = host.windowHandle();
        QVERIFY(window);
        const QPoint away(1, 1);
        for (auto* splitter : {vertical, horizontal}) {
            auto* handle = splitter->handle(1);
            QTest::mouseMove(window, away);
            QTRY_VERIFY(!handle->underMouse());
            const auto rest = handle->grab().toImage();
            const auto hit = handle->mapTo(&host, handle->rect().center());
            QTest::mouseMove(window, hit);
            QTRY_VERIFY(handle->underMouse());
            QTRY_VERIFY(handle->grab().toImage() != rest);
            QTest::mouseMove(window, away);
            QTRY_COMPARE(handle->grab().toImage(), rest);
        }
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    // The existing namespace contract admits colors before theme installation.
    if (!Resources::setThemePalette({{"text", QColor("#456789")}})) return 2;
    Resources::installResources(app);
    if (Resources::fontColor() != QColor("#456789")) return 3;
    Resources::resetThemePalette();
    ResourcesThemeTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "ResourcesThemeTests.moc"
