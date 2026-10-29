#include "ThemeRuntime.h"
#include "ResourcesTheme.h"
#include "ItemViewTheme.h"
#include "Chrome/ThemedTreeWidget.h"
#include "Chrome/ProgressShine.h"

#include <QApplication>
#include <QFocusEvent>
#include <QPointer>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QIconEngine>
#include <QImage>
#include <QPixmapCache>
#include <QAbstractButton>
#include <QStringList>
#include <QTextStream>
#include <QFontDatabase>
#include <QFont>
#include <QMenu>
#include <QMenuBar>
#include <QFrame>
#include <QLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QFontMetrics>
#include <QKeySequence>
#include <QGraphicsDropShadowEffect>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QAbstractSpinBox>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QTextDocument>
#include <QPushButton>
#include <QProgressBar>
#include <QStyleOptionProgressBar>
#include <QTabBar>
#include <QToolButton>
#include <QAbstractItemView>
#include <QHeaderView>
#include <QStatusBar>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QTreeWidget>
#include <QScreen>
#include <QRubberBand>
#include <QPalette>
#include <QProxyStyle>
#include <QStyleFactory>
#include <QColor>
#include <QCursor>
#include <QTableWidget>
#include <QEvent>
#include <QMouseEvent>
#include <QDebug>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QBitmap>
#include <QRegion>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#ifdef Q_OS_WIN
#include <dwmapi.h>
#endif

/** @brief Reads shared geometry from the installed theme snapshot. */
static int presentationMetric(const char* name)
{
    return Resources::ResourcesTheme::instance().metric(name);
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

/**
 * @brief Checks whether two model indexes identify cells in the same row.
 * @param lhs First model index.
 * @param rhs Second model index.
 * @return `true` when both indexes are valid and share a parent and row.
 */
static bool isSameTreeRow(const QModelIndex& lhs, const QModelIndex& rhs)
{
    return lhs.isValid()
        && rhs.isValid()
        && lhs.row() == rhs.row()
        && lhs.parent() == rhs.parent();
}

/**
 * @brief Paints row-mode cells without covering the shared row surface.
 */
class RoundedTreeItemDelegate final : public QStyledItemDelegate
{
public:
    /**
     * @brief Creates a delegate associated with a tree view.
     * @param tree Tree whose viewport supplies the full visible row bounds.
     */
    explicit RoundedTreeItemDelegate(QTreeWidget* tree)
        : QStyledItemDelegate(tree),
          _tree(tree)
    {
        setProperty("_resourcesOwnsItemBackground", true);
    }

    /**
     * @brief Paints item contents without duplicating the row-mode surface.
     * @param painter Painter supplied by the item view.
     * @param option Cell paint options supplied by the item view.
     * @param index Model index of the cell being painted.
     */
    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        if (_tree == nullptr
            || painter == nullptr
            || _tree->property("interactionMode").toString() != QLatin1String("row")) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        QStyleOptionViewItem cellOption(option);
        initStyleOption(&cellOption, index);
        cellOption.state &= ~QStyle::State_MouseOver;
        cellOption.state &= ~QStyle::State_Selected;
        const QWidget* widget = cellOption.widget;
        QStyle* style = widget ? widget->style() : QApplication::style();

        if (cellOption.features & QStyleOptionViewItem::HasCheckIndicator) {
            QStyleOptionViewItem checkOption(cellOption);
            // Row surfaces own background feedback; indicators retain their native hover state.
            checkOption.state.setFlag(QStyle::State_MouseOver, option.state.testFlag(QStyle::State_MouseOver));
            // Primitive painting reads state flags, not the model's checkState.
            checkOption.state &= ~(QStyle::State_On | QStyle::State_Off | QStyle::State_NoChange);
            checkOption.state |= cellOption.checkState == Qt::Checked ? QStyle::State_On
                : cellOption.checkState == Qt::PartiallyChecked ? QStyle::State_NoChange
                : QStyle::State_Off;
            checkOption.rect = style->subElementRect(
                QStyle::SE_ItemViewItemCheckIndicator,
                &cellOption,
                widget);
            if (!checkOption.rect.isEmpty()) {
                style->drawPrimitive(
                    QStyle::PE_IndicatorItemViewItemCheck,
                    &checkOption,
                    painter,
                    widget);
            }
        }

        if (!cellOption.icon.isNull()) {
            const QRect decorationRect = style->subElementRect(
                QStyle::SE_ItemViewItemDecoration,
                &cellOption,
                widget);
            if (!decorationRect.isEmpty()) {
                const QIcon::Mode iconMode =
                    cellOption.state & QStyle::State_Enabled
                        ? QIcon::Normal
                        : QIcon::Disabled;
                cellOption.icon.paint(
                    painter,
                    decorationRect,
                    cellOption.decorationAlignment,
                    iconMode,
                    QIcon::Off);
            }
        }

        if (!cellOption.text.isEmpty()) {
            QRect textRect = style->subElementRect(
                QStyle::SE_ItemViewItemText,
                &cellOption,
                widget);
            if (textRect.isEmpty()) {
                textRect = cellOption.rect.adjusted(6, 0, -6, 0);
            }
            textRect.adjust(3, 0, -3, 0);
            const QFontMetrics metrics(cellOption.font);
            const QString text = metrics.elidedText(
                cellOption.text,
                Qt::ElideRight,
                textRect.width());
            style->drawItemText(
                painter,
                textRect,
                cellOption.displayAlignment,
                cellOption.palette,
                cellOption.state & QStyle::State_Enabled,
                text,
                QPalette::Text);
        }
    }

private:
    QTreeWidget* _tree = nullptr;
};

/** @brief Paints table contents while the shared delegate owns adjoining interaction surfaces. */
class RoundedTableItemDelegate final : public QStyledItemDelegate
{
public:
    /**
     * @brief Creates a row-surface delegate for a table.
     * @param table Table whose viewport supplies the visible row bounds.
     */
    explicit RoundedTableItemDelegate(QTableWidget* table)
        : QStyledItemDelegate(table)
        , _table(table)
    {
        setProperty("_resourcesOwnsItemBackground", true);
    }

    /**
     * @brief Paints one table cell without duplicating the row surface.
     * @param painter Painter supplied by the item view.
     * @param option Cell paint options supplied by the item view.
     * @param index Model index of the cell being painted.
     */
    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        if (_table == nullptr
            || painter == nullptr
            || _table->property("interactionMode").toString() != QLatin1String("row")) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        QStyleOptionViewItem cellOption(option);
        initStyleOption(&cellOption, index);
        cellOption.state &= ~QStyle::State_MouseOver;
        cellOption.state &= ~QStyle::State_Selected;
        const QWidget* widget = cellOption.widget;
        QStyle* style = widget != nullptr ? widget->style() : QApplication::style();

        if (cellOption.features & QStyleOptionViewItem::HasCheckIndicator) {
            QStyleOptionViewItem checkOption(cellOption);
            // Row surfaces own background feedback; indicators retain their native hover state.
            checkOption.state.setFlag(QStyle::State_MouseOver, option.state.testFlag(QStyle::State_MouseOver));
            // Item primitives read style flags, including the unchecked and mixed states.
            checkOption.state &= ~(QStyle::State_On | QStyle::State_Off | QStyle::State_NoChange);
            checkOption.state |= cellOption.checkState == Qt::Checked ? QStyle::State_On
                : cellOption.checkState == Qt::PartiallyChecked ? QStyle::State_NoChange
                : QStyle::State_Off;
            checkOption.rect = style->subElementRect(
                QStyle::SE_ItemViewItemCheckIndicator,
                &cellOption,
                widget);
            if (!checkOption.rect.isEmpty()) {
                style->drawPrimitive(
                    QStyle::PE_IndicatorItemViewItemCheck,
                    &checkOption,
                    painter,
                    widget);
            }
        }

        if (!cellOption.icon.isNull())
        {
            const QRect decorationRect = style->subElementRect(
                QStyle::SE_ItemViewItemDecoration,
                &cellOption,
                widget);
            if (!decorationRect.isEmpty())
            {
                const QIcon::Mode iconMode = cellOption.state & QStyle::State_Enabled
                    ? QIcon::Normal
                    : QIcon::Disabled;
                cellOption.icon.paint(
                    painter,
                    decorationRect,
                    cellOption.decorationAlignment,
                    iconMode,
                    QIcon::Off);
            }
        }

        if (!cellOption.text.isEmpty())
        {
            QRect textRect = style->subElementRect(
                QStyle::SE_ItemViewItemText,
                &cellOption,
                widget);
            if (textRect.isEmpty())
            {
                textRect = cellOption.rect.adjusted(6, 0, -6, 0);
            }
            textRect.adjust(3, 0, -3, 0);
            const QFontMetrics metrics(cellOption.font);
            const QString text = metrics.elidedText(
                cellOption.text,
                Qt::ElideRight,
                textRect.width());
            style->drawItemText(
                painter,
                textRect,
                cellOption.displayAlignment,
                cellOption.palette,
                cellOption.state & QStyle::State_Enabled,
                text,
                QPalette::Text);
        }
    }

private:
    QTableWidget* _table = nullptr;
};

/**
 * @brief Installs the shared row-surface delegate on an opt-in table.
 * @param table Table that exposes `interactionMode="row"`.
 */
static void applyRowTableStyle(QTableWidget* table)
{
    if (table == nullptr
        || table->property("interactionMode").toString() != QLatin1String("row")
        || table->property("_roundedRowDelegateInstalled").toBool()) {
        return;
    }

    table->setItemDelegate(new RoundedTableItemDelegate(table));
    table->setProperty("_roundedRowDelegateInstalled", true);
    table->viewport()->update();
}

