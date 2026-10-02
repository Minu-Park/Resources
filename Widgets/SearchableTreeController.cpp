#include "SearchableTreeController.h"

#include <QAbstractItemModel>
#include <QApplication>
#include <QCursor>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QHoverEvent>
#include <QHeaderView>
#include <QIconEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPersistentModelIndex>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSettings>
#include <QShortcut>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidgetItemIterator>
#include <algorithm>
#include <cmath>

/** Theme metric surface shared by inherited and externally attached tree browsers. */
class SearchableTreeSurface final : public QFrame {
  Q_OBJECT
  Q_PROPERTY(int searchInset READ searchInset WRITE setSearchInset)
  Q_PROPERTY(int searchMaximumWidth READ searchMaximumWidth WRITE setSearchMaximumWidth)
  Q_PROPERTY(QColor favoriteColor READ favoriteColor WRITE setFavoriteColor)
  Q_PROPERTY(qreal searchShadowBlur READ searchShadowBlur WRITE setSearchShadowBlur)
  Q_PROPERTY(int searchShadowOffset READ searchShadowOffset WRITE setSearchShadowOffset)
  Q_PROPERTY(QColor searchShadowColor READ searchShadowColor WRITE setSearchShadowColor)
public:
  SearchableTreeSurface(SearchableTreeController* controller, QWidget* parent)
      : QFrame(parent), controller_(controller) {}
  int searchInset() const { return controller_->searchInset(); }
  void setSearchInset(int value) { controller_->setSearchInset(value); }
  int searchMaximumWidth() const { return controller_->searchMaximumWidth(); }
  void setSearchMaximumWidth(int value) { controller_->setSearchMaximumWidth(value); }
  QColor favoriteColor() const { return controller_->favoriteColor(); }
  void setFavoriteColor(const QColor& value) { controller_->setFavoriteColor(value); }
  qreal searchShadowBlur() const { return controller_->searchShadowBlur(); }
  void setSearchShadowBlur(qreal value) { controller_->setSearchShadowBlur(value); }
  int searchShadowOffset() const { return controller_->searchShadowOffset(); }
  void setSearchShadowOffset(int value) { controller_->setSearchShadowOffset(value); }
  QColor searchShadowColor() const { return controller_->searchShadowColor(); }
  void setSearchShadowColor(const QColor& value) { controller_->setSearchShadowColor(value); }
private:
  SearchableTreeController* controller_;
};

namespace {
/** Centers the visible star outline within the requested icon canvas. */
void paintStar(QPainter* painter, const QRectF& rect, bool filled, const QColor& color) {
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing);
  QPainterPath star;
  for (int point = 0; point < 10; ++point) {
    const double angle = (point * 36.0 - 90.0) * 3.141592653589793 / 180.0;
    const double radius = rect.width() * (point % 2 ? 0.22 : 0.5);
    const QPointF vertex(rect.center().x() + radius * std::cos(angle),
                         rect.center().y() + radius * std::sin(angle));
    if (point == 0) star.moveTo(vertex); else star.lineTo(vertex);
  }
  star.closeSubpath();
  star.translate(rect.center() - star.boundingRect().center());
  painter->setPen(color);
  painter->setBrush(filled ? QBrush(color) : Qt::NoBrush);
  painter->drawPath(star);
  painter->restore();
}

/** Scalable star icon with no dependency on installed symbol fonts or theme assets. */
class StarIconEngine final : public QIconEngine {
public:
  explicit StarIconEngine(SearchableTreeController* tree) : tree_(tree) {}
  QIconEngine* clone() const override { return new StarIconEngine(tree_); }
  QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override {
    QPixmap result(size);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    paint(&painter, QRect(QPoint(), size), mode, state);
    return result;
  }
  void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state) override {
    const bool filled = state == QIcon::On;
    auto color = filled ? tree_->favoriteColor() : tree_->tree()->palette().color(QPalette::Text);
    if (mode == QIcon::Disabled) color = tree_->tree()->palette().color(QPalette::Disabled, QPalette::Text);
    paintStar(painter, rect.adjusted(2, 2, -2, -2), filled, color);
  }
