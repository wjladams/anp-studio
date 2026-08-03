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
 * named groups, read-only) live in collapsible sections. Edit… opens the
 * Participants roster; Collect… opens the Collect judgments hub.
 */
class SessionPanel : public QWidget {
  Q_OBJECT
public:
  explicit SessionPanel(Document* doc, QWidget* parent = nullptr);

signals:
  /** @brief User asked to open the Collect judgments hub. */
  void collectJudgmentsRequested();

public slots:
  /** @brief Rebuilds the roster lists and active-scope pill. */
  void refresh();

private slots:
  void onEditClicked();
  void onCollectClicked();
  void onIndividualClicked(QListWidgetItem* item);
  void onAggregateClicked(QListWidgetItem* item);

private:
  [[nodiscard]] QString activeLabel() const;

  Document* doc_ = nullptr;

  QPushButton* editBtn_ = nullptr;
  QPushButton* collectBtn_ = nullptr;
  QLabel* activePill_ = nullptr;
  QToolBox* toolBox_ = nullptr;
  QListWidget* individualsList_ = nullptr;
  QListWidget* aggregatesList_ = nullptr;
  QLabel* emptyHint_ = nullptr;
};
