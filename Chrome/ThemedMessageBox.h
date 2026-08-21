#pragma once

#include "ThemedDialog.h"

class ThemedMessageBox : public ThemedDialog {
    Q_OBJECT
public:
    enum Icon { Information, Warning, Critical };

    /**
     * @brief Creates a themed information, warning, critical, or question box.
     * @param question When true, creates themed No/Yes buttons instead of OK.
     */
    explicit ThemedMessageBox(
        Icon icon,
        const QString& title,
        const QString& text,
        QWidget* parent = nullptr,
        bool question = false);

    static void critical(QWidget* parent, const QString& title, const QString& text);
    static void warning(QWidget* parent, const QString& title, const QString& text);
    static void information(QWidget* parent, const QString& title, const QString& text);
    /**
     * @brief Shows a themed Yes/No question and returns the user's choice.
     * @return `true` when the user chooses Yes.
     */
    static bool question(QWidget* parent, const QString& title, const QString& text);
};
