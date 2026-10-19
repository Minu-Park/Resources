#include "Resources.h"
#include "ItemViewTheme.h"
#include "Chrome/ThemedTreeWidget.h"

#include <QApplication>
#include <QPointer>
#include <QFile>
#include <QIcon>
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
#include <QGraphicsDropShadowEffect>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
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

#ifdef Q_OS_WIN
#include <dwmapi.h>
#endif

// Global-scope helper to execute Q_INIT_RESOURCE, which relies on global symbols.
// This ensures the resource system registers the compiled .qrc binary data.
inline void initResourcesHelper()
{
    Q_INIT_RESOURCE(Resources);
}

// Shared by popup geometry/effects and the installed QSS token.
static constexpr int MenuShadowGutter = 12;

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
    painter.fillRect(button->rect(), QColor(QStringLiteral("#ffffff")));

    const QColor hoverColor = button->isDown()
        ? QColor(QStringLiteral("#94a3b8"))
        : QColor(QStringLiteral("#cbd5e1"));
    const QRectF hoverRect = QRectF(button->rect()).adjusted(5.0, 5.0, -5.0, -5.0);
    painter.setPen(Qt::NoPen);
    painter.setBrush(hoverColor);
    painter.drawRoundedRect(hoverRect, 4.0, 4.0);

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

/** @brief Preserves progress text contrast over both the empty track and the themed fill. */
static void paintThemedProgressText(QProgressBar* bar)
{
    QStyleOptionProgressBar option;
    option.initFrom(bar);
    option.rect = bar->rect();
    option.minimum = bar->minimum();
    option.maximum = bar->maximum();
    option.progress = bar->value();
    option.textVisible = false;
    option.invertedAppearance = bar->invertedAppearance();
    option.state.setFlag(QStyle::State_Horizontal, bar->orientation() == Qt::Horizontal);
    QPainter painter(bar);
    bar->style()->drawControl(QStyle::CE_ProgressBarGroove, &option, &painter, bar);
    bar->style()->drawControl(QStyle::CE_ProgressBarContents, &option, &painter, bar);
    const auto label = [&](const QColor& color) {
        painter.save();
        painter.setPen(color);
        QRect textRect = bar->rect();
        if (bar->orientation() == Qt::Vertical) {
            textRect = QRect(0, 0, bar->height(), bar->width());
            if (bar->textDirection() == QProgressBar::TopToBottom) {
                painter.translate(bar->width(), 0);
                painter.rotate(90);
            } else {
                painter.translate(0, bar->height());
                painter.rotate(-90);
            }
        }
        painter.drawText(textRect, bar->alignment() | Qt::AlignVCenter | Qt::TextSingleLine, bar->text());
        painter.restore();
    };
    label(bar->palette().color(QPalette::Text));
    if (bar->maximum() <= bar->minimum() || bar->value() < bar->minimum()) return;
    const qreal fraction = (qreal(bar->value()) - bar->minimum())
        / (qreal(bar->maximum()) - bar->minimum());
    QRectF fill = bar->style()->subElementRect(QStyle::SE_ProgressBarContents, &option, bar);
    if (bar->orientation() == Qt::Horizontal) {
        const bool fromRight = bar->isRightToLeft() != bar->invertedAppearance();
        const qreal width = fill.width() * fraction;
        if (fromRight) fill.setLeft(fill.right() - width); else fill.setWidth(width);
    } else {
        const qreal height = fill.height() * fraction;
        if (bar->invertedAppearance()) fill.setHeight(height); else fill.setTop(fill.bottom() - height);
    }
    painter.setClipRect(fill);
    label(bar->palette().color(QPalette::HighlightedText));
}

