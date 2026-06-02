#pragma once

#include "ThemedDialog.h"

class ThemedMessageBox : public ThemedDialog {
    Q_OBJECT
public:
    enum Icon { Information, Warning, Critical };

    explicit ThemedMessageBox(Icon icon, const QString& title, const QString& text, QWidget* parent = nullptr);

    static void critical(QWidget* parent, const QString& title, const QString& text);
    static void warning(QWidget* parent, const QString& title, const QString& text);
    static void information(QWidget* parent, const QString& title, const QString& text);
};
