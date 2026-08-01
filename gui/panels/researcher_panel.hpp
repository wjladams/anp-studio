/**
 * @file researcher_panel.hpp
 * @brief Researcher stage: DSL notebook UI over the open document.
 */

#pragma once

#include <QString>
#include <QVector>
#include <QWidget>
#include <memory>

#include "researcher/researcher_dsl.hpp"

class Document;
class QListWidget;
class QListWidgetItem;
class QLineEdit;
class QLabel;
class QScrollArea;
class QVBoxLayout;
class QTextBrowser;

/**
 * @brief Full Researcher stage: starters, notebook cells, bindings rail.
 */
class ResearcherPanel : public QWidget {
  Q_OBJECT
public:
  explicit ResearcherPanel(Document* doc, QWidget* parent = nullptr);

public slots:
  void refreshBindings();

protected:
  void showEvent(QShowEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
  void onStarterActivated(QListWidgetItem* item);
  void runCurrentInput();
  void clearNotebook();
  void exportHtml();

private:
  struct CellRecord {
    QString command;
    QString html;
    bool ok = false;
  };

  void appendCell(const QString& command, const ResearcherEvalResult& result);
  void setHistoryFromIndex();
  [[nodiscard]] QString buildExportHtml() const;
  [[nodiscard]] QString defaultExportFileName() const;

  Document* doc_ = nullptr;
  std::unique_ptr<ResearcherSession> session_;

  QListWidget* starters_ = nullptr;
  QScrollArea* notebookScroll_ = nullptr;
  QWidget* notebookHost_ = nullptr;
  QVBoxLayout* notebookLay_ = nullptr;
  QLineEdit* input_ = nullptr;
  QLabel* promptLabel_ = nullptr;
  QTextBrowser* bindings_ = nullptr;

  QVector<CellRecord> cells_;
  QStringList history_;
  int historyIndex_ = -1;
  QString historyDraft_;
};
