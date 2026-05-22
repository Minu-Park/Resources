# Resources

Shared Qt resource bundle for Playground-family modules.

## Ownership
- `Resources.qrc` owns the `:/Resources` prefix.
- `Style.qss` owns application QSS, including `GraphicsEngine`, `QCameraWidget`, and `QGocatorWidget` selectors.
- Icons and brand images live here, not in device or rendering modules.

## Integration
- Host apps link `Resources::Resources`.
- Host apps apply `:/Resources/Style.qss`.
- Module code may reference icons through `:/Resources/...` paths.

## Boundary
- Do not move device runtime logic here.
- Do not move rendering logic here.
- Style and asset changes should be solved in this repository first.
