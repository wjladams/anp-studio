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

class ResultsPanel : public QWidget {
  Q_OBJECT
public:
  explicit ResultsPanel(Document* doc, QWidget* parent = nullptr);

public slots:
  void calculate();
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
