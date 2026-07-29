/**
 * @file pairwise_panel.hpp
 * @brief Editor for node and cluster pairwise comparison matrices.
 */

#pragma once

#include <QWidget>

class Document;
class QTableWidget;
class QLabel;

/**
 * @brief Table editor for pairwise judgments (node or cluster mode).
 *
 * Parent / destination selection lives in @ref JudgmentNavPanel; this panel
 * only displays and edits the comparison matrix.
 */
class PairwisePanel : public QWidget {
  Q_OBJECT
public:
  /**
   * @param doc Document containing judgments to edit.
   * @param parent Optional parent widget.
   */
  explicit PairwisePanel(Document* doc, QWidget* parent = nullptr);

public slots:
  /** @brief Rebuilds the comparison table for the current selection. */
  void refresh();
  /** @brief Selects a node as the comparison parent (w.r.t. node). */
  void selectNodeParent(const QString& name);
  /** @brief Selects node parent and destination cluster. */
  void selectNodeLink(const QString& parent, const QString& destCluster);
  /** @brief Selects a cluster as the comparison parent (w.r.t. cluster). */
  void selectClusterParent(const QString& name);

private slots:
  void onCellChanged(int row, int col);

private:
  void rebuildTable();
  [[nodiscard]] bool nodeMode() const { return nodeMode_; }

  Document* doc_ = nullptr;
  bool nodeMode_ = true;
  QString parent_;
  QString destCluster_;
  QTableWidget* table_ = nullptr;
  QLabel* info_ = nullptr;
  bool updating_ = false;
};
