# Resources Release Note

## Unreleased

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
