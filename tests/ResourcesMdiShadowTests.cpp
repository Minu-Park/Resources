#include "Chrome/ThemedMdiArea.h"
#include "Chrome/ThemedMdiContainer.h"

#include <QApplication>
#include <QColor>
#include <QCloseEvent>
#include <QMenuBar>
#include <QMdiSubWindow>
#include <QPointer>
#include <QTest>
#include <QWidget>

#include <algorithm>

namespace {

QWidget* shadowWidget(const ThemedMdiArea& area)
{
    return area.findChild<QWidget*>(QStringLiteral("ThemedMdiShadow"));
}

QMdiSubWindow* addPlainSubWindow(ThemedMdiArea& area,
                                 const QString& name,
                                 const QRect& geometry,
                                 bool showWithoutActivating = false)
{
    auto* subWindow = new QMdiSubWindow;
    subWindow->setObjectName(name);
    subWindow->setWindowFlags(Qt::SubWindow | Qt::FramelessWindowHint);
    subWindow->setAttribute(Qt::WA_DeleteOnClose);
    subWindow->setAttribute(Qt::WA_ShowWithoutActivating, showWithoutActivating);
    subWindow->setWidget(new QWidget);
    area.addSubWindow(subWindow);
    subWindow->setGeometry(geometry);
    subWindow->show();
    return subWindow;
}

void showArea(ThemedMdiArea& area)
{
    area.setFrameShape(QFrame::NoFrame);
    area.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    area.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    area.resize(640, 480);
    area.show();
    QCoreApplication::processEvents();
}

QRect expectedShadowGeometry(const ThemedMdiArea& area,
                             const QWidget& shadow,
                             const QMdiSubWindow& target)
{
    const int extent = shadow.property("shadowExtent").toInt();
    const int offsetX = std::abs(shadow.property("offsetX").toInt());
    const int offsetY = std::abs(shadow.property("offsetY").toInt());
    const int padding = extent + std::max(offsetX, offsetY);
    return target.geometry()
        .adjusted(-padding, -padding, padding, padding)
        .intersected(area.viewport()->rect());
}

QList<QWidget*> directViewportChildren(const ThemedMdiArea& area)
{
    return area.viewport()->findChildren<QWidget*>(
        QString(), Qt::FindDirectChildrenOnly);
}

class CloseIgnoringSubWindow final : public QMdiSubWindow {
protected:
    void closeEvent(QCloseEvent* event) override
    {
        event->ignore();
    }
};

class ViewportReplacingMdiArea final : public ThemedMdiArea {
public:
    void replaceViewport(QWidget* replacement)
    {
        setViewport(replacement);
    }
};

class ScopedEnvironmentVariable final {
public:
    ScopedEnvironmentVariable(const char* name, const QByteArray& value)
        : _name(name)
        , _wasSet(qEnvironmentVariableIsSet(name))
        , _previousValue(qgetenv(name))
    {
        qputenv(name, value);
    }

    ~ScopedEnvironmentVariable()
    {
        if (_wasSet) {
            qputenv(_name.constData(), _previousValue);
        } else {
            qunsetenv(_name.constData());
        }
    }

private:
    QByteArray _name;
    bool _wasSet = false;
    QByteArray _previousValue;
};

} // namespace

class ResourcesMdiShadowTests final : public QObject {
    Q_OBJECT

private slots:
    void activeShadowTracksGeometryAndContract();
    void activeShadowPreservesStackAndActivation();
    void activeShadowTracksStateAttachmentAndLifetime();
    void ignoredCloseRestoresShadow();
    void viewportReplacementRebindsShadow();
    void qssPropertiesDriveShadowMetricsAndCache();
    void profilingTracksPaintWithoutRebuildingCache();
};