/** @brief Paints only internal diagnostics cell dividers, using the themed grid color. */
class DiagnosticsTableItemDelegate final : public QStyledItemDelegate
{
public:
    explicit DiagnosticsTableItemDelegate(QTableWidget* table)
        : QStyledItemDelegate(table), _table(table) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        QStyledItemDelegate::paint(painter, option, index);
        painter->save();
        const QColor gridColor = QColor::fromRgba(static_cast<QRgb>(
            _table->style()->styleHint(QStyle::SH_Table_GridLineColor, &option, _table)));
        // Include the header viewport inset when sharing its painted section boundary.
        // Filled strips match QSS border coverage at fractional display scales.
        painter->setRenderHint(QPainter::Antialiasing, true);
        for (int column = 1; column < _table->columnCount(); ++column) {
            auto* header = _table->horizontalHeader();
            const int boundary = header->viewport()->mapTo(_table,
                QPoint(header->sectionViewportPosition(column) - 1, 0)).x()
                - _table->viewport()->pos().x();
            // Paint with the cell that owns this pixel, including partial viewport updates.
            if (_table->columnAt(boundary) != index.column())
                continue;
            painter->fillRect(QRectF(boundary, option.rect.top(), 1,
                                    _table->rowHeight(index.row())), gridColor);
        }
        if (index.row() > 0)
            painter->fillRect(QRectF(option.rect.left(),
                                    option.rect.top(),
                                    _table->columnWidth(index.column()), 1), gridColor);
        painter->restore();
    }

private:
    QTableWidget* _table;
};

/** @brief Keeps the read-only diagnostics grid inside its shared table shell. */
static void applyDiagnosticsTableStyle(QTableWidget* table)
{
    if (table->objectName() != QLatin1String("GraphicsSettingsPresentationStatsTable")
        || table->property("_diagnosticsDelegateInstalled").toBool())
        return;
    table->setShowGrid(false);
    table->setItemDelegate(new DiagnosticsTableItemDelegate(table));
    table->setProperty("_diagnosticsDelegateInstalled", true);
}

ThemedTreeWidget::ThemedTreeWidget(QWidget* parent)
    : QTreeWidget(parent)
{
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
    setInteractionMode(InteractionMode::Cell);
    setItemDelegate(new RoundedTreeItemDelegate(this));
}

void ThemedTreeWidget::setInteractionMode(InteractionMode mode)
{
    const bool rowMode = mode == InteractionMode::Row;
    setProperty("interactionMode", rowMode ? QStringLiteral("row") : QStringLiteral("cell"));
    setSelectionBehavior(rowMode
        ? QAbstractItemView::SelectRows
        : QAbstractItemView::SelectItems);
    viewport()->update();
}

void ThemedTreeWidget::drawRow(QPainter* painter,
                               const QStyleOptionViewItem& option,
                               const QModelIndex& index) const
{
    if (painter == nullptr || model() == nullptr) {
        return;
    }

    // Direct cell painting must retain Qt's branch geometry and QSS indicators.
    const int branchColumn = treePosition() < 0 ? header()->logicalIndex(0) : treePosition();
    const QRect cell = visualRect(index.siblingAtColumn(branchColumn));
    const int sectionLeft = header()->sectionViewportPosition(branchColumn);
    const int sectionRight = sectionLeft + header()->sectionSize(branchColumn);
    const QRect branches = isRightToLeft()
        ? QRect(cell.right() + 1, option.rect.top(), sectionRight - cell.right() - 1, option.rect.height())
        : QRect(sectionLeft, option.rect.top(), cell.left() - sectionLeft, option.rect.height());
    if (branches.width() > 0) drawBranches(painter, branches, index);

    // Paint the cell contents directly because calling the base drawRow would repaint
    // opaque per-cell backgrounds over the shared row surface.
    const int columnCount = model()->columnCount(index.parent());
    for (int column = 0; column < columnCount; ++column) {
        const QModelIndex cellIndex = index.siblingAtColumn(column);
        const QRect cellRect = visualRect(cellIndex);
        if (!cellIndex.isValid() || !cellRect.intersects(option.rect)) {
            continue;
        }
        const QAbstractItemDelegate* delegate = itemDelegateForIndex(cellIndex);
        if (delegate == nullptr) {
            continue;
        }

        QStyleOptionViewItem cellOption(option);
        cellOption.rect = cellRect;
        cellOption.state.setFlag(QStyle::State_Selected,
            selectionModel() && selectionModel()->isSelected(cellIndex));
        cellOption.state.setFlag(QStyle::State_HasFocus, hasFocus() && currentIndex() == cellIndex);
        const QModelIndex hovered = indexAt(viewport()->mapFromGlobal(QCursor::pos()));
        cellOption.state.setFlag(QStyle::State_MouseOver,
            property("interactionMode").toString() == QLatin1String("row")
                ? isSameTreeRow(cellIndex, hovered) : cellIndex == hovered);
        delegate->paint(painter, cellOption, cellIndex);
    }
}

void ThemedTreeWidget::drawBranches(QPainter* painter, const QRect& rect, const QModelIndex& index) const
{
    if (property("interactionMode").toString() != QLatin1String("row")) {
        QTreeWidget::drawBranches(painter, rect, index);
        return;
    }
    if (!painter || !index.isValid() || !model()->hasChildren(index)
        || (model()->flags(index) & Qt::ItemNeverHasChildren)) return;
    // Native QSS branch drawing first paints a rectangular row panel on Windows.
    // The shared rounded surface already owns that background; paint only its glyph.
    const int width = qMin(indentation(), rect.width());
    const QRect indicator(isRightToLeft() ? rect.left() : rect.right() - width + 1,
                          rect.top(), width, rect.height());
    const QIcon icon(isExpanded(index)
        ? QStringLiteral(":/Resources/icon/drop-down-arrow-48.png")
        : QStringLiteral(":/Resources/icon/forward-48.png"));
    icon.paint(painter, indicator.adjusted((width - 16) / 2, (rect.height() - 16) / 2,
               -(width - 16) / 2, -(rect.height() - 16) / 2), Qt::AlignCenter,
               isEnabled() ? QIcon::Normal : QIcon::Disabled);
}

// ---------------------------------------------------------------------------
static bool isDeviceFeatureTreeViewport(const QObject* object)
{
    const auto* viewport = qobject_cast<const QWidget*>(object);
    const auto* tree = viewport ? qobject_cast<const QTreeWidget*>(viewport->parentWidget()) : nullptr;
    return tree
        && tree->viewport() == viewport
        && tree->property("treeRole").toString() == QLatin1String("DeviceFeatureTree");
}

static QTabBar* graphicsSettingsTabBarFor(QObject* object)
{
    for (QObject* current = object; current != nullptr; current = current->parent())
    {
        auto* tabBar = qobject_cast<QTabBar*>(current);
        if (tabBar == nullptr)
        {
            continue;
        }

        for (QObject* parent = tabBar->parent(); parent != nullptr; parent = parent->parent())
        {
            if (parent->inherits("QTabWidget")
                && parent->objectName() == QLatin1String("GraphicsSettingsPanelTabs"))
            {
                return tabBar;
            }
        }
    }
    return nullptr;
}

static bool isGraphicsSettingsTabScrollerButton(const QToolButton* button)
{
    if (button == nullptr
        || (button->objectName() != QLatin1String("ScrollLeftButton")
            && button->objectName() != QLatin1String("ScrollRightButton")))
    {
        return false;
    }
    return graphicsSettingsTabBarFor(const_cast<QToolButton*>(button)) != nullptr;
}

static void alignGraphicsSettingsTabScrollerButtons(QTabBar* tabBar)
{
    if (tabBar == nullptr || tabBar->count() == 0)
    {
        return;
    }

    // QTabBar positions its scroll buttons against the tab strip's lower edge.
    // Center them against the whole strip instead of a tab rect so the arrow
    // and its hover fill share the same vertical axis as the tab labels.
    const int referenceCenterY = tabBar->rect().center().y() + 1;

    for (auto* button : tabBar->findChildren<QToolButton*>())
    {
        if (!isGraphicsSettingsTabScrollerButton(button))
        {
            continue;
        }

        const int targetY = referenceCenterY - button->height() / 2;
        if (button->y() != targetY)
        {
            button->move(button->x(), targetY);
        }
    }
}

static void scheduleGraphicsSettingsTabScrollerAlignment(QTabBar* tabBar)
{
    if (tabBar == nullptr || tabBar->property("_graphicsSettingsScrollerAlignmentScheduled").toBool())
    {
        return;
    }

    tabBar->setProperty("_graphicsSettingsScrollerAlignmentScheduled", true);
    QTimer::singleShot(0, tabBar, [tabBar]()
    {
        tabBar->setProperty("_graphicsSettingsScrollerAlignmentScheduled", false);
        alignGraphicsSettingsTabScrollerButtons(tabBar);
    });
}

static void paintGraphicsSettingsTabScrollerButton(QToolButton* button)
{
    if (button == nullptr)
    {
        return;
    }

    QPainter painter(button);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(button->rect(), Resources::themePalette().value(QStringLiteral("surface")));

    const QString iconPath = button->objectName() == QLatin1String("ScrollLeftButton")
        ? QStringLiteral(":/Resources/icon/back-48.png")
        : QStringLiteral(":/Resources/icon/forward-48.png");
    const QPixmap arrow = QIcon(iconPath).pixmap(QSize(10, 10), QIcon::Normal, QIcon::Off);
    if (!arrow.isNull())
    {
        constexpr qreal arrowSize = 10.0;
        const QRectF target((button->width() - arrowSize) / 2.0,
                            (button->height() - arrowSize) / 2.0,
                            arrowSize,
                            arrowSize);
        painter.drawPixmap(target, arrow, QRectF(0.0, 0.0, arrow.width(), arrow.height()));
    }
}

