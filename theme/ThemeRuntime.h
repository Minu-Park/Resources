#pragma once
#include <QtSystemDetection>

class QApplication;
class QObject;
class QWindow;
namespace Resources {
/** @brief Installs private native-style and event adapters owned by the theme manager. */
QObject* installThemeRuntime(QApplication& app, QObject* owner);
#ifdef Q_OS_MACOS
/** @brief Disables AppKit's independent native transition on a themed combo popup. */
void disableComboPopupCocoaAnimation(QWindow* window);
#endif
}
