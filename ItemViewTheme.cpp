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
#include <QScrollBar>
#include <QSet>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTreeView>
#include <QLabel>

/** @brief Receives shared surface metrics from QSS without changing the consumer layout. */
class ResourcesItemSelectionSurface final : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal radius MEMBER radius)
    Q_PROPERTY(QColor hoverColor MEMBER hoverColor)
    Q_PROPERTY(QColor currentColor MEMBER currentColor)
    Q_PROPERTY(QColor focusColor MEMBER focusColor)
    Q_PROPERTY(QColor accentColor MEMBER accentColor)
    Q_PROPERTY(int emphasisWeight MEMBER emphasisWeight)
  public:
    explicit ResourcesItemSelectionSurface(QWidget *parent) : QWidget(parent)
    {
        setObjectName(QStringLiteral("ResourcesItemSelectionSurface"));
        hide();
    }
    qreal radius = 0;
    QColor hoverColor;
    QColor currentColor;
    QColor focusColor;
    QColor accentColor;
    int emphasisWeight = QFont::DemiBold;
};

namespace
{
/** @brief MenuInk identity ink; QMenu keeps its own accented hover path. */
enum class MenuInkState { Normal, Hover, Active };

/** @brief Uses the same neutral-glyph recoloring as QMenu, retaining multicolor artwork. */
QIcon menuInkIcon(const QIcon& source, const QSize& size, qreal dpr, QStyle* style, bool accented)
{
    if (!accented || source.isNull()) return source;
    const QPixmap original = source.pixmap(size, dpr, QIcon::Normal);
    const QPixmap ink = style->generatedIconPixmap(QIcon::Active, original, nullptr);
    QIcon result(ink);
    result.addPixmap(ink, QIcon::Active);
    result.addPixmap(ink, QIcon::Selected);
    return result;
}

/** @brief Projects shared row ink onto explicitly marked title/icon labels in index widgets. */
void syncMenuItemWidget(QWidget* widget, MenuInkState ink, const QColor& accent)
{
    if (!widget) return;
    const QString state = ink == MenuInkState::Active ? QStringLiteral("active")
                        : ink == MenuInkState::Hover ? QStringLiteral("hover")
                                                     : QStringLiteral("normal");
    const bool accented = ink == MenuInkState::Active;
    const bool changed = widget->property("itemInkState") != state;
    if (changed) widget->setProperty("itemInkState", state);
    for (auto* label : widget->findChildren<QLabel*>()) {
        if (changed && label->property("itemRole") == QStringLiteral("title")) {
            label->style()->unpolish(label);
            label->style()->polish(label);
            label->update();
        } else if (label->property("itemRole") == QStringLiteral("icon")) {
            const QIcon source = qvariant_cast<QIcon>(label->property("itemIcon"));
            const QSize size = label->property("itemIconSize").toSize();
            const QString key = QStringLiteral("%1:%2:%3:%4:%5x%6")
                .arg(source.cacheKey()).arg(accent.rgba()).arg(int(ink)).arg(label->devicePixelRatioF())
                .arg(size.width()).arg(size.height());
            if (label->property("_resourcesMenuIcon") != key) {
                label->setPixmap(menuInkIcon(source, size, label->devicePixelRatioF(), label->style(), accented)
                                     .pixmap(size, label->devicePixelRatioF()));
                label->setProperty("_resourcesMenuIcon", key);
            }
        }
    }
}

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
        const QWidget* pointerTarget = QApplication::widgetAt(QCursor::pos());
        const bool overIdentityWidget = view_->property("menuItemInk").toBool() && pointerTarget &&
                                       view_->viewport()->isAncestorOf(pointerTarget);
        const QModelIndex hoverIndex = (view_->viewport()->underMouse() || overIdentityWidget) &&
                                               view_->viewport()->rect().contains(cursor)
                                           ? view_->indexAt(cursor) : QModelIndex{};
        const bool mouseOver = rowMode ? hoverIndex.isValid() && hoverIndex.row() == index.row() &&
                                            hoverIndex.parent() == index.parent()
                                      : bool(option.state & QStyle::State_MouseOver);
        const bool hovered = view_->selectionMode() != QAbstractItemView::NoSelection && mouseOver &&
                             (option.state & QStyle::State_Enabled);
        if (view_->property("itemHoverBold").toBool()) {
            auto contents = option;
            contents.state &= ~(QStyle::State_Selected | QStyle::State_MouseOver | QStyle::State_HasFocus);
            if (!hovered) {
                source_->paint(painter, contents, index);
                return;
            }
            initStyleOption(&contents, index);
            contents.font = Resources::itemHoverBoldFont(contents.font);
            contents.fontMetrics = QFontMetrics(contents.font);
            painter->save();
            painter->setFont(contents.font);
            view_->style()->drawControl(QStyle::CE_ItemViewItem, &contents, painter, view_);
            painter->restore();
            return;
        }
        if (view_->property("menuItemInk").toBool()) {
            QStyleOptionViewItem contents(option);
            initStyleOption(&contents, index);
            const bool enabled = option.state & QStyle::State_Enabled;
            // Selection keeps accented ink; hover is neutral semibold. QMenu stays accented on hover.
            // Preserve selected ink while the view is disabled during a pending request.
            // A consumer-marked identity (connected or otherwise current) outranks hover so it
            // never loses its accent under the pointer.
            const bool emphasizedItem = index.data(Resources::EmphasizedItemInkRole).toBool();
            const MenuInkState ink = selected ? MenuInkState::Active
                                   : emphasizedItem ? MenuInkState::Active
                                   : !enabled ? MenuInkState::Normal
                                   : hovered ? MenuInkState::Hover
                                             : MenuInkState::Normal;
            contents.state &= ~(QStyle::State_Selected | QStyle::State_MouseOver | QStyle::State_HasFocus);
            contents.features &= ~QStyleOptionViewItem::Alternate;
            contents.backgroundBrush = Qt::NoBrush;
            // Row selection shares emphasis across cells; source value delegates keep their contracts.
            if ((rowMode || index.column() == 0) && ink != MenuInkState::Normal) {
                contents.font.setWeight(QFont::Weight(surface_->emphasisWeight));
                contents.fontMetrics = QFontMetrics(contents.font);
                if (ink == MenuInkState::Active) {
                    contents.palette.setColor(QPalette::Text, surface_->accentColor);
                    contents.palette.setColor(QPalette::HighlightedText, surface_->accentColor);
                    contents.icon = menuInkIcon(contents.icon, contents.decorationSize,
                                                 view_->devicePixelRatioF(), view_->style(), true);
                }
            }
            QWidget* identity = view_->indexWidget(index);
            if (identity) {
                observeIdentityWidget(identity);
                for (auto* child : identity->findChildren<QWidget*>()) observeIdentityWidget(child);
            }
            syncMenuItemWidget(identity, ink, surface_->accentColor);
            // Identity painting is MenuInk-owned; value delegates and editors stay consumer-owned.
            if (index.column() == 0)
                view_->style()->drawControl(QStyle::CE_ItemViewItem, &contents, painter, view_);
            else
                source_->paint(painter, contents, index);
            return;
        }
        const bool showFill = !index.data(Resources::SuppressItemInteractionFillRole).toBool();
        if (showFill && (selected || hovered))
        {
            const QRect rect = view_->visualRect(index);
            const bool rowHover = hovered && view_->selectionBehavior() == QAbstractItemView::SelectRows;
            const auto adjacent = [&](const QPoint &point, bool horizontal)
            {
                const QModelIndex next = view_->indexAt(point);
                if (!next.isValid() || next == index
                    || next.data(Resources::SuppressItemInteractionFillRole).toBool())
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
            // Seam overlap belongs to this cell's raster clip. Otherwise a later
            // neighbor can erase the current capsule's right or bottom curve.
            painter->setClipRect(rect, Qt::IntersectClip);
            painter->setRenderHint(QPainter::Antialiasing);
            painter->setPen(Qt::NoPen);
            painter->setBrush(selected ? option.palette.highlight() : QBrush(surface_->hoverColor));
            painter->drawPath(path);
            painter->restore();
        }
        auto contents = option;
        const bool current = index == view_->currentIndex() && view_->hasFocus();
        if (showFill && selected && current && !view_->property("joinedSelectionOnly").toBool())
        {
            const QRectF rect(view_->visualRect(index));
            const qreal radius = qMin(surface_->radius, qMin(rect.width(), rect.height()) / 2.0);
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            painter->setPen(Qt::NoPen);
            painter->setBrush(surface_->currentColor);
            painter->drawRoundedRect(rect, radius, radius);
            painter->restore();
        }
        if (selected)
            contents.features &= ~QStyleOptionViewItem::Alternate;
        contents.state &= ~(QStyle::State_Selected | QStyle::State_MouseOver | QStyle::State_HasFocus);
        // Row content delegates suppress native backgrounds themselves, retaining indicator feedback.
        if (source_->property("_resourcesOwnsItemBackground").toBool()
            && view_->property("interactionMode").toString() == QLatin1String("row"))
            contents.state.setFlag(QStyle::State_MouseOver, option.state.testFlag(QStyle::State_MouseOver));
        source_->paint(painter, contents, index);
        if (!selected && current)
        {
            // Ctrl navigation can move the keyboard cursor outside the selection.
            const QRectF rect = QRectF(view_->visualRect(index)).adjusted(1, 1, -1, -1);
            const qreal radius = qMin(surface_->radius, qMin(rect.width(), rect.height()) / 2.0);
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            painter->setPen(QPen(surface_->focusColor, 1));
            painter->setBrush(Qt::NoBrush);
            painter->drawRoundedRect(rect, radius, radius);
            painter->restore();
        }
    }
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QSize hint = source_ ? source_->sizeHint(option, index) : QStyledItemDelegate::sizeHint(option, index);
        if (view_->property("menuItemInk").toBool() && index.column() == 0) {
            QStyleOptionViewItem contents(option);
            initStyleOption(&contents, index);
            QFont emphasis(contents.font);
            emphasis.setWeight(QFont::Weight(surface_->emphasisWeight));
            hint.rwidth() += qMax(0, QFontMetrics(emphasis).horizontalAdvance(contents.text)
                                      - QFontMetrics(contents.font).horizontalAdvance(contents.text));
        }
        return hint;
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
        if (identityWidgets_.contains(qobject_cast<QWidget*>(watched)) &&
            (event->type() == QEvent::Enter || event->type() == QEvent::Leave))
            view_->viewport()->update();
        // Qt tables invalidate only the hovered cell; joined row feedback needs both rows repainted.
        if (watched == view_->viewport() &&
            view_->selectionBehavior() == QAbstractItemView::SelectRows &&
            (event->type() == QEvent::HoverEnter || event->type() == QEvent::HoverMove ||
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
    /** @brief Observes card enter/leave without changing consumer mouse tracking or event delivery. */
    void observeIdentityWidget(QWidget* widget) const
    {
        if (identityWidgets_.contains(widget)) return;
        identityWidgets_.insert(widget);
        widget->installEventFilter(const_cast<ItemSurfaceDelegate*>(this));
        connect(widget, &QObject::destroyed, this, [this, widget] { identityWidgets_.remove(widget); });
    }
    /** @brief Repaints adjoining corners when a selection changes or its model is replaced. */
    void observeSelection() const
    {
        if (selection_ == view_->selectionModel())
            return;
        QObject::disconnect(selectionConnection_);
        QObject::disconnect(currentConnection_);
        selection_ = view_->selectionModel();
        if (selection_)
        {
            selectionConnection_ = QObject::connect(selection_, &QItemSelectionModel::selectionChanged,
                                                    view_->viewport(), qOverload<>(&QWidget::update));
            currentConnection_ = QObject::connect(selection_, &QItemSelectionModel::currentChanged,
                                                  view_->viewport(), qOverload<>(&QWidget::update));
        }
    }
    mutable QPointer<QItemSelectionModel> selection_;
    mutable QMetaObject::Connection selectionConnection_;
    mutable QMetaObject::Connection currentConnection_;
    mutable QSet<QWidget *> forwardedEditors_;
    mutable QSet<QWidget *> identityWidgets_;
    QPersistentModelIndex hoveredRow_;
    QAbstractItemView *view_;
    QPointer<QAbstractItemDelegate> source_;
    ResourcesItemSelectionSurface *surface_;
};

/** @brief Joins a visible horizontal header to the vertical scrollbar's top edge. */
class ItemViewHeaderCap final : public QWidget
{
  public:
    ItemViewHeaderCap(QAbstractItemView *view, QHeaderView *header, QScrollBar *scrollBar)
        : QWidget(scrollBar->parentWidget()), view_(view), header_(header), scrollBar_(scrollBar)
    {
        setObjectName(QStringLiteral("ItemViewHeaderCap"));
        setAttribute(Qt::WA_StyledBackground);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setFocusPolicy(Qt::NoFocus);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        header_->installEventFilter(this);
        scrollBar_->installEventFilter(this);
        if (auto *container = scrollBar_->parentWidget())
            container->installEventFilter(this);
        headerDestroyedConnection_ = connect(header_, &QObject::destroyed, this, [this] {
            header_ = nullptr;
            sync();
        });
    }

    void bindHeader(QHeaderView *header)
    {
        if (header_ == header)
            return;
        if (header_) {
            header_->removeEventFilter(this);
            QObject::disconnect(headerDestroyedConnection_);
            setHeaderCapState(false);
        }
        header_ = header;
        if (header_) {
            header_->installEventFilter(this);
            headerDestroyedConnection_ = connect(header_, &QObject::destroyed, this, [this] {
                header_ = nullptr;
                sync();
            });
        }
        sync();
    }

    void sync()
    {
        const bool active = view_ && header_ && header_->isVisible() && header_->height() > 0 &&
                            scrollBar_ && scrollBar_->isVisible();
        setFixedHeight(active ? header_->height() : 0);
        setVisible(active);
        setHeaderCapState(active);
    }

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == header_ &&
            (event->type() == QEvent::Resize || event->type() == QEvent::Show ||
             event->type() == QEvent::Hide))
            sync();
        else if (watched == scrollBar_ &&
                 (event->type() == QEvent::Show || event->type() == QEvent::Hide))
            sync();
        else if (watched == parentWidget() &&
                 (event->type() == QEvent::Show || event->type() == QEvent::Hide))
            sync();
        return false;
    }

