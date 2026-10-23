#include "Resources.h"
#include "Chrome/ThemedProgressBar.h"
#include <QApplication>
#include <QDir>
#include <QLabel>
#include <QProgressBar>
#include <QTest>
#include <QVBoxLayout>

class ResourcesProgressTests final : public QObject
{
    Q_OBJECT
private:
    static QImage visibleImage(QWidget& widget)
    {
        const qreal ratio = widget.devicePixelRatioF();
        QImage composed(widget.size() * ratio, QImage::Format_RGB32);
        composed.setDevicePixelRatio(ratio);
        composed.fill(Qt::white);
        { QPainter painter(&composed); widget.render(&painter); }
        return composed;
    }

    static void verifyBusyFill(const QImage& image)
    {
        const int y = image.height() / 2;
        const int inset = qMax(2, qRound(2 * image.devicePixelRatio()));
        for (int x = inset; x < image.width() - inset; ++x)
            QVERIFY2(qGray(image.pixel(x, y)) < 200, "Busy fill must stay full instead of moving a chunk across the track");
    }

private slots:
    void cleanup() { Resources::resetThemePalette(); }

    void standardAndLoadingUseIdenticalDesign()
    {
        ThemedProgressBar loading;
        loading.setObjectName("LoadingProgress");
        loading.resize(300, 8);
        QProgressBar standard;
        standard.setTextVisible(false);
        standard.setObjectName("PluginMarketplaceProgress");
        standard.resize(300, 8);
        loading.setValue(0);
        standard.setValue(0);
        loading.show();
        standard.show();
        QApplication::processEvents();
        QCOMPARE(standard.height(), loading.height());
        const auto standardImage = visibleImage(standard);
        const auto loadingImage = visibleImage(loading);
        if (standardImage != loadingImage) {
            standardImage.save(QCoreApplication::applicationDirPath() + "/standard-empty.png");
            loadingImage.save(QCoreApplication::applicationDirPath() + "/loading-empty.png");
        }
        QCOMPARE(standardImage, loadingImage);
        auto* surface = standard.findChild<QWidget*>("ResourcesProgressSurface");
        QVERIFY(surface);
        for (const auto* role : {"trackColor", "startColor", "endColor", "outlineColor"})
            QCOMPARE(surface->property(role), loading.property(role));
        QCOMPARE(loading.property("startColor").value<QColor>(), QColor("#1e5ea6"));
        QCOMPARE(loading.property("endColor").value<QColor>(), QColor("#0f3259"));
        loading.setEnabled(false);
        standard.setEnabled(false);
        QApplication::processEvents();
        for (int value : {0, 1, 8, 37, 64, 100}) {
            loading.setValue(value);
            standard.setValue(value);
            const auto actual = visibleImage(standard);
            const auto expected = visibleImage(loading);
            if (actual != expected) {
                actual.save(QCoreApplication::applicationDirPath() + "/standard-disabled.png");
                expected.save(QCoreApplication::applicationDirPath() + "/loading-disabled.png");
                qInfo() << value << surface->property("trackColor") << loading.property("trackColor")
                        << surface->property("startColor") << loading.property("startColor");
            }
            QCOMPARE(actual, expected);
        }
        loading.setEnabled(true);
        standard.setEnabled(true);
        QVERIFY(Resources::setThemePalette({{"progressFill", QColor("#225588")}, {"progressTrack", QColor("#e0e0e0")}}));
        QApplication::processEvents();
        QCOMPARE(loading.property("startColor").value<QColor>(), QColor("#225588"));
        QCOMPARE(surface->property("startColor"), loading.property("startColor"));
        QCOMPARE(surface->property("trackColor"), loading.property("trackColor"));
    }

    void busyLightMovesOverStationaryFill_data()
    {
        QTest::addColumn<bool>("vertical");
        QTest::addColumn<bool>("rtl");
        QTest::addColumn<bool>("inverted");
        QTest::newRow("horizontal") << false << false << false;
        QTest::newRow("rtl") << false << true << false;
        QTest::newRow("inverted") << false << false << true;
        QTest::newRow("vertical") << true << false << false;
    }

    void busyLightMovesOverStationaryFill()
    {
        QFETCH(bool, vertical);
        QFETCH(bool, rtl);
        QFETCH(bool, inverted);
        QProgressBar bar;
        bar.setTextVisible(false);
        bar.setOrientation(vertical ? Qt::Vertical : Qt::Horizontal);
        bar.setLayoutDirection(rtl ? Qt::RightToLeft : Qt::LeftToRight);
        bar.setInvertedAppearance(inverted);
        // Text-free controls use the standard 6px thickness horizontally.
        if (vertical) bar.setTextVisible(true);
        bar.resize(vertical ? QSize(18, 300) : QSize(300, 6));
        bar.setRange(0, 0);
        const int busyValue = bar.value();
        bar.show();
        QApplication::processEvents();
        const auto before = bar.grab().toImage();
        QTest::qWait(400);
        const auto after = bar.grab().toImage();
        QVERIFY(before != after);
        if (!vertical) { verifyBusyFill(before); verifyBusyFill(after); }
        else {
            for (int y = 3; y < after.height() - 3; ++y)
                QVERIFY(qGray(after.pixel(after.width() / 2, y)) < 200);
        }
        QCOMPARE(bar.minimum(), 0);
        QCOMPARE(bar.maximum(), 0);
        QCOMPARE(bar.value(), busyValue);
        bar.setRange(0, 100);
        bar.setValue(37);
        QApplication::processEvents();
        QCOMPARE(bar.value(), 37);
        const auto determinate = bar.grab().toImage();
        QVERIFY(determinate != after);
        bar.setEnabled(false);
        QTest::qWait(32);
        const auto disabled = bar.grab().toImage();
        QTest::qWait(100);
        QCOMPARE(bar.grab().toImage(), disabled);
    }

    void zeroValueBusyLoadingUsesFullFillAndSharedSweep()
    {
        QWidget scene;
        scene.setFixedWidth(420);
        auto* layout = new QVBoxLayout(&scene);
        layout->setContentsMargins(12, 12, 12, 12);
        layout->setSpacing(8);
        layout->addWidget(new QLabel("Loading reference / stationary fill + moving light"));
        auto* loading = new ThemedProgressBar;
        loading->setObjectName("LoadingProgress");
        loading->setValue(0);
        loading->setBusy(true);
        layout->addWidget(loading);
        layout->addWidget(new QLabel("Plugin installation / same light sweep"));
        auto* plugin = new QProgressBar;
        plugin->setObjectName("PluginMarketplaceProgress");
        plugin->setTextVisible(false);
        plugin->setRange(0, 0);
        layout->addWidget(plugin);
        scene.show();
        QApplication::processEvents();
        const QString directory = qEnvironmentVariable("RESOURCES_PROGRESS_CAPTURE_DIR");
        if (!directory.isEmpty()) QVERIFY(QDir().mkpath(directory));
        QImage first;
        const int frames = directory.isEmpty() ? 4 : 13;
        for (int frame = 0; frame < frames; ++frame) {
            if (frame) QTest::qWait(250);
            const auto image = loading->grab().toImage();
            verifyBusyFill(image);
            if (!frame) first = image;
            verifyBusyFill(plugin->grab().toImage());
            if (frame == 3) QVERIFY(first != image);
            if (!directory.isEmpty())
                QVERIFY(scene.grab().save(directory + QString("/frame-%1.png").arg(frame)));
        }
        QCOMPARE(loading->getValue(), 0);
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    Resources::installResources(app);
    ResourcesProgressTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "ResourcesProgressTests.moc"
