#pragma once
class QApplication;
class QObject;
namespace Resources {
/** @brief Installs private native-style and event adapters owned by the theme manager. */
QObject* installThemeRuntime(QApplication& app, QObject* owner);
}
