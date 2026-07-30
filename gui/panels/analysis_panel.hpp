/**
 * @file analysis_panel.hpp
 * @brief Analysis stage: Synthesis HTML, Sensitivity charts, Influence tables.
 */

#pragma once

#include <QWidget>
#include <QVector>
#include <utility>

class Document;
class QTabWidget;
class QTextBrowser;
class QComboBox;
class QSlider;
class QDoubleSpinBox;
class QStackedWidget;
class QTableWidget;
class QSpinBox;
class SensitivityChartWidget;

/**
 * @brief Full Analysis stage content (left tabs: Synthesis / Sensitivity / Influence).
 */
class AnalysisPanel : public QWidget {
  Q_OBJECT
public:
  explicit AnalysisPanel(Document* doc, QWidget* parent = nullptr);

public slots:
  void refresh();

private slots:
  void onSensModeChanged(int index);
  void onSensPChanged();
  void onInfluenceModeChanged(int index);
  void onInfluenceParamsChanged();

private:
  void rebuildSensWrtNodes();
  void rebuildInfluenceWrtNodes();
  void refreshSynthesisHtml();
  void refreshSensitivity();
  void refreshInfluence();

  [[nodiscard]] std::vector<std::pair<QString, double>> altScoresAtP(
      const QString& wrt,
      double p) const;

  Document* doc_ = nullptr;
  QTabWidget* tabs_ = nullptr;

  QTextBrowser* synthBrowser_ = nullptr;

  QComboBox* sensMode_ = nullptr;
  QComboBox* sensWrt_ = nullptr;
  QSlider* sensSlider_ = nullptr;
  QDoubleSpinBox* sensPSpin_ = nullptr;
  QWidget* sensInteractiveHost_ = nullptr;
  SensitivityChartWidget* sensChart_ = nullptr;

  QComboBox* inflMode_ = nullptr;
  QComboBox* inflWrt_ = nullptr;
  QDoubleSpinBox* inflDeltaUp_ = nullptr;
  QDoubleSpinBox* inflDeltaDown_ = nullptr;
  QSpinBox* inflDecimals_ = nullptr;
  QWidget* inflRawParams_ = nullptr;
  QTableWidget* inflTable_ = nullptr;

  bool updating_ = false;
};