  private:
    void setHeaderCapState(bool active)
    {
        if (!header_ || header_->property("headerScrollCap").toBool() == active)
            return;
        header_->setProperty("headerScrollCap", active);
        header_->style()->unpolish(header_);
        header_->style()->polish(header_);
        header_->update();
    }

    QPointer<QAbstractItemView> view_;
    QPointer<QHeaderView> header_;
    QPointer<QScrollBar> scrollBar_;
    QMetaObject::Connection headerDestroyedConnection_;
};

/** @brief Adds one top-aligned cap without widening the header or changing section geometry. */
void applyItemViewHeaderCap(QAbstractItemView *view, QHeaderView *header)
{
    if (!view || !header)
        return;
    auto *scrollBar = view->verticalScrollBar();
    auto *container = scrollBar->parentWidget();
    if (!container)
        return;
    if (auto *existingWidget = container->findChild<QWidget *>(
            QStringLiteral("ItemViewHeaderCap"), Qt::FindDirectChildrenOnly)) {
        if (auto *existing = dynamic_cast<ItemViewHeaderCap *>(existingWidget)) {
            existing->bindHeader(header);
            return;
        }
    }
    auto *cap = new ItemViewHeaderCap(view, header, scrollBar);
    view->addScrollBarWidget(cap, Qt::AlignTop);
    cap->sync();
}

