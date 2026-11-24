#include "ThemeRuntime.h"

#include <QGuiApplication>
#include <QWindow>
#import <AppKit/AppKit.h>

void Resources::disableComboPopupCocoaAnimation(QWindow* window)
{
    if (!window || !window->handle() || QGuiApplication::platformName() != QLatin1String("cocoa")) return;

    // On Cocoa, Qt exposes the NSView as the window ID.
    auto* view = reinterpret_cast<NSView*>(window->winId());
    [view.window setAnimationBehavior:NSWindowAnimationBehaviorNone];
}
