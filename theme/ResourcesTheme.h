#pragma once

#include "Resources.h"
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QStringList>

namespace Resources {

/** @brief Shared logical-pixel geometry consumed by QSS and runtime painting. */
struct ControlMetrics {
    int contentHeight = 0;
    int contentWidth = 0;
    int paddingX = 0;
    int paddingY = 0;
    int borderWidth = 0;
    int radius = 0;
};

/** @brief Owns process-wide theme definitions, atomic snapshots and application installation.
 *  The host installs the theme on the GUI thread. Rendering helpers consume this
 *  owner's metrics; they do not define their own widget templates.
 */
class ResourcesTheme final : public QObject {
public:
    enum class ControlKind { Field, ToolButton, TreeFrame };
    enum class Density { Default, Compact };

    /** @brief Returns the sole owner, adopted by QApplication at installation. */
    static ResourcesTheme& instance();
    /** @brief Returns all factory palette roles without modifying the installed theme. */
    static ThemePalette defaultThemePalette();
    /** @brief Returns the factory roles for the selected palette authoring group. */
    static ThemePalette defaultPaletteGroup(PaletteGroup group);
    /** @brief Installs the theme once and reapplies its current snapshot on subsequent calls. */
    void install(QApplication& app);
    /** @brief Returns a copy of the active palette, including before installation. */
    ThemePalette themePalette() const;
    /** @brief Validates and applies a nonempty palette patch on the GUI thread. */
    bool setThemePalette(const ThemePalette& colors);
    /** @brief Restores factory colors while preserving current templates and metrics. */
    void resetThemePalette();
    /** @brief Atomically saves the complete current RGB palette. */
    bool savePaletteFile(const QString& path, QString* error = nullptr) const;
    /** @brief Loads a complete RGB palette, restoring factory colors on invalid input. */
    bool loadPaletteFile(const QString& path, QString* error = nullptr);
    /** @brief Atomically reloads QSS and the adjacent metrics.json without changing palette colors. */
    bool reloadThemeFromDirectory(const QString& directory, QString* error = nullptr);
    /** @brief Returns a required positive presentation or widget metric from the active snapshot. */
    int metric(const char* name) const;
    /** @brief Resolves shared geometry for a supported widget kind and density. */
    ControlMetrics metrics(ControlKind kind, Density density = Density::Default) const;
    /** @brief Selects density for a line edit, spin box, combo box or tool button and refreshes geometry. */
    void setDensity(QWidget& widget, Density density);
    /** @brief Applies the nearest consumer actionDensity scope unless density is explicit. */
    void polishWidget(QWidget& widget);

private:
    explicit ResourcesTheme();
    void applyInstalledStyleSheet();
    void refreshWidgets();
    static bool validateMetrics(const QJsonObject& values, QString* error);
    static bool validateStyleSheet(const QString& source, const QJsonObject& metrics,
                                  QString* error);
    static QString renderStyleSheet(QString source, const QJsonObject& metrics,
                                    const ThemePalette& palette);
    static void refreshWidget(QWidget& widget);

    QPointer<QApplication> app_;
    QPointer<QObject> runtime_;
    ThemePalette palette_;
    QJsonObject metrics_;
    QString styleSheetTemplate_;
    QString platformStyleSheet_;
    QStringList themeFileNames_;
};
}
