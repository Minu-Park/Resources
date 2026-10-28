# Theme Layout

`Resources::installResources()` delegates to `ResourcesTheme`, the sole theme owner.
The owner holds the palette, widget metrics, source template and platform additions,
and is adopted by the host QApplication at installation. Palette edits made before
installation are retained. Repeated installation reapplies the active snapshot
without adding fonts, event filters or delegates a second time.

## Definitions and application

- `presentation.json` owns the established typography and layout scale.
- `metrics.json` owns widget geometry shared by QSS and runtime adapters. Names must
  match the built-in contract; values must be positive integer logical pixels up to 256.
- `qss/00_base.qss` owns standard control templates, including spin frames, embedded
  editors and arrow states. Reusable `resourcesDensity` selectors own compact fields,
  and compact actions. The engine toolbar consumes shared action-size and padding
  tokens in its named QSS rule. Later fragments may add named exceptions;
  they must not introduce another type-wide standard-control template. The manager
  rejects duplicate selector/property assignments across the whole candidate,
  including reordered equivalent pseudo-states, and unparsed declaration text.
  Source and platform layers are each validated during installation and reload;
  a platform layer may intentionally override a source definition. Composition
  reports parse errors without replacing its output. Invalid embedded layers
  stop installation, while invalid reload candidates preserve the active snapshot.
  Valid platform variants resolve to one final assignment before QSS is installed.
- `ResourcesTheme::metrics()` resolves field, tool-button and tree-frame geometry
  from the same values substituted into the installed stylesheet.
- `ThemeRuntime.cpp` contains private event, delegate and native-style adapters.
  Runtime geometry reads the owner rather than parsing or caching QSS text.
- Existing Resources namespace functions are the host-facing facade. They contain
  no palette, metric, stylesheet or installation state of their own.

`ResourcesTheme::setDensity()` selects default or compact density for a
line edit, spin box, combo box or tool button and refreshes the widget immediately. Native
input behavior, signals and accessibility remain with Qt. Existing device and
static-source tool buttons are admitted to compact density centrally; an explicit
density selection takes precedence, including after reparenting. Current narrow
Qt tab-scroll descendants are excluded from automatic compact admission. Device
tab-scroll buttons reserve borderless 20x20 slots without control padding/margins;
chrome exceptions retain their own geometry.

## Source reload

`reloadThemeFromDirectory(qssDirectory)` reads every installed QSS fragment and
`qssDirectory/../metrics.json` into a candidate. It validates supported metric names,
integer bounds, template ownership, duplicate assignments, unparsed declaration text
and token references before replacing either active source or
metrics. Failed candidates leave installed QSS, metrics and palette unchanged.
Successful reload preserves user colors and platform additions. Presentation
font/layout tokens remain the compiled contract. QSS authoring still requires Qt
rendering checks; token validation is not a complete QSS syntax validator.

Qt QSS has no import mechanism, so fragment order in `ResourcesTheme::install()`
is part of the theme contract. The design catalog watches both QSS and metrics.

- Put generic widget defaults in the lowest-numbered files.
- Put semantic role and state selectors after their defaults.
- Use dynamic properties for reusable states and density.
- Keep icons under stable `:/Resources/...` virtual paths.
- The consuming application installs the theme; reusable libraries do not.

Current consumer-specific object names identify narrow presentation exceptions.
Replace repeated exceptions with shared roles as the corresponding surfaces are
updated; do not copy a base template into another fragment.
