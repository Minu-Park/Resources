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
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QAbstractItemView>
#include <QHeaderView>
#include <QStatusBar>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QTreeWidget>
#include <QScreen>
#include <QRubberBand>
#include <QPalette>
#include <QColor>
#include <QEvent>
#include <QMouseEvent>
#include <QDebug>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QBitmap>
#include <QRegion>

#ifdef Q_OS_WIN
#include <dwmapi.h>
#endif

// Global-scope helper to execute Q_INIT_RESOURCE, which relies on global symbols.
// This ensures the resource system registers the compiled .qrc binary data.
inline void initResourcesHelper()
{
    Q_INIT_RESOURCE(Resources);
}

// ---------------------------------------------------------------------------
class DeviceFeatureTreeDelegate : public QStyledItemDelegate
{
public:
    explicit DeviceFeatureTreeDelegate(QTreeWidget* tree)
        : QStyledItemDelegate(tree),
          _tree(tree)
    {
    }

    QSize sizeHint(
        const QStyleOptionViewItem& option,
        const QModelIndex& index) const override
    {
        QSize hint = QStyledItemDelegate::sizeHint(option, index);
        hint.setHeight(qMax(hint.height(), 24));
        if (_tree) {
            if (QWidget* editor = _tree->indexWidget(index)) {
                editor->ensurePolished();
                hint.setHeight(qMax(
                    hint.height(),
                    qMin(editor->sizeHint().height(), editor->maximumHeight()) + 2));
            }
        }
        return hint;
    }

private:
    QTreeWidget* _tree = nullptr;
};