private:
  SearchableTreeController* tree_;
};

QString tupleKey(const QStringList& parts) {
  return QString::fromLatin1(QJsonDocument(QJsonArray::fromStringList(parts))
      .toJson(QJsonDocument::Compact).toBase64(QByteArray::Base64UrlEncoding));
}

/** Paints and handles stars in the name cell, leaving the Value editor untouched. */
class FavoriteDelegate final : public QStyledItemDelegate {
public:
  explicit FavoriteDelegate(SearchableTreeController* tree) : QStyledItemDelegate(tree->tree()), tree_(tree), original_(tree->tree()->itemDelegateForColumn(0) ? tree->tree()->itemDelegateForColumn(0) : tree->tree()->itemDelegate()), column_delegate_(tree->tree()->itemDelegateForColumn(0) != nullptr) {
    if (original_) {
      connect(original_, &QAbstractItemDelegate::commitData, this, &QAbstractItemDelegate::commitData);
      connect(original_, &QAbstractItemDelegate::closeEditor, this, &QAbstractItemDelegate::closeEditor);
      connect(original_, &QAbstractItemDelegate::sizeHintChanged, this, &QAbstractItemDelegate::sizeHintChanged);
    }
    tree_->tree()->viewport()->setAttribute(Qt::WA_Hover);
    tree_->tree()->viewport()->installEventFilter(this);
    const auto scrolled = [this] {
      updateHover(tree_->tree()->viewport()->mapFromGlobal(QCursor::pos()));
    };
    connect(tree_->tree()->verticalScrollBar(), &QScrollBar::valueChanged, this, scrolled);
    connect(tree_->tree()->horizontalScrollBar(), &QScrollBar::valueChanged, this, scrolled);
  }

  /** Tracks row hover independently of theme-suppressed item-view mouse moves. */
  bool eventFilter(QObject* watched, QEvent* event) override {
    if (watched == tree_->tree()->viewport()
        && (event->type() == QEvent::HoverEnter || event->type() == QEvent::HoverMove
            || event->type() == QEvent::HoverLeave)) {
      updateHover(event->type() == QEvent::HoverLeave ? QPoint(-1, -1)
          : static_cast<QHoverEvent*>(event)->position().toPoint());
    }
    return watched == tree_->tree()->viewport() ? false : QStyledItemDelegate::eventFilter(watched, event);
  }

  QRect starRect(const QStyleOptionViewItem& option) const {
    const int extent = tree_->tree()->style()->pixelMetric(QStyle::PM_SmallIconSize);
    return QRect(option.rect.right() - extent - 3,
                 option.rect.top() + (option.rect.height() - extent) / 2,
                 extent, extent);
  }

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override {
    auto label = option;
    const bool leaf = index.data(SearchableTreeController::FavoritableRole).toBool();
    if (leaf) label.rect.setRight(starRect(option).left() - 3);
    if (sourceDelegate()) sourceDelegate()->paint(painter, label, index); else QStyledItemDelegate::paint(painter, label, index);
    if (!leaf) return;
    const bool favorite = index.data(SearchableTreeController::FavoriteRole).toBool();
    if (!favorite && index != hovered_) return;
    painter->save();
    auto rect = QRectF(starRect(option)).adjusted(2, 2, -2, -2);
    rect.moveCenter(QPointF(rect.center().x(), QRectF(option.rect).center().y()));
    const auto color = favorite ? tree_->favoriteColor() : option.palette.color(QPalette::Text);
    paintStar(painter, rect, favorite, color);
    painter->restore();
  }

  bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option,
                   const QModelIndex& index) override {
    if (!index.data(SearchableTreeController::FavoritableRole).toBool()) return forwardEvent(event, model, option, index);
    if (event->type() != QEvent::MouseButtonPress && event->type() != QEvent::MouseButtonRelease)
      return forwardEvent(event, model, option, index);
    const auto* mouse = static_cast<QMouseEvent*>(event);
    if (mouse->button() != Qt::LeftButton || !starRect(option).contains(mouse->position().toPoint()))
      return forwardEvent(event, model, option, index);
    if (event->type() == QEvent::MouseButtonRelease) {
      const auto* item = tree_->tree()->itemAt(option.rect.center());
      tree_->toggleFavorite(const_cast<QTreeWidgetItem*>(item));
    }
    return true;
  }

  QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
    return sourceDelegate() ? sourceDelegate()->sizeHint(option, index) : QStyledItemDelegate::sizeHint(option, index);
  }
  QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                        const QModelIndex& index) const override {
    return sourceDelegate() ? sourceDelegate()->createEditor(parent, option, index)
                     : QStyledItemDelegate::createEditor(parent, option, index);
  }
  void setEditorData(QWidget* editor, const QModelIndex& index) const override {
    if (sourceDelegate()) sourceDelegate()->setEditorData(editor, index); else QStyledItemDelegate::setEditorData(editor, index);
  }
  void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override {
    if (sourceDelegate()) sourceDelegate()->setModelData(editor, model, index); else QStyledItemDelegate::setModelData(editor, model, index);
  }
  void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
                            const QModelIndex& index) const override {
    if (sourceDelegate()) sourceDelegate()->updateEditorGeometry(editor, option, index);
    else QStyledItemDelegate::updateEditorGeometry(editor, option, index);
  }
  void destroyEditor(QWidget* editor, const QModelIndex& index) const override {
    if (sourceDelegate()) sourceDelegate()->destroyEditor(editor, index); else QStyledItemDelegate::destroyEditor(editor, index);
  }
  bool helpEvent(QHelpEvent* event, QAbstractItemView* view, const QStyleOptionViewItem& option,
                 const QModelIndex& index) override {
    return sourceDelegate() ? sourceDelegate()->helpEvent(event, view, option, index)
                     : QStyledItemDelegate::helpEvent(event, view, option, index);
  }

private:
  bool forwardEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option,
                    const QModelIndex& index) {
    return sourceDelegate() ? sourceDelegate()->editorEvent(event, model, option, index)
                     : QStyledItemDelegate::editorEvent(event, model, option, index);
  }
  /** Repaints only the name cells whose hover state changed, without altering focus. */
  void updateHover(const QPoint& position) {
    const auto next = tree_->tree()->viewport()->rect().contains(position)
        ? tree_->tree()->indexAt(position).siblingAtColumn(0) : QModelIndex{};
    if (hovered_ == next) return;
    const auto previous = hovered_;
    hovered_ = next;
    if (previous.isValid()) tree_->tree()->viewport()->update(tree_->tree()->visualRect(previous));
    if (hovered_.isValid()) tree_->tree()->viewport()->update(tree_->tree()->visualRect(hovered_));
  }

  SearchableTreeController* tree_;
  QAbstractItemDelegate* sourceDelegate() const {
    // Theme polish may install a new default delegate after attachment.
    return column_delegate_ ? original_.data() : tree_->tree()->itemDelegate();
  }
  QPointer<QAbstractItemDelegate> original_;
  bool column_delegate_;
  QPersistentModelIndex hovered_;
};

} // namespace

SearchableTreeController* SearchableTreeController::attached(const QTreeWidget* tree) {
  return tree ? tree->findChild<SearchableTreeController*>(
      QStringLiteral("SearchableTreeController"), Qt::FindDirectChildrenOnly) : nullptr;
}

