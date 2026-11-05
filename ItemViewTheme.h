#pragma once
class QAbstractItemView;
namespace Resources
{
/** @brief Controls emphasis independently of row/cell selection behavior.
 * MenuInk uses no fill: neutral semibold hover and accented semibold selection.
 * Row selection shares emphasis across all cells; cell selection emphasizes only
 * first-column identities. QMenu keeps its own accented hover. Value columns
 * retain source-delegate painting and editing. Index widgets expose title/icon labels through
 * itemRole, and icon labels retain itemIcon (QIcon) and itemIconSize (QSize)
 * properties. Resources derives their interaction state.
 */
enum class ItemSelectionAppearance { JoinedOnly, CurrentCell, MenuInk };
/** @brief Chooses joined fills or fill-free menu ink; selection and editor behavior stay unchanged. */
void setItemSelectionAppearance(QAbstractItemView *view, ItemSelectionAppearance appearance);
/** @brief Installs shared selection painting while retaining the view's delegate and editors. */
void applyItemViewTheme(QAbstractItemView *view);
} // namespace Resources
