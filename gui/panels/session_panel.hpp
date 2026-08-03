/**
 * @file session_panel.hpp
 * @brief Judgments right column: active judgment session (whose judgments).
 */

#pragma once

#include <QWidget>

class Document;
class QLabel;
class QPushButton;
class QListWidget;
class QListWidgetItem;
class QToolBox;

/**
 * @brief Shows and switches the document's judgment session scope.
 *
 * Individuals (per-participant, editable) and Aggregates (Group average and
 * named groups, read-only) live in collapsible sections. An Edit… button
 * opens the Participants roster dialog.
 */
class SessionPanel : public QWidget {
  Q_OBJECT
public:
  explicit SessionPanel(Document* doc, QWidget* parent = nullptr);

public slots:
  /** @brief Rebuilds the roster lists and active-scope pill. */
  void refresh();

private slots:
  void onEditClicked();
  void onIndividualClicked(QListWidgetItem* item);
  void onAggregateClicked(QListWidgetItem* item);

private:
  [[nodiscard]] QString activeLabel() const;

  Document* doc_ = nullptr;

  QPushButton* editBtn_ = nullptr;
  QLabel* activePill_ = nullptr;
  QToolBox* toolBox_ = nullptr;
  QListWidget* individualsList_ = nullptr;
  QListWidget* aggregatesList_ = nullptr;
  QLabel* emptyHint_ = nullptr;
};
