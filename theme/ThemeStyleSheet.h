#pragma once

#include <QString>

namespace Resources::detail {
/** @brief Composes validated theme declarations with a platform patch before Qt installation. */
QString composePlatformVariant(const QString& source, const QString& platform);
}
