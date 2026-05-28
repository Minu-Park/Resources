#include "Resources.h"

#include <QApplication>
#include <QFile>
#include <QIcon>
#include <QStringList>
#include <QTextStream>
#include <QFontDatabase>
#include <QFont>
#include <QMenu>
#include <QEvent>
#include <QDebug>

// Global-scope helper to execute Q_INIT_RESOURCE, which relies on global symbols.
// This ensures the resource system registers the compiled .qrc binary data.
inline void initResourcesHelper()
{
    Q_INIT_RESOURCE(Resources);
}

// ---------------------------------------------------------------------------
// PopupStyleFilter: removes the native popup frame so QSS border is the only
// visible outline.  Installed once via installResources().
// Saves and restores popup geometry because setWindowFlag() recreates the
// native window and loses the original popup position.
// ---------------------------------------------------------------------------
class PopupStyleFilter : public QObject
{
public:
    explicit PopupStyleFilter(QObject* parent = nullptr) : QObject(parent) {}

protected:
    bool eventFilter(QObject* obj, QEvent* event) override
    {
        if (event->type() == QEvent::Show) {
            if (auto* widget = qobject_cast<QWidget*>(obj)) {
                if ((widget->windowFlags() & Qt::Popup)
                    && !widget->property("_popupStyled").toBool()) {
                    widget->setProperty("_popupStyled", true);
                    QRect geo = widget->geometry();
                    widget->setWindowFlag(Qt::FramelessWindowHint, true);
                    widget->setWindowFlag(Qt::NoDropShadowWindowHint, true);
                    widget->setAttribute(Qt::WA_TranslucentBackground, true);
                    widget->setGeometry(geo); // restore position lost by flag change
                    widget->show();
                    return true;
                }
            }
        }
        return QObject::eventFilter(obj, event);
    }
};

namespace Resources
{

static PopupStyleFilter* s_popupFilter = nullptr;

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

    // Install global event filter to strip native popup borders.
    if (!s_popupFilter) {
        s_popupFilter = new PopupStyleFilter(&app);
    }
    app.installEventFilter(s_popupFilter);
}
}
