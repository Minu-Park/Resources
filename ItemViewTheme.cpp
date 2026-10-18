#include "ItemViewTheme.h"
#include <QAbstractItemView>
#include <QApplication>
#include <QCalendarWidget>
#include <QComboBox>
#include <QCursor>
#include <QHeaderView>
#include <QHelpEvent>
#include <QItemSelectionModel>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QPersistentModelIndex>
#include <QSet>
#include <QStyledItemDelegate>
#include <QTableView>

/** @brief Receives shared surface metrics from QSS without changing the consumer layout. */
class ResourcesItemSelectionSurface final : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal radius MEMBER radius)
    Q_PROPERTY(QColor hoverColor MEMBER hoverColor)
    Q_PROPERTY(QColor focusColor MEMBER focusColor)
  public:
    explicit ResourcesItemSelectionSurface(QWidget *parent) : QWidget(parent)
    {
        setObjectName(QStringLiteral("ResourcesItemSelectionSurface"));
        hide();
    }
    qreal radius = 0;
    QColor hoverColor;
    QColor focusColor;
};

namespace
{
/** @brief Adds adjoining selection corners around an existing delegate, preserving its contracts. */
class ItemSurfaceDelegate final : public QStyledItemDelegate
{
  public:
    ItemSurfaceDelegate(QAbstractItemView *view, QAbstractItemDelegate *source)
        : QStyledItemDelegate(view), view_(view), source_(source),
          surface_(new ResourcesItemSelectionSurface(view))
    {
        surface_->ensurePolished();
        observeSelection();
        view_->viewport()->setMouseTracking(true);
        view_->viewport()->setAttribute(Qt::WA_Hover);
        view_->viewport()->installEventFilter(this);
        connect(source, &QAbstractItemDelegate::commitData, this, &QAbstractItemDelegate::commitData);
        connect(source, &QAbstractItemDelegate::closeEditor, this,
                [this](QWidget *editor, QAbstractItemDelegate::EndEditHint hint)
                {
                    // The view removes our filter on close; remove the forwarded source filter too.
                    if (forwardedEditors_.remove(editor))
                        editor->removeEventFilter(source_);
                    emit closeEditor(editor, hint);
                });
        connect(source, &QAbstractItemDelegate::sizeHintChanged, this,
                &QAbstractItemDelegate::sizeHintChanged);
    }
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        if (!source_)
            return;
        observeSelection();
        const bool selected = option.state & QStyle::State_Selected;
        const bool rowMode = view_->selectionBehavior() == QAbstractItemView::SelectRows;
        const QPoint cursor = view_->viewport()->mapFromGlobal(QCursor::pos());
        const QModelIndex hoverIndex = view_->viewport()->rect().contains(cursor)
                                           ? view_->indexAt(cursor) : QModelIndex{};
        const bool mouseOver = rowMode ? hoverIndex.isValid() && hoverIndex.row() == index.row() &&
                                            hoverIndex.parent() == index.parent()
                                      : bool(option.state & QStyle::State_MouseOver);
        const bool hovered = view_->selectionMode() != QAbstractItemView::NoSelection && mouseOver &&
                             (option.state & QStyle::State_Enabled);
        if (selected || hovered)
        {
            const QRect rect = view_->visualRect(index);
            const bool rowHover = hovered && view_->selectionBehavior() == QAbstractItemView::SelectRows;
            const auto adjacent = [&](const QPoint &point, bool horizontal)
            {
                const QModelIndex next = view_->indexAt(point);
                if (!next.isValid() || next == index)
                    return false;
                if (selected ? !view_->selectionModel()->isSelected(next)
                             : !rowHover || next.parent() != index.parent() || next.row() != index.row())
                    return false;
                const QRect neighbor = view_->visualRect(next);
                // Join only touching visual edges, including moved and hidden sections.
                return horizontal ? neighbor.top() == rect.top() && neighbor.bottom() == rect.bottom()
                                  : neighbor.left() <= rect.left() && neighbor.right() >= rect.right();
            };
            const bool left =
                (selected || rowHover) && adjacent(QPoint(rect.left() - 2, rect.center().y()), true);
            const bool right =
                (selected || rowHover) && adjacent(QPoint(rect.right() + 2, rect.center().y()), true);
            const bool up =
                (selected || rowHover) && adjacent(QPoint(rect.center().x(), rect.top() - 2), false);
            const bool down =
                (selected || rowHover) && adjacent(QPoint(rect.center().x(), rect.bottom() + 2), false);
            const qreal radius = qMin(surface_->radius, qMin(rect.width(), rect.height()) / 2.0);
            const qreal tl = !left && !up ? radius : 0, tr = !right && !up ? radius : 0;
            const qreal bl = !left && !down ? radius : 0, br = !right && !down ? radius : 0;
            // Cover the shared raster edge at fractional DPR; only exterior edges are antialiased.
            const qreal x = rect.left() - (left ? 1 : 0), y = rect.top() - (up ? 1 : 0);
            const qreal x2 = rect.left() + rect.width() + (right ? 1 : 0),
                        y2 = rect.top() + rect.height() + (down ? 1 : 0);
            QPainterPath path;
            path.moveTo(x + tl, y);
            path.lineTo(x2 - tr, y);
            if (tr)
                path.arcTo(QRectF(x2 - 2 * tr, y, 2 * tr, 2 * tr), 90, -90);
            else
                path.lineTo(x2, y);
            path.lineTo(x2, y2 - br);
            if (br)
                path.arcTo(QRectF(x2 - 2 * br, y2 - 2 * br, 2 * br, 2 * br), 0, -90);
            else
                path.lineTo(x2, y2);
            path.lineTo(x + bl, y2);
            if (bl)
                path.arcTo(QRectF(x, y2 - 2 * bl, 2 * bl, 2 * bl), 270, -90);
            else
                path.lineTo(x, y2);
            path.lineTo(x, y + tl);
            if (tl)
                path.arcTo(QRectF(x, y, 2 * tl, 2 * tl), 180, -90);
            else
                path.lineTo(x, y);
            path.closeSubpath();
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            painter->setPen(Qt::NoPen);
            painter->setBrush(selected ? option.palette.highlight() : QBrush(surface_->hoverColor));
            painter->drawPath(path);
            painter->restore();
        }
        auto contents = option;
        if (selected)
            contents.features &= ~QStyleOptionViewItem::Alternate;
        contents.state &= ~(QStyle::State_Selected | QStyle::State_MouseOver | QStyle::State_HasFocus);
        source_->paint(painter, contents, index);
        if (view_->selectionMode() != QAbstractItemView::NoSelection &&
            (option.state & QStyle::State_HasFocus) && view_->hasFocus())
        {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            painter->setPen(QPen(surface_->focusColor, 1));
            painter->setBrush(Qt::NoBrush);
            painter->drawRoundedRect(QRectF(view_->visualRect(index)).adjusted(.5, .5, -.5, -.5),
                                     surface_->radius, surface_->radius);
            painter->restore();
        }
    }
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        return source_ ? source_->sizeHint(option, index) : QStyledItemDelegate::sizeHint(option, index);
    }
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override
    {
        QWidget *editor = source_ ? source_->createEditor(parent, option, index) : nullptr;
        // An outer consumer delegate owns its editor filter when it calls through us.
        if (editor && view_->itemDelegateForIndex(index) == this)
        {
            editor->installEventFilter(source_);
            forwardedEditors_.insert(editor);
            connect(editor, &QObject::destroyed, this,
                    [this, editor] { forwardedEditors_.remove(editor); });
        }
        return editor;
    }
    void destroyEditor(QWidget *editor, const QModelIndex &index) const override
    {
        if (forwardedEditors_.remove(editor))
            editor->removeEventFilter(source_);
        if (source_)
            source_->destroyEditor(editor, index);
        else
            QStyledItemDelegate::destroyEditor(editor, index);
    }
    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        if (source_)
            source_->setEditorData(editor, index);
    }
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override
    {
        if (source_)
            source_->setModelData(editor, model, index);
    }
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const override
    {
        if (source_)
            source_->updateEditorGeometry(editor, option, index);
    }
    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option,
                     const QModelIndex &index) override
    {
        return source_ && source_->editorEvent(event, model, option, index);
    }
    bool helpEvent(QHelpEvent *event, QAbstractItemView *view, const QStyleOptionViewItem &option,
                   const QModelIndex &index) override
    {
        return source_ && source_->helpEvent(event, view, option, index);
    }

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        // Qt tables invalidate only the hovered cell; joined row feedback needs both rows repainted.
        if (watched == view_->viewport() &&
            view_->selectionBehavior() == QAbstractItemView::SelectRows &&
            (event->type() == QEvent::MouseMove || event->type() == QEvent::HoverMove ||
             event->type() == QEvent::Leave || event->type() == QEvent::HoverLeave))
        {
            const QPoint cursor = view_->viewport()->mapFromGlobal(QCursor::pos());
            const auto next = event->type() == QEvent::Leave || event->type() == QEvent::HoverLeave ||
                                      !view_->viewport()->rect().contains(cursor)
                                  ? QModelIndex{} : view_->indexAt(cursor).siblingAtColumn(0);
            if (hoveredRow_ != next)
            {
                hoveredRow_ = next;
                view_->viewport()->update();
            }
        }
        return false;
    } // The source owns its editor event policy.
  private:
    /** @brief Repaints adjoining corners when a selection changes or its model is replaced. */
    void observeSelection() const
    {
        if (selection_ == view_->selectionModel())
            return;
        QObject::disconnect(selectionConnection_);
        selection_ = view_->selectionModel();
        if (selection_)
            selectionConnection_ = QObject::connect(selection_, &QItemSelectionModel::selectionChanged,
                                                    view_->viewport(), qOverload<>(&QWidget::update));
    }
    mutable QPointer<QItemSelectionModel> selection_;
    mutable QMetaObject::Connection selectionConnection_;
    mutable QSet<QWidget *> forwardedEditors_;
    QPersistentModelIndex hoveredRow_;
    QAbstractItemView *view_;
    QPointer<QAbstractItemDelegate> source_;
    ResourcesItemSelectionSurface *surface_;
};
} // namespace