void ResourcesMdiShadowTests::activeShadowTracksGeometryAndContract()
{
    ThemedMdiArea area;
    showArea(area);
    area.setShadowMode(ThemedMdiArea::ShadowMode::ActiveWindowOnly);

    QMdiSubWindow* target = addPlainSubWindow(
        area, QStringLiteral("TargetWindow"), QRect(120, 90, 300, 220));
    area.setActiveSubWindow(target);

    QWidget* shadow = shadowWidget(area);
    QVERIFY(shadow);
    QTRY_VERIFY(shadow->isVisible());
    QCOMPARE(area.activeSubWindow(), target);
    QCOMPARE(shadow->parentWidget(), area.viewport());
    QVERIFY(shadow->testAttribute(Qt::WA_TransparentForMouseEvents));
    QVERIFY(!shadow->testAttribute(Qt::WA_NativeWindow));
    QCOMPARE(shadow->focusPolicy(), Qt::NoFocus);
    QVERIFY(!shadow->acceptDrops());
    QVERIFY(target->graphicsEffect() == nullptr);
    QCOMPARE(shadow->geometry(), expectedShadowGeometry(area, *shadow, *target));

    const QPoint localTargetCenter = target->geometry().center() - shadow->geometry().topLeft();
    QVERIFY(!shadow->mask().contains(localTargetCenter));

    target->setGeometry(160, 120, 340, 240);
    QTRY_COMPARE(shadow->geometry(), expectedShadowGeometry(area, *shadow, *target));
    QCOMPARE(target->geometry(), QRect(160, 120, 340, 240));

    shadow->update();
    QCoreApplication::processEvents();
    QTRY_VERIFY(shadow->property("cacheGeneration").toULongLong() > 0);
    const qulonglong cacheGeneration = shadow->property("cacheGeneration").toULongLong();
    for (int index = 0; index < 20; ++index) {
        target->widget()->update();
        QCoreApplication::processEvents();
    }
    QCOMPARE(shadow->property("cacheGeneration").toULongLong(), cacheGeneration);

    target->setGeometry(area.viewport()->rect());
    QTRY_VERIFY(!shadow->isVisible());
    area.resize(760, 560);
    QTRY_VERIFY(shadow->isVisible());
    QCOMPARE(shadow->geometry(), expectedShadowGeometry(area, *shadow, *target));
    area.resize(640, 480);
    QTRY_VERIFY(!shadow->isVisible());
    target->setGeometry(160, 120, 340, 240);
    QTRY_VERIFY(shadow->isVisible());

    target->setAttribute(Qt::WA_NativeWindow, true);
    QTRY_VERIFY(!shadow->isVisible());
    QCOMPARE(target->geometry(), QRect(160, 120, 340, 240));

    area.setShadowMode(ThemedMdiArea::ShadowMode::Disabled);
    QVERIFY(!shadow->isVisible());
    QCOMPARE(target->geometry(), QRect(160, 120, 340, 240));
}

