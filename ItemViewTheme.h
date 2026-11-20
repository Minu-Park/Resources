#pragma once
#include <Qt>

class QAbstractItemView;
namespace Resources
{
/** @brief Set on an index to omit shared hover and selection fills while retaining ink and interaction. */
inline constexpr int SuppressItemInteractionFillRole = Qt::UserRole + 512;

/** @brief Set on an index to render its identity with the theme's accented emphasis ink.
 *
 * The owning view supplies the fact (a target is connected); Resources owns how that fact is
 * drawn, so the emphasis weight and accent color come from the installed theme rather than a
 * widget-level `QFont`. Consumers never build fonts or colors for this state.
 */
inline constexpr int EmphasizedItemInkRole = Qt::UserRole + 513;

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