SearchableTreeController* SearchableTreeController::attach(QTreeWidget* tree, const Options& options) {
  if (!tree) return nullptr;
  if (auto* existing = attached(tree)) return existing;
  return new SearchableTreeController(tree, options);
}

SearchableTreeController::SearchableTreeController(QTreeWidget* tree, const Options& options)
    : QObject(tree), tree_(tree), options_(options), settings_file_(options.settingsFile),
      favorites_key_(options.favoritesKey.isEmpty() ? QStringLiteral("Favorites/Items") : options.favoritesKey) {
  setObjectName(QStringLiteral("SearchableTreeController"));
  tree_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  tree_->setItemDelegateForColumn(0, new FavoriteDelegate(this));
  tree_->installEventFilter(this);
  tree_->viewport()->installEventFilter(this);
  search_inset_ = tree_->style()->pixelMetric(QStyle::PM_LayoutLeftMargin);
  favorite_color_ = QApplication::palette().color(QPalette::Highlight);

  search_bubble_ = new SearchableTreeSurface(this, tree_);
  search_bubble_->setObjectName(QStringLiteral("DeviceFeatureSearchBubble"));
  auto* layout = new QHBoxLayout(search_bubble_);
  layout->setObjectName(QStringLiteral("DeviceFeatureSearchLayout"));

  search_ = new QLineEdit(search_bubble_);
  search_->setObjectName(QStringLiteral("DeviceFeatureSearchInput"));
  search_->setPlaceholderText(tr("Search items"));
  search_->setAccessibleName(tr("Search items"));
  search_->setToolTip(tr("Search item names and categories (Ctrl+F). Escape clears the search."));
  search_->setMinimumWidth(0);
  auto* clearSearch = new QToolButton(search_bubble_);
  clearSearch->setObjectName(QStringLiteral("DeviceFeatureSearchClear"));
  clearSearch->setIcon(tree_->style()->standardIcon(QStyle::SP_DialogCloseButton));
  clearSearch->setToolButtonStyle(Qt::ToolButtonIconOnly);
  clearSearch->setFocusPolicy(Qt::NoFocus);
  clearSearch->setToolTip(tr("Clear search"));
  clearSearch->setAccessibleName(tr("Clear search"));
  result_count_ = new QLabel(search_bubble_);
  result_count_->setObjectName(QStringLiteral("DeviceFeatureSearchCount"));
  result_count_->setAccessibleName(tr("Matching items"));
  favorites_only_ = new QToolButton(search_bubble_);
  favorites_only_->setObjectName(QStringLiteral("DeviceFeatureFavoritesOnly"));
  favorites_only_->setIcon(QIcon(new StarIconEngine(this)));
  favorites_only_->setCheckable(true);
  favorites_only_->setToolTip(tr("Show favorites only"));
  favorites_only_->setAccessibleName(tr("Show favorites only"));
  favorites_only_->setIconSize(QSize(16, 16));
  layout->addWidget(search_, 1);
  layout->addWidget(clearSearch);
  layout->addWidget(result_count_);
  layout->addWidget(favorites_only_);
  result_count_->hide();
  clearSearch->hide();
  connect(clearSearch, &QToolButton::clicked, search_, [this] {
    search_->clear();
    search_->setFocus(Qt::MouseFocusReason);
  });
  connect(search_, &QLineEdit::textChanged, clearSearch, [clearSearch](const QString& text) {
    clearSearch->setVisible(!text.isEmpty());
  });
  connect(search_, &QLineEdit::textChanged, this, &SearchableTreeController::filterChanged);
  connect(favorites_only_, &QToolButton::toggled, this, [this] {
    filterChanged();
  });
  auto* find = new QShortcut(QKeySequence::Find, tree_);
  find->setContext(Qt::WidgetWithChildrenShortcut);
  connect(find, &QShortcut::activated, search_, [this] { search_->setFocus(); search_->selectAll(); });
  auto* escape = new QShortcut(QKeySequence(Qt::Key_Escape), search_);
  escape->setContext(Qt::WidgetShortcut);
  connect(escape, &QShortcut::activated, search_, &QLineEdit::clear);
  auto* favorite = new QShortcut(QKeySequence(QStringLiteral("Ctrl+D")), tree_);
  favorite->setContext(Qt::WidgetWithChildrenShortcut);
  connect(favorite, &QShortcut::activated, this, [this] { toggleFavorite(tree_->currentItem()); });

  filter_timer_ = new QTimer(this);
  filter_timer_->setSingleShot(true);
  connect(filter_timer_, &QTimer::timeout, this, &SearchableTreeController::refreshFilter);
  const auto changed = [this] {
    scheduleRefresh();
  };
  connect(tree_->model(), &QAbstractItemModel::dataChanged, this, changed);
  connect(tree_->model(), &QAbstractItemModel::rowsInserted, this, changed);
  connect(tree_->model(), &QAbstractItemModel::rowsRemoved, this, changed);
  connect(tree_->model(), &QAbstractItemModel::modelReset, this, changed);
  connect(tree_->verticalScrollBar(), &QScrollBar::rangeChanged, this, [this] { scheduleGeometry(); });
  connect(tree_->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int value) {
    // A scroll requested after scheduling layout takes precedence over its old snapshot.
    // Native layout can temporarily clamp the custom scroll clearance; retain that target.
    const bool layoutClamp = value == tree_->verticalScrollBar()->maximum()
        && geometry_scroll_restore_ > value;
    if (geometry_pending_ && !updating_geometry_ && !layoutClamp)
      geometry_scroll_restore_ = value;
  });
  connect(tree_->horizontalScrollBar(), &QScrollBar::rangeChanged, this, [this] { scheduleGeometry(); });
  connect(tree_->model(), &QAbstractItemModel::modelReset, this, [this] {
    domain_hidden_.clear();
    applied_hidden_.clear();
  });
  loadFavorites();
  filter_timer_->start();
}

