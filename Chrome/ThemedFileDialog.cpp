#include "Chrome/ThemedFileDialog.h"
#include "Chrome/ThemedMessageBox.h"

#include <QCompleter>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QEvent>
#include <QEventLoop>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFrame>
#include <QHeaderView>
#include <QIcon>
#include <QKeyEvent>
#include <QListView>
#include <QComboBox>
#include <QLineEdit>
#include <QBitmap>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRegion>
#include <QSizePolicy>
#include <QSplitter>
#include <QSplitterHandle>
#include <QStyle>
#include <QStyleOptionHeader>
#include <QTreeView>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

class FileDialogHeaderView final : public QHeaderView {
public:
    explicit FileDialogHeaderView(QWidget* parent)
        : QHeaderView(Qt::Horizontal, parent)
    {
    }

protected:
    void paintSection(QPainter* painter, const QRect& rect, int logicalIndex) const override
    {
        QStyleOptionHeader option;
        initStyleOptionForIndex(&option, logicalIndex);
        option.rect = rect;
        option.textAlignment &= ~Qt::AlignVertical_Mask;
        option.textAlignment |= Qt::AlignVCenter;
        painter->save();
        style()->drawControl(QStyle::CE_Header, &option, painter, this);
        painter->restore();

        if (logicalIndex != 0 || !option.text.isEmpty()) {
            return;
        }

        const QString firstSectionText = QCoreApplication::translate("QFileSystemModel", "Name");
        const int sortIndicatorWidth = option.sortIndicator == QStyleOptionHeader::None
            ? 0
            : style()->pixelMetric(QStyle::PM_HeaderMarkSize, &option, this) + 6;
        const QRect labelRect = rect.adjusted(6, 0, -(6 + sortIndicatorWidth), 0);
        painter->save();
        painter->setPen(palette().color(QPalette::Text));
        painter->setFont(font());
        painter->drawText(
            labelRect,
            Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
            firstSectionText);
        painter->restore();
    }
};

