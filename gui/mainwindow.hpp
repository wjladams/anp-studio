/**
 * @file mainwindow.hpp
 * @brief Main application window with menus and dock panels.
 */

#pragma once

#include <QMainWindow>

class Document;
class NetworkCanvas;
class StructurePanel;
class PairwisePanel;
class ResultsPanel;

/**
 * @brief Primary window: file operations, calculation, and docked UI panels.
 */
class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  /**
   * @param parent Optional Qt parent widget.
   */
  explicit MainWindow(QWidget* parent = nullptr);

protected:
  /** @brief Prompts to save before closing if the document is dirty. */
  void closeEvent(QCloseEvent* event) override;

private slots:
  /** @brief Creates a new empty document. */
  void newFile();
  /** @brief Opens a JSON network file. */
  void openFile();
  /** @brief Saves to the current path. @return False if cancelled or failed. */
  bool saveFile();
  /** @brief Saves via file dialog. @return False if cancelled or failed. */
  bool saveFileAs();
  /** @brief Updates the window title from path and dirty state. */
  void updateTitle();
  /** @brief Triggers results calculation in the results panel. */
  void calculate();

private:
  /** @return False if the user cancelled a save prompt. */
  [[nodiscard]] bool maybeSave();

  Document* doc_ = nullptr;
  NetworkCanvas* canvas_ = nullptr;
  StructurePanel* structure_ = nullptr;
  PairwisePanel* pairwise_ = nullptr;
  ResultsPanel* results_ = nullptr;
};
