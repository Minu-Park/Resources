#pragma once

#include <QHash>
#include <QColor>
#include <QSet>
#include "SearchableTreeController.h"

class QFrame;
class QGraphicsDropShadowEffect;
class QLabel;
class QLineEdit;
class QToolButton;
class QTimer;

/** Reusable Qt tree with local search, stable-key favorites and editor-preserving filters. */
class SearchableTreeWidget : public QTreeWidget {
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
  /** Creates a browser; an empty filename keeps favorites in this widget only. */
  explicit SearchableTreeWidget(QWidget* parent = nullptr, const QString& settingsFile = {},
                               const QString& favoritesKey = QStringLiteral("Favorites/Items"));
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
  int searchInset() const { return controller_->searchInset(); }
  /** Applies the theme-controlled overlay inset. */
  void setSearchInset(int value);
  /** Returns the theme-controlled maximum bubble width. */
  int searchMaximumWidth() const { return controller_->searchMaximumWidth(); }
  /** Applies the theme-controlled maximum bubble width. */
  void setSearchMaximumWidth(int value);
  /** Returns the theme color for saved favorites. */
  QColor favoriteColor() const { return controller_->favoriteColor(); }
  /** Applies the theme color for saved favorites. */
  void setFavoriteColor(const QColor& color);
  /** Returns the theme-controlled search surface shadow blur. */
  qreal searchShadowBlur() const { return controller_->searchShadowBlur(); }
  /** Applies the theme-controlled search surface shadow blur. */
  void setSearchShadowBlur(qreal value);
  /** Returns the theme-controlled vertical shadow offset. */
  int searchShadowOffset() const { return controller_->searchShadowOffset(); }
  /** Applies the theme-controlled vertical shadow offset. */
  void setSearchShadowOffset(int value);
  /** Returns the theme-controlled search surface shadow color. */
  QColor searchShadowColor() const { return controller_->searchShadowColor(); }
  /** Applies the theme-controlled search surface shadow color. */
  void setSearchShadowColor(const QColor& color);

protected:
  /** Returns a stable row identity for expansion/current-item restoration; consumers may override. */
  virtual QString itemStateKey(const QTreeWidgetItem* item) const;
  /** Returns searchable row text; ancestor labels are added by the filter. */
  virtual QString itemSearchText(const QTreeWidgetItem* item) const;
  bool viewportEvent(QEvent* event) override;
  void scrollContentsBy(int dx, int dy) override;
  void resizeEvent(QResizeEvent* event) override;
  void showEvent(QShowEvent* event) override;
  void updateGeometries() override;

private:
  SearchableTreeController* controller_ = nullptr;
};