// ---------------------------------------------------------------------------
static bool isDeviceFeatureTreeViewport(const QObject* object)
{
    const auto* viewport = qobject_cast<const QWidget*>(object);
    const auto* tree = viewport ? qobject_cast<const QTreeWidget*>(viewport->parentWidget()) : nullptr;
    return tree
        && tree->viewport() == viewport
        && tree->property("treeRole").toString() == QLatin1String("DeviceFeatureTree");
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
        if (event->type() == QEvent::MouseMove && isDeviceFeatureTreeViewport(obj)) {
            const auto* mouseEvent = static_cast<const QMouseEvent*>(event);
            if (mouseEvent->buttons() == Qt::NoButton) {
                // Persistent cell widgets receive their own hover event first. Do not let
                // the containing item view turn a pointer-only move into a new current item
                // and focus transfer.
                return true;
            }
        }

        if (event->type() == QEvent::Paint) {
            if (auto* menu = qobject_cast<QMenu*>(obj)) {
                paintPopupRoundedRect(menu, true);
            }
            else if (auto* widget = qobject_cast<QWidget*>(obj)) {
                if (widget->objectName() == QLatin1String("AutoCompletePopup") ||
                    widget->objectName() == QLatin1String("SignatureHelpLabel") ||
                    widget->inherits("QTipLabel")) {
                    paintPopupRoundedRect(widget, true);
                } else if (widget->property("_popupStyled").toBool()) {
                    paintPopupRoundedRect(widget, false);
                }
            }
        }

        if (event->type() == QEvent::Show || event->type() == QEvent::Resize) {
            if (auto* menu = qobject_cast<QMenu*>(obj)) {
                applyPopupMask(menu, true);
            }
            else if (auto* widget = qobject_cast<QWidget*>(obj)) {
                if (widget->objectName() == QLatin1String("AutoCompletePopup") ||
                    widget->objectName() == QLatin1String("SignatureHelpLabel") ||
                    widget->inherits("QTipLabel")) {
                    applyPopupMask(widget, true);
                } else if (widget->property("_popupStyled").toBool()) {
                    applyPopupMask(widget, false);
                }
            }
        }

        if (event->type() == QEvent::Polish) {
            if (auto* menu = qobject_cast<QMenu*>(obj)) {
                // Window flags first (single call to avoid multiple native window recreations),
                // then translucent attribute last so it applies to the final native window.
                menu->setWindowFlags(menu->windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
                menu->setAttribute(Qt::WA_TranslucentBackground, true);
            }
            else if (auto* combo = qobject_cast<QComboBox*>(obj)) {
                if (auto* view = combo->view()) {
                    if (auto* popup = view->parentWidget()) {
                        popup->setWindowFlags(popup->windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
                        popup->setAttribute(Qt::WA_TranslucentBackground, true);
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
                    widget->setWindowFlags(widget->windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
                    widget->setAttribute(Qt::WA_TranslucentBackground, true);
                    if (auto* frame = qobject_cast<QFrame*>(widget)) {
                        frame->setFrameShape(QFrame::NoFrame);
                    }
                    widget->setContentsMargins(0, 0, 0, 0);
                    widget->setProperty("_popupStyled", true);
                }
                else if (widget->inherits("QTipLabel")) {
                    widget->setWindowFlags(widget->windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
                    widget->setAttribute(Qt::WA_TranslucentBackground, true);
                    widget->setContentsMargins(1, 1, 1, 1);
                }
                else if (widget->objectName() == QLatin1String("SignatureHelpLabel")) {
                    widget->setWindowFlags(widget->windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
                    widget->setAttribute(Qt::WA_TranslucentBackground, true);
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
                if (auto* tree = qobject_cast<QTreeWidget*>(widget)) {
                    applyDeviceFeatureTree(tree);
                    if (event->type() == QEvent::Show) {
                        applyInitialDeviceFeatureTreeWidth(tree);
                    }
                }
                applyLayoutTheme(widget);
            }
        }

        if (event->type() == QEvent::Show || event->type() == QEvent::LayoutRequest) {
            if (auto* widget = qobject_cast<QWidget*>(obj)) {
                if (widget->objectName() == QLatin1String("RuntimePathsFormContainer")) {
                    scheduleRuntimePathsBrowseButtonAlignment(widget);
                }
                if (event->type() == QEvent::Show && widget->windowFlags() & Qt::Popup) {
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
    static void scheduleRuntimePathsBrowseButtonAlignment(QWidget* formContainer)
    {
        static constexpr auto queuedProperty = "_runtimePathsBrowseAlignmentQueued";
        if (formContainer->property(queuedProperty).toBool()) {
            return;
        }

        formContainer->setProperty(queuedProperty, true);
        QTimer::singleShot(0, formContainer, [formContainer]() {
            formContainer->setProperty("_runtimePathsBrowseAlignmentQueued", false);
            alignRuntimePathsBrowseButtons(formContainer);
        });
    }

    static void alignRuntimePathsBrowseButtons(QWidget* formContainer)
    {
        const auto browseButtons = formContainer->findChildren<QPushButton*>(
            QStringLiteral("RuntimePathsBrowseButton"));
        for (QPushButton* button : browseButtons) {
            auto* grid = qobject_cast<QGridLayout*>(button->parentWidget()->layout());
            if (!grid) {
                continue;
            }

            int row = 0;
            int column = 0;
            int rowSpan = 0;
            int columnSpan = 0;
            grid->getItemPosition(grid->indexOf(button), &row, &column, &rowSpan, &columnSpan);
            QLayoutItem* editorItem = grid->itemAtPosition(row, column - 1);
            auto* editor = editorItem ? qobject_cast<QLineEdit*>(editorItem->widget()) : nullptr;
            if (!editor) {
                continue;
            }

            const QPoint editorOrigin = button->parentWidget()->mapFromGlobal(
                editor->mapToGlobal(QPoint(0, 0)));
            const int editorCenterY = editorOrigin.y() + (editor->height() / 2);
            button->move(button->x(), editorCenterY - (button->height() / 2));
        }
    }

    static void applyDeviceFeatureTree(QTreeWidget* tree)
    {
        if (!tree
            || tree->property("treeRole").toString() != QLatin1String("DeviceFeatureTree")
            || tree->property("_deviceFeatureTreeConfigured").toBool()) {
            return;
        }

        tree->setProperty("_deviceFeatureTreeConfigured", true);
        tree->setRootIsDecorated(true);
        tree->setAnimated(false);
        tree->setAlternatingRowColors(false);
        tree->setUniformRowHeights(false);
        tree->setIndentation(18);
        tree->setItemDelegate(new DeviceFeatureTreeDelegate(tree));

        QHeaderView* header = tree->header();
        header->setStretchLastSection(true);
        header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        header->setSectionResizeMode(0, QHeaderView::Interactive);
        header->setSectionResizeMode(1, QHeaderView::Stretch);
        header->setMinimumSectionSize(60);
    }

    static void applyInitialDeviceFeatureTreeWidth(QTreeWidget* tree)
    {
        if (!tree
            || tree->property("treeRole").toString() != QLatin1String("DeviceFeatureTree")
            || tree->property("_deviceFeatureTreeWidthInitialized").toBool()
            || tree->property("_deviceFeatureTreeWidthPending").toBool()) {
            return;
        }

        tree->setProperty("_deviceFeatureTreeWidthPending", true);
        QTimer::singleShot(0, tree, [tree]
        {
            tree->setProperty("_deviceFeatureTreeWidthPending", false);
            const int availableWidth = tree->viewport()->width();
            if (availableWidth <= 0) {
                return;
            }

            tree->header()->resizeSection(
                0,
                qMax(tree->header()->minimumSectionSize(), availableWidth / 2));
            tree->setProperty("_deviceFeatureTreeWidthInitialized", true);
        });
    }

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
        else if (name == QLatin1String("DeviceInfoLayout")) {
            setLayoutMetrics(layout, QMargins(12, 0, 12, 12), 0);
        }
        else if (name == QLatin1String("DeviceTreePanelLayout")) {
            setLayoutMetrics(layout, QMargins(12, 0, 12, 12), 8);
        }
        else if (name == QLatin1String("DeviceTabbedTreePanelLayout")) {
            setLayoutMetrics(layout, QMargins(12, 8, 12, 12), 8);
        }
        else if (name == QLatin1String("ProcessingRootLayout")) {
            setLayoutMetrics(layout, QMargins(12, 12, 12, 12), 8);
        }
        else if (name == QLatin1String("ProcessingTabBarLayout")) {
            setLayoutMetrics(layout, QMargins(12, 5, 6, 5), 4);
        }
        else if (name == QLatin1String("GraphicsSourceControlsLayout")) {
            setLayoutMetrics(layout, QMargins(0, 0, 0, 0), 0);
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
        else if (name == QLatin1String("RuntimePathsBodyLayout")) {
            setLayoutMetrics(layout, QMargins(0, 0, 0, 0), 16);
        }
        else if (name == QLatin1String("RuntimePathsLeftPanelLayout")) {
            setLayoutMetrics(layout, QMargins(8, 12, 8, 8), 8);
        }
        else if (name == QLatin1String("RuntimePathsListButtonsLayout")) {
            setLayoutMetrics(layout, QMargins(0, 0, 0, 0), 8);
        }
        else if (name == QLatin1String("RuntimePathsFormLayout")) {
            setLayoutMetrics(layout, QMargins(0, 0, 0, 0), 12);
        }
        else if (name == QLatin1String("RuntimePathsFormGridLayout")) {
            if (auto* grid = qobject_cast<QGridLayout*>(layout)) {
                setGridLayoutMetrics(grid, QMargins(8, 12, 8, 8), 8, 8);
            } else {
                setLayoutMetrics(layout, QMargins(8, 12, 8, 8), 8);
            }
        }
        else if (name == QLatin1String("RuntimePathsBottomLayout")) {
            setLayoutMetrics(layout, QMargins(0, 0, 0, 0), 12);
        }
        else if (name == QLatin1String("RuntimePathEntriesConfiguredLayout")) {
            setLayoutMetrics(layout, QMargins(8, 12, 8, 8), 8);
        }
        else if (name == QLatin1String("RuntimePathEntriesConfiguredActionsLayout") ||
                 name == QLatin1String("RuntimePathEntriesActionsLayout")) {
            setLayoutMetrics(layout, QMargins(0, 0, 0, 0), 8);
        }
        else if (name == QLatin1String("RuntimePathEntriesAddLayout")) {
            setLayoutMetrics(layout, QMargins(8, 12, 8, 8), 8);
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

    static void paintPopupRoundedRect(QWidget* widget, bool allRounded)
    {
        const int w = widget->width();
        const int h = widget->height();
        QPainter painter(widget);

        if (allRounded) {
            painter.setRenderHint(QPainter::Antialiasing, false);
            painter.setCompositionMode(QPainter::CompositionMode_Source);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(QStringLiteral("#d9e1ea")));
            painter.drawRoundedRect(QRectF(0.0, 0.0, w, h), 9.0, 9.0);

            painter.setBrush(Qt::white);
            painter.drawRoundedRect(QRectF(1.0, 1.0, w - 2.0, h - 2.0), 8.0, 8.0);

            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            painter.setPen(QPen(QColor(QStringLiteral("#d9e1ea")), 1.0));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(QRectF(0.5, 0.5, w - 1.0, h - 1.0), 8.0, 8.0);
        } else {
            // Bottom-only rounded: top edge is straight (connects flush to combo)
            QPainterPath outerPath;
            outerPath.moveTo(0, 0);
            outerPath.lineTo(w, 0);
            outerPath.lineTo(w, h - 9.0);
            outerPath.arcTo(QRectF(w - 18.0, h - 18.0, 18.0, 18.0), 0, -90);
            outerPath.lineTo(9.0, h);
            outerPath.arcTo(QRectF(0, h - 18.0, 18.0, 18.0), -90, -90);
            outerPath.lineTo(0, 0);

            QPainterPath innerPath;
            innerPath.moveTo(1, 0);
            innerPath.lineTo(w - 1.0, 0);
            innerPath.lineTo(w - 1.0, h - 9.0);
            innerPath.arcTo(QRectF(w - 17.0, h - 17.0, 16.0, 16.0), 0, -90);
            innerPath.lineTo(9.0, h - 1.0);
            innerPath.arcTo(QRectF(1.0, h - 17.0, 16.0, 16.0), -90, -90);
            innerPath.lineTo(1, 0);

            painter.setRenderHint(QPainter::Antialiasing, false);
            painter.setCompositionMode(QPainter::CompositionMode_Source);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(QStringLiteral("#d9e1ea")));
            painter.drawPath(outerPath);

            painter.setBrush(Qt::white);
            painter.drawPath(innerPath);

            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            painter.setPen(QPen(QColor(QStringLiteral("#d9e1ea")), 1.0));
            painter.setBrush(Qt::NoBrush);
            QPainterPath borderPath;
            borderPath.moveTo(0.5, 0);
            borderPath.lineTo(w - 0.5, 0);
            borderPath.lineTo(w - 0.5, h - 8.5);
            borderPath.arcTo(QRectF(w - 16.5, h - 16.5, 16.0, 16.0), 0, -90);
            borderPath.lineTo(8.5, h - 0.5);
            borderPath.arcTo(QRectF(0.5, h - 16.5, 16.0, 16.0), -90, -90);
            borderPath.lineTo(0.5, 0);
            painter.drawPath(borderPath);
        }
    }

    static void applyPopupMask(QWidget* widget, bool allRounded)
    {
        QBitmap bmp(widget->size());
        bmp.fill(Qt::color0);
        QPainter painter(&bmp);
        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.setBrush(Qt::color1);
        painter.setPen(Qt::NoPen);

        if (allRounded) {
            painter.drawRoundedRect(widget->rect(), 9.0, 9.0);
        } else {
            const int w = widget->width();
            const int h = widget->height();
            QPainterPath path;
            path.moveTo(0, 0);
            path.lineTo(w, 0);
            path.lineTo(w, h - 9.0);
            path.arcTo(QRectF(w - 18.0, h - 18.0, 18.0, 18.0), 0, -90);
            path.lineTo(9.0, h);
            path.arcTo(QRectF(0, h - 18.0, 18.0, 18.0), -90, -90);
            path.lineTo(0, 0);
            painter.drawPath(path);
        }

        widget->setMask(QRegion(bmp));
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

    // Disable tooltip and combo box transition effects to prevent black artifacts during DWM animation
    QApplication::setEffectEnabled(Qt::UI_FadeTooltip, false);
    QApplication::setEffectEnabled(Qt::UI_AnimateTooltip, false);
    QApplication::setEffectEnabled(Qt::UI_AnimateCombo, false);

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
        QStringLiteral(":/Resources/theme/qss/30_device_controls.qss"),
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
#ifdef Q_OS_WIN
    styleSheet += QStringLiteral(
        "\nQWidget#ThemedDialogContainer { border-radius: 8px; }\n"
        "QWidget#ThemedDialogTitleBar { border-top-left-radius: 8px; border-top-right-radius: 8px; }\n"
        "QFrame#DockContainerWidget[floatingState=\"true\"],\n"
        "QDockWidget[floatingState=\"true\"] QFrame#DockContainerWidget {\n"
        "    border-bottom-left-radius: 7px;\n"
        "    border-bottom-right-radius: 7px;\n"
        "}\n"
        "QDockWidget[floatingState=\"true\"] QWidget#ThemedDockTitleBar {\n"
        "    border-top-left-radius: 8px;\n"
        "    border-top-right-radius: 8px;\n"
        "}\n"
        "QDockWidget[floatingState=\"true\"] QFrame#DockContainerWidget > QWidget {\n"
        "    border-bottom-left-radius: 6px;\n"
        "    border-bottom-right-radius: 6px;\n"
        "}\n"
        "QStatusBar {\n"
        "    border-bottom-left-radius: 7px;\n"
        "    border-bottom-right-radius: 7px;\n"
        "}\n"
        "QWidget#ThemedMdiContainer { border-radius: 8px; }\n"
        "QWidget#ThemedMdiTitleBar {\n"
        "    border-top-left-radius: 8px;\n"
        "    border-top-right-radius: 8px;\n"
        "}\n"
    );
#endif
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

void paintMainWindowBorder(QWidget* window, QPainter& painter, bool maximized, bool forceRounded)
{
    if (!window) return;

    if (maximized && !forceRounded) {
        painter.fillRect(window->rect(), QColor(QStringLiteral("#ffffff")));
        return;
    }

    // Main-window corners use the translucent backing store instead of a binary
    // QBitmap mask so antialiased edge pixels are not hard-clipped.
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(window->rect(), Qt::transparent);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(QStringLiteral("#d9e1ea")));

    qreal outerRadius = 13.0;
    qreal innerRadius = 12.0;
    getPlatformWindowRadius(outerRadius, innerRadius);

    painter.drawRoundedRect(QRectF(0.0, 0.0, window->width(), window->height()), outerRadius, outerRadius);

    // Fill interior with white (1px inset, slightly smaller radius).
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setBrush(QColor(QStringLiteral("#ffffff")));
    painter.drawRoundedRect(QRectF(1.0, 1.0, window->width() - 2.0, window->height() - 2.0), innerRadius, innerRadius);

    // Draw smooth 1px border.
    painter.setPen(QPen(QColor(QStringLiteral("#d9e1ea")), 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(QRectF(0.5, 0.5, window->width() - 1.0, window->height() - 1.0), innerRadius, innerRadius);
}

void applyWindowPlatformAttributes(QWidget* window)
{
#ifdef Q_OS_WIN
    if (!window) return;
    HWND hwnd = (HWND)window->winId();
    if (hwnd) {
        // 33: DWMWA_WINDOW_CORNER_PREFERENCE, 2: DWMWCP_ROUND
        DWORD cornerPreference = 2;
        DwmSetWindowAttribute(hwnd, 33, &cornerPreference, sizeof(cornerPreference));
        
        MARGINS margins = {-1, -1, -1, -1};
        DwmExtendFrameIntoClientArea(hwnd, &margins);
    }
#endif
    Q_UNUSED(window);
}

void getPlatformWindowRadius(qreal& outer, qreal& inner)
{
#ifdef Q_OS_WIN
    outer = 8.0;
    inner = 7.0;
#else
    outer = 13.0;
    inner = 12.0;
#endif
}

void paintMdiAreaBackground(QWidget* viewport, QPainter& painter, const QPixmap& logo)
{
    if (!viewport) return;
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.fillRect(viewport->rect(), QColor(QStringLiteral("#eeeeee")));

    if (logo.isNull()) return;

    const QSize logoSize = logo.size() / 2;
    const QPoint topLeft(
        (viewport->width() - logoSize.width()) / 2,
        (viewport->height() - logoSize.height()) / 2);
    painter.drawPixmap(QRect(topLeft, logoSize), logo);
}

QString formatLogHtml(int messageType, const QString& timestamp, const QString& message)
{
    QString typeStr;
    QString color;
    switch (messageType) {
        case QtDebugMsg:    typeStr = QStringLiteral("DEBUG"); color = QStringLiteral("#78909c"); break;
        case QtInfoMsg:     typeStr = QStringLiteral("INFO");  color = QStringLiteral("#1e88e5"); break;
        case QtWarningMsg:  typeStr = QStringLiteral("WARN");  color = QStringLiteral("#fb8c00"); break;
        case QtCriticalMsg: typeStr = QStringLiteral("CRIT");  color = QStringLiteral("#e53935"); break;
        case QtFatalMsg:    typeStr = QStringLiteral("FATAL"); color = QStringLiteral("#b71c1c"); break;
        default:            typeStr = QStringLiteral("LOG");   color = QStringLiteral("#7f8c8d"); break;
    }

    return QStringLiteral("<span style=\"color:#7f8c8d;\">[%1]</span> <span style=\"color:%2; font-weight:bold;\">[%3]</span> %4")
        .arg(timestamp, color, typeStr, message.toHtmlEscaped());
}

}
