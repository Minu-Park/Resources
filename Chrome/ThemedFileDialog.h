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
    void applyBottomCornerMask();
    void scheduleActionButtonAlignment();
    void alignActionButtons();
    /**
     * @brief Confirms an existing save target through the themed question box.
     * @note The outer dialog remains open when the user chooses No.
     */
    void handleAccepted();
    /** @brief Shows the current directory as the editable "Look in" path. */
    void syncLookInPath();
    /** @brief Puts the user's unopened path back after Qt rewrites the field. */
    void restoreLookInPath();
    /**
     * @brief Opens the folder typed into "Look in".
     * @note A path that is not an existing folder is refused in a themed message box.
     */
    void openTypedLookInPath();
    /**
     * @brief Completes the typed "Look in" path on Tab without opening it.
     * @note One match, or the suggestion selected in the popup, completes to that folder and a
     * trailing separator. Several matches complete to their shared prefix, then select the first.
     * A relative or "~" path completes from the folder it names and keeps its typed form.
     * @return False when no folder matches, so Tab moves focus instead.
     */
    bool completeLookInPath();

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
    QComboBox* _lookInComboBox = nullptr;
    /** @brief Text the user typed or chose and has not opened yet; null while the field shows the current folder. */
    QString _typedLookInPath;
    bool _refusingLookInPath = false;
    QPushButton* _acceptButton = nullptr;
    QPushButton* _cancelButton = nullptr;
    bool _actionButtonAlignmentQueued = false;
};
