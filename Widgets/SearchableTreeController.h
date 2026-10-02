#pragma once

#include <QHash>
#include <QColor>
#include <QSet>
#include <QTreeWidget>
#include <QPersistentModelIndex>
#include <QPointer>
#include <functional>

class QFrame;
class QGraphicsDropShadowEffect;
class QLabel;
class QLineEdit;
class QToolButton;
class QTimer;

/** Tree-owned Qt controller for local search, stable-key favorites and editor-preserving filters. */
class SearchableTreeController : public QObject {
  Q_OBJECT
  Q_PROPERTY(int searchInset READ searchInset WRITE setSearchInset)
  Q_PROPERTY(int searchMaximumWidth READ searchMaximumWidth WRITE setSearchMaximumWidth)
  Q_PROPERTY(QColor favoriteColor READ favoriteColor WRITE setFavoriteColor)
  Q_PROPERTY(qreal searchShadowBlur READ searchShadowBlur WRITE setSearchShadowBlur)
  Q_PROPERTY(int searchShadowOffset READ searchShadowOffset WRITE setSearchShadowOffset)
  Q_PROPERTY(QColor searchShadowColor READ searchShadowColor WRITE setSearchShadowColor)
public:
  /** Metadata roles kept separate from consumer data in Qt::UserRole. */
  enum Role { FavoriteKeyRole = Qt::UserRole + 256, StateKeyRole, SearchTextRole,
              FavoritableRole, FavoriteRole, RegisteredRole };
  /** Consumer-selected storage and metadata adapters; no device reads are performed. */
  struct Options {
    QString settingsFile;
    QString favoritesKey = QStringLiteral("Favorites/Items");
    std::function<QString(const QTreeWidgetItem*)> favoriteKey;
    std::function<QString(const QTreeWidgetItem*)> stateKey;
    std::function<QString(const QTreeWidgetItem*)> searchText;
    std::function<bool(const QTreeWidgetItem*)> searchable;
  };
  /** Attaches once to an existing tree, retaining its identity, row editors and delegates. */
  static SearchableTreeController* attach(QTreeWidget* tree, const Options& options);
  /** Returns an already attached controller without creating a second search surface. */
  static SearchableTreeController* attached(const QTreeWidget* tree);
  /** Returns the borrowed tree; the tree owns this controller. */
  QTreeWidget* tree() const { return tree_; }
  /** Updates frame-anchored placement and scroll clearance after native tree layout. */
  void updateGeometry();
  /** Supplies the default opaque-key/path identity for subclass adapters. */
  QString defaultItemStateKey(const QTreeWidgetItem* item) const;

  /** Registers an existing searchable row; an empty favorite key disables its star. */
  void registerItem(QTreeWidgetItem* item, const QString& favoriteKey,
                    const QString& searchTerms = {});
  /** Sets consumer-facing search text and accessibility labels. */
  void setSearchPlaceholderText(const QString& text);
  /** Toggles and persists a registered row's favorite state without changing consumer data. */
  void toggleFavorite(QTreeWidgetItem* item);
  /** Reports whether this stable item key is saved in the shared favorite list. */
  bool isFavorite(const QTreeWidgetItem* item) const;
  /** Reapplies local filters after a full or incremental item update. */
  void refreshFilter();
  /** Returns the theme-controlled overlay inset. */
  int searchInset() const { return search_inset_; }
  /** Applies the theme-controlled overlay inset. */
  void setSearchInset(int value);
  /** Returns the theme-controlled maximum bubble width. */
  int searchMaximumWidth() const { return search_maximum_width_; }
  /** Applies the theme-controlled maximum bubble width. */
  void setSearchMaximumWidth(int value);
  /** Returns the theme color for saved favorites. */
  QColor favoriteColor() const { return favorite_color_; }
  /** Applies the theme color for saved favorites. */
  void setFavoriteColor(const QColor& color);
  /** Returns the theme-controlled search surface shadow blur. */
  qreal searchShadowBlur() const { return search_shadow_blur_; }
  /** Applies the theme-controlled search surface shadow blur. */
  void setSearchShadowBlur(qreal value);
  /** Returns the theme-controlled vertical shadow offset. */
  int searchShadowOffset() const { return search_shadow_offset_; }
  /** Applies the theme-controlled vertical shadow offset. */
  void setSearchShadowOffset(int value);
  /** Returns the theme-controlled search surface shadow color. */
  QColor searchShadowColor() const { return search_shadow_color_; }
  /** Applies the theme-controlled search surface shadow color. */
  void setSearchShadowColor(const QColor& color);

protected:
  /** Observes native layout and hover without consuming plugin input events. */
  bool eventFilter(QObject* watched, QEvent* event) override;

private:
  explicit SearchableTreeController(QTreeWidget* tree, const Options& options);
  QString itemStateKey(const QTreeWidgetItem* item) const;
  QString itemSearchText(const QTreeWidgetItem* item) const;
  void scheduleRefresh();
  void scheduleGeometry();

  QString favoriteKey(const QTreeWidgetItem* item) const;
  QString settingsPath() const;
  void loadFavorites();
  void filterChanged();
  bool filterBranch(QTreeWidgetItem* item, const QString& ancestors);
  void positionSearch();
  void updateSearchShadow();

  QPointer<QTreeWidget> tree_;
  Options options_;
  QHash<QPersistentModelIndex, bool> domain_hidden_;
  QHash<QPersistentModelIndex, bool> applied_hidden_;
  bool geometry_pending_ = false;
  int geometry_scroll_restore_ = 0;
  bool positioning_search_ = false;
  QFrame* search_bubble_ = nullptr;
  QGraphicsDropShadowEffect* search_shadow_ = nullptr;
  QLineEdit* search_ = nullptr;
  QToolButton* favorites_only_ = nullptr;
  QLabel* result_count_ = nullptr;
  QTimer* filter_timer_ = nullptr;
  QString settings_file_;
  QString favorites_key_;
  QSet<QString> favorites_;
  QStringList filter_tokens_;
  QHash<QString, bool> saved_expansion_;
  QString saved_current_;
  int saved_vertical_scroll_ = 0;
  int saved_horizontal_scroll_ = 0;
  int matched_ = 0;
  int total_ = 0;
  int search_inset_ = 0;
  int search_maximum_width_ = 0;
  QColor favorite_color_;
  qreal search_shadow_blur_ = 0;
  int search_shadow_offset_ = 0;
  QColor search_shadow_color_ = Qt::transparent;
  bool filtering_ = false;
  bool refreshing_filter_ = false;
  bool updating_geometry_ = false;
};
