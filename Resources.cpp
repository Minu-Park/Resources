#include "Resources.h"

#include <QApplication>
#include <QFile>
#include <QIcon>
#include <QStringList>
#include <QTextStream>
#include <QFontDatabase>
#include <QFont>
#include <QMenu>
#include <QFrame>
#include <QComboBox>
#include <QAbstractItemView>
#include <QStatusBar>
#include <QScreen>
#include <QEvent>
#include <QDebug>

// Global-scope helper to execute Q_INIT_RESOURCE, which relies on global symbols.
// This ensures the resource system registers the compiled .qrc binary data.
inline void initResourcesHelper()
{
    Q_INIT_RESOURCE(Resources);
}

// ---------------------------------------------------------------------------
// ResourceStyleFilter: handles runtime style geometry that QSS cannot express
// reliably. Installed once via installResources().
//
// Removes the native popup frame so QSS border is the only visible outline.
// Saves and restores popup geometry because setWindowFlag() recreates the
// native window and loses the original popup position.
//
// Also repositions QComboBox dropdown popups to appear flush below (or above
// when space is insufficient) the combo widget, giving a connected in-place
// expansion appearance rather than a detached floating popup.
//
// Applies QStatusBar contents insets because stylesheet padding does not move
// child item positions consistently.
// ---------------------------------------------------------------------------
class ResourceStyleFilter : public QObject
{
public:
    explicit ResourceStyleFilter(QObject* parent = nullptr) : QObject(parent) {}

protected:
    bool eventFilter(QObject* obj, QEvent* event) override
    {
        if (event->type() == QEvent::Polish) {
            if (auto* menu = qobject_cast<QMenu*>(obj)) {
                menu->setAttribute(Qt::WA_TranslucentBackground, true);
                menu->setWindowFlag(Qt::FramelessWindowHint, true);
                menu->setWindowFlag(Qt::NoDropShadowWindowHint, true);
            }
            else if (auto* combo = qobject_cast<QComboBox*>(obj)) {
                if (auto* view = combo->view()) {
                    if (auto* popup = view->parentWidget()) {
                        popup->setAttribute(Qt::WA_TranslucentBackground, true);
                        popup->setWindowFlag(Qt::FramelessWindowHint, true);
                        popup->setWindowFlag(Qt::NoDropShadowWindowHint, true);
                        if (auto* frame = qobject_cast<QFrame*>(popup)) {
                            frame->setFrameShape(QFrame::NoFrame);
                        }
                        popup->setContentsMargins(0, 0, 0, 0);
                        popup->setProperty("_popupStyled", true);
                    }
                }
            }
        }

        if (event->type() == QEvent::Polish || event->type() == QEvent::Show) {
            if (auto* statusBar = qobject_cast<QStatusBar*>(obj)) {
                applyStatusBarInsets(statusBar);
            }
        }

        if (event->type() == QEvent::Show) {
            if (auto* widget = qobject_cast<QWidget*>(obj)) {
                if (widget->windowFlags() & Qt::Popup) {
                    if (auto* combo = qobject_cast<QComboBox*>(widget->parent())) {
                        QRect geo = widget->geometry();
                        geo = repositionComboPopup(combo, widget, geo);
                        widget->setGeometry(geo);
                    }
                }
            }
        }
        return QObject::eventFilter(obj, event);
    }

private:
    static void applyStatusBarInsets(QStatusBar* statusBar)
    {
        if (!statusBar) return;
        const QMargins margins(4, 0, 4, 0);
        if (statusBar->contentsMargins() != margins) {
            statusBar->setContentsMargins(margins);
        }
    }

    static QRect repositionComboPopup(QComboBox* combo, QWidget* popup, QRect geo)
    {
        const QPoint comboGlobal = combo->mapToGlobal(QPoint(0, 0));
        const int comboW = combo->width();
        const int comboH = combo->height();

        // Container QFrame draws its own 1px border inside its geometry.
        // To align outer edges with combo (which also has 1px border),
        // position container at combo's left edge and match full width.
        const int containerW = comboW;
        const int containerX = comboGlobal.x();
        const int containerY = comboGlobal.y() + comboH;

        geo.setWidth(containerW);
        geo.setHeight(geo.height());

        if (auto* screen = combo->screen()) {
            const QRect screenGeo = screen->availableGeometry();
            if (containerY + geo.height() > screenGeo.bottom()) {
                geo.moveTo(containerX, comboGlobal.y() - geo.height());
            } else {
                geo.moveTo(containerX, containerY);
            }
        } else {
            geo.moveTo(containerX, containerY);
        }

        return geo;
    }
};

namespace Resources
{

static ResourceStyleFilter* s_styleFilter = nullptr;

void installResources(QApplication& app)
{
    initResourcesHelper();

    // Load embedded fonts
    const QStringList fontFiles = {
        QStringLiteral(":/Resources/theme/fonts/Inter-Regular.ttf"),
        QStringLiteral(":/Resources/theme/fonts/Inter-Bold.ttf"),
        QStringLiteral(":/Resources/theme/fonts/JetBrainsMono-Regular.ttf"),
        QStringLiteral(":/Resources/theme/fonts/JetBrainsMono-Bold.ttf")
    };

    for (const QString& fontFile : fontFiles) {
        int id = QFontDatabase::addApplicationFont(fontFile);
        if (id == -1) {
            qWarning() << "Failed to load font from resource:" << fontFile;
        }
    }

    // Set application default font
    QFont defaultFont(QStringLiteral("Inter"));
#if defined(Q_OS_MAC)
    defaultFont.setPointSizeF(12.0);
#else
    defaultFont.setPointSizeF(10.0);
#endif
    app.setFont(defaultFont);

    const QStringList qssFiles = {
        QStringLiteral(":/Resources/theme/qss/00_base.qss"),
        QStringLiteral(":/Resources/theme/qss/10_graphics_engine.qss"),
        QStringLiteral(":/Resources/theme/qss/20_statusbar.qss"),
        QStringLiteral(":/Resources/theme/qss/30_camera_gocator.qss"),
        QStringLiteral(":/Resources/theme/qss/40_chrome.qss"),
        QStringLiteral(":/Resources/theme/qss/50_static_image.qss"),
        QStringLiteral(":/Resources/theme/qss/60_processing.qss")
    };

    QString styleSheet;
    for (const QString& qssFile : qssFiles) {
        QFile file(qssFile);
        if (file.open(QFile::ReadOnly | QFile::Text)) {
            QTextStream stream(&file);
            styleSheet += stream.readAll();
            styleSheet += QLatin1Char('\n');
        }
    }
    app.setStyleSheet(styleSheet);

    app.setWindowIcon(QIcon(QStringLiteral(":/Resources/Icon.png")));

    // Install global event filter for runtime style geometry that QSS cannot
    // drive reliably, such as QComboBox popup frames and QStatusBar insets.
    if (!s_styleFilter) {
        s_styleFilter = new ResourceStyleFilter(&app);
    }
    app.installEventFilter(s_styleFilter);
}
}