// ---------------------------------------------------------------------------
// ResourceStyleFilter: handles runtime style geometry that QSS cannot express
// reliably. Installed once via installResources().
//
// Removes native popup frames; Resources paints combo shells independently
// of the item view. Preserves geometry when window flags recreate a popup.
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
/** @brief Bounds menu shadow updates to the transparent gutter of the native popup window. */
class MenuDropShadowEffect final : public QGraphicsDropShadowEffect
{
public:
    using QGraphicsDropShadowEffect::QGraphicsDropShadowEffect;

protected:
    /** @brief Keeps layered-window dirty rectangles within the already padded source extent. */
    QRectF boundingRectFor(const QRectF& sourceRect) const override { return sourceRect; }
};

/** @brief Receives progress geometry and colors from the owning QSS template. */
class ResourcesProgressSurface final : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QColor trackColor MEMBER trackColor)
    Q_PROPERTY(QColor startColor MEMBER startColor)
    Q_PROPERTY(QColor endColor MEMBER endColor)
    Q_PROPERTY(QColor outlineColor MEMBER outlineColor)
public:
    explicit ResourcesProgressSurface(QProgressBar* bar) : QWidget(bar) {
        setObjectName(QStringLiteral("ResourcesProgressSurface"));
        hide();
        timer.setInterval(16);
        connect(&timer, &QTimer::timeout, this, [this, bar] {
            if (!bar->isVisible() || !bar->isEnabled()) { timer.stop(); return; }
            phase += 0.005;
            if (phase > 1000.0) phase = 0.0;
            bar->update();
        });
    }
    QColor trackColor;
    QColor startColor;
    QColor endColor;
    QColor outlineColor;
    QTimer timer;
    qreal phase = 0.0;
};

/** @brief Paints clipped capsule progress and preserves text contrast, including very narrow fills. */
static void paintThemedProgressText(QProgressBar* bar)
{
    auto* surface = bar->findChild<ResourcesProgressSurface*>(QStringLiteral("ResourcesProgressSurface"), Qt::FindDirectChildrenOnly);
    if (!surface) { surface = new ResourcesProgressSurface(bar); surface->ensurePolished(); }
    if (bar->isVisible() && bar->isEnabled() && !surface->timer.isActive()) surface->timer.start();
    QPainter painter(bar);
    const bool busy = bar->minimum() == 0 && bar->maximum() == 0;
    const qreal fraction = busy ? 1.0 : bar->value() < bar->minimum() ? 0.0 :
        bar->maximum() == bar->minimum() ? 1.0 : qBound(0.0,
        (qreal(bar->value()) - bar->minimum()) / (qreal(bar->maximum()) - bar->minimum()), 1.0);
    painter.save();
    QSizeF size(bar->size());
    if (bar->orientation() == Qt::Vertical) {
        painter.translate(0, bar->height());
        painter.rotate(-90);
        size.transpose();
    }
    const bool reverse = bar->orientation() == Qt::Horizontal
        ? bar->isRightToLeft() != bar->invertedAppearance() : bar->invertedAppearance();
    if (reverse) { painter.translate(size.width(), 0); painter.scale(-1, 1); }
    const auto canonicalFill = Resources::detail::paintLoadingProgress(painter, QRectF(QPointF(), size),
        fraction, surface->phase, busy, true, bar->isEnabled(), surface->trackColor,
        surface->startColor, surface->endColor, surface->outlineColor);
    const auto fillPath = painter.worldTransform().map(canonicalFill);
    painter.restore();
    if (!bar->isTextVisible() || busy) return;
    const auto label = [&](const QColor& color) {
        painter.save(); painter.setPen(color);
        QRect textRect = bar->rect();
        if (bar->orientation() == Qt::Vertical) {
            textRect = QRect(0, 0, bar->height(), bar->width());
            if (bar->textDirection() == QProgressBar::TopToBottom) { painter.translate(bar->width(), 0); painter.rotate(90); }
            else { painter.translate(0, bar->height()); painter.rotate(-90); }
        }
        painter.drawText(textRect, bar->alignment() | Qt::AlignVCenter | Qt::TextSingleLine, bar->text());
        painter.restore();
    };
    label(bar->palette().color(QPalette::Text));
    painter.setClipPath(fillPath); label(bar->palette().color(QPalette::HighlightedText));
}

/** @brief Suppresses native row-edge selection marks behind the shared joined item surface. */
class ResourcesNativeStyle final : public QProxyStyle
{
public:
    using QProxyStyle::QProxyStyle;

    /** @brief Uses the native default arrow metric when a pixel font has no point size. */
    int pixelMetric(PixelMetric metric, const QStyleOption* option = nullptr,
                    const QWidget* widget = nullptr) const override
    {
        // Qt's Windows 11 style passes widget->font().pointSize() to setPointSize().
        // QSS pixel fonts return -1; its widget-free metric avoids that invalid conversion.
        if (metric == PM_MenuButtonIndicator && widget && widget->font().pointSize() <= 0
            && baseStyle()->objectName().compare(QStringLiteral("windows11"), Qt::CaseInsensitive) == 0)
            return QProxyStyle::pixelMetric(metric, option, nullptr);
        return QProxyStyle::pixelMetric(metric, option, widget);
    }

    /** @brief Keeps combo rows content-sized instead of inheriting Windows 11 touch spacing. */
    QSize sizeFromContents(ContentsType type, const QStyleOption* option, const QSize& contentsSize,
                           const QWidget* widget = nullptr) const override
    {
        QSize size = QProxyStyle::sizeFromContents(type, option, contentsSize, widget);
        const auto* view = qobject_cast<const QAbstractItemView*>(widget);
        const auto* item = qstyleoption_cast<const QStyleOptionViewItem*>(option);
        if (type == CT_ItemViewItem && view && item
            && qobject_cast<const QComboBox*>(view->window()->parentWidget())) {
            int height = qMax(item->fontMetrics.height(), contentsSize.height());
            if (item->features.testFlag(QStyleOptionViewItem::HasDecoration))
                height = qMax(height, item->decorationSize.height());
            size.setHeight(height + 2);
        }
        return size;
    }

    void drawPrimitive(PrimitiveElement element, const QStyleOption* option, QPainter* painter,
                       const QWidget* widget = nullptr) const override
    {
        if (element == PE_PanelItemViewRow && qobject_cast<const QAbstractItemView*>(widget)) {
            if (const auto* item = qstyleoption_cast<const QStyleOptionViewItem*>(option)) {
                auto neutral = *item;
                neutral.state &= ~(State_Selected | State_HasFocus);
                QProxyStyle::drawPrimitive(element, &neutral, painter, widget);
                return;
            }
        }
        QProxyStyle::drawPrimitive(element, option, painter, widget);
    }
};

/** @brief Returns whether every visible pixel uses the neutral glyph ink or the active text role. */
static bool isNeutralGlyph(const QPixmap& pixmap, const QColor& text)
{
    const QImage image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
    const QColor assetInk(0x3c, 0x3c, 0x3b);
    // Not `near`: Windows headers define it as an empty macro.
    const auto matchesInk = [](QRgb pixel, const QColor& ink) {
        return qAbs(qRed(pixel) - ink.red()) <= 40 && qAbs(qGreen(pixel) - ink.green()) <= 40
            && qAbs(qBlue(pixel) - ink.blue()) <= 40;
    };
    bool visible = false;
    for (int y = 0; y < image.height(); ++y) {
        const auto* row = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            // Faint antialiasing edges carry little reliable color; their alpha still follows the glyph.
            if (qAlpha(row[x]) < 32) continue;
            if (!matchesInk(row[x], assetInk) && !matchesInk(row[x], text)) return false;
            visible = true;
        }
    }
    return visible;
}

/** @brief Recolors neutral glyph artwork with an interaction role, leaving other artwork unchanged. */
static QPixmap tintedGlyph(const QPixmap& base, const QColor& ink, const QColor& text)
{
    if (base.isNull()) return base;
    const QString key = QStringLiteral("resources-glyph-state:%1:%2:%3")
        .arg(base.cacheKey()).arg(ink.rgb(), 0, 16).arg(text.rgb(), 0, 16);
    QPixmap result;
    if (QPixmapCache::find(key, &result)) return result;
    result = base;
    if (isNeutralGlyph(base, text)) {
        QImage image = base.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
        image.setDevicePixelRatio(1.0);
        QPainter painter(&image);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(image.rect(), ink);
        painter.end();
        result = QPixmap::fromImage(std::move(image));
        result.setDevicePixelRatio(base.devicePixelRatio());
    }
    QPixmapCache::insert(key, result);
    return result;
}

/** @brief Recolors generated Active-mode glyphs, which Qt requests for hovered menu rows.
 *  Qt derives a missing Active pixmap through the application style, so menu actions keep
 *  their consumer-owned icons. Non-neutral artwork and other modes keep the base behavior.
 */
class ResourcesIconStateStyle final : public QProxyStyle
{
public:
    using QProxyStyle::QProxyStyle;

    QPixmap generatedIconPixmap(QIcon::Mode mode, const QPixmap& pixmap, const QStyleOption* option) const override
    {
        if (mode != QIcon::Active) return QProxyStyle::generatedIconPixmap(mode, pixmap, option);
        const auto palette = Resources::ResourcesTheme::instance().themePalette();
        return tintedGlyph(pixmap, palette.value(QStringLiteral("selectionAccent")),
                           palette.value(QStringLiteral("text")));
    }
};

