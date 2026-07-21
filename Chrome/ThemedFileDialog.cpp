#include "Chrome/ThemedFileDialog.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHeaderView>
#include <QIcon>
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
        option.textAlignment |= Qt::AlignVCenter;
        style()->drawControl(QStyle::CE_Header, &option, painter, this);
    }
};

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
            button->setIconSize(QSize(16, 16));
            if (button->objectName() == QLatin1String("backButton")) {
                button->setIcon(QIcon(QStringLiteral(":/Resources/Icons/icons8-back-48.png")));
            } else if (button->objectName() == QLatin1String("forwardButton")) {
                button->setIcon(QIcon(QStringLiteral(":/Resources/Icons/icons8-forward-48.png")));
            } else if (button->objectName() == QLatin1String("toParentButton")) {
                button->setIcon(QIcon(QStringLiteral(":/Resources/Icons/icons8-collapse-arrow-48.png")));
            } else if (button->objectName() == QLatin1String("newFolderButton")) {
                button->setIcon(QIcon(QStringLiteral(":/Resources/Icons/icons8-new-folder-48.png")));
            } else if (button->objectName() == QLatin1String("listModeButton")) {
                button->setIcon(QIcon(QStringLiteral(":/Resources/Icons/icons8-menu-48.png")));
            } else if (button->objectName() == QLatin1String("detailModeButton")) {
                button->setIcon(QIcon(QStringLiteral(":/Resources/Icons/icons8-list-48.png")));
            }
            button->style()->unpolish(button);
            button->style()->polish(button);
        }
    }

    if (auto* itemView = _fileDialog->findChild<QTreeView*>("treeView")) {
        QHeaderView* originalHeader = itemView->header();
        auto* header = new FileDialogHeaderView(itemView);
        header->setProperty("fileDialogRole", "itemHeader");
        header->setSectionsClickable(originalHeader->sectionsClickable());
        header->setSectionsMovable(originalHeader->sectionsMovable());
        header->setStretchLastSection(originalHeader->stretchLastSection());
        header->setMinimumSectionSize(originalHeader->minimumSectionSize());
        header->setDefaultSectionSize(originalHeader->defaultSectionSize());
        header->setSortIndicatorShown(originalHeader->isSortIndicatorShown());
        header->setSortIndicator(originalHeader->sortIndicatorSection(), originalHeader->sortIndicatorOrder());
        header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        header->restoreState(originalHeader->saveState());
        itemView->setHeader(header);
        header->style()->unpolish(header);
        header->style()->polish(header);
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
    connect(_fileDialog, &QFileDialog::accepted, this, &QDialog::accept);
    connect(_fileDialog, &QFileDialog::rejected, this, &QDialog::reject);

    resize(sizeHint().expandedTo(QSize(720, 520)));
    QTimer::singleShot(0, this, [this] {
        applyBottomCornerMask();
        scheduleActionButtonAlignment();
    });
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

    if (watched->property("fileDialogRole") == QLatin1String("sidebarHandle")
        && event->type() == QEvent::Paint) {
        auto* handle = static_cast<QWidget*>(watched);
        QPainter painter(handle);
        painter.fillRect(handle->rect(), _fileDialog->palette().base());
        return true;
    }

    return ThemedDialog::eventFilter(watched, event);
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
