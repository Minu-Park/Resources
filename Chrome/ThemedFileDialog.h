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
    /** @brief Selects one existing directory, or returns an empty string when cancelled. */
    static QString getExistingDirectory(
        QWidget* parent,
        const QString& title,
        const QString& directory = {});
    static QString getSaveFileName(
        QWidget* parent,
        const QString& title,
        const QString& filePath = {},
        const QString& filter = {},
        QString* selectedFilter = nullptr);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /** @brief Waits for a native window-modal picker while preserving synchronous results. */
    int execWindowModal();
    void applyBottomCornerMask();
    void scheduleActionButtonAlignment();
    void alignActionButtons();
    /**
     * @brief Confirms an existing save target through the themed question box.
     * @note The outer dialog remains open when the user chooses No.
     */
    void handleAccepted();

    enum class Mode {
        OpenOne,
        OpenMany,
        Directory,
        Save
    };

    ThemedFileDialog(
        QWidget* parent,
        const QString& title,
        const QString& directory,
        const QString& filter,
        Mode mode);

    Mode _mode;
    QFileDialog* _fileDialog = nullptr;
    QDialogButtonBox* _buttonBox = nullptr;
    QLineEdit* _fileNameEdit = nullptr;
    QComboBox* _fileTypeComboBox = nullptr;
    QPushButton* _acceptButton = nullptr;
    QPushButton* _cancelButton = nullptr;
    bool _actionButtonAlignmentQueued = false;
};