/** @brief Uses Qt's keyboard-navigation state to avoid highlighting retained mouse focus. */
static bool hasButtonKeyboardFocus(const QAbstractButton& button)
{
    const QVariant keyboard = qApp->property("_resourcesKeyboardNavigation");
    const bool navigation = keyboard.isValid() ? keyboard.toBool()
        : button.window()->testAttribute(Qt::WA_KeyboardFocusChange);
    return button.isEnabled() && button.isVisible() && button.hasFocus()
        && button.isActiveWindow() && navigation;
}

/** @brief Shares focus-visible state with QSS without changing focus ownership or tab order. */
static void syncButtonKeyboardFocus(QAbstractButton* button)
{
    if (!button || (!qobject_cast<QToolButton*>(button) && !qobject_cast<QPushButton*>(button))) return;
    const bool visible = hasButtonKeyboardFocus(*button);
    if (button->property("resourcesKeyboardFocus") == QVariant(visible)) return;
    button->setProperty("resourcesKeyboardFocus", visible);
    if (button->testAttribute(Qt::WA_WState_Polished)) {
        button->style()->unpolish(button);
        button->style()->polish(button);
    }
    button->update();
}

/** @brief Returns whether a hovered toggle must keep showing its new state.
 *  A toggle that changed while engaged keeps its state glyph until the pointer leaves,
 *  so a click never flips straight to the opposite preview and reads as a failed command.
 *  State is sampled at paint time because consumers often toggle under a QSignalBlocker.
 */
static bool toggleGlyphPreviewSuppressed(QAbstractButton& button)
{
    static constexpr char lastCheckedKey[] = "_resourcesGlyphLastChecked";
    static constexpr char suppressedKey[] = "_resourcesGlyphPreviewSuppressed";
    const bool engaged = button.underMouse();
    const QVariant lastChecked = button.property(lastCheckedKey);
    bool suppressed = engaged && button.property(suppressedKey).toBool();
    if (engaged && lastChecked.isValid() && lastChecked.toBool() != button.isChecked()) suppressed = true;
    if (lastChecked != QVariant(button.isChecked())) button.setProperty(lastCheckedKey, button.isChecked());
    if (button.property(suppressedKey).toBool() != suppressed) button.setProperty(suppressedKey, suppressed);
    return suppressed;
}

/** @brief Paints a button's glyph in hover and pressed palette roles.
 *  Qt selects icon modes only from enabled, focus and auto-raise hover state; a press or open
 *  menu never selects a mode. The engine therefore reads its owning button at paint time.
 *  A checkable button whose icon supplies distinct Off and On artwork is a state toggle: Off and
 *  On name the current state, and hover, focus or press preview the opposite state's artwork.
 *  Disabled rendering and non-neutral artwork remain the source icon's own.
 */
class ButtonGlyphStateEngine final : public QIconEngine
{
public:
    ButtonGlyphStateEngine(const QIcon& source, QAbstractButton* owner)
        : source_(source), owner_(owner),
          pairedStates_(!source.availableSizes(QIcon::Normal, QIcon::Off).isEmpty()
                        && !source.availableSizes(QIcon::Normal, QIcon::On).isEmpty())
    {
    }

    QIconEngine* clone() const override { return new ButtonGlyphStateEngine(source_, owner_.data()); }
    QString key() const override { return QStringLiteral("ResourcesButtonGlyphState"); }
    QString iconName() override { return source_.name(); }
    bool isNull() override { return source_.isNull(); }
    QSize actualSize(const QSize& size, QIcon::Mode mode, QIcon::State state) override
    {
        return source_.actualSize(size, mode, state);
    }
    QList<QSize> availableSizes(QIcon::Mode mode, QIcon::State state) override
    {
        return source_.availableSizes(mode, state);
    }
    void addPixmap(const QPixmap& pixmap, QIcon::Mode mode, QIcon::State state) override
    {
        source_.addPixmap(pixmap, mode, state);
    }
    void addFile(const QString& fileName, const QSize& size, QIcon::Mode mode, QIcon::State state) override
    {
        source_.addFile(fileName, size, mode, state);
    }
    QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override
    {
        return scaledPixmap(size, mode, state, 1.0);
    }
    QPixmap scaledPixmap(const QSize& size, QIcon::Mode mode, QIcon::State state, qreal scale) override
    {
        QIcon::State shown = state;
        const QColor ink = mode == QIcon::Disabled ? QColor() : interactionInk(state, &shown);
        if (!ink.isValid()) return source_.pixmap(size, scale, mode, shown);
        const QColor text = Resources::ResourcesTheme::instance().themePalette().value(QStringLiteral("text"));
        return tintedGlyph(source_.pixmap(size, scale, QIcon::Normal, shown), ink, text);
    }
    void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state) override
    {
        const qreal scale = painter->device() ? painter->device()->devicePixelRatio() : qApp->devicePixelRatio();
        const QPixmap pixmap = scaledPixmap(rect.size(), mode, state, scale);
        QRect target(QPoint(), pixmap.deviceIndependentSize().toSize());
        target.moveCenter(rect.center());
        painter->drawPixmap(target, pixmap);
    }

private:
    /** @brief Selects the ink and, for state toggles, the artwork state to draw.
     *  Single-artwork buttons mirror the state outline: press, open menu or checked wins over
     *  pointer hover. State toggles draw a preview of the opposite state in `hoverBorder` and the
     *  pressed target in `focusBorder`; at rest, checked follows the requested icon state so an
     *  Off-state query keeps the supplied artwork.
     */
    QColor interactionInk(QIcon::State state, QIcon::State* shown) const
    {
        QAbstractButton* button = owner_.data();
        if (!button || !button->isEnabled() || button->property("glyphStateTint") == QVariant(false)) return {};
        const QMenu* menu = nullptr;
        if (const auto* tool = qobject_cast<const QToolButton*>(button)) menu = tool->menu();
        else if (const auto* push = qobject_cast<const QPushButton*>(button)) menu = push->menu();
        const auto palette = Resources::ResourcesTheme::instance().themePalette();
        const bool active = button->isDown() || (menu && menu->isVisible());
        const bool engaged = button->underMouse();
        const bool checkedState = state == QIcon::On && button->isCheckable() && button->isChecked();
        if (pairedStates_ && button->isCheckable()
            && button->property("iconStateRole").toString() != QStringLiteral("action")) {
            const bool suppressed = toggleGlyphPreviewSuppressed(*button);
            if (active || (engaged && !suppressed)) {
                *shown = state == QIcon::On ? QIcon::Off : QIcon::On;
                return palette.value(active ? QStringLiteral("focusBorder") : QStringLiteral("hoverBorder"));
            }
            return checkedState ? palette.value(QStringLiteral("focusBorder")) : QColor();
        }
        if (active || checkedState) return palette.value(QStringLiteral("focusBorder"));
        if (engaged) return palette.value(QStringLiteral("hoverBorder"));
        return {};
    }

    QIcon source_;
    QPointer<QAbstractButton> owner_;
    bool pairedStates_ = false;
};

/** @brief Returns whether a tool or push button shows its icon without visible text. */
static bool showsIconOnly(const QAbstractButton& button)
{
    if (const auto* tool = qobject_cast<const QToolButton*>(&button)) {
        const auto style = tool->toolButtonStyle();
        if (style == Qt::ToolButtonTextOnly) return false;
        return style == Qt::ToolButtonIconOnly || style == Qt::ToolButtonFollowStyle || tool->text().isEmpty();
    }
    return button.text().isEmpty();
}

/** @brief Gives a tool or push button's current icon glyph states once per icon value.
 *  `resourcesGlyphButton` marks icon-only neutral glyphs, whose state is shown by glyph color
 *  instead of an outline. Icon, text and style changes are re-evaluated on the next paint.
 */
static void adoptButtonGlyphStates(QAbstractButton& button)
{
    static constexpr char adoptedKey[] = "_resourcesGlyphStateIcon";
    static constexpr char neutralKey[] = "_resourcesGlyphStateNeutral";
    const QIcon icon = button.icon();
    // `glyphStateTint=false` keeps the supplied artwork and the button's own state styling.
    const bool optedOut = button.property("glyphStateTint") == QVariant(false);
    if (!optedOut && !icon.isNull() && button.property(adoptedKey) != QVariant(icon.cacheKey())) {
        const QSize size = button.iconSize().isValid() ? button.iconSize() : QSize(16, 16);
        const QColor text = Resources::ResourcesTheme::instance().themePalette().value(QStringLiteral("text"));
        const bool neutral = isNeutralGlyph(icon.pixmap(size, button.devicePixelRatio()), text);
        button.setIcon(QIcon(new ButtonGlyphStateEngine(icon, &button)));
        button.setProperty(adoptedKey, button.icon().cacheKey());
        button.setProperty(neutralKey, neutral);
    }
    const bool glyphButton = !optedOut && !icon.isNull() && button.property(neutralKey).toBool()
        && showsIconOnly(button);
    if (button.property("resourcesGlyphButton") != QVariant(glyphButton)) {
        button.setProperty("resourcesGlyphButton", glyphButton);
        if (button.testAttribute(Qt::WA_WState_Polished)) {
            button.style()->unpolish(&button);
            button.style()->polish(&button);
            // A direct repolish sends no StyleChange; the dropped outline changes the size hint.
            button.updateGeometry();
        }
    }
}

