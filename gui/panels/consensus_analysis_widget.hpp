/**
 * @file consensus_analysis_widget.hpp
 * @brief Consensus / Variance Analysis: disagreement, coverage, cohorts.
 */

#pragma once

#include <QWidget>

class Document;
class QComboBox;
class QLabel;
class QStackedWidget;
class QTableWidget;
class QButtonGroup;
class QVBoxLayout;

/**
 * @brief Multi-user consensus pane (per-user judgments, not only effective slots).
 */
class ConsensusAnalysisWidget : public QWidget {
  Q_OBJECT
public:
  explicit ConsensusAnalysisWidget(Document* doc, QWidget* parent = nullptr);

public slots:
  void refresh();

private slots:
  void onModeChanged(int id);
  void onControlsChanged();

private:
  void rebuildBlockCombo();
  void rebuildCompareCombo();
  void rebuildCohortGroupCombos();
  void refreshDisagreement();
  void refreshCoverage();
  void refreshCohorts();

  [[nodiscard]] QStringList compareParticipantIds() const;

  Document* doc_ = nullptr;
  QComboBox* blockCombo_ = nullptr;
  QComboBox* compareCombo_ = nullptr;
  QButtonGroup* modeGroup_ = nullptr;
  QStackedWidget* modeStack_ = nullptr;

  QLabel* emptyLabel_ = nullptr;
  QWidget* content_ = nullptr;

  QTableWidget* disagreementTable_ = nullptr;
  QLabel* disagreementTitle_ = nullptr;
  QLabel* disagreementLegend_ = nullptr;
  QLabel* distNote_ = nullptr;

  QTableWidget* coverageTable_ = nullptr;

  QComboBox* cohortLeft_ = nullptr;
  QComboBox* cohortRight_ = nullptr;
  QLabel* cohortHelp_ = nullptr;
  QWidget* cohortBarsHost_ = nullptr;
  QVBoxLayout* cohortBarsLay_ = nullptr;
  QLabel* cohortDistNote_ = nullptr;

  bool updating_ = false;
};