ThemedFileDialog::ThemedFileDialog(
    QWidget* parent,
    const QString& title,
    const QString& directory,
    const QString& filter,
    Mode mode)
    : ThemedDialog(title, parent)
    , _mode(mode)
{
    contentLayout()->setContentsMargins(0, 0, 0, 0);
    contentLayout()->setSpacing(0);

    _fileDialog = new QFileDialog(contentWidget());
    _fileDialog->setOption(QFileDialog::DontUseNativeDialog, true);
    _fileDialog->setWindowFlags(Qt::Widget);
    _fileDialog->setNameFilter(filter);
    _fileDialog->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    _fileDialog->installEventFilter(this);

    switch (mode) {
    case Mode::OpenOne:
        _fileDialog->setFileMode(QFileDialog::ExistingFile);
        _fileDialog->setAcceptMode(QFileDialog::AcceptOpen);
        break;
    case Mode::OpenMany:
        _fileDialog->setFileMode(QFileDialog::ExistingFiles);
        _fileDialog->setAcceptMode(QFileDialog::AcceptOpen);
        break;
    case Mode::Directory:
        _fileDialog->setOption(QFileDialog::ShowDirsOnly, true);
        _fileDialog->setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
        _fileDialog->setFileMode(QFileDialog::Directory);
        _fileDialog->setAcceptMode(QFileDialog::AcceptOpen);
        break;
    case Mode::Save:
        _fileDialog->setFileMode(QFileDialog::AnyFile);
        _fileDialog->setAcceptMode(QFileDialog::AcceptSave);
        // QFileDialog's built-in overwrite prompt bypasses the application theme.
        // The outer themed dialog confirms the selected path instead.
        _fileDialog->setOption(QFileDialog::DontConfirmOverwrite, true);
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
    } else if (!directory.isEmpty()) {
        // QFileDialog starts in Qt's last visited directory. An empty
        // setDirectory() call would clear it and open the working directory.
        _fileDialog->setDirectory(directory);
    }

    // Qt's "Look in" combo only lists the current path and its history. Make it
    // editable so a typed folder path opens directly. This runs after
    // setAcceptMode(), which turns the combo read-only for save dialogs.
    _lookInComboBox = _fileDialog->findChild<QComboBox*>(QStringLiteral("lookInCombo"));
    if (_lookInComboBox) {
        _lookInComboBox->setEditable(true);
        // The combo's default completer offers its folder names and selects combo rows.
        _lookInComboBox->setCompleter(nullptr);
        if (QLineEdit* pathEdit = _lookInComboBox->lineEdit()) {
            pathEdit->installEventFilter(this);
            // On Return and when editing finishes, QComboBox selects a row whose name matches
            // the typed text, and QFileDialog then opens that row. A typed "first" would open
            // an ancestor named "first" instead of the child folder; this dialog opens paths itself.
            disconnect(pathEdit, &QLineEdit::returnPressed, _lookInComboBox, nullptr);
            disconnect(pathEdit, &QLineEdit::editingFinished, _lookInComboBox, nullptr);
            connect(pathEdit, &QLineEdit::returnPressed, this, &ThemedFileDialog::openTypedLookInPath);
            connect(pathEdit, &QLineEdit::textEdited, this, [this](const QString& text) {
                _typedLookInPath = text;
            });
            // QComboBox rewrites the field with the current row's text whenever the file-system
            // model refreshes that row, including while the user is typing. Restore at once so no
            // event-loop turn shows, or reads, the rewritten text.
            connect(pathEdit, &QLineEdit::textChanged, this, [this, pathEdit] {
                if (!pathEdit->isModified()) {
                    restoreLookInPath();
                }
            });

            auto* folders = new QFileSystemModel(pathEdit);
            folders->setFilter(QDir::AllDirs | QDir::Drives | QDir::NoDotAndDotDot);
            folders->setRootPath(QString());
            auto* completer = new QCompleter(folders, pathEdit);
            completer->setCompletionMode(QCompleter::PopupCompletion);
#if defined(Q_OS_LINUX)
            completer->setCaseSensitivity(Qt::CaseSensitive);
#else
            completer->setCaseSensitivity(Qt::CaseInsensitive);
#endif
            completer->popup()->setObjectName(QStringLiteral("AutoCompletePopup"));
            // QCompleter treats Tab like Return and opens the selection. This later filter
            // runs first and turns Tab into completion instead.
            completer->popup()->installEventFilter(this);
            pathEdit->setCompleter(completer);
            // Choosing a suggestion opens it.
            // QLineEdit writes the chosen path before this slot runs, and the restore briefly
            // keeps the typed prefix; opening the recorded choice then shows its full path.
            connect(completer, qOverload<const QString&>(&QCompleter::activated), this, [this](const QString& path) {
                _typedLookInPath = path;
                QTimer::singleShot(0, this, &ThemedFileDialog::openTypedLookInPath);
            });
        }
        connect(_fileDialog, &QFileDialog::directoryEntered, this, &ThemedFileDialog::syncLookInPath);
        syncLookInPath();
    }

    for (QSplitter* splitter : _fileDialog->findChildren<QSplitter*>()) {
        if (splitter->orientation() == Qt::Horizontal) {
            splitter->setProperty("fileDialogRole", "sidebar");
            splitter->setHandleWidth(4);
            for (int index = 1; index < splitter->count(); ++index) {
                if (QSplitterHandle* handle = splitter->handle(index)) {
                    handle->setProperty("fileDialogRole", "sidebarHandle");
                    handle->installEventFilter(this);
                }
            }
        }
    }

    for (QListView* sidebar : _fileDialog->findChildren<QListView*>()) {
        if (sidebar->inherits("QSidebar")) {
            sidebar->setProperty("fileDialogRole", "sidebar");
            sidebar->setFrameShape(QFrame::NoFrame);
        }
    }

    for (const char* objectName : {"backButton", "forwardButton", "toParentButton", "newFolderButton", "listModeButton", "detailModeButton"}) {
        if (auto* button = _fileDialog->findChild<QToolButton*>(QLatin1String(objectName))) {
            button->setProperty("fileDialogRole", "navigation");
            button->setIconSize(QSize(14, 14));
            if (button->objectName() == QLatin1String("backButton")) {
                button->setIcon(QIcon(QStringLiteral(":/Resources/icon/back-48.png")));
            } else if (button->objectName() == QLatin1String("forwardButton")) {
                button->setIcon(QIcon(QStringLiteral(":/Resources/icon/forward-48.png")));
            } else if (button->objectName() == QLatin1String("toParentButton")) {
                button->setIcon(QIcon(QStringLiteral(":/Resources/icon/collapse-arrow-48.png")));
            } else if (button->objectName() == QLatin1String("newFolderButton")) {
                button->setIcon(QIcon(QStringLiteral(":/Resources/icon/new-folder-48.png")));
            } else if (button->objectName() == QLatin1String("listModeButton")) {
                button->setIcon(QIcon(QStringLiteral(":/Resources/icon/menu-48.png")));
            } else if (button->objectName() == QLatin1String("detailModeButton")) {
                button->setIcon(QIcon(QStringLiteral(":/Resources/icon/list-48.png")));
            }
            button->style()->unpolish(button);
            button->style()->polish(button);
        }
    }

    if (auto* itemView = _fileDialog->findChild<QTreeView*>("treeView")) {
        itemView->setProperty("fileDialogRole", "itemView");
        itemView->style()->unpolish(itemView);
        itemView->style()->polish(itemView);
#if defined(Q_OS_MACOS)
        // macOS can provide an empty first QFileDialog header. Keep Windows'
        // native header, whose model-specific state must remain intact.
        if (QHeaderView* header = itemView->header()) {
            auto* centeredHeader = new FileDialogHeaderView(itemView);
            centeredHeader->setModel(itemView->model());
            centeredHeader->setProperty("fileDialogRole", "itemHeader");
            centeredHeader->setSectionsClickable(header->sectionsClickable());
            centeredHeader->setSectionsMovable(header->sectionsMovable());
            centeredHeader->setStretchLastSection(header->stretchLastSection());
            centeredHeader->setMinimumSectionSize(header->minimumSectionSize());
            centeredHeader->setDefaultSectionSize(header->defaultSectionSize());
            centeredHeader->setSortIndicatorShown(header->isSortIndicatorShown());
            centeredHeader->setSortIndicator(header->sortIndicatorSection(), header->sortIndicatorOrder());
            centeredHeader->restoreState(header->saveState());
            itemView->setHeader(centeredHeader);
            centeredHeader->style()->unpolish(centeredHeader);
            centeredHeader->style()->polish(centeredHeader);
        }
#endif
    }

    if (auto* itemView = _fileDialog->findChild<QListView*>("listView")) {
        itemView->setProperty("fileDialogRole", "itemView");
        itemView->style()->unpolish(itemView);
        itemView->style()->polish(itemView);
    }

    #if defined(Q_OS_MACOS)
    _buttonBox = _fileDialog->findChild<QDialogButtonBox*>("buttonBox");
    _fileNameEdit = _fileDialog->findChild<QLineEdit*>("fileNameEdit");
    _fileTypeComboBox = _fileDialog->findChild<QComboBox*>("fileTypeCombo");
    if (_buttonBox) {
        _acceptButton = _buttonBox->button(QDialogButtonBox::Open);
        if (!_acceptButton) {
            _acceptButton = _buttonBox->button(QDialogButtonBox::Save);
        }
        _cancelButton = _buttonBox->button(QDialogButtonBox::Cancel);
        _buttonBox->installEventFilter(this);
    }
    #endif

    contentLayout()->addWidget(_fileDialog);
    connect(_fileDialog, &QFileDialog::accepted, this, &ThemedFileDialog::handleAccepted);
    connect(_fileDialog, &QFileDialog::rejected, this, &QDialog::reject);

    // QFileDialog's size hint expands with the current folder's view state.
    // Open every new dialog at a stable working size instead.
    resize(720, 520);
    QTimer::singleShot(0, this, [this] {
        applyBottomCornerMask();
        scheduleActionButtonAlignment();
    });
}