class ResourceStyleFilter : public QObject
{
public:
    explicit ResourceStyleFilter(QObject* parent = nullptr) : QObject(parent)
    {
        connect(qApp, &QApplication::focusChanged, this, [](QWidget* before, QWidget* after) {
            syncButtonKeyboardFocus(qobject_cast<QAbstractButton*>(before));
            syncButtonKeyboardFocus(qobject_cast<QAbstractButton*>(after));
        });
        connect(qApp, &QGuiApplication::applicationStateChanged, this,
                [](Qt::ApplicationState state) {
            syncButtonKeyboardFocus(qobject_cast<QAbstractButton*>(QApplication::focusWidget()));
            if (state == Qt::ApplicationActive) return;
            for (QWidget* widget : QApplication::topLevelWidgets()) {
                if (auto* menu = qobject_cast<QMenu*>(widget); menu && menu->isVisible()) {
                    menu->close();
                }
            }
        });
    }

protected:
    bool eventFilter(QObject* obj, QEvent* event) override
    {
        // Modality survives automatic focus transfers and popup restoration. Pointer movement
        // alone must not erase a keyboard user's focus indicator.
        if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::TouchBegin
            || event->type() == QEvent::Wheel) {
            qApp->setProperty("_resourcesKeyboardNavigation", false);
            syncButtonKeyboardFocus(qobject_cast<QAbstractButton*>(QApplication::focusWidget()));
        } else if (event->type() == QEvent::KeyPress || event->type() == QEvent::Shortcut) {
            qApp->setProperty("_resourcesKeyboardNavigation", true);
            syncButtonKeyboardFocus(qobject_cast<QAbstractButton*>(QApplication::focusWidget()));
        } else if (event->type() == QEvent::FocusIn) {
            const auto reason = static_cast<QFocusEvent*>(event)->reason();
            if (reason == Qt::MouseFocusReason) qApp->setProperty("_resourcesKeyboardNavigation", false);
            else if (reason == Qt::ShortcutFocusReason) qApp->setProperty("_resourcesKeyboardNavigation", true);
        }
        if (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut
            || event->type() == QEvent::EnabledChange || event->type() == QEvent::Show
            || event->type() == QEvent::Hide || event->type() == QEvent::WindowActivate
            || event->type() == QEvent::WindowDeactivate || event->type() == QEvent::Paint) {
            syncButtonKeyboardFocus(qobject_cast<QAbstractButton*>(obj));
        }
        if (event->type() == QEvent::DynamicPropertyChange) {
            const auto name = static_cast<QDynamicPropertyChangeEvent*>(event)->propertyName();
            if (name == "layoutRole")
                applyLayoutTheme(qobject_cast<QLayout*>(obj));
            if (name == "actionDensity") {
                if (auto* scope = qobject_cast<QWidget*>(obj)) {
                    for (auto* button : scope->findChildren<QToolButton*>())
                        Resources::ResourcesTheme::instance().polishWidget(*button);
                }
            }
        }
        if (event->type() == QEvent::StyleChange)
            applyLayoutTheme(qobject_cast<QWidget*>(obj));
        if (event->type() == QEvent::Polish || event->type() == QEvent::Show
            || event->type() == QEvent::ParentChange) {
            if (auto* widget = qobject_cast<QWidget*>(obj))
                Resources::ResourcesTheme::instance().polishWidget(*widget);
            if (event->type() == QEvent::ParentChange) {
                if (auto* container = qobject_cast<QWidget*>(obj)) {
                    for (auto* button : container->findChildren<QToolButton*>())
                        Resources::ResourcesTheme::instance().polishWidget(*button);
                }
            }
        }

        if (event->type() == QEvent::Enter || event->type() == QEvent::Leave
            || event->type() == QEvent::EnabledChange || event->type() == QEvent::Hide) {
            auto* input = qobject_cast<QWidget*>(obj);
            if (input && (qobject_cast<QComboBox*>(input) || qobject_cast<QAbstractSpinBox*>(input))) {
                const bool hovered = input->isEnabled() && event->type() != QEvent::Leave
                    && event->type() != QEvent::Hide
                    && (event->type() == QEvent::Enter || input->underMouse());
                if (input->property("resourcesIndicatorHovered") != QVariant(hovered)) {
                    // One visual hover state covers the complete composite field, including its editor.
                    input->setProperty("resourcesIndicatorHovered", hovered);
                    input->style()->unpolish(input);
                    input->style()->polish(input);
                    input->update();
                }
            }
        }
        if (event->type() == QEvent::Polish || event->type() == QEvent::Show
            || event->type() == QEvent::StyleChange || event->type() == QEvent::Paint) {
            if (auto* button = qobject_cast<QPushButton*>(obj)) {
                const bool hasIcon = !button->icon().isNull();
                if (button->property("resourcesIconButton") != QVariant(hasIcon)) {
                    // QAbstractButton has no iconChanged signal; paint also admits runtime icon updates.
                    button->setProperty("resourcesIconButton", hasIcon);
                    if (button->testAttribute(Qt::WA_WState_Polished)) {
                        button->style()->unpolish(button);
                        button->style()->polish(button);
                        button->updateGeometry();
                    }
                }
            }
            // Runtime icon replacement is admitted at the next paint, before the button draws it.
            if (auto* button = qobject_cast<QAbstractButton*>(obj);
                button && (qobject_cast<QToolButton*>(button) || qobject_cast<QPushButton*>(button)))
                adoptButtonGlyphStates(*button);
        }
        if (event->type() == QEvent::MouseButtonRelease || event->type() == QEvent::ApplicationDeactivate
            || (obj == pressedInput_ && (event->type() == QEvent::Hide || event->type() == QEvent::EnabledChange))) {
            setPressedInput(nullptr);
        }
        if (event->type() == QEvent::MouseButtonPress
            && static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) {
            auto* widget = qobject_cast<QWidget*>(obj);
            QWidget* input = nullptr;
            for (auto* current = widget; current; current = current->parentWidget()) {
                if (qobject_cast<QComboBox*>(current) || qobject_cast<QAbstractSpinBox*>(current)) {
                    input = current;
                    break;
                }
                if (qobject_cast<QLineEdit*>(current) || qobject_cast<QPlainTextEdit*>(current)
                    || qobject_cast<QTextEdit*>(current)) input = current;
                if (current->isWindow()) break;
            }
            setPressedInput(input && input->isEnabled() ? input : nullptr);
        }
        if (event->type() == QEvent::ActionAdded || event->type() == QEvent::ActionChanged
            || event->type() == QEvent::ActionRemoved || event->type() == QEvent::LayoutDirectionChange) {
            if (auto* menu = qobject_cast<QMenu*>(obj)) prepareMenuCheckIndicators(menu);
        }
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
        if (event->type() == QEvent::DevicePixelRatioChange) {
            if (auto* widget = qobject_cast<QWidget*>(obj)) {
                if (isDeviceFeatureTreeViewport(widget)) applyFeatureTreeViewportMask(widget);
                for (auto* tree : widget->findChildren<QTreeWidget*>()) {
                    if (isDeviceFeatureTreeViewport(tree->viewport())) applyFeatureTreeViewportMask(tree->viewport());
                }
            }
        }
#endif
        if (event->type() == QEvent::Show || event->type() == QEvent::Resize
            || event->type() == QEvent::Move || event->type() == QEvent::StyleChange) {
            if (auto* viewport = qobject_cast<QWidget*>(obj); isDeviceFeatureTreeViewport(viewport)) {
                applyFeatureTreeViewportMask(viewport);
            }
        }
        if (event->type() == QEvent::Polish || event->type() == QEvent::Show
            || event->type() == QEvent::StyleChange) {
            if (auto* widget = qobject_cast<QWidget*>(obj)) applyTextDocumentTheme(widget);
        }
        if (event->type() == QEvent::Polish || event->type() == QEvent::Show) {
            if (auto* widget = qobject_cast<QWidget*>(obj); comboPopupOwner(widget)) {
                prepareComboPopup(widget);
            }
        }

        if (event->type() == QEvent::Polish || event->type() == QEvent::Show
            || event->type() == QEvent::StyleChange) {
            if (auto* menu = qobject_cast<QMenu*>(obj)) {
                prepareMenuPopup(menu);
                if (event->type() == QEvent::Show) positionMenuShell(menu);
            }
        }

        if (event->type() == QEvent::Show
            || event->type() == QEvent::Resize
            || event->type() == QEvent::LayoutRequest)
        {
            if (auto* tabBar = qobject_cast<QTabBar*>(obj);
                tabBar != nullptr && graphicsSettingsTabBarFor(tabBar) == tabBar)
            {
                scheduleGraphicsSettingsTabScrollerAlignment(tabBar);
            }
        }