void ResourcesMdiShadowTests::activeShadowPreservesStackAndActivation()
{
    ThemedMdiArea area;
    showArea(area);
    area.setShadowMode(ThemedMdiArea::ShadowMode::ActiveWindowOnly);

    QMdiSubWindow* first = addPlainSubWindow(
        area, QStringLiteral("FirstWindow"), QRect(60, 60, 320, 240));
    QMdiSubWindow* second = addPlainSubWindow(
        area, QStringLiteral("SecondWindow"), QRect(90, 80, 320, 240));
    QMdiSubWindow* third = addPlainSubWindow(
        area, QStringLiteral("ThirdWindow"), QRect(120, 100, 320, 240));
    QWidget* shadow = shadowWidget(area);
    QVERIFY(shadow);

    const auto verifyActivePair = [&](QMdiSubWindow* target) {
        area.setActiveSubWindow(target);
        QTRY_COMPARE(area.activeSubWindow(), target);
        QTRY_VERIFY(shadow->isVisible());
        const QList<QWidget*> children = directViewportChildren(area);
        const int shadowIndex = children.indexOf(shadow);
        const int targetIndex = children.indexOf(target);
        QVERIFY(shadowIndex >= 0);
        QVERIFY(targetIndex >= 0);
        QCOMPARE(shadowIndex + 1, targetIndex);
        QCOMPARE(area.activeSubWindow(), target);
    };

    verifyActivePair(first);
    verifyActivePair(third);
    verifyActivePair(second);

    const QRect firstGeometry = first->geometry();
    const QRect secondGeometry = second->geometry();
    const QRect thirdGeometry = third->geometry();
    QMdiSubWindow* passive = addPlainSubWindow(
        area, QStringLiteral("PassiveWindow"), QRect(150, 120, 260, 180), true);
    QCoreApplication::processEvents();
    QVERIFY(passive->testAttribute(Qt::WA_ShowWithoutActivating));
    // The offscreen platform can activate a newly shown QMdiSubWindow even
    // with WA_ShowWithoutActivating. Re-establish the host's active window,
    // then verify that showing and stacking the shadow never changes it.
    area.setActiveSubWindow(second);
    QTRY_COMPARE(area.activeSubWindow(), second);
    area.setShadowMode(ThemedMdiArea::ShadowMode::Disabled);
    area.setShadowMode(ThemedMdiArea::ShadowMode::ActiveWindowOnly);
    QTRY_COMPARE(area.activeSubWindow(), second);
    QCOMPARE(first->geometry(), firstGeometry);
    QCOMPARE(second->geometry(), secondGeometry);
    QCOMPARE(third->geometry(), thirdGeometry);
    QVERIFY(passive->isVisible());
    verifyActivePair(second);
}

void ResourcesMdiShadowTests::activeShadowTracksStateAttachmentAndLifetime()
{
    ThemedMdiArea area;
    showArea(area);
    area.setShadowMode(ThemedMdiArea::ShadowMode::ActiveWindowOnly);

    auto* target = new QMdiSubWindow;
    target->setWindowFlags(Qt::SubWindow | Qt::FramelessWindowHint);
    target->setAttribute(Qt::WA_DeleteOnClose);
    auto* content = new QWidget;
    auto* menuBar = new QMenuBar;
    auto* container = new ThemedMdiContainer(target, content, menuBar, target);
    target->setWidget(container);
    area.addSubWindow(target);
    target->setGeometry(100, 80, 360, 260);
    target->show();
    area.setActiveSubWindow(target);

    QWidget* shadow = shadowWidget(area);
    QVERIFY(shadow);
    QTRY_VERIFY(shadow->isVisible());

    QWidget* detachedContent = container->takeContent();
    QCOMPARE(detachedContent, content);
    QVERIFY(!shadow->isVisible());
    container->restoreContent(detachedContent);
    QTRY_VERIFY(shadow->isVisible());

    target->showMaximized();
    QTRY_VERIFY(!shadow->isVisible());
    target->showNormal();
    area.setActiveSubWindow(target);
    QTRY_VERIFY(shadow->isVisible());

    area.setViewMode(QMdiArea::TabbedView);
    QTRY_VERIFY(!shadow->isVisible());
    area.setViewMode(QMdiArea::SubWindowView);
    area.setActiveSubWindow(target);
    QTRY_VERIFY(shadow->isVisible());

    target->hide();
    QVERIFY(!shadow->isVisible());
    target->show();
    area.setActiveSubWindow(target);
    QTRY_VERIFY(shadow->isVisible());

    area.setActiveSubWindow(nullptr);
    QTRY_VERIFY(!shadow->isVisible());
    area.setActiveSubWindow(target);
    QTRY_VERIFY(shadow->isVisible());

    QPointer<QMdiSubWindow> guard(target);
    target->close();
    QTRY_VERIFY(guard.isNull());
    QVERIFY(!shadow->isVisible());
    QCOMPARE(area.findChildren<QWidget*>(QStringLiteral("ThemedMdiShadow")).size(), 1);
}