void ThemedFileDialog::handleAccepted()
{
    if (_mode == Mode::Save)
    {
        // QFileDialog hides its embedded widget before emitting accepted().
        // Restore it before opening the modal overwrite question so the
        // themed dialog keeps its file-list content visible behind the box.
        _fileDialog->show();
        _fileDialog->raise();

        const QString selectedFile = _fileDialog->selectedFiles().value(0);
        if (!selectedFile.isEmpty() && QFileInfo::exists(selectedFile))
        {
            const QString message = tr("%1 already exists.\nDo you want to replace it?")
                .arg(QFileInfo(selectedFile).fileName());
            if (!ThemedMessageBox::question(this, tr("Confirm Overwrite"), message))
            {
                return;
            }
        }
    }

    accept();
}

bool ThemedFileDialog::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == _fileDialog && event->type() == QEvent::Resize) {
        applyBottomCornerMask();
        scheduleActionButtonAlignment();
    }

    if (watched == _buttonBox
        && (event->type() == QEvent::Resize
            || event->type() == QEvent::Show
            || event->type() == QEvent::LayoutRequest)) {
        scheduleActionButtonAlignment();
    }

    if (_lookInComboBox && _lookInComboBox->lineEdit() && _lookInComboBox->lineEdit()->completer()
        && watched == _lookInComboBox->lineEdit()->completer()->popup()
        && event->type() == QEvent::KeyPress
        && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Tab) {
        completeLookInPath();
        return true;
    }

    if (_lookInComboBox && watched == _lookInComboBox->lineEdit()) {
        if (event->type() == QEvent::KeyPress) {
            const int key = static_cast<QKeyEvent*>(event)->key();
            // While a path is being typed, Tab completes it instead of leaving the field.
            // With no matching folder it moves focus, which abandons the path as any other Tab does.
            if (key == Qt::Key_Tab && !_typedLookInPath.isNull() && completeLookInPath()) {
                return true;
            }
            // Consume Return so it does not also reach the dialog's default button.
            if (key == Qt::Key_Return || key == Qt::Key_Enter) {
                openTypedLookInPath();
                return true;
            }
            // The first Escape restores the current path; the next one cancels the dialog.
            if (key == Qt::Key_Escape && !_typedLookInPath.isNull()) {
                syncLookInPath();
                return true;
            }
        } else if (event->type() == QEvent::FocusOut) {
            // Moving to another control abandons an unopened path. Window activation changes,
            // including the refusal box and popups, can arrive after the box closes; they keep
            // the typed text for correction.
            const Qt::FocusReason reason = static_cast<QFocusEvent*>(event)->reason();
            const bool userLeft = reason == Qt::TabFocusReason || reason == Qt::BacktabFocusReason
                || reason == Qt::MouseFocusReason || reason == Qt::ShortcutFocusReason;
            if (userLeft && !_refusingLookInPath) {
                syncLookInPath();
            }
        }
    }

    if (watched->property("fileDialogRole") == QLatin1String("sidebarHandle")
        && event->type() == QEvent::Paint) {
        auto* handle = static_cast<QWidget*>(watched);
        QPainter painter(handle);
        painter.fillRect(handle->rect(), _fileDialog->palette().base());
        return true;
    }

    return ThemedDialog::eventFilter(watched, event);
}

