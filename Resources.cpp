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
#include <QLayout>
#include <QGridLayout>
#include <QComboBox>
#include <QAbstractItemView>
#include <QStatusBar>
#include <QScreen>
#include <QRubberBand>
#include <QPalette>
#include <QColor>
#include <QEvent>
#include <QDebug>
#include <QPainter>
#include <QPen>

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
//
// Applies docking QRubberBand visuals at runtime because platform styles can
// paint the native dock target preview without honoring the global QSS rule.
//
// Applies named QLayout margins/spacing because QSS cannot express layout
// geometry. Modules expose semantic layout object names while Resources owns
// the shared layout metrics.
// ---------------------------------------------------------------------------
class ResourceStyleFilter : public QObject
{
public:
    explicit ResourceStyleFilter(QObject* parent = nullptr) : QObject(parent) {}

protected:
    bool eventFilter(QObject* obj, QEvent* event) override
    {
        if (event->type() == QEvent::Paint) {
            if (obj->objectName() == QLatin1String("AutoCompletePopup")) {
                if (auto* widget = qobject_cast<QWidget*>(obj)) {
                    QPainter painter(widget);
                    painter.setRenderHint(QPainter::Antialiasing);
                    painter.setBrush(Qt::white);
                    painter.setPen(QPen(QColor(0xcf, 0xd9, 0xe4), 1));
                    QRectF rect = widget->rect();
                    rect.adjust(0.5, 0.5, -0.5, -0.5);
                    painter.drawRoundedRect(rect, 8.0, 8.0);
                }
            }
        }

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
            else if (auto* rubberBand = qobject_cast<QRubberBand*>(obj)) {
                applyDockRubberBandStyle(rubberBand);
            }
            else if (auto* widget = qobject_cast<QWidget*>(obj)) {
                if (widget->inherits("QComboBoxPrivateContainer") || 
                    (widget->windowFlags() & Qt::Popup && widget->parent() && widget->parent()->inherits("QComboBox")) ||
                    widget->objectName() == QLatin1String("AutoCompletePopup")) {
                    widget->setAttribute(Qt::WA_TranslucentBackground, true);
                    widget->setWindowFlag(Qt::FramelessWindowHint, true);
                    widget->setWindowFlag(Qt::NoDropShadowWindowHint, true);
                    if (auto* frame = qobject_cast<QFrame*>(widget)) {
                        frame->setFrameShape(QFrame::NoFrame);
                    }
                    widget->setContentsMargins(0, 0, 0, 0);
                    widget->setProperty("_popupStyled", true);
                }
            }
        }

        if (event->type() == QEvent::Polish || event->type() == QEvent::Show) {
            if (auto* statusBar = qobject_cast<QStatusBar*>(obj)) {
                applyStatusBarInsets(statusBar);
            }
            else if (auto* rubberBand = qobject_cast<QRubberBand*>(obj)) {
                applyDockRubberBandStyle(rubberBand);
            }
            else if (auto* widget = qobject_cast<QWidget*>(obj)) {
                applyLayoutTheme(widget);
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

    static void applyLayoutTheme(QWidget* widget)
    {
        if (!widget) return;
        applyLayoutTheme(widget->layout());

        const auto layouts = widget->findChildren<QLayout*>();
        for (QLayout* layout : layouts) {
            applyLayoutTheme(layout);
        }
    }

    static void applyLayoutTheme(QLayout* layout)
    {
        if (!layout) return;

        const QString name = layout->objectName();
        if (name == QLatin1String("DeviceRootLayout")) {
            setLayoutMetrics(layout, QMargins(0, 0, 0, 0), 0);
        }
        else if (name == QLatin1String("DeviceTopBarLayout")) {
            setLayoutMetrics(layout, QMargins(12, 12, 12, 12), 10);
        }
        else if (name == QLatin1String("DeviceSelectorLayout")) {
            setLayoutMetrics(layout, QMargins(0, 0, 0, 0), 8);
        }
        else if (name == QLatin1String("DeviceToolLayout")) {
            setLayoutMetrics(layout, QMargins(0, 0, 0, 0), 6);
        }
        else if (name == QLatin1String("DeviceTreePanelLayout")) {
            setLayoutMetrics(layout, QMargins(12, 0, 12, 12), 8);
        }
        else if (name == QLatin1String("ProcessingRootLayout")) {
            setLayoutMetrics(layout, QMargins(12, 12, 12, 12), 8);
        }
        else if (name == QLatin1String("ProcessingInteractiveGroupLayout")) {
            setLayoutMetrics(layout, QMargins(12, 14, 12, 12), 8);
        }
        else if (name == QLatin1String("ProcessingInteractiveGridLayout")) {
            if (auto* grid = qobject_cast<QGridLayout*>(layout)) {
                setGridLayoutMetrics(grid, QMargins(0, 0, 0, 0), 12, 8);
            } else {
                setLayoutMetrics(layout, QMargins(0, 0, 0, 0), 8);
            }
        }

        for (int i = 0; i < layout->count(); ++i) {
            if (QLayoutItem* item = layout->itemAt(i)) {
                applyLayoutTheme(item->layout());
            }
        }
    }

    static void setLayoutMetrics(QLayout* layout, const QMargins& margins, int spacing)
    {
        if (layout->contentsMargins() != margins) {
            layout->setContentsMargins(margins);
        }
        if (layout->spacing() != spacing) {
            layout->setSpacing(spacing);
        }
    }

    static void setGridLayoutMetrics(QGridLayout* layout, const QMargins& margins, int horizontalSpacing, int verticalSpacing)
    {
        if (layout->contentsMargins() != margins) {
            layout->setContentsMargins(margins);
        }
        if (layout->horizontalSpacing() != horizontalSpacing) {
            layout->setHorizontalSpacing(horizontalSpacing);
        }
        if (layout->verticalSpacing() != verticalSpacing) {
            layout->setVerticalSpacing(verticalSpacing);
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

    static void applyDockRubberBandStyle(QRubberBand* rubberBand)
    {
        if (!rubberBand || rubberBand->property("_resourcesRubberBandStyled").toBool()) {
            return;
        }

        rubberBand->setProperty("_resourcesRubberBandStyled", true);
        rubberBand->setAttribute(Qt::WA_StyledBackground, true);
        rubberBand->setAttribute(Qt::WA_TranslucentBackground, false);
        rubberBand->setAutoFillBackground(true);

        QPalette palette = rubberBand->palette();
        palette.setColor(QPalette::Window, QColor(255, 255, 255, 220));
        rubberBand->setPalette(palette);
        rubberBand->setStyleSheet(QStringLiteral(
            "QRubberBand {"
            " background-color: rgba(255, 255, 255, 220);"
            " border: 1px solid #d9e1ea;"
            "}"));
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

#if !defined(Q_OS_MAC)
    app.setWindowIcon(QIcon(QStringLiteral(":/Resources/AppIcons/AppIcon.png")));
#endif

    // Install global event filter for runtime style geometry that QSS cannot
    // drive reliably, such as QComboBox popup frames and QStatusBar insets.
    if (!s_styleFilter) {
        s_styleFilter = new ResourceStyleFilter(&app);
    }
    app.installEventFilter(s_styleFilter);
}
}
