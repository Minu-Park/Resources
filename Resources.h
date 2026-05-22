#pragma once

class QApplication;

namespace Resources
{
/**
 * @brief Initializes compiled resources, sets application style sheet,
 *        and configures application-wide resources like window icon.
 * @param app The main QApplication instance.
 */
void installResources(QApplication& app);
}
