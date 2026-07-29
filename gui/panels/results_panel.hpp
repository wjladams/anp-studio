/**
 * @file results_panel.hpp
 * @brief Displays matrices and alternative priorities (Synthesis center).
 */

#pragma once

#include <QWidget>
#include <string>
#include <utility>
#include <vector>

#include "anpcpp/matrix.hpp"

class Document;
class QTabWidget;
class QTableWidget;
class QComboBox;
class QLineEdit;
class QLabel;
class QPushButton;

class ResultsPanel : public QWidget {
  Q_OBJECT
public:
  explicit ResultsPanel(Document* doc, QWidget* parent = nullptr);

  /** @return Synth kind combo for the Synthesis left column (may be reparented). */
  [[nodiscard]] QComboBox* synthesisKindBox() const { return synthKind_; }
  [[nodiscard]] QLineEdit* customExprEdit() const { return customExpr_; }
  [[nodiscard]] QPushButton* calculateButton() const { return calcButton_; }
  [[nodiscard]] QLabel* staleLabel() const { return staleLabel_; }

public slots:
  void calculate();
  void refreshSynthesisControls();
  void refreshStaleBadge();

signals:
  void alternativesUpdated(const std::vector<std::pair<QString, double>>& ranked);

private slots:
  void onSynthesisKindChanged(int index);
  void onCustomExprEdited();

private:
  void fillMatrix(QTableWidget* table,
                  const anpcpp::Matrix& m,
                  const std::vector<std::string>& rowLabels,
                  const std::vector<std::string>& colLabels);
  void fillVector(QTableWidget* table,
                  const anpcpp::Vector& v,
                  const std::vector<std::string>& labels);

  Document* doc_ = nullptr;
  QComboBox* synthKind_ = nullptr;
  QLineEdit* customExpr_ = nullptr;
  QPushButton* calcButton_ = nullptr;
  QLabel* staleLabel_ = nullptr;
  QWidget* controlsHost_ = nullptr;
  QTabWidget* tabs_ = nullptr;
  QTableWidget* unscaled_ = nullptr;
  QTableWidget* clusterW_ = nullptr;
  QTableWidget* scaled_ = nullptr;
  QTableWidget* limit_ = nullptr;
  QTableWidget* global_ = nullptr;
  QTableWidget* alts_ = nullptr;
};
