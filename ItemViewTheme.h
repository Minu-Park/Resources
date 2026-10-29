#pragma once
class QAbstractItemView;
namespace Resources
{
/** @brief Controls emphasis independently of row/cell selection behavior.
 * MenuInk paints first-column identities like QMenu: no fill and accented semibold
 * hover/selection. Other columns retain source-delegate painting. Index widgets
 * expose title/icon labels through itemRole, and icon labels retain itemIcon (QIcon)
 * and itemIconSize (QSize) properties. Resources derives their interaction state.
 */
enum class ItemSelectionAppearance { JoinedOnly, CurrentCell, MenuInk };
/** @brief Chooses joined fills or fill-free menu ink; selection and editor behavior stay unchanged. */
void setItemSelectionAppearance(QAbstractItemView *view, ItemSelectionAppearance appearance);
/** @brief Installs shared selection painting while retaining the view's delegate and editors. */
void applyItemViewTheme(QAbstractItemView *view);
} // namespace Resources
