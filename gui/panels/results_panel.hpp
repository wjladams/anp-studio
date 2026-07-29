/**
 * @file results_panel.hpp
 * @brief Displays supermatrices, limit matrix, and alternative priorities.
 */

#pragma once

#include <QWidget>
#include <string>
#include <vector>

#include "cppanp/matrix.hpp"

class Document;
class QTabWidget;
class QTableWidget;
class QComboBox;
class QLineEdit;

/**
 * @brief Tabbed view of calculation results and synthesis options.
 */
class ResultsPanel : public QWidget {
  Q_OBJECT
public:
  /**
   * @param doc Document to calculate and display.
   * @param parent Optional parent widget.
   */
  explicit ResultsPanel(Document* doc, QWidget* parent = nullptr);

public slots:
  /** @brief Recalculates matrices and alternative priorities from the model. */
  void calculate();
  /** @brief Syncs synthesis kind / custom expression controls with the model. */
  void refreshSynthesisControls();

private slots:
  void onSynthesisKindChanged(int index);
  void onCustomExprEdited();

private:
  void fillMatrix(QTableWidget* table,
                  const cppanp::Matrix& m,
                  const std::vector<std::string>& rowLabels,
                  const std::vector<std::string>& colLabels);
  void fillVector(QTableWidget* table,
                  const cppanp::Vector& v,
                  const std::vector<std::string>& labels);

  Document* doc_ = nullptr;
  QComboBox* synthKind_ = nullptr;
  QLineEdit* customExpr_ = nullptr;
  QTabWidget* tabs_ = nullptr;
  QTableWidget* unscaled_ = nullptr;
  QTableWidget* clusterW_ = nullptr;
  QTableWidget* scaled_ = nullptr;
  QTableWidget* limit_ = nullptr;
  QTableWidget* global_ = nullptr;
  QTableWidget* alts_ = nullptr;
};
