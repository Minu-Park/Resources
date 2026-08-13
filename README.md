# Resources

Resources is a standalone Qt 6 resource, theme, and reusable window-chrome library. It registers compiled assets and installs one ordered QSS theme into a consumer-owned `QApplication`.

## Capabilities

- Compile icons, images, and ordered `theme/qss/*.qss` fragments into a Qt resource collection.
- Install the resource collection and stylesheet through `Resources::installResources()`.
- Provide reusable main-window, MDI, dock, dialog, file-dialog, message-box, splash, loading, and progress chrome.
- Expose an opt-in cached MDI shadow implementation with a focused offscreen test target.

The module owns presentation only. It must not own device, session, acquisition, or renderer behavior, and its controls must remain functionally usable when a consumer does not install the theme.

## Requirements

- CMake 3.21 or newer and a C++17 compiler.
- Qt 6.4 or newer with Core and Widgets.
- Qt Test only when `RESOURCES_BUILD_MDI_SHADOW_TESTS=ON`.

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

## Theme Contract

Consumers expose semantic dynamic properties such as `status`, `state`, or `messageState`, plus stable role-oriented object names only when Qt lacks a suitable semantic selector. The QSS owns colors, spacing, radii, weights, and icon variants. After changing a dynamic property, repolish the widget when an immediate visual update is required.

Legacy consumer-specific object-name selectors remain in the current theme for compatibility. New selectors must use generic widget defaults or documented semantic roles; expanding the legacy set would make this module dependent on an unknown consumer topology.

See [theme/README.md](theme/README.md) for file ordering and extension rules.

## Validation

For shadow changes, configure `RESOURCES_BUILD_MDI_SHADOW_TESTS=ON` and run `ResourcesMdiShadowTests` with the offscreen Qt platform. Automated tests do not replace visual checks for native window composition, fractional scaling, and platform-specific chrome.
