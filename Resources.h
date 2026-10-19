#pragma once

#include <QtGlobal>
#include <QColor>
#include "Chrome/WindowPlatformAttributes.h"

#ifdef Q_OS_WIN
#define PLATFORM_RADIUS(defaultVal, winVal) (winVal)
#else
#define PLATFORM_RADIUS(defaultVal, winVal) (defaultVal)
#endif

class QApplication;
class QWidget;
class QPainter;
class QPixmap;
class QString;

namespace Resources
{
/**
 * @brief Initializes compiled resources, sets application style sheet,
 *        and configures application-wide resources like window icon.
 * @param app The main QApplication instance.
 */
void installResources(QApplication& app);

/**
 * @brief Returns the installed default font color.
 * @return Current font color. The initial value is `#3c3c3b`.
 */
QColor fontColor();

/**
 * @brief Replaces the default font color and reapplies the installed stylesheet.
 * @param color RGB color. An invalid color is ignored, and any alpha is discarded.
 * @return `true` when the color is stored. Stylesheet application waits until `installResources()`.
 * @note Call on the GUI thread. Shared ink and selection text follow this color; status and muted colors do not.
 */
bool setFontColor(const QColor& color);

/**
 * @brief Returns the installed chrome border color.
 * @return Current border color. The initial value is `#d9d9d8`, the neutral pair of icon ink `#3c3c3b`.
 */
QColor borderColor();

/**
 * @brief Replaces the chrome border color and reapplies the installed stylesheet.
 * @param color RGB color. An invalid color is ignored, and any alpha is discarded.
 * @return `true` when the color is stored. Stylesheet application waits until `installResources()`.
 * @note Call on the GUI thread. Window, dialog, dock, pane, group, menu, statusbar, and separator lines follow this color, including borders painted in C++. Control, hover, focus, open-combo, accent, edit, warning, and error borders stay fixed.
 */
bool setBorderColor(const QColor& color);

/**
 * @brief Renders the frameless main window border and background with correct rounded edges.
 */
void paintMainWindowBorder(QWidget* window, QPainter& painter, bool maximized, bool forceRounded);

/**
 * @brief Retrieves the outer and inner corner radius based on the current operating system.
 */
void getPlatformWindowRadius(qreal& outer, qreal& inner);

/**
 * @brief Fills the MDI area background and draws the branding logo cleanly.
 */
void paintMdiAreaBackground(QWidget* viewport, QPainter& painter, const QPixmap& logo);

/**
 * @brief Standardized HTML formatter for system log messages.
 */
QString formatLogHtml(int messageType, const QString& timestamp, const QString& message);
}
