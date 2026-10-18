#pragma once
class QAbstractItemView;
namespace Resources
{
/** @brief Controls emphasis independently of row/cell selection behavior. */
enum class ItemSelectionAppearance { JoinedOnly, CurrentCell };
/** @brief Chooses joined selection alone or an additional current-cell fill (the default). */
void setItemSelectionAppearance(QAbstractItemView *view, ItemSelectionAppearance appearance);
/** @brief Installs shared selection painting while retaining the view's delegate and editors. */
void applyItemViewTheme(QAbstractItemView *view);
} // namespace Resources
