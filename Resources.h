#pragma once

#include <QtGlobal>

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
 * @brief Renders the frameless main window border and background with correct rounded edges.
 */
void paintMainWindowBorder(QWidget* window, QPainter& painter, bool maximized, bool forceRounded);

/**
 * @brief Applies platform-specific window rendering attributes (e.g., DWM rounded corners and extend frame on Windows).
 */
void applyWindowPlatformAttributes(QWidget* window);

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
