#include "Resources.h"
#include "theme/ResourcesTheme.h"
#include "theme/ThemeStyleSheet.h"
#include <QApplication>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QRegularExpression>
#include <QSpinBox>
#include <QTabBar>
#include <QTemporaryDir>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QLabel>
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

        // Single-artwork toggles keep their glyph and only change ink.
        hover(single, true);
        QCOMPARE(alphaOf(shown(single)), offAlpha);
        QCOMPARE(inkOf(shown(single)), hoverInk);
        single->setChecked(true);
        QCOMPARE(alphaOf(shown(single)), offAlpha);
        QCOMPARE(inkOf(shown(single)), activeInk);
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
