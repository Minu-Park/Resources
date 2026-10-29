#include "Resources.h"
#include "theme/ResourcesTheme.h"
#include <QApplication>
#include <QWidget>
#include <QPainter>
#include <QPen>
#include <QPixmap>

namespace Resources {
void installResources(QApplication& app) { ResourcesTheme::instance().install(app); }
ThemePalette defaultThemePalette() { return ResourcesTheme::defaultThemePalette(); }
ThemePalette themePalette() { return ResourcesTheme::instance().themePalette(); }
bool setThemePalette(const ThemePalette& colors) { return ResourcesTheme::instance().setThemePalette(colors); }
void resetThemePalette() { ResourcesTheme::instance().resetThemePalette(); }
ThemePalette defaultPaletteGroup(PaletteGroup group) { return ResourcesTheme::defaultPaletteGroup(group); }
bool savePaletteFile(const QString& path, QString* error) { return ResourcesTheme::instance().savePaletteFile(path, error); }
bool loadPaletteFile(const QString& path, QString* error) { return ResourcesTheme::instance().loadPaletteFile(path, error); }
bool reloadThemeFromDirectory(const QString& directory, QString* error) { return ResourcesTheme::instance().reloadThemeFromDirectory(directory, error); }
QColor fontColor() { return themePalette().value(QStringLiteral("text")); }
bool setFontColor(const QColor& color) { return setThemePalette({{QStringLiteral("text"), color}}); }
QColor borderColor() { return themePalette().value(QStringLiteral("border")); }
bool setBorderColor(const QColor& color) { return setThemePalette({{QStringLiteral("border"), color}}); }

void paintMainWindowBorder(QWidget* window, QPainter& painter, bool maximized, bool forceRounded)
{
    if (!window) return;

    if (maximized && !forceRounded) {
        painter.fillRect(window->rect(), Resources::themePalette().value(QStringLiteral("surface")));
        return;
    }

    // Main-window corners use the translucent backing store instead of a binary
    // QBitmap mask so antialiased edge pixels are not hard-clipped.
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(window->rect(), Qt::transparent);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(borderColor());

    qreal outerRadius = 13.0;
    qreal innerRadius = 12.0;
    getPlatformWindowRadius(outerRadius, innerRadius);

    painter.drawRoundedRect(QRectF(0.0, 0.0, window->width(), window->height()), outerRadius, outerRadius);

    // Fill interior with white (1px inset, slightly smaller radius).
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setBrush(Resources::themePalette().value(QStringLiteral("surface")));
    painter.drawRoundedRect(QRectF(1.0, 1.0, window->width() - 2.0, window->height() - 2.0), innerRadius, innerRadius);

    // Draw smooth 1px border.
    painter.setPen(QPen(borderColor(), 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(QRectF(0.5, 0.5, window->width() - 1.0, window->height() - 1.0), innerRadius, innerRadius);
}

void getPlatformWindowRadius(qreal& outer, qreal& inner)
{
#ifdef Q_OS_WIN
    outer = 8.0;
    inner = 7.0;
#else
    outer = 13.0;
    inner = 12.0;
#endif
}

void paintMdiAreaBackground(QWidget* viewport, QPainter& painter, const QPixmap& logo)
{
    if (!viewport) return;
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.fillRect(viewport->rect(), Resources::themePalette().value(QStringLiteral("workspace")));

    if (logo.isNull()) return;

    const QSize logoSize = logo.size() / 2;
    const QPoint topLeft(
        (viewport->width() - logoSize.width()) / 2,
        (viewport->height() - logoSize.height()) / 2);
    painter.drawPixmap(QRect(topLeft, logoSize), logo);
}

QString formatLogHtml(int messageType, const QString& timestamp, const QString& message)
{
    QString typeStr;
    QString color;
    switch (messageType) {
        case QtDebugMsg:    typeStr = QStringLiteral("DEBUG"); color = themePalette().value(QStringLiteral("secondaryText")).name(QColor::HexRgb); break;
        case QtInfoMsg:     typeStr = QStringLiteral("INFO");  color = themePalette().value(QStringLiteral("selectionAccent")).name(QColor::HexRgb); break;
        case QtWarningMsg:  typeStr = QStringLiteral("WARN");  color = themePalette().value(QStringLiteral("warning")).name(QColor::HexRgb); break;
        case QtCriticalMsg: typeStr = QStringLiteral("CRIT");  color = themePalette().value(QStringLiteral("error")).name(QColor::HexRgb); break;
        case QtFatalMsg:    typeStr = QStringLiteral("FATAL"); color = themePalette().value(QStringLiteral("errorText")).name(QColor::HexRgb); break;
        default:            typeStr = QStringLiteral("LOG");   color = themePalette().value(QStringLiteral("secondaryText")).name(QColor::HexRgb); break;
    }

    return QStringLiteral("<span style=\"color:%5;\">[%1]</span> <span style=\"color:%2; font-weight:bold;\">[%3]</span> %4")
        .arg(timestamp, color, typeStr, message.toHtmlEscaped(), themePalette().value(QStringLiteral("secondaryText")).name(QColor::HexRgb));
}

}
