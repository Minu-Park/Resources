# Resources

Resources is a standalone Qt 6 resource, theme, and reusable window-chrome library. It registers compiled assets and installs one ordered QSS theme into a consumer-owned `QApplication`.

## Capabilities

- Compile icons, images, and ordered `theme/qss/*.qss` fragments into a Qt resource collection.
- Install the resource collection and stylesheet through `Resources::installResources()`.
- Provide reusable main-window, MDI, dock, dialog, file-dialog, message-box, splash, loading, and progress chrome.
- Provide a Qt-only searchable tree base with stable-key favorites, editor-preserving filtering, shortcuts, and scrollbar-independent overlay geometry.
- Expose an opt-in cached MDI shadow implementation with a focused offscreen test target.

The module owns presentation only. It must not own device, session, acquisition, or renderer behavior, and its controls must remain functionally usable when a consumer does not install the theme.

## Requirements

- CMake 3.21 or newer and a C++17 compiler.
- Qt 6.4 or newer with Core and Widgets.
- Qt Test when a Resources test option is enabled.

## Integration

```cmake
add_subdirectory(path/to/Resources Resources-build)
target_link_libraries(consumer PRIVATE Resources::Resources)
```

Install the resources once from the application entry point:

```cpp
#include "Resources.h"
#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    Resources::installResources(app);
    return app.exec();
}
```

Use virtual paths such as `:/Resources/Icons/...`; never depend on a source-tree path at runtime.

## Searchable tree widgets

Link `Resources::Widgets` and include `Widgets/SearchableTreeWidget.h` to use
`SearchableTreeWidget`, a `QTreeWidget` subclass that needs only Qt Widgets.
This target does not link theme assets or install the Resources theme.
`Resources::Resources` also exposes it transitively for themed applications.

```cmake
add_subdirectory(path/to/Resources Resources-build)
target_link_libraries(consumer PRIVATE Resources::Widgets)
```

```cpp
SearchableTreeWidget tree(nullptr, settingsFile, QStringLiteral("Browser/Favorites"));
auto* category = new QTreeWidgetItem(&tree, {QStringLiteral("Acquisition")});
auto* row = new QTreeWidgetItem(category, {QStringLiteral("Exposure"), QStringLiteral("42")});
tree.registerItem(row, QStringLiteral("sensor/exposure"), QStringLiteral("ExposureTime"));
```

Existing `QTreeWidget` instances can use the same implementation without changing
their concrete type or consumer module. Include `Widgets/SearchableTreeController.h`
and attach a tree-owned controller once:

```cpp
SearchableTreeController::Options options;
options.settingsFile = settingsFile;
options.favoritesKey = QStringLiteral("Browser/Favorites");
options.favoriteKey = [](const QTreeWidgetItem* item) {
    return item->childCount() == 0 ? item->data(0, Qt::UserRole).toString() : QString();
};
options.searchText = [](const QTreeWidgetItem* item) {
    return item->text(0) + QChar(' ') + item->data(0, Qt::UserRole).toString();
};
auto* browser = SearchableTreeController::attach(existingTree, options);
```

Options also accept stable state identity and searchable-row predicates. Providers
read existing local metadata; they must not query devices. Row updates are observed
automatically and batched on the event loop. A column-zero delegate proxy preserves
the original delegate's painting, sizing, editing and editor signals; value-column
widgets stay in place. Intrinsic hidden rows stay hidden when search clears.
`SearchableTreeWidget` is a thin inheritance adapter over this same controller.
Theme metrics are applied to the controller's `SearchableTreeSurface` child;
the host remains responsible for installing the theme.

An omitted settings filename keeps favorites local to that widget. Persistent
browsers synchronize only when their absolute settings filename and key match;
missing rows do not remove saved keys. Consumers own opaque favorite identities,
settings namespaces, and domain metadata. Browser roles start at
`Qt::UserRole + 256`, preserving consumers' ordinary user roles.
Override `itemSearchText()` and `itemStateKey()` in a domain subclass when needed,
or populate `SearchTextRole` and `StateKeyRole`. Call `refreshFilter()` after a
batch update; existing row editors are retained. Unregistered leaves are searchable
by label without stars. An empty favorite key
also registers a searchable row without a star. A consumer may register any row,
including a parent row; matching descendants remain reachable through ancestors.

Ctrl+F focuses search, Escape clears it, and Ctrl+D toggles the current registered
row. Saved stars stay visible; other stars appear on row hover. The search surface
is centered in the tree frame with permanent scrollbar clearance. It resizes only
with its containing tree or theme metrics. Configure
`RESOURCES_BUILD_WIDGET_TESTS=ON` to run the Qt-only behavior/geometry tests.

## Theme Contract

Consumers expose semantic dynamic properties such as `status`, `state`, or `messageState`, plus stable role-oriented object names only when Qt lacks a suitable semantic selector. The QSS owns colors, spacing, radii, weights, and icon variants. After changing a dynamic property, repolish the widget when an immediate visual update is required.

Legacy consumer-specific object-name selectors remain in the current theme for compatibility. New selectors must use generic widget defaults or documented semantic roles; expanding the legacy set would make this module dependent on an unknown consumer topology.

See [theme/README.md](theme/README.md) for file ordering and extension rules.

## Validation

For shadow changes, configure `RESOURCES_BUILD_MDI_SHADOW_TESTS=ON` and run `ResourcesMdiShadowTests` with the offscreen Qt platform. Automated tests do not replace visual checks for native window composition, fractional scaling, and platform-specific chrome.