QString SearchableTreeController::settingsPath() const {
  return settings_file_.isEmpty() ? QString()
      : QDir::cleanPath(QFileInfo(settings_file_).absoluteFilePath());
}

void SearchableTreeController::loadFavorites() {
  if (settings_file_.isEmpty()) return;
  QSettings settings(settingsPath(), QSettings::IniFormat);
  const auto saved = settings.value(favorites_key_).toStringList();
  favorites_ = QSet<QString>(saved.begin(), saved.end());
}

QString SearchableTreeController::favoriteKey(const QTreeWidgetItem* item) const {
  return item->data(0, FavoriteKeyRole).toString();
}

QString SearchableTreeController::defaultItemStateKey(const QTreeWidgetItem* item) const {
  const auto explicitKey = item->data(0, StateKeyRole).toString();
  if (!explicitKey.isEmpty()) return explicitKey;
  if (item->data(0, FavoritableRole).toBool()) return tupleKey({QStringLiteral("item"), favoriteKey(item)});
  QStringList path;
  for (auto* ancestor = item; ancestor; ancestor = ancestor->parent())
    path.prepend(ancestor->text(0));
  path.prepend(QStringLiteral("path"));
  return tupleKey(path);
}

QString SearchableTreeController::itemSearchText(const QTreeWidgetItem* item) const {
  if (options_.searchText) return options_.searchText(item);
  return item->text(0) + QChar(' ') + item->data(0, SearchTextRole).toString();
}

QString SearchableTreeController::itemStateKey(const QTreeWidgetItem* item) const {
  return options_.stateKey ? options_.stateKey(item) : defaultItemStateKey(item);
}

void SearchableTreeController::registerItem(QTreeWidgetItem* item, const QString& favoriteKey,
                                       const QString& searchTerms) {
  if (!item || item->treeWidget() != tree_) return;
  item->setData(0, RegisteredRole, true);
  item->setData(0, FavoriteKeyRole, favoriteKey);
  item->setData(0, SearchTextRole, searchTerms);
  item->setData(0, FavoritableRole, !favoriteKey.isEmpty());
  item->setData(0, FavoriteRole, isFavorite(item));
}