class ResourceStyleFilter : public QObject
{
public:
    explicit ResourceStyleFilter(QObject* parent = nullptr) : QObject(parent)
    {
        connect(qApp, &QGuiApplication::applicationStateChanged, this,
                [](Qt::ApplicationState state) {
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
            if (auto* bar = qobject_cast<QProgressBar*>(obj); bar && bar->isTextVisible()) {
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
                editor && qobject_cast<QComboBox*>(editor->parentWidget()) && editor->styleSheet().isEmpty()) {
                // Qt excludes embedded editors from application QSS unless they have a local sheet.
                // Opt in to the shared rule without overriding a consumer's explicit editor theme.
                editor->setStyleSheet(QStringLiteral("/* Resources: style the embedded combo editor. */"));
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
    /** @brief Shares menu leading columns and check marks without changing toolbar icons. */
    static void prepareMenuCheckIndicators(QMenu* menu)
    {
        if (menu->property("resourcesMenuPreparing").toBool()) return;
        menu->setProperty("resourcesMenuPreparing", true);
        bool leadingColumn = false;
        for (QAction* action : menu->actions()) {
            const QVariant original = action->property("resourcesMenuIconVisibility");
            if (action->isCheckable()) {
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
        const int gutter = MenuShadowGutter;
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

    /** @brief Gives the single QSS shell a bounded Resources-owned shadow across native-style polish. */
    static void prepareMenuPopup(QMenu* menu)
    {
        prepareMenuCheckIndicators(menu);
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
        shadow->setBlurRadius(MenuShadowGutter);
        shadow->setOffset(0, 2);
        shadow->setColor(QColor(32, 42, 56, 36));
        menu->clearMask(); // Preserve the stylesheet's fractional edge coverage at every DPR.
        if (menu->geometry() != geometry) menu->setGeometry(geometry);
        if (visible && !menu->isVisible()) menu->show();
    }

    /** @brief Clips feature content inside the shared frame without restoring bottom padding. */
    static void applyFeatureTreeViewportMask(QWidget* viewport)
    {
        const auto* tree = qobject_cast<const QTreeWidget*>(viewport->parentWidget());
        if (!tree || viewport->size().isEmpty()) return;
        // Read the common frame metric from its owning QSS rather than duplicating
        // a presentation constant in a device module or runtime hook.
        static const QSize metrics = [] {
            QFile file(QStringLiteral(":/Resources/theme/qss/00_base.qss"));
            if (!file.open(QIODevice::ReadOnly)) return QSize(0, 0);
            const auto block = QRegularExpression(QStringLiteral("QTreeWidget\\s*\\{([^}]*)\\}"))
                .match(QString::fromUtf8(file.readAll()));
            const QString rule = block.captured(1);
            const auto radius = QRegularExpression(QStringLiteral("border-radius:\\s*(\\d+)px")).match(rule);
            const auto border = QRegularExpression(QStringLiteral("border:\\s*(\\d+)px")).match(rule);
            return QSize(radius.captured(1).toInt(), border.captured(1).toInt());
        }();
        const int radius = metrics.width();
        if (radius <= 0) { viewport->clearMask(); return; }
        // A binary child mask must stay inside the parent's antialiased border.
        // Whole logical pixel squares need two DIPs of arc clearance at fractional
        // scales. Guard only the corner arcs; the straight bottom remains flush.
        const int inset = metrics.height() + 2;
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
        QPixmap icon = QIcon(QStringLiteral(":/Resources/Icons/icons8-update-48.png"))
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

        const QString name = layout->objectName();
        if (name == QLatin1String("ScriptExecutionLimitsFormLayout")) {
            setLayoutMetrics(layout, QMargins(0, 0, 0, 0), 12);
            if (auto* form = qobject_cast<QFormLayout*>(layout)) {
                form->setHorizontalSpacing(24);
                form->setVerticalSpacing(12);
            }
        }
        else if (name == QLatin1String("PluginMarketplaceRootLayout")) {
            setLayoutMetrics(layout, QMargins(8, 8, 8, 8), 6);
        }
        else if (name == QLatin1String("PluginMarketplaceToolbarLayout")
            || name == QLatin1String("PluginMarketplaceActionsLayout")) {
            setLayoutMetrics(layout, QMargins(0, 0, 0, 0), 6);
        }
        else if (name == QLatin1String("PluginMarketplaceListLayout")
                 || name == QLatin1String("PluginMarketplaceShellLayout")) {
            setLayoutMetrics(layout, QMargins(0, 0, 0, 0), 4);
        }
        else if (name == QLatin1String("PluginMarketplaceCardLayout")) {
            setLayoutMetrics(layout, QMargins(6, 5, 6, 5), 6);
        }
        else if (name == QLatin1String("PluginMarketplaceCardTextLayout")) {
            setLayoutMetrics(layout, QMargins(0, 0, 0, 0), 1);
        }
        else if (name == QLatin1String("PluginMarketplaceCardStatusLayout")) {
            setLayoutMetrics(layout, QMargins(0, 0, 0, 0), 4);
        }
        else if (name == QLatin1String("PluginMarketplaceDetailsLayout")) {
            setLayoutMetrics(layout, QMargins(12, 12, 12, 12), 8);
        }
        else if (name == QLatin1String("PluginMarketplaceIdentityLayout")) {
            setLayoutMetrics(layout, QMargins(0, 0, 0, 0), 8);
        }
        else if (name == QLatin1String("PluginMarketplaceTextLayout")) {
            setLayoutMetrics(layout, QMargins(0, 0, 0, 0), 2);
        }
        else if (name == QLatin1String("DeviceRootLayout")) {
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
        else if (name == QLatin1String("DeviceFeatureSearchLayout")) {
            setLayoutMetrics(layout, QMargins(7, 0, 7, 0), 6);
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
        else if (name == QLatin1String("AnalysisPanelRootLayout")) {
            setLayoutMetrics(layout, QMargins(0, 0, 0, 0), 0);
        }
        else if (name == QLatin1String("AnalysisStatisticsTableLayout")) {
            setLayoutMetrics(layout, QMargins(12, 12, 12, 12), 0);
        }
        else if (name == QLatin1String("GraphicsSettingsPanelRootLayout")) {
            setLayoutMetrics(layout, QMargins(0, 0, 0, 0), 0);
        }
        else if (name == QLatin1String("GraphicsSettingsPanelHeaderLayout")) {
            setLayoutMetrics(layout, QMargins(12, 4, 12, 4), 6);
        }
        else if (name == QLatin1String("GraphicsSettingsDisplayPageLayout") ||
                 name == QLatin1String("GraphicsSettingsSurfacePageLayout") ||
                 name == QLatin1String("GraphicsSettingsPointCloudPageLayout") ||
                 name == QLatin1String("GraphicsSettingsRangePageLayout")) {
            setLayoutMetrics(layout, QMargins(12, 12, 12, 12), 12);
        }
        else if (layout->property("graphicsSettingsRole").toString() == QLatin1String("section")) {
            setLayoutMetrics(layout, QMargins(8, 8, 8, 8), 8);
        }
        else if (layout->property("graphicsSettingsRole").toString() == QLatin1String("form")) {
            setLayoutMetrics(layout, QMargins(0, 0, 0, 0), 8);
            if (auto* form = qobject_cast<QFormLayout*>(layout)) {
                form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
                form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
                form->setHorizontalSpacing(8);
                for (int row = 0; row < form->rowCount(); ++row) {
                    auto* item = form->itemAt(row, QFormLayout::LabelRole);
                    auto* label = item ? qobject_cast<QLabel*>(item->widget()) : nullptr;
                    if (!label) continue;
                    // A font-relative shared column keeps independent sections aligned.
                    const int width = label->fontMetrics().horizontalAdvance(
                        QStringLiteral("Minimum (mm)"));
                    label->setFixedWidth(width);
                    label->setWordWrap(true);
                }
            }
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
            setLayoutMetrics(layout, QMargins(0, 0, 0, 0), 8);
        }
        else if (name == QLatin1String("RuntimePathsListButtonsLayout")) {
            setLayoutMetrics(layout, QMargins(0, 0, 0, 0), 8);
        }
        else if (name == QLatin1String("RuntimePathsFormLayout")) {
            setLayoutMetrics(layout, QMargins(0, 0, 0, 0), 12);
        }
        else if (name == QLatin1String("RuntimePathsFormGridLayout")) {
            if (auto* grid = qobject_cast<QGridLayout*>(layout)) {
                setGridLayoutMetrics(grid, QMargins(0, 0, 0, 0), 8, 8);
            } else {
                setLayoutMetrics(layout, QMargins(0, 0, 0, 0), 8);
            }
        }
        else if (name == QLatin1String("RuntimePathsBottomLayout")) {
            setLayoutMetrics(layout, QMargins(0, 0, 0, 0), 12);
        }
        else if (name == QLatin1String("RuntimePathEntriesConfiguredLayout")) {
            setLayoutMetrics(layout, QMargins(0, 0, 0, 0), 8);
        }
        else if (name == QLatin1String("RuntimePathEntriesConfiguredActionsLayout") ||
                 name == QLatin1String("RuntimePathEntriesActionsLayout")) {
            setLayoutMetrics(layout, QMargins(0, 0, 0, 0), 8);
        }
        else if (name == QLatin1String("RuntimePathEntriesAddLayout")) {
            setLayoutMetrics(layout, QMargins(0, 0, 0, 0), 8);
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
        const QColor borderColor(combo ? QColor(0x8b, 0xad, 0xc7) : Resources::borderColor());
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

namespace Resources
{

static ResourceStyleFilter* s_styleFilter = nullptr;
static QString s_styleSheetTemplate;
static QColor s_fontColor(0x3c, 0x3c, 0x3b);
static QColor s_borderColor(0xd9, 0xd9, 0xd8);
static QPointer<QApplication> s_app;

namespace {
void applyInstalledStyleSheet()
{
    if (!s_app) {
        return;
    }

    QString styleSheet = s_styleSheetTemplate;
    styleSheet.replace(QStringLiteral("@FONT_COLOR@"), s_fontColor.name(QColor::HexRgb));
    styleSheet.replace(QStringLiteral("@BORDER_COLOR@"), s_borderColor.name(QColor::HexRgb));
    s_app->setStyleSheet(styleSheet);

    QPalette palette = s_app->palette();
    palette.setColor(QPalette::WindowText, s_fontColor);
    palette.setColor(QPalette::Text, s_fontColor);
    palette.setColor(QPalette::ButtonText, s_fontColor);
    s_app->setPalette(palette);
}
}

QColor fontColor()
{
    return s_fontColor;
}

bool setFontColor(const QColor& color)
{
    if (!color.isValid()) {
        return false;
    }

    s_fontColor = QColor(color.red(), color.green(), color.blue());
    applyInstalledStyleSheet();
    return true;
}

QColor borderColor()
{
    return s_borderColor;
}

bool setBorderColor(const QColor& color)
{
    if (!color.isValid()) {
        return false;
    }

    s_borderColor = QColor(color.red(), color.green(), color.blue());
    applyInstalledStyleSheet();
    return true;
}

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
        QStringLiteral(":/Resources/theme/qss/60_processing.qss"),
        QStringLiteral(":/Resources/theme/qss/70_plugin_manager.qss")
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
    styleSheet.replace(QStringLiteral("@MENU_SHADOW_GUTTER@"), QString::number(MenuShadowGutter));
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
    );
#endif
    s_app = &app;
    s_styleSheetTemplate = styleSheet;
    applyInstalledStyleSheet();

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
    painter.setBrush(borderColor());

    qreal outerRadius = 13.0;
    qreal innerRadius = 12.0;
    getPlatformWindowRadius(outerRadius, innerRadius);

    painter.drawRoundedRect(QRectF(0.0, 0.0, window->width(), window->height()), outerRadius, outerRadius);

    // Fill interior with white (1px inset, slightly smaller radius).
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setBrush(QColor(QStringLiteral("#ffffff")));
    painter.drawRoundedRect(QRectF(1.0, 1.0, window->width() - 2.0, window->height() - 2.0), innerRadius, innerRadius);

    // Draw smooth 1px border.
    painter.setPen(QPen(borderColor(), 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(QRectF(0.5, 0.5, window->width() - 1.0, window->height() - 1.0), innerRadius, innerRadius);
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
        case QtDebugMsg:    typeStr = QStringLiteral("DEBUG"); color = QStringLiteral("#8a8a89"); break;
        case QtInfoMsg:     typeStr = QStringLiteral("INFO");  color = QStringLiteral("#3c3c3b"); break;
        case QtWarningMsg:  typeStr = QStringLiteral("WARN");  color = QStringLiteral("#fb8c00"); break;
        case QtCriticalMsg: typeStr = QStringLiteral("CRIT");  color = QStringLiteral("#e53935"); break;
        case QtFatalMsg:    typeStr = QStringLiteral("FATAL"); color = QStringLiteral("#b71c1c"); break;
        default:            typeStr = QStringLiteral("LOG");   color = QStringLiteral("#8a8a89"); break;
    }

    return QStringLiteral("<span style=\"color:#8a8a89;\">[%1]</span> <span style=\"color:%2; font-weight:bold;\">[%3]</span> %4")
        .arg(timestamp, color, typeStr, message.toHtmlEscaped());
}

}
