#include "Chrome/ThemedMdiArea.h"
#include "Chrome/ThemedMdiContainer.h"
#include "Resources.h"

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

QList<QWidget*> shadowWidgets(const ThemedMdiArea& area)
{
    return area.findChildren<QWidget*>(QStringLiteral("ThemedMdiShadow"));
}

QWidget* shadowWidgetFor(const ThemedMdiArea& area, const QMdiSubWindow* target)
{
    const QList<QWidget*> shadows = shadowWidgets(area);
    const auto iterator = std::find_if(
        shadows.cbegin(), shadows.cend(),
        [target](const QWidget* shadow) {
            return shadow->property("targetWindow").value<QObject*>() == target;
        });
    return iterator == shadows.cend() ? nullptr : *iterator;
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

QRect localTargetRect(const QWidget& shadow, const QMdiSubWindow& target)
{
    return QRect(target.geometry().topLeft() - shadow.geometry().topLeft(),
                 target.size());
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
    void allVisibleShadowsTrackGeometryStackAndIsolation();
    void activeShadowTracksStateAttachmentAndLifetime();
    void ignoredCloseRestoresShadow();
    void viewportReplacementRebindsShadow();
    void qssPropertiesDriveShadowMetricsAndCache();
    void frameRadiusDrivesContentAndShadowSilhouettes();
    void profilingTracksPaintWithoutRebuildingCache();
    void installedThemeKeepsMdiRadiusConsistent();
};

void ResourcesMdiShadowTests::activeShadowTracksGeometryAndContract()
{
    ThemedMdiArea area;
    showArea(area);
    area.setShadowMode(ThemedMdiArea::ShadowMode::ActiveWindowOnly);

    QMdiSubWindow* target = addPlainSubWindow(
        area, QStringLiteral("TargetWindow"), QRect(120, 90, 300, 220));
    area.setActiveSubWindow(target);

    QPointer<QWidget> shadow = shadowWidgetFor(area, target);
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

    const QRect targetRect = localTargetRect(*shadow, *target);
    const QPoint localTargetCenter = targetRect.center();
    QVERIFY(!shadow->mask().contains(localTargetCenter));
    QVERIFY(shadow->property("cornerRadius").toInt() > 0);
    QVERIFY(shadow->mask().contains(targetRect.topLeft()));
    QVERIFY(!shadow->mask().contains(QPoint(targetRect.center().x(), targetRect.top())));
    QVERIFY(!shadow->mask().contains(QPoint(targetRect.left(), targetRect.center().y())));

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
    area.setShadowMode(ThemedMdiArea::ShadowMode::Disabled);
    QTRY_VERIFY(shadow.isNull());
    area.setShadowMode(ThemedMdiArea::ShadowMode::ActiveWindowOnly);
    shadow = shadowWidgetFor(area, target);
    QVERIFY(shadow);
    QVERIFY(!shadow->isVisible());
    area.resize(760, 560);
    QTRY_VERIFY(shadow->isVisible());
    QCOMPARE(shadow->geometry(), expectedShadowGeometry(area, *shadow, *target));
    {
        const QList<QWidget*> children = directViewportChildren(area);
        QCOMPARE(children.indexOf(shadow) + 1, children.indexOf(target));
    }
    area.resize(640, 480);
    QTRY_VERIFY(!shadow->isVisible());
    target->setGeometry(160, 120, 340, 240);
    QTRY_VERIFY(shadow->isVisible());

    const QList<QRect> clippedTargetGeometries = {
        QRect(-20, 80, 340, 240),
        QRect(140, -20, 340, 240),
        QRect(area.viewport()->width() - 320, 80, 340, 240),
        QRect(140, area.viewport()->height() - 220, 340, 240),
        QRect(-20, -20, 340, 240)};
    for (const QRect& geometry : clippedTargetGeometries) {
        target->setGeometry(geometry);
        QTRY_VERIFY(shadow->isVisible());
        QTRY_COMPARE(shadow->geometry(), expectedShadowGeometry(area, *shadow, *target));
        QVERIFY(!shadow->mask().isEmpty());
    }
    target->setGeometry(160, 120, 340, 240);
    QTRY_VERIFY(shadow->isVisible());

    target->setAttribute(Qt::WA_NativeWindow, true);
    QTRY_VERIFY(!shadow->isVisible());
    QCOMPARE(target->geometry(), QRect(160, 120, 340, 240));

    area.setShadowMode(ThemedMdiArea::ShadowMode::Disabled);
    QTRY_VERIFY(shadow.isNull());
    QVERIFY(shadowWidgets(area).isEmpty());
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
    const auto verifyActivePair = [&](QMdiSubWindow* target) {
        area.setActiveSubWindow(target);
        QTRY_COMPARE(area.activeSubWindow(), target);
        QWidget* shadow = nullptr;
        QTRY_VERIFY((shadow = shadowWidgetFor(area, target)) != nullptr);
        QTRY_VERIFY(shadow->isVisible());
        for (QWidget* candidate : shadowWidgets(area)) {
            QCOMPARE(candidate->isVisible(), candidate == shadow);
        }
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

void ResourcesMdiShadowTests::allVisibleShadowsTrackGeometryStackAndIsolation()
{
    ThemedMdiArea area;
    showArea(area);
    area.setShadowMode(ThemedMdiArea::ShadowMode::AllVisibleWindows);

    QMdiSubWindow* first = addPlainSubWindow(
        area, QStringLiteral("AllFirstWindow"), QRect(60, 60, 300, 220));
    QMdiSubWindow* second = addPlainSubWindow(
        area, QStringLiteral("AllSecondWindow"), QRect(90, 80, 300, 220));
    QMdiSubWindow* third = addPlainSubWindow(
        area, QStringLiteral("AllThirdWindow"), QRect(120, 100, 300, 220), true);
    area.setActiveSubWindow(second);

    QTRY_COMPARE(shadowWidgets(area).size(), 3);
    const auto verifyAllPairs = [&]() {
        const QList<QWidget*> children = directViewportChildren(area);
        for (QMdiSubWindow* target : area.subWindowList(QMdiArea::StackingOrder)) {
            QWidget* shadow = shadowWidgetFor(area, target);
            if (!shadow || !shadow->isVisible()
                || shadow->geometry() != expectedShadowGeometry(area, *shadow, *target)
                || children.indexOf(shadow) + 1 != children.indexOf(target)) {
                return false;
            }
            if (!shadow->testAttribute(Qt::WA_TransparentForMouseEvents)
                || shadow->testAttribute(Qt::WA_NativeWindow)
                || shadow->focusPolicy() != Qt::NoFocus
                || shadow->acceptDrops()) {
                return false;
            }
        }
        return true;
    };
    QTRY_VERIFY(verifyAllPairs());

    const QPoint passThroughPoint(
        second->geometry().left() - 2,
        second->geometry().center().y());
    QWidget* passThroughShadow = shadowWidgetFor(area, second);
    QVERIFY(passThroughShadow);
    QVERIFY(passThroughShadow->geometry().contains(passThroughPoint));
    QVERIFY(passThroughShadow->mask().contains(
        passThroughPoint - passThroughShadow->pos()));
    QWidget* hitWidget = area.viewport()->childAt(passThroughPoint);
    while (hitWidget && hitWidget->parentWidget() != area.viewport()) {
        hitWidget = hitWidget->parentWidget();
    }
    QCOMPARE(hitWidget, first);
    const auto globalViewportChildAt = [&]() {
        QWidget* candidate = QApplication::widgetAt(
            area.viewport()->mapToGlobal(passThroughPoint));
        while (candidate && candidate->parentWidget() != area.viewport()) {
            candidate = candidate->parentWidget();
        }
        return candidate;
    };
    QTRY_COMPARE(globalViewportChildAt(), static_cast<QWidget*>(first));

    const QList<QMdiSubWindow*> activationOrder = {first, third, second};
    for (QMdiSubWindow* target : activationOrder) {
        area.setActiveSubWindow(target);
        QTRY_COMPARE(area.activeSubWindow(), target);
        QTRY_VERIFY(verifyAllPairs());
        QCOMPARE(shadowWidgets(area).size(), 3);
    }

    first->raise();
    QTRY_VERIFY(verifyAllPairs());
    third->lower();
    QTRY_VERIFY(verifyAllPairs());
    second->raise();
    QTRY_VERIFY(verifyAllPairs());

    QWidget* firstShadow = shadowWidgetFor(area, first);
    QWidget* secondShadow = shadowWidgetFor(area, second);
    QWidget* thirdShadow = shadowWidgetFor(area, third);
    QVERIFY(firstShadow);
    QVERIFY(secondShadow);
    QVERIFY(thirdShadow);
    QCOMPARE(firstShadow->property("shadowColor").value<QColor>(), QColor(22, 32, 43, 14));
    QCOMPARE(firstShadow->property("shadowExtent").toInt(), 6);
    QCOMPARE(firstShadow->property("offsetX").toInt(), 0);
    QCOMPARE(firstShadow->property("offsetY").toInt(), 1);
    QTRY_VERIFY(firstShadow->property("cacheGeneration").toULongLong() > 0);
    const QRect secondShadowGeometry = secondShadow->geometry();
    const QRect thirdShadowGeometry = thirdShadow->geometry();
    first->setGeometry(40, 45, 330, 230);
    QTRY_COMPARE(firstShadow->geometry(), expectedShadowGeometry(area, *firstShadow, *first));
    QCOMPARE(secondShadow->geometry(), secondShadowGeometry);
    QCOMPARE(thirdShadow->geometry(), thirdShadowGeometry);

    first->hide();
    QTRY_VERIFY(!firstShadow->isVisible());
    QVERIFY(secondShadow->isVisible());
    QVERIFY(thirdShadow->isVisible());
    first->show();
    QTRY_VERIFY(firstShadow->isVisible());

    first->showMinimized();
    QTRY_VERIFY(!firstShadow->isVisible());
    QVERIFY(secondShadow->isVisible());
    QVERIFY(thirdShadow->isVisible());
    first->showNormal();
    QTRY_VERIFY(firstShadow->isVisible());

    first->showMaximized();
    QTRY_VERIFY(!firstShadow->isVisible());
    QVERIFY(secondShadow->isVisible());
    QVERIFY(thirdShadow->isVisible());
    first->showNormal();
    QTRY_VERIFY(firstShadow->isVisible());

    third->setAttribute(Qt::WA_AlwaysStackOnTop, true);
    third->move(third->x() + 1, third->y());
    QTRY_VERIFY(!thirdShadow->isVisible());
    QVERIFY(firstShadow->isVisible());
    QVERIFY(secondShadow->isVisible());
    third->setAttribute(Qt::WA_AlwaysStackOnTop, false);
    third->move(third->x() - 1, third->y());
    QTRY_VERIFY(thirdShadow->isVisible());

    QPointer<QMdiSubWindow> thirdGuard(third);
    QPointer<QWidget> thirdShadowGuard(thirdShadow);
    third->deleteLater();
    QTRY_VERIFY(thirdGuard.isNull());
    QTRY_VERIFY(thirdShadowGuard.isNull());
    QTRY_COMPARE(shadowWidgets(area).size(), 2);
    QVERIFY(firstShadow->isVisible());
    QVERIFY(secondShadow->isVisible());

    area.setActiveSubWindow(second);
    area.setShadowMode(ThemedMdiArea::ShadowMode::ActiveWindowOnly);
    QTRY_VERIFY(!firstShadow->isVisible());
    QTRY_VERIFY(secondShadow->isVisible());
    area.setShadowMode(ThemedMdiArea::ShadowMode::AllVisibleWindows);
    QTRY_VERIFY(firstShadow->isVisible());
    QTRY_VERIFY(secondShadow->isVisible());

    QPointer<QWidget> firstShadowGuard(firstShadow);
    QPointer<QWidget> secondShadowGuard(secondShadow);
    area.setShadowMode(ThemedMdiArea::ShadowMode::Disabled);
    QTRY_VERIFY(firstShadowGuard.isNull());
    QTRY_VERIFY(secondShadowGuard.isNull());
    QVERIFY(shadowWidgets(area).isEmpty());
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

    QPointer<QWidget> shadow = shadowWidgetFor(area, target);
    QVERIFY(shadow);
    QTRY_VERIFY(shadow->isVisible());
    QTRY_VERIFY(!content->mask().isEmpty());

    QWidget* detachedContent = container->takeContent();
    QCOMPARE(detachedContent, content);
    QVERIFY(!shadow->isVisible());
    QVERIFY(detachedContent->mask().isEmpty());
    container->restoreContent(detachedContent);
    QTRY_VERIFY(shadow->isVisible());
    QTRY_VERIFY(!content->mask().isEmpty());

    target->showMaximized();
    QTRY_VERIFY(!shadow->isVisible());
    QTRY_VERIFY(content->mask().isEmpty());
    target->showNormal();
    area.setActiveSubWindow(target);
    QTRY_VERIFY(shadow->isVisible());
    QTRY_VERIFY(!content->mask().isEmpty());

    target->showMinimized();
    QTRY_VERIFY(!shadow->isVisible());
    QTRY_VERIFY(content->mask().isEmpty());
    target->showNormal();
    area.setActiveSubWindow(target);
    QTRY_VERIFY(shadow->isVisible());
    QTRY_VERIFY(!content->mask().isEmpty());

    target->showFullScreen();
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
    QTRY_VERIFY(shadow.isNull());
    QCOMPARE(shadowWidgets(area).size(), 0);
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

    QWidget* shadow = shadowWidgetFor(area, target);
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
    area.setShadowMode(ThemedMdiArea::ShadowMode::AllVisibleWindows);
    QMdiSubWindow* target = addPlainSubWindow(
        area, QStringLiteral("ViewportTarget"), QRect(100, 80, 320, 240));
    QMdiSubWindow* secondTarget = addPlainSubWindow(
        area, QStringLiteral("SecondViewportTarget"), QRect(150, 120, 300, 220));
    area.setActiveSubWindow(target);

    QPointer<QWidget> originalShadow(shadowWidgetFor(area, target));
    QPointer<QWidget> originalSecondShadow(shadowWidgetFor(area, secondTarget));
    QVERIFY(originalShadow);
    QVERIFY(originalSecondShadow);
    QTRY_VERIFY(originalShadow->isVisible() && originalSecondShadow->isVisible());

    auto* replacement = new QWidget;
    area.replaceViewport(replacement);
    QCOMPARE(area.viewport(), replacement);
    QTRY_VERIFY(originalShadow.isNull());
    QTRY_VERIFY(originalSecondShadow.isNull());

    QWidget* reboundShadow = nullptr;
    QWidget* reboundSecondShadow = nullptr;
    QTRY_VERIFY((reboundShadow = shadowWidgetFor(area, target)) != nullptr);
    QTRY_VERIFY((reboundSecondShadow = shadowWidgetFor(area, secondTarget)) != nullptr);
    QCOMPARE(reboundShadow->parentWidget(), replacement);
    QCOMPARE(reboundSecondShadow->parentWidget(), replacement);
    QCOMPARE(target->parentWidget(), replacement);
    QCOMPARE(secondTarget->parentWidget(), replacement);
    QVERIFY(!target->isMinimized());
    QVERIFY(!target->isMaximized());
    QVERIFY(!target->testAttribute(Qt::WA_NativeWindow));
    QVERIFY(!reboundShadow->isVisible());
    target->show();
    secondTarget->show();
    area.setActiveSubWindow(target);
    QTRY_COMPARE(area.activeSubWindow(), target);
    QTRY_VERIFY(reboundShadow->isVisible() && reboundSecondShadow->isVisible());
    QCOMPARE(reboundShadow->geometry(),
             expectedShadowGeometry(area, *reboundShadow, *target));
    QCOMPARE(reboundSecondShadow->geometry(),
             expectedShadowGeometry(area, *reboundSecondShadow, *secondTarget));
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

    QWidget* shadow = shadowWidgetFor(area, target);
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

    const QRect firstTargetRect = localTargetRect(*shadow, *target);
    QVERIFY(shadow->mask().contains(firstTargetRect.topLeft()));
    QTRY_VERIFY(shadow->property("cacheCornerAlpha").toInt() > 0);
    QCOMPARE(shadow->property("cacheInteriorAlpha").toInt(), 0);

    qApp->setStyleSheet(QStringLiteral(
        "QWidget#ThemedMdiShadow {"
        " qproperty-shadowColor: rgba(20, 30, 40, 60);"
        " qproperty-shadowExtent: 18;"
        " qproperty-cornerRadius: 0;"
        " qproperty-offsetX: 2;"
        " qproperty-offsetY: 4;"
        " }"));
    QTRY_COMPARE(shadow->property("cornerRadius").toInt(), 0);
    shadow->update();
    QCoreApplication::processEvents();
    QTRY_VERIFY(shadow->property("cacheGeneration").toULongLong() > firstGeneration);
    QVERIFY(!shadow->mask().contains(localTargetRect(*shadow, *target).topLeft()));

    const qulonglong secondGeneration = shadow->property("cacheGeneration").toULongLong();
    qApp->setStyleSheet(QStringLiteral(
        "QWidget#ThemedMdiShadow {"
        " qproperty-shadowColor: rgba(20, 30, 40, 60);"
        " qproperty-shadowExtent: 18;"
        " qproperty-cornerRadius: 7;"
        " qproperty-offsetX: 2;"
        " qproperty-offsetY: 4;"
        " }"));
    QTRY_COMPARE(shadow->property("cornerRadius").toInt(), 7);
    QTRY_VERIFY(shadow->mask().contains(localTargetRect(*shadow, *target).topLeft()));
    shadow->update();
    QCoreApplication::processEvents();
    QTRY_VERIFY(shadow->property("cacheGeneration").toULongLong() > secondGeneration);

    qApp->setStyleSheet(previousStyleSheet);
}

void ResourcesMdiShadowTests::frameRadiusDrivesContentAndShadowSilhouettes()
{
    const QString previousStyleSheet = qApp->styleSheet();
    qApp->setStyleSheet(QStringLiteral(
        "QWidget#ThemedMdiContainer { qproperty-frameCornerRadius: 8; }"
        "QWidget#ThemedMdiShadow { qproperty-cornerRadius: 12; }"));

    ThemedMdiArea area;
    showArea(area);
    area.setShadowMode(ThemedMdiArea::ShadowMode::AllVisibleWindows);

    auto* target = new QMdiSubWindow;
    target->setWindowFlags(Qt::SubWindow | Qt::FramelessWindowHint);
    auto* content = new QWidget;
    auto* container = new ThemedMdiContainer(target, content, new QMenuBar, target);
    target->setWidget(container);
    area.addSubWindow(target);
    target->setGeometry(100, 80, 360, 260);
    target->show();
    area.setActiveSubWindow(target);

    auto* secondTarget = new QMdiSubWindow;
    secondTarget->setWindowFlags(Qt::SubWindow | Qt::FramelessWindowHint);
    auto* secondContent = new QWidget;
    auto* secondContainer = new ThemedMdiContainer(
        secondTarget, secondContent, new QMenuBar, secondTarget);
    secondTarget->setWidget(secondContainer);
    area.addSubWindow(secondTarget);
    secondTarget->setGeometry(180, 140, 320, 220);
    secondTarget->show();
    secondContainer->setFrameCornerRadius(4);

    QWidget* shadow = shadowWidgetFor(area, target);
    QWidget* secondShadow = shadowWidgetFor(area, secondTarget);
    QVERIFY(shadow);
    QVERIFY(secondShadow);
    QTRY_VERIFY(shadow->isVisible() && secondShadow->isVisible());
    QTRY_COMPARE(container->frameCornerRadius(), 8);
    QTRY_COMPARE(shadow->property("cornerRadius").toInt(), 8);
    QTRY_COMPARE(secondShadow->property("cornerRadius").toInt(), 4);
    QTRY_VERIFY(!content->size().isEmpty());
    QTRY_VERIFY(!content->mask().isEmpty());
    QVERIFY(!content->mask().contains(QPoint(0, content->height() - 1)));

    const QSize resizedContentSize(
        std::max(content->width() - 24, 32),
        std::max(content->height() - 24, 32));
    content->resize(resizedContentSize);
    QCOMPARE(content->size(), resizedContentSize);
    QCOMPARE(content->mask().boundingRect(), content->rect());
    QVERIFY(!content->mask().contains(QPoint(0, content->height() - 1)));
    QVERIFY(!content->mask().contains(
        QPoint(content->width() - 1, content->height() - 1)));
    QVERIFY(content->mask().contains(
        QPoint(content->width() / 2, content->height() - 1)));

    const QSize grownContentSize = resizedContentSize + QSize(12, 12);
    content->resize(grownContentSize);
    QCOMPARE(content->size(), grownContentSize);
    QCOMPARE(content->mask().boundingRect(), content->rect());
    QVERIFY(!content->mask().contains(QPoint(0, content->height() - 1)));
    QVERIFY(!content->mask().contains(
        QPoint(content->width() - 1, content->height() - 1)));
    QVERIFY(content->mask().contains(
        QPoint(content->width() / 2, content->height() - 1)));

    container->setFrameCornerRadius(0);
    QTRY_COMPARE(shadow->property("cornerRadius").toInt(), 0);
    QCOMPARE(secondShadow->property("cornerRadius").toInt(), 4);
    QTRY_VERIFY(content->mask().isEmpty());
    QVERIFY(!shadow->mask().contains(localTargetRect(*shadow, *target).topLeft()));

    container->setFrameCornerRadius(7);
    QTRY_COMPARE(shadow->property("cornerRadius").toInt(), 7);
    QCOMPARE(secondShadow->property("cornerRadius").toInt(), 4);
    QTRY_VERIFY(!content->mask().isEmpty());
    QTRY_VERIFY(shadow->mask().contains(localTargetRect(*shadow, *target).topLeft()));

    qApp->setStyleSheet(previousStyleSheet);
}

void ResourcesMdiShadowTests::profilingTracksPaintWithoutRebuildingCache()
{
    {
        ScopedEnvironmentVariable disabledProfile("RESOURCES_MDI_SHADOW_PROFILE", "0");
        ThemedMdiArea disabledArea;
        showArea(disabledArea);
        disabledArea.setShadowMode(ThemedMdiArea::ShadowMode::ActiveWindowOnly);
        QMdiSubWindow* disabledTarget = addPlainSubWindow(
            disabledArea, QStringLiteral("UnprofiledWindow"), QRect(100, 80, 320, 240));
        disabledArea.setActiveSubWindow(disabledTarget);
        QWidget* disabledShadow = nullptr;
        QTRY_VERIFY((disabledShadow = shadowWidgetFor(disabledArea, disabledTarget)) != nullptr);
        QVERIFY(disabledShadow);
        QVERIFY(!disabledShadow->property("profilingEnabled").toBool());
    }

    ScopedEnvironmentVariable enabledProfile("RESOURCES_MDI_SHADOW_PROFILE", "1");
    ViewportReplacingMdiArea area;
    showArea(area);
    area.setShadowMode(ThemedMdiArea::ShadowMode::AllVisibleWindows);
    QMdiSubWindow* target = addPlainSubWindow(
        area, QStringLiteral("ProfiledWindow"), QRect(100, 80, 320, 240));
    QMdiSubWindow* secondTarget = addPlainSubWindow(
        area, QStringLiteral("SecondProfiledWindow"), QRect(150, 120, 300, 220));
    area.setActiveSubWindow(target);

    QWidget* shadow = shadowWidgetFor(area, target);
    QWidget* secondShadow = shadowWidgetFor(area, secondTarget);
    QVERIFY(shadow);
    QVERIFY(secondShadow);
    QTRY_VERIFY(shadow->isVisible() && secondShadow->isVisible());
    QVERIFY(shadow->property("profilingEnabled").toBool());
    QTRY_VERIFY(shadow->property("profilePaintCount").toULongLong() > 0);
    QTRY_VERIFY(shadow->property("profileCacheBuildCount").toULongLong() > 0);

    const qulonglong firstGeneration = shadow->property("cacheGeneration").toULongLong();
    const qulonglong secondGeneration = secondShadow->property("cacheGeneration").toULongLong();
    const qulonglong firstPaintCount = shadow->property("profilePaintCount").toULongLong();
    const qulonglong firstCacheBuildCount = shadow->property("profileCacheBuildCount").toULongLong();
    for (int index = 0; index < 20; ++index) {
        shadow->repaint();
        secondShadow->repaint();
        QCoreApplication::processEvents();
    }
    QTRY_VERIFY(shadow->property("profilePaintCount").toULongLong() > firstPaintCount);
    QCOMPARE(shadow->property("profileCacheBuildCount").toULongLong(), firstCacheBuildCount);
    QCOMPARE(shadow->property("cacheGeneration").toULongLong(), firstGeneration);
    QCOMPARE(secondShadow->property("cacheGeneration").toULongLong(), secondGeneration);

    QPointer<QWidget> originalShadow(shadow);
    QPointer<QWidget> originalSecondShadow(secondShadow);
    area.replaceViewport(new QWidget);
    QTRY_VERIFY(originalShadow.isNull());
    QTRY_VERIFY(originalSecondShadow.isNull());
    QWidget* reboundShadow = nullptr;
    QTRY_VERIFY((reboundShadow = shadowWidgetFor(area, target)) != nullptr);
    QVERIFY(reboundShadow->property("profilingEnabled").toBool());
    target->show();
    secondTarget->show();
    area.setActiveSubWindow(target);
    QTRY_VERIFY(reboundShadow->isVisible());
    QTRY_VERIFY(shadowWidgetFor(area, secondTarget)->isVisible());
    const qulonglong reboundPaintCount = reboundShadow->property("profilePaintCount").toULongLong();
    reboundShadow->repaint();
    QCoreApplication::processEvents();
    QTRY_VERIFY(reboundShadow->property("profilePaintCount").toULongLong() > reboundPaintCount);
}

void ResourcesMdiShadowTests::installedThemeKeepsMdiRadiusConsistent()
{
    const QString previousStyleSheet = qApp->styleSheet();
    Resources::installResources(*qApp);

    ThemedMdiArea area;
    showArea(area);
    area.setShadowMode(ThemedMdiArea::ShadowMode::AllVisibleWindows);

    auto* target = new QMdiSubWindow;
    target->setWindowFlags(Qt::SubWindow | Qt::FramelessWindowHint);
    auto* content = new QWidget;
    auto* container = new ThemedMdiContainer(target, content, new QMenuBar, target);
    target->setWidget(container);
    area.addSubWindow(target);
    target->setGeometry(100, 80, 360, 260);
    target->show();
    area.setActiveSubWindow(target);

    QWidget* shadow = nullptr;
    QTRY_VERIFY((shadow = shadowWidgetFor(area, target)) != nullptr);
    QTRY_VERIFY(shadow->isVisible());
    QTRY_COMPARE(container->frameCornerRadius(), 12);
    QTRY_COMPARE(shadow->property("cornerRadius").toInt(), 12);
    QTRY_VERIFY(!content->mask().isEmpty());

    qApp->setStyleSheet(previousStyleSheet);
}

QTEST_MAIN(ResourcesMdiShadowTests)

#include "ResourcesMdiShadowTests.moc"