void SearchableTreeController::setSearchPlaceholderText(const QString& text) {
  search_->setPlaceholderText(text);
  search_->setAccessibleName(text);
}

bool SearchableTreeController::isFavorite(const QTreeWidgetItem* item) const {
  return item && item->treeWidget() == tree_ && item->data(0, FavoritableRole).toBool()
      && favorites_.contains(favoriteKey(item));
}

void SearchableTreeController::toggleFavorite(QTreeWidgetItem* item) {
  if (!item || item->treeWidget() != tree_ || !item->data(0, FavoritableRole).toBool()) return;
  // Read the current store before toggling so sibling panels do not overwrite each other.
  loadFavorites();
  const auto key = favoriteKey(item);
  if (favorites_.contains(key)) favorites_.remove(key); else favorites_.insert(key);
  if (settings_file_.isEmpty()) {
    refreshFilter();
    return;
  }
  QDir().mkpath(QFileInfo(settingsPath()).absolutePath());
  QSettings settings(settingsPath(), QSettings::IniFormat);
  auto sorted = favorites_.values();
  sorted.sort();
  settings.setValue(favorites_key_, sorted);
  settings.sync();
  // Synchronize only browsers sharing the same explicitly selected file and key.
  for (auto* widget : QApplication::allWidgets()) {
    if (auto* tree = attached(qobject_cast<QTreeWidget*>(widget))) {
      if (tree->settingsPath() == settingsPath() && tree->favorites_key_ == favorites_key_) {
        tree->loadFavorites();
        tree->refreshFilter();
      }
    }
  }
}

void SearchableTreeController::filterChanged() {
  const bool active = !search_->text().trimmed().isEmpty() || favorites_only_->isChecked();
  if (active && !filtering_) {
    saved_expansion_.clear();
    for (QTreeWidgetItemIterator it(tree_); *it; ++it)
      saved_expansion_.insert(itemStateKey(*it), (*it)->isExpanded());
    saved_current_ = tree_->currentItem() ? itemStateKey(tree_->currentItem()) : QString();
    saved_vertical_scroll_ = tree_->verticalScrollBar()->value();
    saved_horizontal_scroll_ = tree_->horizontalScrollBar()->value();
  }
  filtering_ = active;
  refreshFilter();
  if (!active) {
    for (QTreeWidgetItemIterator it(tree_); *it; ++it) {
      const auto key = itemStateKey(*it);
      if (saved_expansion_.contains(key)) (*it)->setExpanded(saved_expansion_.value(key));
      if (key == saved_current_) tree_->setCurrentItem(*it);
    }
    tree_->doItemsLayout();
    updateGeometry();
    geometry_scroll_restore_ = saved_vertical_scroll_;
    tree_->verticalScrollBar()->setValue(saved_vertical_scroll_);
    tree_->horizontalScrollBar()->setValue(saved_horizontal_scroll_);
    saved_expansion_.clear();
  }
}

