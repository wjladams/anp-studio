/**
 * @file researcher_panel.hpp
 * @brief Researcher stage: multi-notebook DSL UI (browser-style tabs).
 */

#pragma once

#include <QString>
#include <QVector>
#include <QWidget>
#include <memory>

#include "document.hpp"
#include "researcher/researcher_dsl.hpp"

class QListWidget;
class QListWidgetItem;
class QLineEdit;
class QLabel;
class QScrollArea;
class QVBoxLayout;
class QTextBrowser;
class QTabBar;
class QToolButton;
class QMenu;

/**
 * @brief Researcher stage: notebook tabs, starters + bindings rail.
 */
class ResearcherPanel : public QWidget {
  Q_OBJECT
public:
  explicit ResearcherPanel(Document* doc, QWidget* parent = nullptr);

public slots:
  void refreshBindings();
  /** @brief Rebuild tabs and active notebook from the document session. */
  void reloadFromDocument();

protected:
  void showEvent(QShowEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
  void onStarterActivated(QListWidgetItem* item);
  void runCurrentInput();
  void clearNotebook();
  void exportHtml();
  void onTabChanged(int index);
  void onTabCloseRequested(int index);
  void onTabDoubleClicked(int index);
  void newNotebook();
  void duplicateNotebook();
  void deleteNotebook();
  void addNotebookTab();

private:
  void appendCell(const QString& command, const ResearcherEvalResult& result);
  void clearNotebookUi(bool keepEphemeralHelp);
  void rebuildTabBar();
  void loadActiveNotebookIntoUi();
  void persistActiveNotebookToModel(bool markDirty);
  void syncSessionToDocument(bool markDirty);
  void ensureNotebooks();
  void setHistoryFromIndex();
  void updateTabLabels();
  [[nodiscard]] QString buildExportHtml() const;
  [[nodiscard]] QString defaultExportFileName() const;
  [[nodiscard]] static QString middleTruncate(const QString& name);

  Document* doc_ = nullptr;
  std::unique_ptr<ResearcherSession> session_;

  QToolButton* menuBtn_ = nullptr;
  QTabBar* tabBar_ = nullptr;
  QToolButton* addTabBtn_ = nullptr;
  QListWidget* starters_ = nullptr;
  QScrollArea* notebookScroll_ = nullptr;
  QWidget* notebookHost_ = nullptr;
  QVBoxLayout* notebookLay_ = nullptr;
  QLineEdit* input_ = nullptr;
  QLabel* promptLabel_ = nullptr;
  QTextBrowser* bindings_ = nullptr;

  QVector<ResearcherNotebook> notebooks_;
  int activeIndex_ = 0;
  QVector<ResearcherCell> cells_;
  QStringList history_;
  int historyIndex_ = -1;
  QString historyDraft_;
  bool updatingTabs_ = false;
};
