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
  tokens in its named QSS rule. Plot actions share the compact-action template
  and its size/radius tokens. Matching selector families share one declaration
  block; unrelated roles retain separate rules even
  when factory values happen to coincide, preserving state precedence and ownership.
  Later fragments must not introduce another type-wide standard-control template. The manager
  rejects duplicate selector/property assignments across the whole candidate,
  including reordered equivalent pseudo-states, and unparsed declaration or block
  text. Unbalanced braces are rejected in each fragment before ownership checks.
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
input behavior, signals and accessibility remain with Qt. Consumers declare
`actionDensity="compact"` or `"default"` on an action scope using Qt properties
alone. The nearest scope controls descendant tool buttons; an explicit
`resourcesDensity` selection takes precedence after reparenting or a live scope
change. Consumer class names are not recognized. Current narrow
Qt tab-scroll descendants are excluded from automatic compact admission. Device
tab-scroll buttons reserve borderless 20x20 slots without control padding/margins;
chrome exceptions retain their own geometry.

## Semantic patterns

Consumers expose intent through Qt dynamic properties without linking Resources.
Object names remain useful for diagnostics and composition but do not select layout
metrics. `presentationLayout` retains its existing general page/form contract and
takes precedence when present. Otherwise `layoutRole` selects one shared pattern:

| Roles | Purpose |
| --- | --- |
| `flush`, `actions`, `row`, `stack`, `sections` | Zero-inset action/row/stack composition with the corresponding spacing. |
| `caption-stack`, `text-stack` | Dense text composition; spacing comes from layout metrics. |
| `content`, `page`, `compact-panel`, `section`, `inset` | Shared inset panel and page compositions. |
| `card`, `header`, `tab-strip` | Compact chrome compositions with retained factory insets. |
| `bottom-panel`, `tabbed-panel`, `search` | Asymmetric content and search compositions. |
| `form`, `wide-form`, `label-form`, `field-grid` | Form/grid spacing and label-column behavior. |

Common spacing reads presentation tokens. Compact spacing/insets, caption/text
spacing, card vertical inset and search inset read `LAYOUT_*` metrics from the same
atomic source snapshot as QSS. Live role changes and source reload update existing
layouts. Unknown roles retain Qt/consumer layout behavior.

`controlRole="compactAction"` shares the compact-action template and metrics.
`controlRole="devicePanel"` selects the shared device surface and borderless
20px tab-scroll slots; the separate `actionDensity` scope selects its action sizes.
`controlRole="statusBubble"` uses `statusBubbleVariant="menu"`, `"action"` or
`"alert"` for existing menu/text/alert states. `logState` and `performancePath`
retain their diagnostic meaning; disabled, hover/focus and active border roles
remain distinct. A pressed or open menu bubble also draws its label in `selectionAccent`. A shared pattern does not erase icon gutters, close hit targets,
native popup corrections or content-owned rendering.

Before adding a role, record its meaning, supported widget/layout kinds, states,
metric inputs and owning implementation here. Validate live role changes,
explicit/nested density scopes, reparenting and metric reload where applicable,
and render the affected production and fixture surfaces. Unknown roles preserve
Qt/consumer behavior but still require review for misspellings or an unsupported
contract. The current source policy report identifies structural design candidates;
it does not yet validate literal `layoutRole`, `controlRole` or
`statusBubbleVariant` values. Review those values against this contract rather
than treating a clean policy result as role-value validation.

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

Menu hover paints no fill: the row text uses `selectionAccent`, and the theme
runtime's style proxy recolors Qt's generated Active-mode pixmap of neutral action
glyphs with the same role. Action icons are not replaced. Palette changes clear
`QPixmapCache` because Qt caches generated pixmaps without the palette in the key.
The hovered label also uses `WEIGHT_EMPHASIS`; rest labels stay normal weight. Before each
popup the runtime reserves the emphasized label width through the menu's minimum width,
keeping any consumer minimum, so hover neither clips nor resizes a row. Checked rows paint
no fill; check and radio glyphs, or the invoking control, show the current value.
The shared menu template keeps 4px vertical padding for the existing row height.

- Put generic widget defaults in the lowest-numbered files.
- Put semantic role and state selectors after their defaults.
- Use dynamic properties for reusable states and density.
- Keep icons under stable `:/Resources/...` virtual paths.
- Ship one neutral (`#3c3c3b`) artwork per glyph. The theme runtime wraps tool- and
  push-button icons and recolors neutral pixels with `hoverBorder` (hover/focus) or
  `focusBorder` (pressed/checked/open), because Qt selects no icon mode for a press.
  Non-neutral artwork, disabled rendering and the rest state pass through unchanged.
  Icon-only neutral buttons get `resourcesGlyphButton="true"`, and `00_base.qss` drops
  their state outline; selectors with their own state pattern still take precedence.
- The consuming application installs the theme; reusable libraries do not.

Current consumer-specific object names identify narrow presentation exceptions.
Replace repeated exceptions with shared roles as the corresponding surfaces are
updated; do not copy a base template into another fragment.
