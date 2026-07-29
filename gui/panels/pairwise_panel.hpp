/**
 * @file pairwise_panel.hpp
 * @brief Editor for node and cluster pairwise comparison matrices.
 */

#pragma once

#include <QWidget>

class Document;
class QComboBox;
class QTableWidget;
class QLabel;
class QRadioButton;

/**
 * @brief Table editor for pairwise judgments (node or cluster mode).
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
  /** @brief Rebuilds parent/destination controls and the comparison table. */
  void refresh();
  /** @brief Selects a node as the comparison parent (w.r.t. node). */
  void selectNodeParent(const QString& name);
  /** @brief Selects node parent and destination cluster. */
  void selectNodeLink(const QString& parent, const QString& destCluster);
  /** @brief Selects a cluster as the comparison parent (w.r.t. cluster). */
  void selectClusterParent(const QString& name);

private slots:
  void onParentModeChanged();
  void onParentChanged(int index);
  void onDestClusterChanged(int index);
  void onCellChanged(int row, int col);

private:
  void rebuildTable();
  [[nodiscard]] bool nodeMode() const;

  Document* doc_ = nullptr;
  QRadioButton* nodeMode_ = nullptr;
  QRadioButton* clusterMode_ = nullptr;
  QComboBox* parentBox_ = nullptr;
  QComboBox* destClusterBox_ = nullptr;
  QTableWidget* table_ = nullptr;
  QLabel* info_ = nullptr;
  bool updating_ = false;
};
