/**
 * @file analysis_panel.hpp
 * @brief Analysis stage: left-nav tree + Synthesis / Sensitivity / Influence panes.
 */

#pragma once

#include <QWidget>
#include <QVector>
#include <utility>

class Document;
class QTextBrowser;
class QComboBox;
class QSlider;
class QDoubleSpinBox;
class QStackedWidget;
class QTableWidget;
class QSpinBox;
class QTreeWidget;
class QTreeWidgetItem;
class SensitivityChartWidget;

/**
 * @brief Full Analysis stage: hierarchical left nav and stacked calculation panes.
 */
class AnalysisPanel : public QWidget {
  Q_OBJECT
public:
  enum class Page {
    Synthesis = 0,
    SensOverview,
    SensInteractive,
    SensGlobal,
    InflOverview,
    InflRaw,
    InflRank,
    InflMarginal,
  };

  explicit AnalysisPanel(Document* doc, QWidget* parent = nullptr);

public slots:
  void refresh();

private slots:
  void onNavItemActivated(QTreeWidgetItem* item, int column);
  void onNavCurrentChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);
  void onOverviewAnchorClicked(const QUrl& url);
  void onInfluenceParamsChanged();

private:
  void buildNavTree();
  void updateSubnetNavVisibility();
  void selectNavForPage(Page page, const QString& anchor = QString());
  void navigateTo(Page page, const QString& anchor = QString());
  void rebuildSensWrtNodes();
  void rebuildInfluenceWrtNodes();
  void refreshSynthesisHtml();
  void refreshSensitivity();
  void refreshInfluence();
  void fillWrtCombo(QComboBox* combo, const QString& prefer);

  [[nodiscard]] std::vector<std::pair<QString, double>> altScoresAtP(
      const QString& wrt,
      double p) const;

  Document* doc_ = nullptr;
  QTreeWidget* nav_ = nullptr;
  QStackedWidget* stack_ = nullptr;

  QTreeWidgetItem* synthItem_ = nullptr;
  QTreeWidgetItem* subnetNavItem_ = nullptr;
  QString synthAnchor_;

  QTextBrowser* synthBrowser_ = nullptr;
  QTextBrowser* sensOverview_ = nullptr;
  QTextBrowser* inflOverview_ = nullptr;

  QComboBox* sensWrtInteractive_ = nullptr;
  QSlider* sensSlider_ = nullptr;
  QDoubleSpinBox* sensPSpin_ = nullptr;
  SensitivityChartWidget* sensChartInteractive_ = nullptr;

  QComboBox* sensWrtGlobal_ = nullptr;
  SensitivityChartWidget* sensChartGlobal_ = nullptr;

  QComboBox* inflWrtRaw_ = nullptr;
  QDoubleSpinBox* inflDeltaUp_ = nullptr;
  QDoubleSpinBox* inflDeltaDown_ = nullptr;
  QSpinBox* inflDecimals_ = nullptr;
  QTableWidget* inflTableRaw_ = nullptr;

  QComboBox* inflWrtRank_ = nullptr;
  QTableWidget* inflTableRank_ = nullptr;

  QComboBox* inflWrtMarginal_ = nullptr;
  QTableWidget* inflTableMarginal_ = nullptr;

  bool updating_ = false;
  bool navigating_ = false;
};