bool SearchableTreeController::filterBranch(QTreeWidgetItem* item, const QString& ancestors) {
  const QPersistentModelIndex index(tree_->indexFromItem(item));
  if (!domain_hidden_.contains(index)
      || (applied_hidden_.contains(index) && item->isHidden() != applied_hidden_.value(index)))
    domain_hidden_.insert(index, item->isHidden());
  if (options_.favoriteKey) {
    const auto key = options_.favoriteKey(item);
    item->setData(0, FavoriteKeyRole, key);
    item->setData(0, FavoritableRole, !key.isEmpty());
  }
  const bool available = !domain_hidden_.value(index);
  const bool searchable = options_.searchable ? options_.searchable(item)
      : item->childCount() == 0 || item->data(0, RegisteredRole).toBool();
  const auto text = ancestors + QChar(' ') + itemSearchText(item);
  const bool ownMatch = std::all_of(filter_tokens_.begin(), filter_tokens_.end(),
      [&text](const QString& token) { return text.contains(token, Qt::CaseInsensitive); });
  bool visible = false;
  for (int i = 0; i < item->childCount(); ++i)
    visible = filterBranch(item->child(i), text) || visible;
  if (searchable && available) {
    ++total_;
    const bool favorite = isFavorite(item);
    if (item->data(0, FavoriteRole).toBool() != favorite) item->setData(0, FavoriteRole, favorite);
    const bool match = ownMatch && (!favorites_only_->isChecked() || favorite);
    visible = visible || match;
    if (match) ++matched_;
  } else if (!filtering_) visible = true;
  visible = visible && available;
  item->setHidden(!visible);
  applied_hidden_.insert(index, !visible);
  if (filtering_ && visible && item->childCount() > 0) item->setExpanded(true);
  return visible;
}

void SearchableTreeController::refreshFilter() {
  if (!tree_ || refreshing_filter_) return;
  const int previousScroll = tree_->verticalScrollBar()->value();
  // Removed rows invalidate persistent indices; do not retain dead visibility state.
  for (auto it = domain_hidden_.begin(); it != domain_hidden_.end(); )
    if (!it.key().isValid()) it = domain_hidden_.erase(it); else ++it;
  for (auto it = applied_hidden_.begin(); it != applied_hidden_.end(); )
    if (!it.key().isValid()) it = applied_hidden_.erase(it); else ++it;
  refreshing_filter_ = true;
  filter_tokens_ = search_->text().trimmed().split(QRegularExpression(QStringLiteral("\\s+")),
                                                 Qt::SkipEmptyParts);
  matched_ = total_ = 0;
  for (int i = 0; i < tree_->topLevelItemCount(); ++i) filterBranch(tree_->topLevelItem(i), {});
  result_count_->setVisible(filtering_);
  result_count_->setText(matched_ ? QStringLiteral("%1/%2").arg(matched_).arg(total_)
                                : tr("No results"));
  result_count_->setAccessibleDescription(tr("%1 of %2 items match").arg(matched_).arg(total_));
  tree_->doItemsLayout();
  updateGeometry();
  tree_->verticalScrollBar()->setValue(previousScroll);
  if (geometry_pending_) geometry_scroll_restore_ = previousScroll;
  refreshing_filter_ = false;
}

void SearchableTreeController::setSearchInset(int value) {
  search_inset_ = qMax(0, value);
  if (search_bubble_) { positionSearch(); updateGeometry(); }
}

void SearchableTreeController::setSearchMaximumWidth(int value) {
  search_maximum_width_ = qMax(0, value);
  if (search_bubble_) positionSearch();
}

void SearchableTreeController::setFavoriteColor(const QColor& color) {
  favorite_color_ = color;
  if (favorites_only_) favorites_only_->setIcon(QIcon(new StarIconEngine(this)));
  tree_->viewport()->update();
}

void SearchableTreeController::setSearchShadowBlur(qreal value) {
  search_shadow_blur_ = qMax(qreal(0), value);
  updateSearchShadow();
}

void SearchableTreeController::setSearchShadowOffset(int value) {
  search_shadow_offset_ = value;
  updateSearchShadow();
}

void SearchableTreeController::setSearchShadowColor(const QColor& color) {
  search_shadow_color_ = color;
  updateSearchShadow();
}

