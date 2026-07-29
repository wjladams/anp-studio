/**
 * @file mainwindow.hpp
 * @brief Main application window with stage-based shell.
 */

#pragma once

#include <QMainWindow>

class Document;
class NetworkCanvas;
class StructurePanel;
class PairwisePanel;
class ResultsPanel;
class InspectorPanel;
class JudgmentNavPanel;
class RatingsPanel;
class SessionStubPanel;
class SynthesisSummaryPanel;
class QStackedWidget;
class QButtonGroup;
class QLabel;
class QAction;

/**
 * @brief Primary window: stages Structure / Judgments / Synthesis.
 */
class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  enum class Stage { Structure = 0, Judgments = 1, Synthesis = 2 };

  explicit MainWindow(QWidget* parent = nullptr);

public slots:
  void setStage(Stage stage);

protected:
  void closeEvent(QCloseEvent* event) override;

private slots:
  void newFile();
  void openFile();
  bool saveFile();
  bool saveFileAs();
  void updateTitle();
  void updateBreadcrumb();
  void calculate();
  void onDocumentSelectionChanged(const QString& cluster, const QString& node);
  void onJudgmentNodeSelected(const QString& parent,
                              const QString& destCluster,
                              bool ratings);
  void onJudgmentClusterSelected(const QString& parent);

private:
  [[nodiscard]] bool maybeSave();
  void buildStagePages();

  Document* doc_ = nullptr;
  Stage stage_ = Stage::Structure;

  QStackedWidget* stages_ = nullptr;
  QButtonGroup* stageButtons_ = nullptr;
  QLabel* breadcrumb_ = nullptr;
  QAction* connectModeAction_ = nullptr;

  NetworkCanvas* canvas_ = nullptr;
  StructurePanel* structure_ = nullptr;
  InspectorPanel* inspector_ = nullptr;

  JudgmentNavPanel* judgmentNav_ = nullptr;
  PairwisePanel* pairwise_ = nullptr;
  RatingsPanel* ratings_ = nullptr;
  SessionStubPanel* sessionStub_ = nullptr;
  QStackedWidget* judgmentCenter_ = nullptr;

  ResultsPanel* results_ = nullptr;
  SynthesisSummaryPanel* synthesisSummary_ = nullptr;
};
