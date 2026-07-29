/**
 * @file synthesis_summary_panel.hpp
 * @brief Ranked alternative summary for the Synthesis stage.
 */

#pragma once

#include <QWidget>
#include <utility>
#include <vector>

class Document;
class QLabel;
class QListWidget;

class SynthesisSummaryPanel : public QWidget {
  Q_OBJECT
public:
  explicit SynthesisSummaryPanel(Document* doc, QWidget* parent = nullptr);

public slots:
  void setAlternatives(const std::vector<std::pair<QString, double>>& ranked);
  void refreshStale();

private:
  Document* doc_ = nullptr;
  QLabel* staleLabel_ = nullptr;
  QListWidget* list_ = nullptr;
};
