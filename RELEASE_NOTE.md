# Resources Release Note

## Unreleased

- Fixed manual MDI edge and corner resizing so a pointer that overshoots the dynamic minimum boundary returns to the clamped frame before growth resumes.
- Added opt-in cached MDI shadows for every eligible subwindow, with independent stacking, aggregate profiling, a lighter compact shadow, and a consistent 12px frame/title/content silhouette. The binary bottom content mask now keeps a one-DIP corner guard so the styled frame retains its antialiased edge during resize.

## v1.1.0

- Added a script-oriented viewer icon to the shared Qt resource catalog.
