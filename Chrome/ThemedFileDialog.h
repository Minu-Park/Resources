#pragma once

#include "Chrome/ThemedDialog.h"

#include <QString>
#include <QStringList>

class QFileDialog;
class QEvent;
class QComboBox;
class QDialogButtonBox;
class QLineEdit;
class QPushButton;

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

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void applyBottomCornerMask();
    void scheduleActionButtonAlignment();
    void alignActionButtons();

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
    QDialogButtonBox* _buttonBox = nullptr;
    QLineEdit* _fileNameEdit = nullptr;
    QComboBox* _fileTypeComboBox = nullptr;
    QPushButton* _acceptButton = nullptr;
    QPushButton* _cancelButton = nullptr;
    bool _actionButtonAlignmentQueued = false;
};