/** @brief Paints a plain combo popup's current value and tracked row without interaction fills. */
class ComboPopupDelegate final : public QStyledItemDelegate
{
    Q_OBJECT
  public:
    ComboPopupDelegate(QAbstractItemView *view, QComboBox *combo, QAbstractItemDelegate *source)
        : QStyledItemDelegate(view), view_(view), combo_(combo), source_(source),
          surface_(new ResourcesItemSelectionSurface(view))
    {
        surface_->ensurePolished();
        observeSelection();
        view_->viewport()->setAttribute(Qt::WA_Hover);
        view_->viewport()->installEventFilter(this);
        connect(source, &QAbstractItemDelegate::commitData, this, &QAbstractItemDelegate::commitData);
        connect(source, &QAbstractItemDelegate::closeEditor, this,
                [this](QWidget *editor, QAbstractItemDelegate::EndEditHint hint)
                {
                    if (forwardedEditors_.remove(editor))
                        editor->removeEventFilter(source_);
                    emit closeEditor(editor, hint);
                });
        connect(source, &QAbstractItemDelegate::sizeHintChanged, this,
                &QAbstractItemDelegate::sizeHintChanged);
        connect(combo_, &QComboBox::currentIndexChanged, view_->viewport(),
                qOverload<>(&QWidget::update));
        connect(combo_, &QComboBox::currentTextChanged, view_->viewport(),
                qOverload<>(&QWidget::update));
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        if (!source_)
            return;
        observeSelection();
        auto contents = option;
        clearInteractionState(contents);
        const bool separator = index.data(Qt::AccessibleDescriptionRole).toString() ==
                               QLatin1String("separator");
        if (separator) {
            source_->paint(painter, contents, index);
            return;
        }
        const bool enabled = (option.state & QStyle::State_Enabled) &&
                             (index.flags() & Qt::ItemIsEnabled);
        if (!enabled) {
            contents.state &= ~QStyle::State_Enabled;
            const QColor disabledInk = QApplication::palette().color(QPalette::Disabled, QPalette::Text);
            contents.palette.setColor(QPalette::Text, disabledInk);
            contents.palette.setColor(QPalette::HighlightedText, disabledInk);
            source_->paint(painter, contents, index);
            return;
        }

        const bool active = combo_ && combo_->currentIndex() >= 0 && index.isValid() &&
                            index.parent() == combo_->rootModelIndex() &&
                            index.row() == combo_->currentIndex();
        const QModelIndex tracked = trackedIndex();
        const bool hovered = tracked.isValid() && tracked.parent() == index.parent() &&
                             tracked.row() == index.row();
        if (!active && !hovered) {
            source_->paint(painter, contents, index);
            return;
        }

        initStyleOption(&contents, index);
        clearInteractionState(contents);
        contents.font.setWeight(QFont::Weight(surface_->emphasisWeight));
        contents.fontMetrics = QFontMetrics(contents.font);
        const QColor ink = active ? surface_->accentColor : option.palette.color(QPalette::Text);
        contents.palette.setColor(QPalette::Text, ink);
        contents.palette.setColor(QPalette::HighlightedText, ink);
        painter->save();
        painter->setFont(contents.font);
        view_->style()->drawControl(QStyle::CE_ItemViewItem, &contents, painter, view_);
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        return source_ ? source_->sizeHint(option, index)
                       : QStyledItemDelegate::sizeHint(option, index);
    }

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override
    {
        QWidget *editor = source_ ? source_->createEditor(parent, option, index) : nullptr;
        if (editor && view_->itemDelegateForIndex(index) == this) {
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
        if (source_) source_->setEditorData(editor, index);
    }

    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override
    {
        if (source_) source_->setModelData(editor, model, index);
    }

    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const override
    {
        if (source_) source_->updateEditorGeometry(editor, option, index);
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

    bool matches(QAbstractItemView *view, QComboBox *combo) const
    {
        return view_ == view && combo_ == combo;
    }

    QAbstractItemDelegate *sourceDelegate() const { return source_; }

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == view_->viewport() &&
            (event->type() == QEvent::MouseMove || event->type() == QEvent::HoverMove ||
             event->type() == QEvent::Enter || event->type() == QEvent::Leave ||
             event->type() == QEvent::HoverEnter || event->type() == QEvent::HoverLeave))
            view_->viewport()->update();
        return false;
    }

