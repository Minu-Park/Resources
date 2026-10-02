#include "SearchableTreeWidget.h"
#include <QResizeEvent>
#include <QScrollBar>

SearchableTreeWidget::SearchableTreeWidget(QWidget* parent, const QString& settingsFile,
                                         const QString& favoritesKey) : QTreeWidget(parent) {
  SearchableTreeController::Options options;
  options.settingsFile = settingsFile;
  options.favoritesKey = favoritesKey;
  options.stateKey = [this](const QTreeWidgetItem* item) { return itemStateKey(item); };
  options.searchText = [this](const QTreeWidgetItem* item) { return itemSearchText(item); };
  controller_ = SearchableTreeController::attach(this, options);
}

QString SearchableTreeWidget::itemStateKey(const QTreeWidgetItem* item) const {
  return controller_->defaultItemStateKey(item);
}
QString SearchableTreeWidget::itemSearchText(const QTreeWidgetItem* item) const {
  return item->text(0) + QChar(' ') + item->data(0, SearchTextRole).toString();
}
void SearchableTreeWidget::registerItem(QTreeWidgetItem* item, const QString& key, const QString& terms) {
  controller_->registerItem(item, key, terms);
}
void SearchableTreeWidget::setSearchPlaceholderText(const QString& text) { controller_->setSearchPlaceholderText(text); }
void SearchableTreeWidget::toggleFavorite(QTreeWidgetItem* item) { controller_->toggleFavorite(item); }
bool SearchableTreeWidget::isFavorite(const QTreeWidgetItem* item) const { return controller_->isFavorite(item); }
void SearchableTreeWidget::refreshFilter() { controller_->refreshFilter(); }
void SearchableTreeWidget::setSearchInset(int value) { if (controller_) controller_->setSearchInset(value); }
void SearchableTreeWidget::setSearchMaximumWidth(int value) { if (controller_) controller_->setSearchMaximumWidth(value); }
void SearchableTreeWidget::setFavoriteColor(const QColor& value) { if (controller_) controller_->setFavoriteColor(value); }
void SearchableTreeWidget::setSearchShadowBlur(qreal value) { if (controller_) controller_->setSearchShadowBlur(value); }
void SearchableTreeWidget::setSearchShadowOffset(int value) { if (controller_) controller_->setSearchShadowOffset(value); }
void SearchableTreeWidget::setSearchShadowColor(const QColor& value) { if (controller_) controller_->setSearchShadowColor(value); }
bool SearchableTreeWidget::viewportEvent(QEvent* event) { return QTreeWidget::viewportEvent(event); }
void SearchableTreeWidget::scrollContentsBy(int dx, int dy) {
  QTreeWidget::scrollContentsBy(dx, dy);
  if (controller_) controller_->updateGeometry();
}
void SearchableTreeWidget::resizeEvent(QResizeEvent* event) {
  QTreeWidget::resizeEvent(event);
  if (controller_) controller_->updateGeometry();
}
void SearchableTreeWidget::showEvent(QShowEvent* event) {
  QTreeWidget::showEvent(event);
  if (controller_) controller_->updateGeometry();
}
void SearchableTreeWidget::updateGeometries() {
  const int previousScroll = verticalScrollBar()->value();
  QTreeWidget::updateGeometries();
  if (controller_) {
    controller_->updateGeometry();
    verticalScrollBar()->setValue(previousScroll);
  }
}
