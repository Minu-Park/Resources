#include "Resources.h"
#include "theme/ResourcesTheme.h"
#include <QApplication>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QToolButton>
#include <QTreeWidget>
#include <QtTest>

// These identities exercise the existing consumer admission without loading a device backend.
class QCameraWidget : public QWidget { Q_OBJECT public: using QWidget::QWidget; };
class StaticImageControlWidget : public QWidget { Q_OBJECT public: using QWidget::QWidget; };

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
        QCameraWidget device;
        StaticImageControlWidget source;
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
        QCameraWidget device;
        StaticImageControlWidget source;
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
    void borderlessRolePreservesButtonInputAndGeometry()
    {
        QPushButton button("Action");
        button.setCheckable(true);
        button.resize(120, 24); button.show(); QApplication::processEvents();
        const auto size = button.size();
        auto& theme = ResourcesTheme::instance();
        theme.setRole(button, ResourcesTheme::ControlRole::Borderless);
        QTest::mouseMove(&button, button.rect().center()); QApplication::processEvents();
        const auto image = button.grab().toImage();
        QCOMPARE(image.pixelColor(image.width() / 2, 0), Resources::themePalette().value("surface"));
        QTest::mouseClick(&button, Qt::LeftButton);
        QVERIFY(button.isChecked());
        QCOMPARE(button.size(), size);
        theme.setRole(button, ResourcesTheme::ControlRole::Default);
        QCOMPARE(button.property("resourcesControlRole").toString(), QString("default"));
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
        QVERIFY(qApp->styleSheet().contains("border-radius: 18px"));
        spin.resize(180, 100);
        QCOMPARE(spin.height(), 30);
        QCOMPARE(Resources::fontColor(), QColor("#234567"));
    }
    void invalidSnapshotPreservesPaletteMetricsAndStyle_data()
    {
        QTest::addColumn<QString>("fault");
        for (const auto& fault : {"missing-fragment", "unknown-token", "duplicate-definition", "reordered-state", "subtype-template", "missing-metrics", "unknown-metric", "fractional-metric", "negative-metric"})
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
        QCOMPARE(Resources::themePalette(), colors);
        QCOMPARE(qApp->styleSheet(), style);
        QCOMPARE(ResourcesTheme::instance().metric("FIELD_CONTENT_HEIGHT"), height);
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