  private:
    static void clearInteractionState(QStyleOptionViewItem &option)
    {
        option.state &= ~(QStyle::State_Selected | QStyle::State_MouseOver | QStyle::State_HasFocus);
        option.features &= ~QStyleOptionViewItem::Alternate;
        option.backgroundBrush = Qt::NoBrush;
    }

    QModelIndex trackedIndex() const
    {
        if (!view_)
            return {};
        QWidget *viewport = view_->viewport();
        const QPoint cursor = viewport->mapFromGlobal(QCursor::pos());
        if (viewport->underMouse() && viewport->rect().contains(cursor)) {
            const QModelIndex pointer = view_->indexAt(cursor);
            if (pointer.isValid()) return pointer;
        }
        return view_->currentIndex();
    }

    void observeSelection() const
    {
        if (selection_ == view_->selectionModel())
            return;
        QObject::disconnect(selectionConnection_);
        QObject::disconnect(currentConnection_);
        selection_ = view_->selectionModel();
        if (selection_) {
            selectionConnection_ = QObject::connect(selection_, &QItemSelectionModel::selectionChanged,
                                                     view_->viewport(), qOverload<>(&QWidget::update));
            currentConnection_ = QObject::connect(selection_, &QItemSelectionModel::currentChanged,
                                                   view_->viewport(), qOverload<>(&QWidget::update));
        }
    }