        if (event->type() == QEvent::Enter || event->type() == QEvent::Leave) {
            if (auto* button = qobject_cast<QToolButton*>(obj);
                button && button->objectName() == QLatin1String("AnalysisTabCloseButton")) {
                applyAnalysisTabCloseButtonIcon(button, event->type() == QEvent::Enter);
            }
        }

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
            if (auto* bar = qobject_cast<QProgressBar*>(obj)) {
                paintThemedProgressText(bar);
                return true;
            }
            if (auto* widget = qobject_cast<QWidget*>(obj); widget && !qobject_cast<QMenu*>(obj)) {
                if (auto* button = qobject_cast<QToolButton*>(widget);
                    button && isGraphicsSettingsTabScrollerButton(button)
                    && button->isEnabled() && (button->underMouse() || button->isDown())) {
                    paintGraphicsSettingsTabScrollerButton(button);
                    return true;
                }
                if (widget->objectName() == QLatin1String("AutoCompletePopup") ||
                    widget->objectName() == QLatin1String("SignatureHelpLabel") ||
                    widget->inherits("QTipLabel")) {
                    paintPopupRoundedRect(widget, true);
                } else if (comboPopupOwner(widget) || widget->property("_popupStyled").toBool()) {
                    paintPopupRoundedRect(widget, false);
                    if (comboPopupOwner(widget)) return true;
                }
            }
        }

        if (event->type() == QEvent::Show || event->type() == QEvent::Resize) {
            if (auto* widget = qobject_cast<QWidget*>(obj); widget && !qobject_cast<QMenu*>(obj)) {
                if (widget->objectName() == QLatin1String("AutoCompletePopup") ||
                    widget->objectName() == QLatin1String("SignatureHelpLabel") ||
                    widget->inherits("QTipLabel")) {
                    applyPopupMask(widget, true);
                } else if (comboPopupOwner(widget) || widget->property("_popupStyled").toBool()) {
                    applyPopupMask(widget, false);
                }
            }
        }

        if (event->type() == QEvent::Polish || event->type() == QEvent::Show) {
            if (auto* editor = qobject_cast<QLineEdit*>(obj);
                editor && (qobject_cast<QComboBox*>(editor->parentWidget())
                    || qobject_cast<QAbstractSpinBox*>(editor->parentWidget())) && editor->styleSheet().isEmpty()) {
                // Qt excludes embedded editors from application QSS unless they have a local sheet.
                // Opt in to the shared rule without overriding a consumer's explicit editor theme.
                editor->setStyleSheet(QStringLiteral("/* Resources: style the embedded field editor. */"));
            }
        }

        if (event->type() == QEvent::Polish) {
            if (auto* combo = qobject_cast<QComboBox*>(obj)) {
                if (auto* view = combo->view()) {
                    prepareComboPopup(view->window());
                }
            }
            else if (auto* rubberBand = qobject_cast<QRubberBand*>(obj)) {
                applyDockRubberBandStyle(rubberBand);
            }
            else if (auto* widget = qobject_cast<QWidget*>(obj)) {
                if (auto* button = qobject_cast<QToolButton*>(widget);
                    button && button->objectName() == QLatin1String("AnalysisTabCloseButton")) {
                    applyAnalysisTabCloseButtonIcon(button, false);
                }
                else if (auto* table = qobject_cast<QTableWidget*>(widget)) {
                    applyRowTableStyle(table);
                    applyDiagnosticsTableStyle(table);
                }
                else if (widget->objectName() == QLatin1String("AutoCompletePopup")) {
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

        if (event->type() == QEvent::Polish || event->type() == QEvent::PaletteChange) {
            if (auto* label = qobject_cast<QLabel*>(obj);
                label && label->objectName() == QLatin1String("PluginMarketplaceUpdateIcon")) {
                applyPluginUpdateIcon(label);
            }
        }

        if (event->type() == QEvent::Hide) {
            if (auto* popup = qobject_cast<QWidget*>(obj); popup && popup->property("_popupStyled").toBool()) {
                if (auto* combo = qobject_cast<QComboBox*>(popup->parentWidget())) {
                    setComboPopupDirection(combo, {});
                }
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
                        const bool above = geo.top() < combo->mapToGlobal(QPoint()).y();
                        setComboPopupDirection(combo, above ? QStringLiteral("above") : QStringLiteral("below"));
                        prepareComboPopup(widget);
                        widget->setGeometry(geo);
                        applyPopupMask(widget, false);
                    }
                }
            }
        }

        if (event->type() == QEvent::Polish || event->type() == QEvent::Show) {
            if (auto* view = qobject_cast<QAbstractItemView*>(obj)) Resources::applyItemViewTheme(view);
        }
        return QObject::eventFilter(obj, event);
    }