void ThemedFileDialog::syncLookInPath()
{
    QLineEdit* pathEdit = _lookInComboBox ? _lookInComboBox->lineEdit() : nullptr;
    if (!pathEdit) {
        return;
    }

    _typedLookInPath = QString();
    // Windows' "My Computer" root has no file-system path; keep Qt's own label there.
    const QString current = _fileDialog->directory().path();
    const QString path = QDir::isRelativePath(current) ? pathEdit->text() : QDir::toNativeSeparators(current);
    if (pathEdit->text() != path) {
        pathEdit->setText(path);
    }
    pathEdit->setModified(false);
}

void ThemedFileDialog::restoreLookInPath()
{
    QLineEdit* pathEdit = _lookInComboBox ? _lookInComboBox->lineEdit() : nullptr;
    if (!pathEdit) {
        return;
    }
    if (_typedLookInPath.isNull()) {
        syncLookInPath();
        return;
    }
    if (pathEdit->text() != _typedLookInPath) {
        pathEdit->setText(_typedLookInPath);
        pathEdit->setModified(true);
    }
}

void ThemedFileDialog::openTypedLookInPath()
{
    QLineEdit* pathEdit = _lookInComboBox ? _lookInComboBox->lineEdit() : nullptr;
    if (!pathEdit) {
        return;
    }

    // QCompleter passes Return to the field before it reports the chosen suggestion.
    // Opening the typed text now would reset the suggestions and turn that choice stale.
    if (QCompleter* completer = pathEdit->completer(); completer && completer->popup()->isVisible()) {
        if (completer->popup()->currentIndex().isValid()) {
            return;
        }
        completer->popup()->hide();
    }

    // Prefer the recorded path: a chosen suggestion is recorded before the field shows it.
    const QString typed = (_typedLookInPath.isNull() ? pathEdit->text() : _typedLookInPath).trimmed();
    QString path = QDir::fromNativeSeparators(typed);
    if (path == QLatin1String("~") || path.startsWith(QLatin1String("~/"))) {
        path.replace(0, 1, QDir::homePath());
    }
    if (path.isEmpty()) {
        syncLookInPath();
        return;
    }

    // A relative path is resolved from the folder currently shown.
    const QFileInfo target(QDir::cleanPath(_fileDialog->directory().absoluteFilePath(path)));
    if (!target.isDir()) {
        _typedLookInPath = typed;
        _refusingLookInPath = true;
        ThemedMessageBox::warning(this, tr("Folder Not Found"),
            tr("%1\nThis is not an existing folder. Check the path and try again.").arg(typed));
        _refusingLookInPath = false;
        restoreLookInPath();
        pathEdit->setFocus();
        pathEdit->selectAll();
        return;
    }

    _fileDialog->setDirectory(target.absoluteFilePath());
    syncLookInPath();
}

