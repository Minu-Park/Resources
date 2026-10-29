# Resources Release Note

## Unreleased

- Add `ThemedFileDialog::getExistingDirectory()` for single-folder selection.

- Select layout patterns by semantic Qt roles and shared layout metrics instead of consumer object names. Read action density from the nearest explicit scope, preserving individual overrides and tab-scroller geometry without consumer class-name admission. Share compact processing actions and status-button patterns.

- Match menu hover to item-view hover with the shared `itemHover` palette role, 12px capsule corners and enough vertical padding for the rounded row body.

- Remove redundant shorthand/edge assignments and share matching chrome, status-bubble, processing-header and indicator selector groups while preserving their state scopes. Resolve plot action size and radius from the same compact-action metrics as other consumers.

- Reject unmatched theme braces and text outside complete rule blocks before fragment ownership checks, atomic reload or platform composition; mirror structural rejection in the source policy guard.

- Validate source and platform theme layers at installation and reload, propagate composition errors without replacing output, and explicitly require extension-menu coverage in the overflow acceptance fixture.

- Keep an open combo's connecting edge white over hover, focus and pressed borders; verify the popup and control share the active focus palette color, including live palette changes. Make UI acceptance use native horizontal style options, matching high-DPI rasters, menu-overflow access and completed desktop composition.

- Reject unparsed QSS declarations before installation or atomic reload, exercise platform patch composition independently of the operating system, preserve device tab-scroll buttons' borderless 20px slots, and apply shared metrics to the engine toolbar. Remove unused toolbar-density and borderless-role APIs.

- Centralize palette, widget metrics, source reload and application installation in `ResourcesTheme`; keep the existing namespace API as a stateless host facade and runtime painters as private adapters.
- Reject repeated selector/property definitions and late type-wide control templates during source reload; resolve explicit platform variants before installing QSS. Remove repeated base, overlay-scrollbar, status-button and search-button assignments without changing their final values.
- Share spin-box geometry and compact action densities through one theme definition. Reload QSS and widget metrics together, reject incomplete/invalid candidates atomically, and update tree corner masks from the active metrics instead of cached QSS parsing.

- Fill the Libraries list panel and its action strip with the white surface instead of hover gray; the top divider alone separates the actions.

- Add the `itemHover` palette role (`#f6f9fc` by default) for a light-blue tree, list and table hover that stays lighter than the selection. Saved `theme.json` files without this role are incomplete and restore the built-in palette.

- Hover and select device feature-tree rows as one row with the shared blue selection, without a separate current-cell capsule.

- Preserve toggle hover artwork in shared row-mode tree/table delegates while keeping row backgrounds independently painted.

- Add neutral vertical `plug-connected` and `plug-disconnected` glyphs in 24px and 48px sizes for attach/detach commands.

- Add blue hover feedback to outline toggles: light-blue Off outline/thumb on a pale-blue track and tinted On/Mixed, preserving disabled artwork, geometry and native interaction.

- Keep the search bubble height stable under plain Qt styling when typing reveals the clear button.

- Restore GraphicsEngine mode/view status-bubble outlines: neutral at rest or disabled, light blue on hover/focus, and accent while pressed, checked or open.

- Align the default-palette regression with the distinct hover/focus and active border colors.

- Center search clear/favorite controls in matching 20px slots with equal horizontal insets; use the shared active blue for selected favorites.

- Add an opt-in search-bubble border-state proposal while keeping the inner editor borderless and current product defaults unchanged.

- Apply shared hover/focus/held input borders inside device trees, preserving neutral row surfaces, disabled styling and parameter behavior.

- Apply the shared 4px capsule track to horizontal sliders, keeping white handles, distinct hover/drag outlines and a neutral disabled track.

- Apply compact 4x2 symmetric vector chevrons to all combo, spin and date/time inputs: neutral at rest, light blue on hover, dark blue for open/pressed states and light gray when unavailable, without changing hit areas.

- Add an opt-in state-only radio proposal with a thin outline, blue selected dot and distinct disabled selection; keep default radio artwork unchanged.

- Apply outline toggles by default to checkboxes, item-view checks and checkable group boxes, preserving keyboard focus, hit areas and native behavior. Remove the toggle opt-in path and superseded switch artwork.

