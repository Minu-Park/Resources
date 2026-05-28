#include "Resources.h"

#include <QApplication>
#include <QFile>
#include <QIcon>
#include <QStringList>
#include <QTextStream>
#include <QFontDatabase>
#include <QFont>
#include <QDebug>

// Global-scope helper to execute Q_INIT_RESOURCE, which relies on global symbols.
// This ensures the resource system registers the compiled .qrc binary data.
inline void initResourcesHelper()
{
    Q_INIT_RESOURCE(Resources);
}

namespace Resources
{
void installResources(QApplication& app)
{
    initResourcesHelper();

    // Load embedded fonts
    const QStringList fontFiles = {
        QStringLiteral(":/Resources/theme/fonts/Inter-Regular.ttf"),
        QStringLiteral(":/Resources/theme/fonts/Inter-Bold.ttf"),
        QStringLiteral(":/Resources/theme/fonts/JetBrainsMono-Regular.ttf"),
        QStringLiteral(":/Resources/theme/fonts/JetBrainsMono-Bold.ttf")
    };

    for (const QString& fontFile : fontFiles) {
        int id = QFontDatabase::addApplicationFont(fontFile);
        if (id == -1) {
            qWarning() << "Failed to load font from resource:" << fontFile;
        }
    }

    // Set application default font
    QFont defaultFont(QStringLiteral("Inter"));
#if defined(Q_OS_MAC)
    defaultFont.setPointSizeF(12.0);
#else
    defaultFont.setPointSizeF(10.0);
#endif
    app.setFont(defaultFont);

    const QStringList qssFiles = {
        QStringLiteral(":/Resources/theme/qss/00_base.qss"),
        QStringLiteral(":/Resources/theme/qss/10_graphics_engine.qss"),
        QStringLiteral(":/Resources/theme/qss/20_statusbar.qss"),
        QStringLiteral(":/Resources/theme/qss/30_camera_gocator.qss"),
        QStringLiteral(":/Resources/theme/qss/40_chrome.qss"),
        QStringLiteral(":/Resources/theme/qss/50_static_image.qss"),
        QStringLiteral(":/Resources/theme/qss/60_processing.qss")
    };

    QString styleSheet;
    for (const QString& qssFile : qssFiles) {
        QFile file(qssFile);
        if (file.open(QFile::ReadOnly | QFile::Text)) {
            QTextStream stream(&file);
            styleSheet += stream.readAll();
            styleSheet += QLatin1Char('\n');
        }
    }
    app.setStyleSheet(styleSheet);

    app.setWindowIcon(QIcon(QStringLiteral(":/Resources/Icon.png")));
}
}
