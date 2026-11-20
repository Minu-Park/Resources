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
| `card`, `header`, `console-header`, `tab-strip` | Compact chrome compositions with retained factory insets. |
| `bottom-panel`, `tabbed-panel`, `search` | Asymmetric content and search compositions. |
| `form`, `wide-form`, `label-form`, `field-grid`, `compact-form` | Form/grid spacing and label-column behavior. |

Common spacing reads presentation tokens `SPACE_TIGHT`, `SPACE_ROW`, `SPACE_CONTENT`
and `SPACE_SECTION`. The other sources are per role. `content`, `page` and `inset`
inset by `SPACE_CONTENT`; `compact-panel` and `section` inset by `SPACE_ROW`; `card`
insets horizontally by `LAYOUT_COMPACT_INSET` and vertically by `LAYOUT_CARD_INSET_Y`;
`header` insets horizontally by `SPACE_CONTENT` and vertically by `SPACE_TIGHT`;
`tab-strip` insets left by `SPACE_CONTENT`, right by `LAYOUT_COMPACT_INSET` and
top/bottom by `LAYOUT_CARD_INSET_Y`; `console-header` insets left/right by `SPACE_ROW`
and top/bottom by zero; `bottom-panel` insets by `SPACE_CONTENT` with a zero top,
`tabbed-panel` by `SPACE_CONTENT` with a `SPACE_ROW` top, and `search` horizontally
by `LAYOUT_SEARCH_INSET_X`. `actions`, `compact-panel`, `card`, `header`, `search`
and `compact-form` space by `LAYOUT_COMPACT_SPACING`; `stack`, `tab-strip` and
`console-header` space by `SPACE_TIGHT`; `caption-stack` and `text-stack` space by
`LAYOUT_CAPTION_SPACING` and `LAYOUT_TEXT_SPACING`; `flush` and `inset` space by zero.
On `QFormLayout`, `compact-form` sets horizontal spacing to `SPACE_ROW` and vertical
spacing to `LAYOUT_COMPACT_SPACING` without the `label-form` fixed label width or word
wrap; on other layouts its spacing is `LAYOUT_COMPACT_SPACING`. These `LAYOUT_*`
metrics come from the same atomic source snapshot as QSS. Live role changes and
source reload update existing layouts. Unknown roles retain Qt/consumer layout
behavior.

`controlRole="compactAction"` shares the compact-action template and metrics.
`controlRole="devicePanel"` selects the shared device surface and borderless
20px tab-scroll slots; the separate `actionDensity` scope selects its action sizes.
`controlRole="statusBubble"` marks every clickable status bubble, and all of them
share one state rule: pointer hover draws border and label in `hoverBorder`; pressed,
checked or open draws the border in `focusBorder` and the label in `selectionAccent`;
active interaction takes precedence over hover and keyboard focus. Buttons use
`statusBubbleVariant="menu"`, `"action"` or `"alert"` only for rest presentation
(menu indicator, `statusBorder`, `logState`). A plain `QWidget` with the role is a
composite bubble: Resources sets transient `interactionHovered` while the pointer is
anywhere inside it and `interactionPressed` while a press inside it is held, and its
text buttons follow the bubble state. `logState` and `performancePath` retain their
diagnostic meaning at rest. A shared pattern does not erase icon gutters, close hit
targets, native popup corrections or content-owned rendering.

`statusRole="deviceCounter"` on a `QLabel` selects the shared numeric telemetry
bubble in the device-panel counter row or a statusbar: surface capsule,
`controlBorder` outline, primary text and JetBrains Mono digits. The role owns the
capsule geometry, so the panel row does not depend on the statusbar height or
border rules. Use it for live counters such as fps, delivered frames and drops.
`failedState="warning"` switches the outline and text to the warning roles for a
nonzero failed counter. New panels use `deviceCounter` for every counter; the
former `statusRole="deviceFps"` and `statusRole="deviceFailed"` roles and the
per-plugin status-label selectors were removed with the plugin widget libraries.

`stateRole="deviceState"` on a panel-header `QLabel` selects the compact state
token (`Idle`, `Connected` or `Live`): primary text and widget-bold weight without
a bubble or border. The token is presentation-only; the state value and its
transitions remain consumer-owned.

`tabRole="DeviceScopeCard"` on `Resources::DeviceScopeCard` selects the in-card
scope composition: one `QFrame` card, with the tree-frame border and radius, around
a `QTabBar`, a full-width 1px `border` separator and a transparent page stack.
`DeviceFeatureTree` pages inside the card drop their own frame border and radius
and keep their header, rows and search surface. The role owns presentation only;
the composition adds no painting and no tab behavior beyond page switching.

`tableRole="diagnostics"` on a read-only `QTableWidget` installs the shared
diagnostics cell delegate: Qt grid lines are disabled and only internal row/column
dividers are painted in the themed grid color, inside the shared bubble header and
rounded table shell. Set it before the table is first polished; it applies to any
column count and is not re-evaluated after installation.

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
At `QEvent::Show`, Resources selects a screen from Qt's global menu geometry. It checks the top-left point, then the center, then falls back to `QWidget::screen()`. Open-direction checks and shadow-gutter clamping share the selected screen. Qt provides both the menu geometry and screen bounds in logical coordinates.
On macOS, Resources observes `QEvent::ToolTip` on buttons and matches the native label's Show or Move event and text within that event-loop turn. Qt owns the label, including reuse and dismissals. Qt sends `QShowEvent` from `QWidgetPrivate::show_helper()` before `show_sys()` maps the native window, so Resources relocates the label on Show before it can appear at Qt's default position. When Qt reuses a visible label, `QToolTip::showText()` moves it inside the same turn and Resources relocates it on Move with a re-entrancy guard around its own `move()`. Placement is 14 logical px to the right of the hover position at cursor height, flips left if needed and clamps within the anchor screen's available geometry. Qt's position is kept when neither side fits. A request with no matching label event expires on the next event-loop turn through a posted cleanup event, so later calls to `QToolTip::showText()` keep Qt's placement. No timer delays the placement itself. Windows and Linux use Qt's placement.

- Put generic widget defaults in the lowest-numbered files.
- Put semantic role and state selectors after their defaults.
- Use dynamic properties for reusable states and density.
- Keep icons under stable `:/Resources/...` virtual paths.
- Ship one neutral (`#3c3c3b`) artwork per glyph. The theme runtime wraps tool- and
  push-button icons and recolors neutral pixels with `hoverBorder` (pointer hover) or
  `focusBorder` (pressed/checked/open), because Qt selects no icon mode for a press.
  Non-neutral artwork, disabled rendering and the rest state pass through unchanged.
  Icon-only neutral buttons get `resourcesGlyphButton="true"`, and `00_base.qss` drops
  their state outline; selectors with their own state pattern still take precedence.
- The consuming application installs the theme; reusable libraries do not.

Current consumer-specific object names identify narrow presentation exceptions.
Replace repeated exceptions with shared roles as the corresponding surfaces are
updated; do not copy a base template into another fragment.

Button keyboard navigation uses the Resources-owned `resourcesKeyboardFocus` state and a thin `focusBorder` outline, including icon-only glyph buttons. Pointer clicks clear this visual state without clearing native focus. The event filter preserves keyboard modality across automatic focus transfers and popup restoration; text-entry focus remains unchanged.
