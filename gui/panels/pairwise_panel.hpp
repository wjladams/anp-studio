/**
 * @file pairwise_panel.hpp
 * @brief Editor for node and cluster pairwise comparison matrices.
 */

#pragma once

#include <QWidget>

#include "anpcpp/pairwise.hpp"

class Document;
class QTableWidget;
class QLabel;
class QStackedWidget;
class QScrollArea;
class QPushButton;
class QButtonGroup;
class QVBoxLayout;

/**
 * @brief Pairwise judgment editor with Matrix and Questionnaire views.
 *
 * Parent / destination selection lives in @ref JudgmentNavPanel.
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
  /** @brief Rebuilds the active view for the current selection. */
  void refresh();
  /** @brief Selects a node as the comparison parent (w.r.t. node). */
  void selectNodeParent(const QString& name);
  /** @brief Selects node parent and destination cluster. */
  void selectNodeLink(const QString& parent, const QString& destCluster);
  /** @brief Selects a cluster as the comparison parent (w.r.t. cluster). */
  void selectClusterParent(const QString& name);

private slots:
  void onCellChanged(int row, int col);
  void onViewModeChanged();
  void onQuestionnaireAnswered(int row, int col, double value);

private:
  void rebuildViews();
  void rebuildMatrix(const anpcpp::PairwiseJudgments* pw);
  void rebuildQuestionnaire(const anpcpp::PairwiseJudgments* pw);
  void updateInfo(const anpcpp::PairwiseJudgments* pw);
  [[nodiscard]] const anpcpp::PairwiseJudgments* currentPairwise() const;
  void applyComparison(const QString& a, const QString& b, double value);
  [[nodiscard]] bool nodeMode() const { return nodeMode_; }

  Document* doc_ = nullptr;
  bool nodeMode_ = true;
  QString parent_;
  QString destCluster_;

  QPushButton* matrixBtn_ = nullptr;
  QPushButton* questionnaireBtn_ = nullptr;
  QButtonGroup* viewGroup_ = nullptr;
  QStackedWidget* views_ = nullptr;
  QTableWidget* table_ = nullptr;
  QScrollArea* questionnaireScroll_ = nullptr;
  QWidget* questionnaireHost_ = nullptr;
  QVBoxLayout* questionnaireLay_ = nullptr;
  QLabel* info_ = nullptr;
  bool updating_ = false;
};