    mutable QPointer<QItemSelectionModel> selection_;
    mutable QMetaObject::Connection selectionConnection_;
    mutable QMetaObject::Connection currentConnection_;
    mutable QSet<QWidget *> forwardedEditors_;
    QAbstractItemView *view_;
    QPointer<QComboBox> combo_;
    QPointer<QAbstractItemDelegate> source_;
    ResourcesItemSelectionSurface *surface_;
};
} // namespace

QFont Resources::itemHoverBoldFont(QFont base)
{
    base.setWeight(QFont::Bold);
    return base;
}

void Resources::setItemSelectionAppearance(QAbstractItemView *view, ItemSelectionAppearance appearance)
{
    if (!view)
        return;
    view->setProperty("joinedSelectionOnly", appearance == ItemSelectionAppearance::JoinedOnly);
    view->setProperty("menuItemInk", appearance == ItemSelectionAppearance::MenuInk);
    view->setProperty("itemHoverBold", appearance == ItemSelectionAppearance::HoverBold);
    view->viewport()->update();
}

void Resources::applyItemViewTheme(QAbstractItemView *view)
{
    if (!view || qobject_cast<QHeaderView *>(view) || !view->itemDelegate())
        return;
    for (QWidget *ancestor = view; ancestor; ancestor = ancestor->parentWidget())
    {
        if (qobject_cast<QComboBox *>(ancestor) || qobject_cast<QCalendarWidget *>(ancestor) ||
            ancestor->objectName() == QLatin1String("AutoCompletePopup"))
            return;
    }
    QHeaderView *header = nullptr;
    if (auto *tree = qobject_cast<QTreeView *>(view))
        header = tree->header();
    else if (auto *table = qobject_cast<QTableView *>(view))
        header = table->horizontalHeader();
    applyItemViewHeaderCap(view, header);
    if (dynamic_cast<ItemSurfaceDelegate *>(view->itemDelegate()))
        return;
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

void Resources::applyComboPopupTheme(QAbstractItemView *view, QComboBox *combo)
{
    if (!view || !combo || !view->itemDelegate())
        return;
    const auto wrappers = view->findChildren<ComboPopupDelegate *>(QString(), Qt::FindDirectChildrenOnly);
    for (auto *wrapper : wrappers) {
        if (view->itemDelegate() == wrapper && wrapper->matches(view, combo))
            return;
    }

    QAbstractItemDelegate *source = view->itemDelegate();
    if (auto *wrapper = dynamic_cast<ComboPopupDelegate *>(source))
        source = wrapper->sourceDelegate();
    for (auto *wrapper : wrappers) {
        if (source && source->parent() == wrapper)
            source->setParent(view);
        delete wrapper;
    }
    if (!source)
        source = new QStyledItemDelegate(view);

    view->setItemDelegate(new ComboPopupDelegate(view, combo, source));
    // Qt may retain direct connections to a previous delegate; forwarding must be the sole route.
    QObject::disconnect(source, &QAbstractItemDelegate::commitData, view, nullptr);
    QObject::disconnect(source, &QAbstractItemDelegate::closeEditor, view, nullptr);
    QObject::disconnect(source, &QAbstractItemDelegate::sizeHintChanged, view, nullptr);
}
#include "ItemViewTheme.moc"
