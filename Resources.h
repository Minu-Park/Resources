#pragma once

#include <QtGlobal>
#include <QColor>
#include <QMap>
#include <QString>
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

/** @brief Named application colors; defaultThemePalette() enumerates the supported roles. */
using ThemePalette = QMap<QString, QColor>;

/** @brief Returns the complete factory palette without changing the installed theme. */
ThemePalette defaultThemePalette();

/** @brief Returns a copy of the current process-wide palette, including before installation. */
ThemePalette themePalette();

/**
 * @brief Atomically applies a nonempty patch of known palette roles and refreshes themed widgets.
 * @param colors Role/color pairs from defaultThemePalette(); invalid colors or unknown roles reject the whole patch.
 * @return Whether all entries were accepted. Alpha is discarded; nothing is persisted.
 * @note Call on the GUI thread. Before installation, values are stored for the next host installation.
 */
bool setThemePalette(const ThemePalette& colors);

/** @brief Restores every factory color on the GUI thread without changing application settings. */
void resetThemePalette();

/** @brief Groups surface/feedback and font roles for authoring-tool presentation. */
enum class PaletteGroup { Theme, Font };

/** @brief Returns factory roles belonging to one authoring-tool color group. */
ThemePalette defaultPaletteGroup(PaletteGroup group);

/** @brief Atomically saves the complete current theme, including font colors, as RGB JSON. */
bool savePaletteFile(const QString& path, QString* error = nullptr);

/** @brief Validates and replaces the complete theme from RGB JSON.
 *  @note Call on the GUI thread. Missing or invalid files restore every built-in color; false returns a diagnostic.
 */
bool loadPaletteFile(const QString& path, QString* error = nullptr);

/**
 * @brief Reloads the complete installed QSS fragment set from a host-selected directory.
 * @param directory Directory containing every installed qss fragment; missing files reject the whole reload.
 * @param error Optional failure description. Current colors and platform chrome rules are preserved.
 * @return Whether the complete source theme was installed. Call on the GUI thread after installResources().
 */
bool reloadThemeFromDirectory(const QString& directory, QString* error = nullptr);

/**
 * @brief Returns the installed default font color.
 * @return Current font color. The initial value is `#3c3c3b`.
 */
QColor fontColor();

/**
 * @brief Replaces the default font color and reapplies the installed stylesheet.
 * @param color RGB color. An invalid color is ignored, and any alpha is discarded.
 * @return `true` when the color is stored. Stylesheet application waits until `installResources()`.
 * @note Call on the GUI thread. Shared ink, selection text and native accent marks follow this color; status and muted colors do not.
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