void SearchableTreeController::updateSearchShadow() {
  if (!search_bubble_) return;
  const bool enabled = search_shadow_blur_ > 0 && search_shadow_color_.alpha() > 0;
  if (!search_shadow_ && !enabled) return;
  if (!search_shadow_) {
    search_shadow_ = new QGraphicsDropShadowEffect(search_bubble_);
    search_bubble_->setGraphicsEffect(search_shadow_);
  }
  search_shadow_->setBlurRadius(search_shadow_blur_);
  search_shadow_->setOffset(0, search_shadow_offset_);
  search_shadow_->setColor(search_shadow_color_);
  search_shadow_->setEnabled(enabled);
}

void SearchableTreeController::positionSearch() {
  if (!tree_ || !search_bubble_ || positioning_search_) return;
  positioning_search_ = true;
  search_bubble_->ensurePolished();
  // Anchor to the tree frame, reserving both scrollbar gutters even while hidden.
  // The overlay is a sibling of the viewport, so scrolling cannot move or clip it.
  const auto area = tree_->contentsRect();
  const int verticalGutter = tree_->verticalScrollBar()->sizeHint().width();
  const int horizontalGutter = tree_->horizontalScrollBar()->sizeHint().height();
  const int available = qMax(0, area.width() - 2 * (search_inset_ + verticalGutter));
  const int width = search_maximum_width_ ? qMin(search_maximum_width_, available) : available;
  // QSS constrains the outer height; sizeHint may initially report only content height.
  // Use the same bounds QWidget will apply, so its first layout cannot shift the anchor.
  const int height = qBound(search_bubble_->minimumHeight(),
      search_bubble_->sizeHint().height(), search_bubble_->maximumHeight());
  search_bubble_->setGeometry(area.left() + (area.width() - width) / 2,
      qMax(area.top(), area.bottom() + 1 - horizontalGutter - height - search_inset_), width, height);
  search_bubble_->raise();
  positioning_search_ = false;
}

void SearchableTreeController::updateGeometry() {
  if (!tree_ || updating_geometry_) return;
  updating_geometry_ = true;
  positionSearch();
  const int previousScroll = tree_->verticalScrollBar()->value();
  int contentHeight = 0;
  const auto visit = [&](auto&& self, QTreeWidgetItem* item) -> void {
    if (item->isHidden()) return;
    contentHeight += tree_->visualItemRect(item).height();
    if (item->isExpanded())
      for (int i = 0; i < item->childCount(); ++i) self(self, item->child(i));
  };
  for (int i = 0; i < tree_->topLevelItemCount(); ++i) visit(visit, tree_->topLevelItem(i));
  const int viewportBottom = tree_->viewport()->mapTo(tree_, QPoint(0, tree_->viewport()->height())).y();
  const int reserve = qMax(0, viewportBottom - search_bubble_->y() + search_inset_);
  tree_->verticalScrollBar()->setRange(0, qMax(0, contentHeight - tree_->viewport()->height() + reserve));
  tree_->verticalScrollBar()->setPageStep(qMax(1, tree_->viewport()->height() - reserve));
  tree_->verticalScrollBar()->setValue(previousScroll);
  updating_geometry_ = false;
}

void SearchableTreeController::scheduleRefresh() {
  if (!refreshing_filter_) filter_timer_->start();
}

void SearchableTreeController::scheduleGeometry() {
  if (updating_geometry_ || geometry_pending_) return;
  geometry_pending_ = true;
  geometry_scroll_restore_ = tree_->verticalScrollBar()->value();
  QTimer::singleShot(0, this, [this] {
    geometry_pending_ = false;
    updateGeometry();
    if (tree_) tree_->verticalScrollBar()->setValue(geometry_scroll_restore_);
  });
}

bool SearchableTreeController::eventFilter(QObject* watched, QEvent* event) {
  if (!tree_) return false;
  if (watched == tree_ || watched == tree_->viewport()) {
    if (event->type() == QEvent::Resize || event->type() == QEvent::Show
        || event->type() == QEvent::StyleChange || event->type() == QEvent::LayoutRequest)
      scheduleGeometry();
  }
  return false;
}

#include "SearchableTreeController.moc"
