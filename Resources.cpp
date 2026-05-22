#include "Resources.h"

#include <QApplication>
#include <QFile>
#include <QIcon>
#include <QTextStream>

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

    QFile file(QStringLiteral(":/Resources/Style.qss"));
    if (file.open(QFile::ReadOnly | QFile::Text))
    {
        QTextStream stream(&file);
        app.setStyleSheet(stream.readAll());
    }

    app.setWindowIcon(QIcon(QStringLiteral(":/Resources/Icon.png")));
}
}
