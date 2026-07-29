/**
 * @file judgment_nav_panel.hpp
 * @brief Top selector for judgment parent / destination / kind.
 */

#pragma once

#include <QWidget>

class Document;
class QComboBox;
class QLabel;
class QPushButton;
class QButtonGroup;

/**
 * @brief Horizontal judgment selector: Node|Cluster, Wrt, Other Cluster.
 *
 * Wrt lists only parents that have outgoing connections. Other Cluster lists
 * only destination clusters connected from the selected Wrt node.
 */
class JudgmentNavPanel : public QWidget {
  Q_OBJECT
public:
  explicit JudgmentNavPanel(Document* doc, QWidget* parent = nullptr);

public slots:
  void refresh();

signals:
  /** @brief Node parent selected (pairwise or ratings). */
  void nodeJudgmentSelected(const QString& parent,
                            const QString& destCluster,
                            bool ratings);
  /** @brief Cluster parent selected (pairwise only). */
  void clusterJudgmentSelected(const QString& parent);

private slots:
  void onModeChanged();
  void onWrtChanged();
  void onOtherChanged();
  void onSwitchToPairwise();
  void onSwitchToRatings();

private:
  void emitCurrent();
  void rebuildWrtList();
  void rebuildOtherList();
  [[nodiscard]] bool nodeMode() const;

  Document* doc_ = nullptr;
  QButtonGroup* modeGroup_ = nullptr;
  QPushButton* nodeModeBtn_ = nullptr;
  QPushButton* clusterModeBtn_ = nullptr;
  QLabel* wrtLabel_ = nullptr;
  QComboBox* wrtBox_ = nullptr;
  QLabel* otherLabel_ = nullptr;
  QComboBox* otherBox_ = nullptr;
  QPushButton* toPairwise_ = nullptr;
  QPushButton* toRatings_ = nullptr;
  QLabel* coverageLabel_ = nullptr;
  bool updating_ = false;
  bool preferRatings_ = false;
};
