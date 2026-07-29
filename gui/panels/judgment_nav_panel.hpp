/**
 * @file judgment_nav_panel.hpp
 * @brief Navigator of judgment parents with coverage and kind.
 */

#pragma once

#include <QWidget>

class Document;
class QTreeWidget;
class QComboBox;
class QLabel;
class QPushButton;

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

private:
  void onItemClicked();
  void onSwitchToPairwise();
  void onSwitchToRatings();

  Document* doc_ = nullptr;
  QTreeWidget* tree_ = nullptr;
  QComboBox* filter_ = nullptr;
  QLabel* coverageLabel_ = nullptr;
  QPushButton* toPairwise_ = nullptr;
  QPushButton* toRatings_ = nullptr;
  QString currentParent_;
  QString currentDest_;
  bool currentIsNode_ = true;
};
