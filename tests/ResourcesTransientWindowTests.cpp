#include "Chrome/ThemedLoadingWidget.h"
#include "Resources.h"

#include <QApplication>
#include <QMenu>
#include <QPushButton>
#include <QProcess>
#include <QSignalSpy>
#include <QTest>

class ResourcesTransientWindowTests : public QObject
{
    Q_OBJECT
private slots:
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