private:
    QPointer<QWidget> pressedInput_;

    /** @brief Exposes a held pointer press for input frames without changing focus or input handling. */
    void setPressedInput(QWidget* input)
    {
        if (pressedInput_ == input) return;
        const auto polish = [](QWidget* widget, bool pressed) {
            if (!widget) return;
            widget->setProperty("interactionPressed", pressed);
            widget->style()->unpolish(widget);
            widget->style()->polish(widget);
            widget->update();
        };
        const QPointer<QWidget> previous = pressedInput_;
        pressedInput_ = input;
        polish(previous, false);
        polish(pressedInput_, true);
    }

    /** @brief Shares menu leading columns and check marks without changing toolbar icons. */
    static void prepareMenuCheckIndicators(QMenu* menu)
    {
        if (menu->property("resourcesMenuPreparing").toBool()) return;
        menu->setProperty("resourcesMenuPreparing", true);
        const bool usesIcons = menu->property("menuPresentation").toString() == QLatin1String("icons");
        bool leadingColumn = false;
        for (QAction* action : menu->actions()) {
            const QVariant original = action->property("resourcesMenuIconVisibility");
            if (action->isCheckable() && !usesIcons) {
                if (!original.isValid())
                    action->setProperty("resourcesMenuIconVisibility", action->isIconVisibleInMenu());
                if (action->isIconVisibleInMenu()) action->setIconVisibleInMenu(false);
            } else if (original.isValid()) {
                action->setProperty("resourcesMenuIconVisibility", QVariant());
                action->setIconVisibleInMenu(original.toBool());
            }
            if (action->isVisible() && !action->isSeparator())
                leadingColumn |= action->isCheckable()
                    || (!action->icon().isNull() && action->isIconVisibleInMenu());
        }
        const QString direction = menu->layoutDirection() == Qt::RightToLeft
            ? QStringLiteral("rtl") : QStringLiteral("ltr");
        if (menu->property("resourcesMenuHasLeadingColumn") != QVariant(leadingColumn)
            || menu->property("resourcesMenuDirection").toString() != direction) {
            menu->setProperty("resourcesMenuHasLeadingColumn", leadingColumn);
            menu->setProperty("resourcesMenuDirection", direction);
            if (menu->testAttribute(Qt::WA_WState_Polished)) {
                menu->style()->unpolish(menu);
                menu->style()->polish(menu);
                // Direct style polish can reinstall a native effect without sending a style event.
                prepareMenuPopup(menu);
                menu->updateGeometry();
                menu->update();
            }
        }
        menu->setProperty("resourcesMenuPreparing", false);
    }

    /** @brief Keeps the visible shell at the requested anchor while reserving an external shadow gutter. */
    static void positionMenuShell(QMenu* menu)
    {
        QMenu* parentMenu = nullptr;
        for (QWidget* widget : QApplication::topLevelWidgets()) {
            auto* parent = qobject_cast<QMenu*>(widget);
            if (parent && parent != menu && parent->isVisible() && parent->activeAction()
                && parent->activeAction()->menu() == menu) {
                parentMenu = parent;
                break;
            }
        }
        const int gutter = presentationMetric("MENU_SHADOW_GUTTER");
        const QRect geometry = menu->geometry();
        bool opensLeft = menu->isRightToLeft();
        bool opensUp = false;
        if (parentMenu) {
            opensLeft = geometry.center().x() < parentMenu->geometry().center().x();
        } else if (const QScreen* screen = menu->screen()) {
            const QRect bounds = screen->availableGeometry();
            const int frame = menu->style()->pixelMetric(QStyle::PM_MenuDesktopFrameWidth, nullptr, menu);
            const QPoint cursor = QCursor::pos();
            // Qt snaps cursor popups above the cursor or clamps programmatic popups to the screen.
            const bool cursorAtRight = qAbs(geometry.right() + 1 - cursor.x()) <= 3
                && cursor.y() >= geometry.top() - 3 && cursor.y() <= geometry.bottom() + 3;
            const bool cursorAtBottom = qAbs(geometry.bottom() + 1 + frame - cursor.y()) <= 3
                && cursor.x() >= geometry.left() - 3 && cursor.x() <= geometry.right() + 3;
            opensLeft = geometry.right() >= bounds.right() - frame - 1 || cursorAtRight;
            if (geometry.left() <= bounds.left() + frame + 1) opensLeft = false;
            opensUp = geometry.bottom() >= bounds.bottom() - frame - 1 || cursorAtBottom;
            // Menu-bar popups may be opened by keyboard, away from the cursor.
            for (QObject* object : menu->menuAction()->associatedObjects()) {
                if (auto* bar = qobject_cast<QMenuBar*>(object); bar && bar->isVisible()) {
                    const QRect action = bar->actionGeometry(menu->menuAction());
                    opensLeft = geometry.center().x() < bar->mapToGlobal(action.center()).x();
                    break;
                }
            }
        }
        // Qt already aligns a submenu's first action vertically with its parent action.
        QPoint position = menu->pos() + QPoint(opensLeft ? gutter : -gutter,
                                               parentMenu ? 0 : (opensUp ? gutter : -gutter));
        if (const QScreen* screen = menu->screen()) {
            const QRect bounds = screen->availableGeometry();
            // Keep the visible shell on-screen; transparent shadow gutters may cross an edge.
            position.setX(qBound(bounds.left() - gutter, position.x(),
                                qMax(bounds.left() - gutter, bounds.right() - menu->width() + gutter + 1)));
            position.setY(qBound(bounds.top() - gutter, position.y(),
                                qMax(bounds.top() - gutter, bounds.bottom() - menu->height() + gutter + 1)));
        }
        menu->move(position);
    }

    /** @brief Reserves the semibold hover label width, so hovering never clips or resizes a row.
     *  Qt sizes every row with its normal-state font, while QSS paints the selected row with
     *  `WEIGHT_EMPHASIS`. The reservation runs before each popup sizes its rows; a consumer's
     *  own minimum width is retained.
     */
    static void reserveMenuSelectionWidth(QMenu* menu)
    {
        static constexpr char baseKey[] = "_resourcesMenuBaseMinimumWidth";
        static constexpr char reservedKey[] = "_resourcesMenuReservedMinimumWidth";
        int base = menu->minimumWidth();
        if (menu->property(reservedKey) == QVariant(base)) base = menu->property(baseKey).toInt();
        const auto width = [](const QFontMetrics& metrics, const QString& text) {
            return metrics.boundingRect(QRect(), Qt::TextSingleLine | Qt::TextShowMnemonic, text).width();
        };
        int labelExtra = 0;
        int shortcutExtra = 0;
        for (QAction* action : menu->actions()) {
            if (!action->isVisible() || action->isSeparator()) continue;
            const QFont normal = action->font().resolve(menu->font());
            QFont emphasis = normal;
            emphasis.setWeight(QFont::Weight(presentationMetric("WEIGHT_EMPHASIS")));
            const QFontMetrics normalMetrics(normal);
            const QFontMetrics emphasisMetrics(emphasis);
            QString label = action->text();
            QString shortcut;
            if (const auto tab = label.indexOf(u'\t'); tab >= 0) {
                shortcut = label.mid(tab + 1);
                label.truncate(tab);
            } else if (!action->shortcut().isEmpty()) {
                shortcut = action->shortcut().toString(QKeySequence::NativeText);
            }
            labelExtra = qMax(labelExtra, width(emphasisMetrics, label) - width(normalMetrics, label));
            if (!shortcut.isEmpty())
                shortcutExtra = qMax(shortcutExtra, emphasisMetrics.horizontalAdvance(shortcut)
                                                        - normalMetrics.horizontalAdvance(shortcut));
        }
        // QMenu recomputes cached row geometry, which reads minimumWidth(), only after a font,
        // style or action change; a font-change notification requests exactly that.
        const auto invalidateRows = [menu] {
            QEvent change(QEvent::FontChange);
            QCoreApplication::sendEvent(menu, &change);
        };
        menu->setMinimumWidth(base);
        invalidateRows();
        const int extra = labelExtra + shortcutExtra;
        const int reserved = extra > 0 ? qMax(base, menu->sizeHint().width() + extra) : base;
        menu->setMinimumWidth(reserved);
        invalidateRows();
        menu->setProperty(baseKey, base);
        menu->setProperty(reservedKey, reserved);
    }

    /** @brief Gives the single QSS shell a bounded Resources-owned shadow across native-style polish. */
    static void prepareMenuPopup(QMenu* menu)
    {
        prepareMenuCheckIndicators(menu);
        if (!menu->property("_resourcesMenuSelectionWidth").toBool()) {
            menu->setProperty("_resourcesMenuSelectionWidth", true);
            QObject::connect(menu, &QMenu::aboutToShow, menu, [menu] { reserveMenuSelectionWidth(menu); });
        }
        const auto flags = menu->windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint;
        const QRect geometry = menu->geometry();
        const bool visible = menu->isVisible();
        if (menu->windowFlags() != flags) menu->setWindowFlags(flags);
        menu->setAttribute(Qt::WA_TranslucentBackground, true);
        menu->setAttribute(Qt::WA_OpaquePaintEvent, false);
        // Qt's Windows 11 style installs a widget shadow even with NoDropShadowWindowHint.
        // Replace it with a centered, low-opacity effect whose full extent fits the QSS gutter.
        auto* shadow = qobject_cast<QGraphicsDropShadowEffect*>(menu->graphicsEffect());
        if (!shadow || !shadow->property("resourcesMenuShadow").toBool()) {
            shadow = new MenuDropShadowEffect(menu);
            shadow->setProperty("resourcesMenuShadow", true);
            menu->setGraphicsEffect(shadow);
        }
        shadow->setBlurRadius(presentationMetric("MENU_SHADOW_GUTTER"));
        shadow->setOffset(0, 2);
        auto shadowColor = Resources::themePalette().value(QStringLiteral("shadow"));
        shadowColor.setAlpha(36);
        shadow->setColor(shadowColor);
        menu->clearMask(); // Preserve the stylesheet's fractional edge coverage at every DPR.
        if (menu->geometry() != geometry) menu->setGeometry(geometry);
        if (visible && !menu->isVisible()) menu->show();
    }

    /** @brief Clips feature content inside the shared frame without restoring bottom padding. */
    static void applyFeatureTreeViewportMask(QWidget* viewport)
    {
        const auto* tree = qobject_cast<const QTreeWidget*>(viewport->parentWidget());
        if (!tree || viewport->size().isEmpty()) return;
        const auto metrics = Resources::ResourcesTheme::instance().metrics(
            Resources::ResourcesTheme::ControlKind::TreeFrame);
        const int radius = metrics.radius;
        if (radius <= 0) { viewport->clearMask(); return; }
        // A binary child mask must stay inside the parent's antialiased border.
        // Whole logical pixel squares need two DIPs of arc clearance at fractional
        // scales. Guard only the corner arcs; the straight bottom remains flush.
        const int inset = metrics.borderWidth + 2;
        QPainterPath outline;
        outline.addRoundedRect(QRectF(tree->rect()).adjusted(inset, inset, -inset, -inset),
                               qMax(0, radius - inset), qMax(0, radius - inset));
        QRegion region(outline.toFillPolygon().toPolygon());
        region += QRect(0, 0, tree->width(), qMax(0, tree->height() - radius));
        region += QRect(radius, qMax(0, tree->height() - radius),
                        qMax(0, tree->width() - 2 * radius), radius);
        region.translate(-viewport->pos());
        region &= viewport->rect();
        // A fractional backing-store row can straddle the straight QSS border.
        // Keep that binary paint row in the frame, without changing layout.
        const qreal dpr = viewport->devicePixelRatioF();
        if (!qFuzzyCompare(dpr, qRound(dpr))) {
            region &= QRect(0, 0, viewport->width(), qMax(0, viewport->height() - 1));
        }
        if (viewport->mask() != region) viewport->setMask(region);
    }

    /** @brief Gives themed editor/log surfaces one QSS-owned content gutter without a document inset. */
    static void applyTextDocumentTheme(QWidget* widget)
    {
        const QString name = widget->objectName();
        if (name != QLatin1String("ProcessingConsoleLog") && name != QLatin1String("ProcessingParamEditor")
            && name != QLatin1String("SystemLogViewer")) return;
        QTextDocument* document = nullptr;
        if (auto* plain = qobject_cast<QPlainTextEdit*>(widget)) document = plain->document();
        else if (auto* rich = qobject_cast<QTextEdit*>(widget)) document = rich->document();
        if (document && document->documentMargin() != 0) document->setDocumentMargin(0);
    }

    /** @brief Identifies standard combo popup windows through their public widget ownership. */
    static QComboBox* comboPopupOwner(QWidget* widget)
    {
        return widget && widget->windowType() == Qt::Popup
            ? qobject_cast<QComboBox*>(widget->parentWidget()) : nullptr;
    }

    /** @brief Applies a combo shell at polish and every opening, including existing or replaced views. */
    static void prepareComboPopup(QWidget* popup)
    {
        if (!comboPopupOwner(popup)) return;
        popup->setProperty("_popupStyled", true);
        const auto flags = popup->windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint;
        const QRect geometry = popup->geometry();
        const bool visible = popup->isVisible();
        if (popup->windowFlags() != flags) popup->setWindowFlags(flags);
        popup->setAttribute(Qt::WA_TranslucentBackground, true);
        // Windows' Qt style can install a widget effect despite NoDropShadowWindowHint.
        // The shared shell owns the outline; a second shadow pollutes its alpha edge.
        if (qobject_cast<QGraphicsDropShadowEffect*>(popup->graphicsEffect())) popup->setGraphicsEffect(nullptr);
        popup->clearMask();
        if (auto* frame = qobject_cast<QFrame*>(popup)) frame->setFrameShape(QFrame::NoFrame);
        popup->setContentsMargins(0, 0, 0, 0);
        if (popup->geometry() != geometry) popup->setGeometry(geometry);
        if (visible && !popup->isVisible()) popup->show();
    }

    /** @brief Renders the existing update glyph in the indicator's theme color. */
    static void applyPluginUpdateIcon(QLabel* label)
    {
        QPixmap icon = QIcon(QStringLiteral(":/Resources/icon/installing-updates-48.png"))
            .pixmap(QSize(12, 12), label->devicePixelRatioF());
        if (icon.isNull()) return;
        QPainter painter(&icon);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(icon.rect(), label->palette().color(QPalette::WindowText));
        painter.end();
        label->setPixmap(icon);
    }

    /** @brief Keeps the combo's joined corners synchronized with its visible popup. */
    static void setComboPopupDirection(QComboBox* combo, const QString& direction)
    {
        if (combo->property("popupDirection").toString() == direction) return;
        combo->setProperty("popupDirection", direction);
        combo->style()->unpolish(combo);
        combo->style()->polish(combo);
        combo->update();
    }

    static void applyAnalysisTabCloseButtonIcon(QToolButton* button, const bool hover)
    {
        if (!button) return;
        const QString path = hover
            ? QStringLiteral(":/Resources/icon/close-window-rounded-48.png")
            : QStringLiteral(":/Resources/icon/close-window-outline-48.png");
        button->setIcon(QIcon(path));
        button->setIconSize(QSize(16, 16));
    }

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
        // Feature rows select as one row; omit the current-cell capsule so the Feature
        // cell is not emphasized separately from its value editor.
        tree->setSelectionBehavior(QAbstractItemView::SelectRows);
        Resources::setItemSelectionAppearance(tree, Resources::ItemSelectionAppearance::JoinedOnly);
        tree->setItemDelegate(new DeviceFeatureTreeDelegate(tree));

        QHeaderView* header = tree->header();
        // Explicit Stretch mode lets Value shrink without retaining its previous width.
        header->setStretchLastSection(false);
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

        const QString role = layout->property("layoutRole").toString();
        const QString presentation = layout->property("presentationLayout").toString();
        if (!presentation.isEmpty()) {
            const bool page = presentation == QLatin1String("page");
            const bool content = presentation == QLatin1String("content");
            const int inset = page || content ? presentationMetric("SPACE_CONTENT") : 0;
            const int spacing = page || presentation == QLatin1String("columns")
                ? presentationMetric("SPACE_SECTION") : content
                ? presentationMetric("SPACE_CONTENT") : presentationMetric("SPACE_ROW");
            setLayoutMetrics(layout, QMargins(inset, inset, inset, inset), spacing);
            if (auto* form = qobject_cast<QFormLayout*>(layout)) {
                form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
                form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
                form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
                form->setHorizontalSpacing(presentationMetric("SPACE_ROW"));
                form->setVerticalSpacing(presentationMetric("SPACE_ROW"));
                form->setRowWrapPolicy(layout->property("presentationStacked").toBool()
                    ? QFormLayout::WrapAllRows : QFormLayout::WrapLongRows);
            }
        }
        else if (!role.isEmpty()) {
            const int tight = presentationMetric("SPACE_TIGHT");
            const int row = presentationMetric("SPACE_ROW");
            const int content = presentationMetric("SPACE_CONTENT");
            const int section = presentationMetric("SPACE_SECTION");
            const auto& theme = Resources::ResourcesTheme::instance();
            const int compact = theme.metric("LAYOUT_COMPACT_SPACING");
            const int compactInset = theme.metric("LAYOUT_COMPACT_INSET");
            const int caption = theme.metric("LAYOUT_CAPTION_SPACING");
            const int text = theme.metric("LAYOUT_TEXT_SPACING");
            const int cardY = theme.metric("LAYOUT_CARD_INSET_Y");
            const int searchX = theme.metric("LAYOUT_SEARCH_INSET_X");
            struct Pattern { const char* role; QMargins margins; int spacing; };
            const Pattern patterns[] = {
                {"flush", {}, 0}, {"actions", {}, compact}, {"stack", {}, tight},
                {"row", {}, row}, {"caption-stack", {}, caption}, {"text-stack", {}, text},
                {"sections", {}, section}, {"form", {}, content}, {"wide-form", {}, content},
                {"content", {content, content, content, content}, row},
                {"page", {content, content, content, content}, content},
                {"compact-panel", {row, row, row, row}, compact},
                {"card", {compactInset, cardY, compactInset, cardY}, compact},
                {"bottom-panel", {content, 0, content, content}, row},
                {"tabbed-panel", {content, row, content, content}, row},
                {"search", {searchX, 0, searchX, 0}, compact},
                {"tab-strip", {content, cardY, compactInset, cardY}, tight},
                {"inset", {content, content, content, content}, 0},
                {"header", {content, tight, content, tight}, compact},
                {"section", {row, row, row, row}, row},
                {"field-grid", {}, row}, {"label-form", {}, row},
            };
            for (const auto& pattern : patterns) {
                if (role != QLatin1String(pattern.role)) continue;
                setLayoutMetrics(layout, pattern.margins, pattern.spacing);
                break;
            }
            if (role == QLatin1String("search")) {
                for (int index = 0; index < layout->count(); ++index)
                    layout->itemAt(index)->setAlignment(Qt::AlignVCenter);
            } else if (role == QLatin1String("field-grid")) {
                if (auto* grid = qobject_cast<QGridLayout*>(layout))
                    setGridLayoutMetrics(grid, {}, content, row);
            } else if (role == QLatin1String("wide-form")) {
                if (auto* form = qobject_cast<QFormLayout*>(layout)) {
                    form->setHorizontalSpacing(2 * content);
                    form->setVerticalSpacing(content);
                }
            } else if (role == QLatin1String("label-form")) {
                if (auto* form = qobject_cast<QFormLayout*>(layout)) {
                    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
                    form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
                    form->setHorizontalSpacing(row);
                    for (int index = 0; index < form->rowCount(); ++index) {
                        auto* item = form->itemAt(index, QFormLayout::LabelRole);
                        auto* label = item ? qobject_cast<QLabel*>(item->widget()) : nullptr;
                        if (!label) continue;
                        label->setFixedWidth(label->fontMetrics().horizontalAdvance(QStringLiteral("Minimum (mm)")));
                        label->setWordWrap(true);
                    }
                }
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

    /** @brief Overlaps the connecting border so native rounding cannot separate the two shells. */
    static QRect repositionComboPopup(QComboBox* combo, QWidget* popup, QRect geo)
    {
        Q_UNUSED(popup);
        const QPoint comboGlobal = combo->mapToGlobal(QPoint(0, 0));
        const int comboW = combo->width();
        const int comboH = combo->height();

        // Resources paints the container's 1px border inside its geometry.
        // To align outer edges with combo (which also has 1px border),
        // position container at combo's left edge and match full width.
        const int containerW = comboW;
        const int containerX = comboGlobal.x();
        // The control retains a white 1px connecting border to keep its size.
        // Cover that row with the popup's straight side borders. An exact edge
        // adjacency can expose a physical-pixel gap after native DPI rounding.
        // One additional logical row covers fractional border rasterization.
        const int joinOverlap = 2;
        const int containerY = comboGlobal.y() + comboH - joinOverlap;

        geo.setWidth(containerW);
        geo.setHeight(geo.height());

        if (auto* screen = combo->screen()) {
            const QRect screenGeo = screen->availableGeometry();
            if (containerY + geo.height() > screenGeo.bottom()) {
                geo.moveTo(containerX, comboGlobal.y() - geo.height() + joinOverlap);
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
        const auto* combo = qobject_cast<QComboBox*>(widget->parentWidget());
        const QColor borderColor(combo ? Resources::themePalette().value(QStringLiteral("focusBorder")) : Resources::borderColor());
        if (!allRounded && combo && combo->property("popupDirection") == QStringLiteral("above")) {
            painter.translate(w, h);
            painter.rotate(180);
        }

        if (allRounded) {
            painter.setRenderHint(QPainter::Antialiasing, false);
            painter.setCompositionMode(QPainter::CompositionMode_Source);
            painter.setPen(Qt::NoPen);
            painter.setBrush(borderColor);
            painter.drawRoundedRect(QRectF(0.0, 0.0, w, h), 9.0, 9.0);

            painter.setBrush(Qt::white);
            painter.drawRoundedRect(QRectF(1.0, 1.0, w - 2.0, h - 2.0), 8.0, 8.0);

            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            painter.setPen(QPen(borderColor, 1.0));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(QRectF(0.5, 0.5, w - 1.0, h - 1.0), 8.0, 8.0);
        } else {
            // Clear old edge coverage before repainting a resized translucent shell.
            painter.setCompositionMode(QPainter::CompositionMode_Source);
            painter.fillRect(QRectF(0, 0, w, h), Qt::transparent);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            // Concentric antialiased fills preserve complete coverage at the
            // tangent. A separately antialiased fill and stroke can leave a gap.
            const auto shellPath = [w, h](qreal inset) {
                const qreal radius = 8.5 - inset;
                const qreal diameter = radius * 2;
                QPainterPath path;
                path.moveTo(w - inset, 0);
                path.lineTo(w - inset, h - 8.5);
                path.arcTo(QRectF(w - 8.5 - radius, h - 8.5 - radius, diameter, diameter), 0, -90);
                path.lineTo(8.5, h - inset);
                path.arcTo(QRectF(inset, h - 8.5 - radius, diameter, diameter), -90, -90);
                path.lineTo(inset, 0);
                path.closeSubpath();
                return path;
            };
            painter.setPen(Qt::NoPen);
            painter.setBrush(borderColor);
            painter.drawPath(shellPath(0));
            painter.setBrush(Qt::white);
            painter.drawPath(shellPath(1));
        }
    }

    /**
     * @brief Applies the binary mask that clips a popup to its painted shell.
     * @param widget Popup widget whose mask is being updated.
     * @param allRounded Whether all four popup corners are rounded.
     * @note The mask uses the same logical extent as the painter's QRectF so
     *       the right and bottom antialiased edges are not clipped early.
     */
    static void applyPopupMask(QWidget* widget, bool allRounded)
    {
        if (comboPopupOwner(widget)) {
            // Translucent combo shells preserve fractional alpha at every DPR.
            widget->clearMask();
            return;
        }
        QBitmap bmp(widget->size());
        bmp.fill(Qt::color0);
        QPainter painter(&bmp);
        if (!allRounded) {
            if (const auto* combo = qobject_cast<QComboBox*>(widget->parentWidget());
                combo && combo->property("popupDirection") == QStringLiteral("above")) {
                painter.translate(widget->width(), widget->height());
                painter.rotate(180);
            }
        }
        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.setBrush(Qt::color1);
        painter.setPen(Qt::NoPen);

        if (allRounded) {
            painter.drawRoundedRect(
                QRectF(0.0, 0.0, widget->width(), widget->height()), 9.0, 9.0);
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
            " border: 1px solid %1;"
            "}").arg(Resources::borderColor().name(QColor::HexRgb)));
    }
};

namespace Resources {
QObject* installThemeRuntime(QApplication& app, QObject* owner)
{
#ifdef Q_OS_WIN
    if (app.style()->objectName() == QLatin1String("windows11")) {
        auto* nativeStyle = new ResourcesNativeStyle(QStyleFactory::create(QStringLiteral("windows11")));
        app.setStyle(nativeStyle);
    }
#endif
    // The proxy adopts the current style; QApplication deletes only styles it parents itself.
    app.setStyle(new ResourcesIconStateStyle(app.style()));
    auto* filter = new ResourceStyleFilter(owner);
    app.installEventFilter(filter);
    return filter;
}
}

#include "ThemeRuntime.moc"
