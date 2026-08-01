/**
 * @file mainwindow.hpp
 * @brief Main application window with stage-based shell.
 */

#pragma once

#include <QMainWindow>

class Document;
class NetworkCanvas;
class PairwisePanel;
class AnalysisPanel;
class ResearcherPanel;
class InspectorPanel;
class JudgmentNavPanel;
class RatingsPanel;
class JudgmentPrioritiesPanel;
class QStackedWidget;
class QButtonGroup;
class QWidget;
class QHBoxLayout;
class QAction;
class QMenu;
class QComboBox;

/**
 * @brief Primary window: stages Structure / Judgments / Analysis.
 */
class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  enum class Stage { Structure = 0, Judgments = 1, Analysis = 2, Researcher = 3 };

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
  void onNetworkPathChosen(int index);
  void onDocumentSelectionChanged(const QString& cluster, const QString& node);
  void onJudgmentNodeSelected(const QString& parent,
                              const QString& destCluster,
                              bool ratings);
  void onJudgmentClusterSelected(const QString& parent);
  void onNodeActivated(const QString& name);
  void openRecentFile();
  void clearRecentFiles();
  void rebuildRecentMenu();
  void rebuildSampleMenu();
  void openSampleFile();

private:
  [[nodiscard]] bool maybeSave();
  [[nodiscard]] bool openPath(const QString& path);
  [[nodiscard]] bool openSamplePath(const QString& path);
  void rememberRecentFile(const QString& path);
  void buildStagePages();
  [[nodiscard]] static QString samplesDirectory();
  [[nodiscard]] static QString sampleDisplayName(const QString& fileName);

  static constexpr int kMaxRecentFiles = 10;

  Document* doc_ = nullptr;
  Stage stage_ = Stage::Structure;

  QStackedWidget* stages_ = nullptr;
  QButtonGroup* stageButtons_ = nullptr;
  QWidget* breadcrumbBar_ = nullptr;
  QHBoxLayout* breadcrumbLay_ = nullptr;
  QComboBox* networkPathCombo_ = nullptr;
  QAction* connectModeAction_ = nullptr;
  QButtonGroup* structureModeButtons_ = nullptr;
  QMenu* recentMenu_ = nullptr;
  QMenu* sampleMenu_ = nullptr;

  NetworkCanvas* canvas_ = nullptr;
  InspectorPanel* inspector_ = nullptr;

  JudgmentNavPanel* judgmentNav_ = nullptr;
  PairwisePanel* pairwise_ = nullptr;
  RatingsPanel* ratings_ = nullptr;
  JudgmentPrioritiesPanel* judgmentPriorities_ = nullptr;
  QStackedWidget* judgmentCenter_ = nullptr;

  AnalysisPanel* analysis_ = nullptr;
  ResearcherPanel* researcher_ = nullptr;
};
