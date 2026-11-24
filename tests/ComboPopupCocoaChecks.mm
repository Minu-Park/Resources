#include <QWidget>
#import <AppKit/AppKit.h>

bool comboPopupCocoaAnimationDisabled(QWidget& popup)
{
    auto* view = reinterpret_cast<NSView*>(popup.internalWinId());
    NSWindow* window = view.window;
    return window && window.animationBehavior == NSWindowAnimationBehaviorNone;
}