bool ThemedFileDialog::completeLookInPath()
{
    QLineEdit* pathEdit = _lookInComboBox ? _lookInComboBox->lineEdit() : nullptr;
    QCompleter* completer = pathEdit ? pathEdit->completer() : nullptr;
    if (!completer) {
        return false;
    }

    // The folder model matches absolute paths only. Complete a relative or "~" path from the
    // absolute folder it names, as Return would open it, and keep the form the user typed.
    const QString typed = pathEdit->text();
    QString prefix = typed;
    QString expanded = QDir::fromNativeSeparators(typed);
    if (expanded == QLatin1String("~") || expanded.startsWith(QLatin1String("~/"))) {
        expanded.replace(0, 1, QDir::homePath());
    }
    if (expanded != QDir::fromNativeSeparators(typed) || QDir::isRelativePath(expanded)) {
        const bool folder = expanded.endsWith(QLatin1Char('/'));
        prefix = QDir::cleanPath(_fileDialog->directory().absoluteFilePath(expanded));
        if (folder && !prefix.endsWith(QLatin1Char('/'))) {
            prefix += QLatin1Char('/');
        }
        prefix = QDir::toNativeSeparators(prefix);
    }
    const auto inTypedForm = [&](const QString& completion) {
        return prefix == typed ? completion : typed + completion.mid(prefix.size());
    };

    QAbstractItemView* popup = completer->popup();
    const bool chosen = popup->isVisible() && popup->currentIndex().isValid();
    const int chosenRow = chosen ? popup->currentIndex().row() : 0;
    completer->setCompletionPrefix(prefix);
    const int count = completer->completionCount();
    if (count == 0) {
        return false;
    }

    QString completion;
    if (chosen || count == 1) {
        completer->setCurrentRow(qMin(chosenRow, count - 1));
        completion = completer->currentCompletion();
        if (!completion.endsWith(QDir::separator()) && !completion.endsWith(QLatin1Char('/'))) {
            completion += QDir::separator();
        }
    } else {
        // Extend to the prefix every suggestion shares, as a shell does.
        completer->setCurrentRow(0);
        completion = completer->currentCompletion();
        for (int row = 1; row < count && !completion.isEmpty(); ++row) {
            completer->setCurrentRow(row);
            const QString candidate = completer->currentCompletion();
            int shared = 0;
            while (shared < completion.size() && shared < candidate.size()
                   && QString::compare(completion.mid(shared, 1), candidate.mid(shared, 1), completer->caseSensitivity()) == 0) {
                ++shared;
            }
            completion.truncate(shared);
        }
        if (completion.size() <= prefix.size()) {
            // Nothing more is shared: select the first suggestion for the next Tab.
            completer->complete();
            popup->setCurrentIndex(completer->completionModel()->index(0, 0));
            return true;
        }
    }

    // Record the completion first: the text change restores whatever is recorded.
    _typedLookInPath = inTypedForm(completion);
    pathEdit->setText(_typedLookInPath);
    pathEdit->setModified(true);
    // Offer the next level, or close the popup when the folder has no subfolders.
    completer->setCompletionPrefix(completion);
    completer->complete();
    if (completer->completionCount() == 0) {
        popup->hide();
    }
    return true;
}

