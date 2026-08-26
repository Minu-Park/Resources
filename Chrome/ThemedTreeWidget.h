#pragma once

#include <QTreeWidget>

class QPainter;
class QStyleOptionViewItem;

/**
 * @brief QTreeWidget with optional Resources-owned cell or row interaction surfaces.
 * @note The default mode is `InteractionMode::Cell`. Select `InteractionMode::Row`
 *       before the widget is shown when hover and selection should span columns.
 */
class ThemedTreeWidget : public QTreeWidget
{
public:
    /**
     * @brief Selects whether interaction feedback is painted per cell or per row.
     */
    enum class InteractionMode {
        Cell,
        Row
    };

    /**
     * @brief Creates a tree widget that uses the shared interaction-surface modes.
     * @param parent Optional parent widget that owns the tree.
     */
    explicit ThemedTreeWidget(QWidget* parent = nullptr);

    /**
     * @brief Selects the interaction surface and matching selection behavior.
     * @param mode Cell for the default per-cell surface or Row for one surface
     *             spanning the visible columns.
     */
    void setInteractionMode(InteractionMode mode);

protected:
    /**
     * @brief Paints a row-wide rounded interaction surface in row mode.
     * @param painter Painter supplied by the tree view.
     * @param option Row paint options supplied by the tree view.
     * @param index Model index identifying the row being painted.
     */
    void drawRow(QPainter* painter,
                 const QStyleOptionViewItem& option,
                 const QModelIndex& index) const override;
};