void ResourcesMdiShadowTests::ignoredCloseRestoresShadow()
{
    ThemedMdiArea area;
    showArea(area);
    area.setShadowMode(ThemedMdiArea::ShadowMode::ActiveWindowOnly);

    auto* target = new CloseIgnoringSubWindow;
    target->setWindowFlags(Qt::SubWindow | Qt::FramelessWindowHint);
    target->setWidget(new QWidget);
    area.addSubWindow(target);
    target->setGeometry(100, 80, 320, 240);
    target->show();
    area.setActiveSubWindow(target);

    QWidget* shadow = shadowWidget(area);
    QVERIFY(shadow);
    QTRY_VERIFY(shadow->isVisible());
    QVERIFY(!target->close());
    QVERIFY(target->isVisible());
    QTRY_VERIFY(shadow->isVisible());
    QCOMPARE(area.activeSubWindow(), target);
}

void ResourcesMdiShadowTests::viewportReplacementRebindsShadow()
{
    ViewportReplacingMdiArea area;
    showArea(area);
    area.setShadowMode(ThemedMdiArea::ShadowMode::ActiveWindowOnly);
    QMdiSubWindow* target = addPlainSubWindow(
        area, QStringLiteral("ViewportTarget"), QRect(100, 80, 320, 240));
    area.setActiveSubWindow(target);

    QPointer<QWidget> originalShadow(shadowWidget(area));
    QVERIFY(originalShadow);
    QTRY_VERIFY(originalShadow->isVisible());

    auto* replacement = new QWidget;
    area.replaceViewport(replacement);
    QCOMPARE(area.viewport(), replacement);
    QTRY_VERIFY(originalShadow.isNull());

    QWidget* reboundShadow = shadowWidget(area);
    QVERIFY(reboundShadow);
    QCOMPARE(reboundShadow->parentWidget(), replacement);
    QCOMPARE(target->parentWidget(), replacement);
    QVERIFY(!target->isMinimized());
    QVERIFY(!target->isMaximized());
    QVERIFY(!target->testAttribute(Qt::WA_NativeWindow));
    QVERIFY(!reboundShadow->isVisible());
    target->show();
    area.setActiveSubWindow(target);
    QTRY_COMPARE(area.activeSubWindow(), target);
    QTRY_VERIFY(reboundShadow->isVisible());
    QCOMPARE(reboundShadow->geometry(),
             expectedShadowGeometry(area, *reboundShadow, *target));
}

void ResourcesMdiShadowTests::qssPropertiesDriveShadowMetricsAndCache()
{
    const QString previousStyleSheet = qApp->styleSheet();
    qApp->setStyleSheet(QStringLiteral(
        "QWidget#ThemedMdiShadow {"
        " qproperty-shadowColor: rgba(20, 30, 40, 60);"
        " qproperty-shadowExtent: 18;"
        " qproperty-cornerRadius: 9;"
        " qproperty-offsetX: 2;"
        " qproperty-offsetY: 4;"
        " }"));

    ThemedMdiArea area;
    showArea(area);
    area.setShadowMode(ThemedMdiArea::ShadowMode::ActiveWindowOnly);
    QMdiSubWindow* target = addPlainSubWindow(
        area, QStringLiteral("StyledWindow"), QRect(100, 80, 320, 240));
    area.setActiveSubWindow(target);

    QWidget* shadow = shadowWidget(area);
    QVERIFY(shadow);
    QTRY_VERIFY(shadow->isVisible());
    QCOMPARE(shadow->property("shadowColor").value<QColor>(), QColor(20, 30, 40, 60));
    QCOMPARE(shadow->property("shadowExtent").toInt(), 18);
    QCOMPARE(shadow->property("cornerRadius").toInt(), 9);
    QCOMPARE(shadow->property("offsetX").toInt(), 2);
    QCOMPARE(shadow->property("offsetY").toInt(), 4);
    QCOMPARE(shadow->geometry(), expectedShadowGeometry(area, *shadow, *target));
    QTRY_VERIFY(shadow->property("cacheGeneration").toULongLong() > 0);
    const qulonglong firstGeneration = shadow->property("cacheGeneration").toULongLong();

    qApp->setStyleSheet(QStringLiteral(
        "QWidget#ThemedMdiShadow {"
        " qproperty-shadowColor: rgba(20, 30, 40, 60);"
        " qproperty-shadowExtent: 18;"
        " qproperty-cornerRadius: 7;"
        " qproperty-offsetX: 2;"
        " qproperty-offsetY: 4;"
        " }"));
    QTRY_COMPARE(shadow->property("cornerRadius").toInt(), 7);
    shadow->update();
    QCoreApplication::processEvents();
    QTRY_VERIFY(shadow->property("cacheGeneration").toULongLong() > firstGeneration);

    qApp->setStyleSheet(previousStyleSheet);
}