void ThemedFileDialog::applyBottomCornerMask()
{
    if (!_fileDialog || _fileDialog->size().isEmpty()) {
        return;
    }

    constexpr qreal cornerRadius = 12.0;
    const int width = _fileDialog->width();
    const int height = _fileDialog->height();

    QBitmap mask(_fileDialog->size());
    mask.fill(Qt::color0);

    QPainter painter(&mask);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setBrush(Qt::color1);
    painter.setPen(Qt::NoPen);

    QPainterPath path;
    path.moveTo(0, 0);
    path.lineTo(width, 0);
    path.lineTo(width, height - cornerRadius);
    path.arcTo(QRectF(width - (2.0 * cornerRadius), height - (2.0 * cornerRadius), 2.0 * cornerRadius, 2.0 * cornerRadius), 0, -90);
    path.lineTo(cornerRadius, height);
    path.arcTo(QRectF(0, height - (2.0 * cornerRadius), 2.0 * cornerRadius, 2.0 * cornerRadius), -90, -90);
    path.closeSubpath();
    painter.drawPath(path);

    _fileDialog->setMask(QRegion(mask));
}

void ThemedFileDialog::scheduleActionButtonAlignment()
{
    if (_actionButtonAlignmentQueued || !_buttonBox || !_fileNameEdit || !_fileTypeComboBox) {
        return;
    }

    _actionButtonAlignmentQueued = true;
    QTimer::singleShot(0, this, [this] {
        _actionButtonAlignmentQueued = false;
        alignActionButtons();
    });
}

void ThemedFileDialog::alignActionButtons()
{
    if (!_buttonBox || !_fileNameEdit || !_fileTypeComboBox) {
        return;
    }

    const auto alignTo = [this](QPushButton* button, QWidget* rowWidget) {
        if (!button || !rowWidget) {
            return;
        }

        const QPoint rowCenter = _buttonBox->mapFromGlobal(rowWidget->mapToGlobal(rowWidget->rect().center()));
        button->move(
            qMax(0, _buttonBox->width() - button->width()),
            rowCenter.y() - (button->height() / 2));
    };

    alignTo(_acceptButton, _fileNameEdit);
    alignTo(_cancelButton, _fileTypeComboBox);
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
    ThemedFileDialog dialog(parent, title, directory, {}, Mode::Directory);
    return dialog.exec() == QDialog::Accepted ? dialog._fileDialog->selectedFiles() : QStringList{};
}

QString ThemedFileDialog::getExistingDirectory(
    QWidget* parent,
    const QString& title,
    const QString& directory)
{
    ThemedFileDialog dialog(parent, title, directory, {}, Mode::Directory);
    return dialog.exec() == QDialog::Accepted ? dialog._fileDialog->selectedFiles().value(0) : QString{};
}

QString ThemedFileDialog::getSaveFileName(
    QWidget* parent,
    const QString& title,
    const QString& filePath,
    const QString& filter,
    QString* selectedFilter)
{
    ThemedFileDialog dialog(parent, title, filePath, filter, Mode::Save);
    if (dialog.exec() != QDialog::Accepted)
    {
        return {};
    }
    if (selectedFilter)
    {
        *selectedFilter = dialog._fileDialog->selectedNameFilter();
    }
    return dialog._fileDialog->selectedFiles().value(0);
}