- Keep System Logs and icon-action backgrounds neutral without darker pressed or checked fills.

- Highlight the selected plugin's detail title with the shared accent color (#15487f by default).

- Keep enabled button backgrounds unchanged across hover, focus, pressed and checked states.

- Give tool buttons and icon-bearing push buttons a rounded state outline, including after runtime icon changes: none at rest or disabled, light blue on hover/focus and dark blue while pressed or checked. A reserved transparent 1px border keeps toolbar, compact 20px and marketplace action sizes unchanged. Window, dialog, dock, panel and tab close glyphs, tab scroll arrows, calendar navigation, search clear, favorites, console Clear and minimized-session controls stay borderless. Text-only push buttons retain light-blue hover/focus and dark-blue pressed/checked borders.

- Restore the splash title accent color (#15487f by default).

- Expand main title-bar menu items vertically and keep enabled menu targets ahead of frame-resize hit testing so the space above and below each label remains clickable.

- Apply light-blue hover/focus borders and accent pressed/checked/open borders consistently to input frames and interactive status bubbles; passive labels and disabled controls retain neutral borders.

- Fix MDI, dock/widget and dialog title bars to one shared 11px scale, including nested dialog contexts.
- Remove conflicting library-list typography rules; dialog list and field values use normal weight.

- Restore original non-dialog widget title sizes and neutral wording; apply one consistent typography scale inside dialogs only.
- Use #15487f for selected-tab text and bottom underlines. Preserve original selection capsule rendering with nearly white blue row/current-cell fills and original icon artwork.
- Separate structural/widget borders from interactive item borders and correct histogram summary text specificity.

- Apply shared popup title and caption roles to the production histogram bin editor.

- Apply shared surface, border, secondary and warning colors to analysis plots and normal body typography to measurement tables.

- Use one complete loading-bar renderer for standard progress controls: matching track, blue gradient, rounded fill, outline and light sweep, including busy package operations.
- Normalize remaining panel, readout and notification spacing to the shared layout scale.
- Center message-dialog icons against both short and wrapped message blocks.

- Define shared font families, logical-pixel sizes, weights and layout metrics in one embedded presentation token file; resolve tokens for installation and source-QSS reload.
- Standardize semantic text roles, dialog/dock/section titles, normal-weight controls and library fields, and secondary metadata colors.

- Add compact list-bottom action bars and concise path hints for library configuration dialogs.
- Remove retired library-dialog layout metric rules.

- Style the staged Core update notification's Update and Later actions, and keep that notice from adopting the clickable-body hover.

- Add a neutral, borderless close glyph in 24px and 48px sizes for compact loading surfaces.

- Apply consistent settings-page spacing to General, Color and Diagnostics categories.

- Publish the supplied debug glyph in 24px and 48px sizes.
- Avoid invalid point-size conversion when Windows 11 measures dropdown indicators for QSS pixel fonts.
- Show display-mode menu glyphs and current-row emphasis without additional radio indicators.
- Use the shared tool-button styling for script build and load actions instead of dedicated outlined controls.
- Add atomic complete-theme JSON persistence, including font colors; missing or invalid files restore every built-in color.
- Skip global theme repolish when an accepted palette patch leaves every color unchanged.
- Use the border role for table grid lines independently of hover backgrounds.
- Restore original blue splash/loading title, version, status and percentage text.
- Center horizontal sliders in 24px input rows.
- Consolidate shared artwork under icon/, publish the supplied plot and script action glyphs, and use the supplied empty/selected radio artwork.

## v1.3.0 (2026-10-20)

- Add neutral GenICam, Heliotis and LMI icon variants in 24px and 48px sizes.

- Restore the original blue gradient on splash and loading progress surfaces.

- Publish shared connect and disconnect artwork in both supplied sizes.

- Unify shared selection, focus, controls and package surfaces on the neutral ink ramp while preserving diagnostic state colors.
- Expose factory/current palette queries, atomic role patches, full reset and complete source-QSS reload through the public API; share colors with QSS and native painters.
- Remove leading menu/combo markers and compact item content insets and switch/text spacing.
- Keep composite fields inside one border and align text across line, spin, combo and date/time editors; compact Windows 11 combo rows with matching hover/selected corners.
- Render full-canvas neutral 36x20 vector switches with 14px thumbs and consistent disabled geometry; reserve 8px between checkable group titles and their frame.
- Use clipped capsule progress fills; splash/loading retain their blue gradient and update surfaces follow public progress colors.
- Inset ordinary tab pages and draw a continuous header divider; dedicated flat panes retain their own spacing.
- Use supplied folder/file-view PNG artwork and the existing neutral warning triangle and radio assets.

- Use `#3c3c3b` as the default font color and let `setFontColor()` replace the shared text ink.
- Use neutral `#d9d9d8` as the default chrome border so it matches the `#3c3c3b` icon ink, and let `setBorderColor()` replace that border.
- Recolor toolbar dividers, source tabs, status bubbles, and System Logs onto the same neutral ink. Warning and error colors stay.
- Publish the menu, trash, and chevron glyphs used by host actions and shared controls.
- Publish play, pause, plus, minus, AI, camera, update, and certificate glyphs for the matching host actions.
- Publish image-view, toggles, refresh, download, network, about, question, error, cameras, stop, and check glyphs for the matching controls.

- Publish the trial toolbar glyphs under `icon/` for GraphicsEngine actions that have a matching symbol.
- Use outline window glyphs for close, minimize, and maximize, and the rounded glyph while those buttons are hovered.

- Draw `frameRole="none"` text browsers without a field, hover, or focus border.

- Preserve keyboard focus on unselected current cells and expose joined-only selection appearance independently of row/cell behavior.

- Fill the current selected cell with a stronger capsule inside joined selections, align its outer edges, and share balanced selection colors and item text/switch spacing across trees, lists and tables.
- Clip joined selection backgrounds to their owning cells so later neighbors do not overwrite the current cell's right and bottom curves.

- Present marketplace refresh as a borderless icon action and align compact execution-limit forms.

- Clear row hover when another window or popup covers the viewport, and preserve consumer mouse-tracking settings.

- Preserve full-row table hover and commit editors exactly once when consumer delegates call through the shared surface delegate.

- Join adjacent tree, list, and table selections into one rounded surface; retain consumer delegates, editors, and dedicated popup behavior.
- Use compact shared item insets, aligned radio/switch geometry, consistent progress colors and readable fill-overlaid text, and styled date/calendar controls.

- Restore the shared menu shadow, flags, and mask immediately after live menu-column repolish.

- Use inset popup dots, lightweight menu check marks, shared mixed-menu label columns, and rounded directional submenu chevrons. Keep widget switches and exclusive menu radios.

- Align embedded editable combo text with read-only controls and keep text-only menus free of unused leading columns.

- Preserve diagnostics dividers during individual-cell repaints, share one menu shadow metric with QSS, correct flipped popup shell anchors, and keep exclusive menu choices as radio indicators.

- Keep exclusive menu radio indicators circular by clearing inherited switch padding; verify rendered diameter and corner pixels across display scales.

- Align mixed-menu labels across ordinary commands, checks, radios, and submenus while preserving compact text-only menus.

- Align presentation diagnostics header/body dividers and draw only internal grid lines, preserving the shared bubble header and rounded table shell.

- Give default list items 3px vertical padding so switch indicators remain separated between rows, while retaining dedicated combo and file-list spacing.

- Unify standalone, item-view, and group-box check indicators as switches, including mixed/disabled states and focus treatment, while preserving native input, state, and selection semantics.

- Use a small left-side menu hover dot with dedicated icon spacing, and keep menu-bar hover and open states free of fills, outlines, and underlines.

- Add a subtle menu-only shadow inside a transparent stylesheet gutter, retaining smooth shell corners and submenu action alignment; keep native layered-window updates within that padded extent.

- Render menus through one antialiased QSS shell without binary corner masks or native-style widget shadows, preventing dark corner wedges and preserving smooth curves at fractional display scales.

- Apply common graphics settings group-box insets, spacing and font-relative label columns across all sections, including narrow panels.

- Export reusable dialog support without theme assets, preserving the existing platform window behavior.

- Export Resources::Widgets as an installed Qt-only package; allow consumers to omit theme assets and window chrome.

- Add an opt-in `controlRole="switch"` presentation for native checkboxes, with
  scalable on/off indicators and disabled states.

- Preserve the latest scroll request during deferred search-surface layout so scrolling to the final row is not undone.

- Add SearchableTreeController to attach shared search/favorites to existing QTreeWidget instances, preserving consumer delegates, editor signals, metadata and intrinsic row visibility; keep SearchableTreeWidget as its inheritance adapter.

- Add the Qt-only Resources::Widgets target and reusable SearchableTreeWidget with stable-key favorites, local name/category filtering, editor-preserving state restoration, and a frame-anchored search overlay independent of scrollbar visibility.

- Overlap combo popup connecting borders by two logical pixels in both opening directions, covering the border and one raster guard row so native rounding cannot expose a gap.

- Clip feature-tree content inside the shared lower corner arcs so rows and search shadows cannot erase the antialiased frame, without adding a bottom padding band.

- Remove bottom padding from tree views while retaining header spacing. Remove vertical padding and duplicate document margins from themed editor/log surfaces, preserving horizontal text gutters.

- Map checked, unchecked, and partial tree item states to indicator flags; theme row-mode checkboxes with outlined borders and existing check/minus assets so every state remains visible and distinguishable.

- Render combo popup corners with concentric antialiased alpha fills instead of binary underpainting and native masks; remove platform-installed widget shadows that contaminate the shell outline. Validate both outer arcs and native desktop composition across popup contexts and display scales.

- Paint only branch glyphs in row mode so native branch panels cannot cover the rounded hover surface or its corners.

- Provide a reusable bold section-title label role for borderless sections.
- Add muted `description` and `caption` label text roles for introductory and status text.

- Restore themed expand/collapse indicators when tree rows use direct cell painting.
- Stop row-mode tree branch areas from painting a square hover/selection fill over the rounded row surface.
- Flatten runtime-path layout metrics and retire duplicated list styling in favor of the shared tree template.

- Indicate log warnings and errors through orange and red status-button borders only, preserving the normal surface and text styling.

- Keep the feature-search favorites button background and border transparent in all interaction states; selection fills only its star.

- Add a centered, padded vector X and compact hover/pressed states for the feature-search clear control.

- Remove dock scroll-area background overrides that covered combo popup shells. Prepare every opening, including existing/replaced views, and retain masks after resize; cover device selectors and feature editors.

- Use neutral update indicators and outlined primary/secondary plugin actions with subdued hover, focus, and disabled states.

- Join combo popups with one border, direction-aware corners, and restored closed geometry; preserve this behavior for compact controls.

- Match the search surface to the feature-header white background, border and 9px corners; retain its 24px content height and compact controls, with a subtle theme-controlled shadow and no surrounding gradient/blur.

- Style compact plugin list/detail panels, version controls, install actions, and progress; centralize tighter outer/card/detail spacing and smaller fonts, icons, and controls.

- Let feature-tree Value columns shrink with their viewport instead of preserving an old width and forcing horizontal scrollbars.

- Register enabled UI tests when built as a subdirectory as well as standalone.

- Register OneGen controls with shared compact device styling, reuse flat device tab panes, and inset feature-tree content above rounded bottom corners.

- Hide non-modal always-on-top loading windows while the application is inactive, restoring only requested windows on return; dismiss open menus on application deactivation.

- Use Inter for visualization statusbar controls and summaries; retain JetBrains Mono for pointer coordinates/color values, zoom percentages, and FPS.

- Remove status-dependent font colors while preserving existing statusbar backgrounds, borders, geometry, and font weights.

- Registered the GenICam icon in the shared Qt resource catalog.
- Return the active file-type filter from themed Save dialogs for format-aware consumers.
- Renamed the static image/3D source control QSS class selectors to `StaticImageControlWidget` and `Static3DDataControlWidget`.
- Renamed the code editor gutter selector to `CodeEditorLineNumberArea` so the editor widget keeps its own object name.
- Updated row-mode tree painting to use the current Qt per-index delegate lookup API.


- Added the rounded host update-discovery notification shell and its dismiss-button styling.
- Made update-notification text labels transparent so the rounded surface has no rectangular white patches.
- Tightened Runtime Paths form label spacing and kept its status message at the bottom of the properties panel.
- Registered the trash icon in the shared Qt resource catalog.
- Registered the downloading-updates icon and assigned it to plugin update actions.
- Added compact plugin-manager styles for a borderless removal action and inline download progress.
- Added QSS-provided download and update actions, status labels, and a rounded inline progress track for plugin operations.
- Added QSS selectors for two-line installed/available version labels, installation-state status colors, and drag-reorder feedback.
- Removed visible plugin-order arrow actions; installed rows now provide the ordering surface through tree drag-and-drop.
- Captured the rendered plugin row for drag feedback, used row-midpoint insertion, and removed the native full-row drop rectangle.
- Placed the installation action at the leading edge only when present, kept update immediately after the name, and kept removal at the far trailing edge without reserving an empty leading slot.
- Made plugin names bold and mapped Installed, Update available, and Not installed to normal, green, and blue status colors respectively.
- Matched the Name label's first-character inset to the official header section padding.
- Kept the installed and available version lines on equal styled heights for balanced row spacing.
- Derived plugin version line heights from the active font metrics so weighted version text stays vertically centered.
- Removed vertical row-mode item margins while retaining a horizontal content inset so embedded version lines remain compact and unclipped.
- Painted installed and available versions in one transparent cell with shared baseline slots and style-derived top/bottom breathing room so item-view geometry cannot clip either line.
- Made plugin status surfaces transparent and aligned icon-action hit areas with the status row height and cell frame inset.
- Inset the inline plugin download track so it follows the rounded status button corners.
- Rounded and inset the default table/tree item hover and selection surfaces.
- Reserved an explicit plugin row height so rounded action controls are not clipped by item-widget bounds.
- Added an opt-in row-mode tree widget that paints one header-aligned rounded interaction surface across all visible columns while preserving the default cell mode and specialized device-feature tree presentation.
- Aligned row-mode text insets and horizontal bounds with the shared header geometry while retaining a small vertical hover gap.
- Added an optional rounded trailing cap to the custom progress bar so download progress ends with a smooth `)` edge instead of a flat cutoff.
- Added opt-in row-mode QTableWidget painting so Measure-style tables use one continuous rounded surface across the full single-line row.
- Kept row-mode table text readable while the shared hover surface is active.
- Fixed row-mode hover and selection surfaces from covering text in preceding cells while preserving the continuous row shape.

## v1.2.1

- Fixed popup corner clipping by aligning the binary mask with the full painted widget bounds.
- Removed obsolete Heliotis feature-tab selectors now that its controls use one unified feature tree.
- Added a semantic native-tile status rule that changes only the GraphicsEngine summary bubble border to blue after a completed native tile is displayed.
- Replaced save-file overwrite confirmation with the themed message box, added themed Yes/No question buttons, and kept the embedded file list visible while confirming.
- Guarded MDI minimum-size updates against transient invalid viewport sizes.
- Added regression coverage for MDI minimum-size validity across minimize and restore transitions.

## v1.2.0

- Added the light translucent rounded presentation for the profiling Y-axis range editor popup.
- Kept the Y-axis range editor frame background and border transparent so its custom-painted rounded shell remains the single visible surface.
- Registered the cursor and text-cursor icons for profiling mode controls.
- Added a shared compact plot-action role for 20px profiling mode, clear, zoom, and fit controls, including the checked-mode state.
- Added a subtle per-button hover state for the shared plot-action controls, with a stronger hover treatment for the selected mode.
- Added the `AnalysisStatisticsTableLayout` theme role with a 12px content inset for table panels.
- Replaced corrupted consumer-specific documentation with a standalone theme and integration contract.
- Unified Graphics Settings Display section and form layout metrics with the Surface and Point Cloud rendering tabs by removing nested default margins.
- Applied the shared static-source control presentation to 3D Data panels, including compact action buttons and themed file lists.
- Registered the front-view icon in the shared resource catalog for 3D Data sessions.
- Fixed manual MDI edge and corner resizing so a pointer that overshoots the dynamic minimum boundary returns to the clamped frame before growth resumes.
- Added opt-in cached MDI shadows for every eligible subwindow, with independent stacking, aggregate profiling, a lighter compact shadow, and a consistent 12px frame/title/content silhouette. The binary bottom content mask now keeps a one-DIP corner guard so the styled frame retains its antialiased edge during resize.

## v1.1.0

- Added a script-oriented viewer icon to the shared Qt resource catalog.