void ResourcesMdiShadowTests::profilingTracksPaintWithoutRebuildingCache()
{
    {
        ScopedEnvironmentVariable disabledProfile("RESOURCES_MDI_SHADOW_PROFILE", "0");
        ThemedMdiArea disabledArea;
        QWidget* disabledShadow = shadowWidget(disabledArea);
        QVERIFY(disabledShadow);
        QVERIFY(!disabledShadow->property("profilingEnabled").toBool());
    }

    ScopedEnvironmentVariable enabledProfile("RESOURCES_MDI_SHADOW_PROFILE", "1");
    ViewportReplacingMdiArea area;
    showArea(area);
    area.setShadowMode(ThemedMdiArea::ShadowMode::ActiveWindowOnly);
    QMdiSubWindow* target = addPlainSubWindow(
        area, QStringLiteral("ProfiledWindow"), QRect(100, 80, 320, 240));
    area.setActiveSubWindow(target);

    QWidget* shadow = shadowWidget(area);
    QVERIFY(shadow);
    QTRY_VERIFY(shadow->isVisible());
    QVERIFY(shadow->property("profilingEnabled").toBool());
    QTRY_VERIFY(shadow->property("profilePaintCount").toULongLong() > 0);
    QTRY_VERIFY(shadow->property("profileCacheBuildCount").toULongLong() > 0);

    const qulonglong firstGeneration = shadow->property("cacheGeneration").toULongLong();
    const qulonglong firstPaintCount = shadow->property("profilePaintCount").toULongLong();
    const qulonglong firstCacheBuildCount = shadow->property("profileCacheBuildCount").toULongLong();
    for (int index = 0; index < 20; ++index) {
        shadow->repaint();
        QCoreApplication::processEvents();
    }
    QTRY_VERIFY(shadow->property("profilePaintCount").toULongLong() > firstPaintCount);
    QCOMPARE(shadow->property("profileCacheBuildCount").toULongLong(), firstCacheBuildCount);
    QCOMPARE(shadow->property("cacheGeneration").toULongLong(), firstGeneration);

    QPointer<QWidget> originalShadow(shadow);
    area.replaceViewport(new QWidget);
    QTRY_VERIFY(originalShadow.isNull());
    QWidget* reboundShadow = shadowWidget(area);
    QVERIFY(reboundShadow);
    QVERIFY(reboundShadow->property("profilingEnabled").toBool());
    target->show();
    area.setActiveSubWindow(target);
    QTRY_VERIFY(reboundShadow->isVisible());
    const qulonglong reboundPaintCount = reboundShadow->property("profilePaintCount").toULongLong();
    reboundShadow->repaint();
    QCoreApplication::processEvents();
    QTRY_VERIFY(reboundShadow->property("profilePaintCount").toULongLong() > reboundPaintCount);
}

QTEST_MAIN(ResourcesMdiShadowTests)

#include "ResourcesMdiShadowTests.moc"
