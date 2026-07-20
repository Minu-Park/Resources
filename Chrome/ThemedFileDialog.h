#pragma once

#include "Chrome/ThemedDialog.h"

#include <QString>
#include <QStringList>

class QFileDialog;

class ThemedFileDialog final : public ThemedDialog {
public:
    static QString getOpenFileName(
        QWidget* parent,
        const QString& title,
        const QString& directory = {},
        const QString& filter = {});
    static QStringList getOpenFileNames(
        QWidget* parent,
        const QString& title,
        const QString& directory = {},
        const QString& filter = {});
    static QStringList getExistingDirectories(
        QWidget* parent,
        const QString& title,
        const QString& directory = {});
    static QString getSaveFileName(
        QWidget* parent,
        const QString& title,
        const QString& filePath = {},
        const QString& filter = {});

private:
    enum class Mode {
        OpenOne,
        OpenMany,
        DirectoryMany,
        Save
    };

    ThemedFileDialog(
        QWidget* parent,
        const QString& title,
        const QString& directory,
        const QString& filter,
        Mode mode);

    QFileDialog* _fileDialog = nullptr;
};
