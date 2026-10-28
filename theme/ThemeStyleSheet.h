#pragma once

#include <QString>

namespace Resources::detail {
/** @brief Composes theme declarations, preserving output on a source or patch parse failure. */
bool composePlatformVariant(const QString& source, const QString& platform,
                            QString& output, QString* error = nullptr);
}
