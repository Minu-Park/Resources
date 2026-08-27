# Resources Release Note

## Unreleased

- Added the shared in-plot Readout surface styling for profiling values and comparison summaries.

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
