#include "Chrome/ThemedFileDialog.h"

#include <QDialog>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QSizePolicy>
#include <QSplitter>
#include <QTreeView>
#include <QVBoxLayout>

ThemedFileDialog::ThemedFileDialog(
    QWidget* parent,
    const QString& title,
    const QString& directory,
    const QString& filter,
    Mode mode)
    : ThemedDialog(title, parent)
{
    contentLayout()->setContentsMargins(0, 0, 0, 0);
    contentLayout()->setSpacing(0);

    _fileDialog = new QFileDialog(contentWidget());
    _fileDialog->setOption(QFileDialog::DontUseNativeDialog, true);
    _fileDialog->setWindowFlags(Qt::Widget);
    _fileDialog->setNameFilter(filter);
    _fileDialog->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    switch (mode) {
    case Mode::OpenOne:
        _fileDialog->setFileMode(QFileDialog::ExistingFile);
        _fileDialog->setAcceptMode(QFileDialog::AcceptOpen);
        break;
    case Mode::OpenMany:
        _fileDialog->setFileMode(QFileDialog::ExistingFiles);
        _fileDialog->setAcceptMode(QFileDialog::AcceptOpen);
        break;
    case Mode::DirectoryMany:
        _fileDialog->setOption(QFileDialog::ShowDirsOnly, true);
        _fileDialog->setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
        _fileDialog->setFileMode(QFileDialog::ExistingFiles);
        _fileDialog->setAcceptMode(QFileDialog::AcceptOpen);
        break;
    case Mode::Save:
        _fileDialog->setFileMode(QFileDialog::AnyFile);
        _fileDialog->setAcceptMode(QFileDialog::AcceptSave);
        _fileDialog->setOption(QFileDialog::DontConfirmOverwrite, false);
        break;
    }

    if (mode == Mode::Save) {
        const QFileInfo fileInfo(directory);
        if (fileInfo.isDir()) {
            _fileDialog->setDirectory(fileInfo.absoluteFilePath());
        } else if (!directory.isEmpty()) {
            _fileDialog->setDirectory(fileInfo.absolutePath());
            _fileDialog->selectFile(fileInfo.fileName());
        }
    } else {
        _fileDialog->setDirectory(directory);
    }

    for (QSplitter* splitter : _fileDialog->findChildren<QSplitter*>()) {
        if (splitter->orientation() == Qt::Horizontal) {
            splitter->setProperty("fileDialogRole", "sidebar");
        }
    }

    if (auto* itemView = _fileDialog->findChild<QTreeView*>("treeView")) {
        itemView->header()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }

    contentLayout()->addWidget(_fileDialog);
    connect(_fileDialog, &QFileDialog::accepted, this, &QDialog::accept);
    connect(_fileDialog, &QFileDialog::rejected, this, &QDialog::reject);

    resize(sizeHint().expandedTo(QSize(720, 520)));
}

QString ThemedFileDialog::getOpenFileName(
    QWidget* parent,
    const QString& title,
    const QString& directory,
    const QString& filter)
{
    ThemedFileDialog dialog(parent, title, directory, filter, Mode::OpenOne);
    return dialog.exec() == QDialog::Accepted ? dialog._fileDialog->selectedFiles().value(0) : QString{};
}

QStringList ThemedFileDialog::getOpenFileNames(
    QWidget* parent,
    const QString& title,
    const QString& directory,
    const QString& filter)
{
    ThemedFileDialog dialog(parent, title, directory, filter, Mode::OpenMany);
    return dialog.exec() == QDialog::Accepted ? dialog._fileDialog->selectedFiles() : QStringList{};
}

QStringList ThemedFileDialog::getExistingDirectories(
    QWidget* parent,
    const QString& title,
    const QString& directory)
{
    ThemedFileDialog dialog(parent, title, directory, {}, Mode::DirectoryMany);
    return dialog.exec() == QDialog::Accepted ? dialog._fileDialog->selectedFiles() : QStringList{};
}

QString ThemedFileDialog::getSaveFileName(
    QWidget* parent,
    const QString& title,
    const QString& filePath,
    const QString& filter)
{
    ThemedFileDialog dialog(parent, title, filePath, filter, Mode::Save);
    return dialog.exec() == QDialog::Accepted ? dialog._fileDialog->selectedFiles().value(0) : QString{};
}