void Resources::applyItemViewTheme(QAbstractItemView *view)
{
    if (!view || qobject_cast<QHeaderView *>(view) || !view->itemDelegate() ||
        dynamic_cast<ItemSurfaceDelegate *>(view->itemDelegate()))
        return;
    for (QWidget *ancestor = view; ancestor; ancestor = ancestor->parentWidget())
    {
        if (qobject_cast<QComboBox *>(ancestor) || qobject_cast<QCalendarWidget *>(ancestor) ||
            ancestor->objectName() == QLatin1String("AutoCompletePopup"))
            return;
    }
    auto *source = view->itemDelegate();
    view->setItemDelegate(new ItemSurfaceDelegate(view, source));
    // Qt may retain direct connections to a previous delegate; forwarding must be the sole route.
    QObject::disconnect(source, &QAbstractItemDelegate::commitData, view, nullptr);
    QObject::disconnect(source, &QAbstractItemDelegate::closeEditor, view, nullptr);
    QObject::disconnect(source, &QAbstractItemDelegate::sizeHintChanged, view, nullptr);
    if (auto *table = qobject_cast<QTableView *>(view))
    {
        // Leave consumer row sizes and resize modes intact; change only Qt's untouched default.
        auto *header = table->verticalHeader();
        if (header->defaultSectionSize() ==
            header->style()->pixelMetric(QStyle::PM_HeaderDefaultSectionSizeVertical, nullptr, header))
            header->setDefaultSectionSize(qMax(30, view->fontMetrics().height() + 8));
    }
}
#include "ItemViewTheme.moc"
