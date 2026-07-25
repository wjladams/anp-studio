#pragma once

#include <QMainWindow>

class Document;
class NetworkCanvas;
class StructurePanel;
class PairwisePanel;
class ResultsPanel;

class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  explicit MainWindow(QWidget* parent = nullptr);

protected:
  void closeEvent(QCloseEvent* event) override;

private slots:
  void newFile();
  void openFile();
  bool saveFile();
  bool saveFileAs();
  void updateTitle();
  void calculate();

private:
  [[nodiscard]] bool maybeSave();

  Document* doc_ = nullptr;
  NetworkCanvas* canvas_ = nullptr;
  StructurePanel* structure_ = nullptr;
  PairwisePanel* pairwise_ = nullptr;
  ResultsPanel* results_ = nullptr;
};
