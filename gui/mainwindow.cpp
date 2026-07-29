#include "mainwindow.hpp"

#include "canvas/network_canvas.hpp"
#include "commands/network_commands.hpp"
#include "document.hpp"
#include "panels/inspector_panel.hpp"
#include "panels/judgment_nav_panel.hpp"
#include "panels/pairwise_panel.hpp"
#include "panels/ratings_panel.hpp"
#include "panels/results_panel.hpp"
#include "panels/session_stub_panel.hpp"
#include "panels/synthesis_summary_panel.hpp"

#include <QAction>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QToolBar>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  doc_ = new Document(this);

  auto* central = new QWidget(this);
  auto* rootLay = new QVBoxLayout(central);
  rootLay->setContentsMargins(12, 8, 12, 8);
  rootLay->setSpacing(8);

  auto* stageRow = new QHBoxLayout;
  stageButtons_ = new QButtonGroup(this);
  stageButtons_->setExclusive(true);
  auto addStageBtn = [&](const QString& text, Stage s) {
    auto* b = new QPushButton(text, central);
    b->setObjectName(QStringLiteral("stageTab"));
    b->setCheckable(true);
    b->setFlat(true);
    b->setCursor(Qt::PointingHandCursor);
    stageButtons_->addButton(b, static_cast<int>(s));
    stageRow->addWidget(b);
  };
  addStageBtn(QStringLiteral("Structure"), Stage::Structure);
  addStageBtn(QStringLiteral("Judgments"), Stage::Judgments);
  addStageBtn(QStringLiteral("Synthesis"), Stage::Synthesis);
  stageRow->addStretch();
  rootLay->addLayout(stageRow);

  breadcrumbBar_ = new QWidget(central);
  breadcrumbBar_->setObjectName(QStringLiteral("breadcrumbBar"));
  breadcrumbLay_ = new QHBoxLayout(breadcrumbBar_);
  breadcrumbLay_->setContentsMargins(0, 0, 0, 0);
  breadcrumbLay_->setSpacing(4);
  rootLay->addWidget(breadcrumbBar_);

  stages_ = new QStackedWidget(central);
  rootLay->addWidget(stages_, 1);
  setCentralWidget(central);

  buildStagePages();

  connect(stageButtons_, &QButtonGroup::idClicked, this, [this](int id) {
    setStage(static_cast<Stage>(id));
  });

  connect(canvas_, &NetworkCanvas::selectionChanged, this,
          [this](const QString& cluster, const QString& node) {
            doc_->setSelection(cluster, node);
          });
  connect(canvas_, &NetworkCanvas::nodeActivated, this,
          &MainWindow::onNodeActivated);
  connect(doc_, &Document::selectionChanged, this,
          &MainWindow::onDocumentSelectionChanged);
  connect(doc_, &Document::viewNetworkChanged, this, &MainWindow::updateBreadcrumb);
  connect(doc_, &Document::modelChanged, this, &MainWindow::updateBreadcrumb);

  connect(judgmentNav_, &JudgmentNavPanel::nodeJudgmentSelected, this,
          &MainWindow::onJudgmentNodeSelected);
  connect(judgmentNav_, &JudgmentNavPanel::clusterJudgmentSelected, this,
          &MainWindow::onJudgmentClusterSelected);
  connect(results_, &ResultsPanel::alternativesUpdated, synthesisSummary_,
          &SynthesisSummaryPanel::setAlternatives);

  auto* fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
  fileMenu->addAction(QStringLiteral("&New"), this, &MainWindow::newFile,
                      QKeySequence::New);
  fileMenu->addAction(QStringLiteral("&Open…"), this, &MainWindow::openFile,
                      QKeySequence::Open);
  fileMenu->addAction(QStringLiteral("&Save"), this, &MainWindow::saveFile,
                      QKeySequence::Save);
  fileMenu->addAction(QStringLiteral("Save &As…"), this, &MainWindow::saveFileAs,
                      QKeySequence::SaveAs);
  fileMenu->addSeparator();
  fileMenu->addAction(QStringLiteral("&Quit"), this, &QWidget::close,
                      QKeySequence::Quit);

  auto* editMenu = menuBar()->addMenu(QStringLiteral("&Edit"));
  editMenu->addAction(doc_->undoStack()->createUndoAction(
      this, QStringLiteral("Undo")));
  editMenu->addAction(doc_->undoStack()->createRedoAction(
      this, QStringLiteral("Redo")));

  auto* netMenu = menuBar()->addMenu(QStringLiteral("&Network"));
  connectModeAction_ =
      netMenu->addAction(QStringLiteral("Connect Mode"), this, [this]() {
        canvas_->setConnectMode(!canvas_->connectMode());
      });
  netMenu->addAction(QStringLiteral("Up Subnetwork"), doc_,
                     &Document::popSubnet);
  netMenu->addAction(QStringLiteral("Root Network"), doc_,
                     &Document::popToRoot);

  auto* computeMenu = menuBar()->addMenu(QStringLiteral("&Compute"));
  computeMenu->addAction(QStringLiteral("Calculate"), this,
                         &MainWindow::calculate, QKeySequence(Qt::Key_F5));

  auto* tb = addToolBar(QStringLiteral("Main"));
  tb->addAction(QStringLiteral("New"), this, &MainWindow::newFile);
  tb->addAction(QStringLiteral("Open"), this, &MainWindow::openFile);
  tb->addAction(QStringLiteral("Save"), this, &MainWindow::saveFile);
  tb->addSeparator();
  tb->addAction(doc_->undoStack()->createUndoAction(this));
  tb->addAction(doc_->undoStack()->createRedoAction(this));
  tb->addSeparator();
  tb->addAction(QStringLiteral("Calculate"), this, &MainWindow::calculate);

  connect(doc_, &Document::dirtyChanged, this, [this](bool) { updateTitle(); });
  connect(doc_, &Document::pathChanged, this, [this](const QString&) {
    updateTitle();
  });

  if (auto* b = stageButtons_->button(static_cast<int>(Stage::Structure))) {
    b->setChecked(true);
  }
  setStage(Stage::Structure);
  resize(1400, 900);
  updateTitle();
  updateBreadcrumb();
  statusBar()->showMessage(
      QStringLiteral("Structure → Judgments → Synthesis"));
}

void MainWindow::buildStagePages() {
  // Structure: canvas | inspector
  canvas_ = new NetworkCanvas(doc_, this);
  inspector_ = new InspectorPanel(doc_, this);
  auto* structurePage = new QWidget(stages_);
  auto* sSplit = new QSplitter(Qt::Horizontal, structurePage);
  sSplit->addWidget(canvas_);
  sSplit->addWidget(inspector_);
  sSplit->setStretchFactor(0, 4);
  sSplit->setStretchFactor(1, 1);
  auto* sLay = new QVBoxLayout(structurePage);
  sLay->setContentsMargins(0, 0, 0, 0);
  sLay->addWidget(sSplit);
  stages_->addWidget(structurePage);

  // Judgments: top selector | pairwise/ratings | session
  judgmentNav_ = new JudgmentNavPanel(doc_, this);
  pairwise_ = new PairwisePanel(doc_, this);
  ratings_ = new RatingsPanel(doc_, this);
  sessionStub_ = new SessionStubPanel(this);
  judgmentCenter_ = new QStackedWidget(this);
  judgmentCenter_->addWidget(pairwise_);
  judgmentCenter_->addWidget(ratings_);
  auto* judgmentsPage = new QWidget(stages_);
  auto* jLay = new QVBoxLayout(judgmentsPage);
  jLay->setContentsMargins(0, 0, 0, 0);
  jLay->setSpacing(8);
  jLay->addWidget(judgmentNav_);
  auto* jSplit = new QSplitter(Qt::Horizontal, judgmentsPage);
  jSplit->addWidget(judgmentCenter_);
  jSplit->addWidget(sessionStub_);
  jSplit->setStretchFactor(0, 4);
  jSplit->setStretchFactor(1, 1);
  jLay->addWidget(jSplit, 1);
  stages_->addWidget(judgmentsPage);

  // Synthesis: calc controls | matrices | summary
  results_ = new ResultsPanel(doc_, this);
  synthesisSummary_ = new SynthesisSummaryPanel(doc_, this);
  auto* synthesisPage = new QWidget(stages_);
  auto* ySplit = new QSplitter(Qt::Horizontal, synthesisPage);
  auto* leftCalc = new QWidget(ySplit);
  auto* leftLay = new QVBoxLayout(leftCalc);
  leftLay->addWidget(new QLabel(QStringLiteral("Calculate"), leftCalc));
  auto* calcBtn = new QPushButton(QStringLiteral("Calculate (F5)"), leftCalc);
  calcBtn->setObjectName(QStringLiteral("primaryButton"));
  calcBtn->setCursor(Qt::PointingHandCursor);
  connect(calcBtn, &QPushButton::clicked, this, &MainWindow::calculate);
  leftLay->addWidget(calcBtn);
  leftLay->addWidget(results_->staleLabel());
  leftLay->addStretch();
  ySplit->addWidget(leftCalc);
  ySplit->addWidget(results_);
  ySplit->addWidget(synthesisSummary_);
  ySplit->setStretchFactor(0, 1);
  ySplit->setStretchFactor(1, 3);
  ySplit->setStretchFactor(2, 1);
  auto* yLay = new QVBoxLayout(synthesisPage);
  yLay->setContentsMargins(0, 0, 0, 0);
  yLay->addWidget(ySplit);
  stages_->addWidget(synthesisPage);
}

void MainWindow::setStage(Stage stage) {
  stage_ = stage;
  stages_->setCurrentIndex(static_cast<int>(stage));
  if (auto* b = stageButtons_->button(static_cast<int>(stage))) {
    b->setChecked(true);
  }
  if (connectModeAction_ != nullptr) {
    connectModeAction_->setEnabled(stage == Stage::Structure);
  }
  if (stage == Stage::Structure) {
    canvas_->select(doc_->selectedCluster(), doc_->selectedNode());
  } else if (stage == Stage::Judgments) {
    judgmentNav_->refresh();
  }
}

void MainWindow::onDocumentSelectionChanged(const QString& cluster,
                                            const QString& node) {
  if (stage_ == Stage::Structure) {
    canvas_->select(cluster, node);
  }
}

void MainWindow::onJudgmentNodeSelected(const QString& parent,
                                        const QString& destCluster,
                                        bool ratings) {
  if (ratings) {
    judgmentCenter_->setCurrentWidget(ratings_);
    ratings_->selectLink(parent, destCluster);
  } else {
    judgmentCenter_->setCurrentWidget(pairwise_);
    pairwise_->selectNodeLink(parent, destCluster);
  }
}

void MainWindow::onJudgmentClusterSelected(const QString& parent) {
  judgmentCenter_->setCurrentWidget(pairwise_);
  pairwise_->selectClusterParent(parent);
}

void MainWindow::onNodeActivated(const QString& name) {
  doc_->undoStack()->push(new EnsureSubnetCmd(doc_, name));
  doc_->pushSubnet(name);
}

void MainWindow::updateBreadcrumb() {
  while (QLayoutItem* item = breadcrumbLay_->takeAt(0)) {
    if (QWidget* w = item->widget()) {
      w->deleteLater();
    }
    delete item;
  }

  const QStringList parts = doc_->breadcrumb();
  auto* caption = new QLabel(QStringLiteral("Network:"), breadcrumbBar_);
  caption->setObjectName(QStringLiteral("breadcrumbCaption"));
  breadcrumbLay_->addWidget(caption);

  for (int i = 0; i < parts.size(); ++i) {
    if (i > 0) {
      auto* sep = new QLabel(QStringLiteral("/"), breadcrumbBar_);
      sep->setObjectName(QStringLiteral("breadcrumbSep"));
      breadcrumbLay_->addWidget(sep);
    }
    const bool isCurrent = (i == parts.size() - 1);
    if (isCurrent) {
      auto* cur = new QLabel(parts[i], breadcrumbBar_);
      cur->setObjectName(QStringLiteral("breadcrumbCurrent"));
      breadcrumbLay_->addWidget(cur);
    } else {
      auto* link = new QPushButton(parts[i], breadcrumbBar_);
      link->setObjectName(QStringLiteral("breadcrumbLink"));
      link->setFlat(true);
      link->setCursor(Qt::PointingHandCursor);
      // Index i → stack depth i+1 (Root is depth 1).
      const int depth = i + 1;
      connect(link, &QPushButton::clicked, this, [this, depth]() {
        doc_->popToDepth(depth);
      });
      breadcrumbLay_->addWidget(link);
    }
  }
  breadcrumbLay_->addStretch();
}

void MainWindow::updateTitle() {
  QString title = QStringLiteral("ANP Studio");
  if (!doc_->path().isEmpty()) title += QStringLiteral(" — ") + doc_->path();
  else title += QStringLiteral(" — untitled");
  if (doc_->isDirty()) title += QStringLiteral(" *");
  setWindowTitle(title);
}

void MainWindow::newFile() {
  if (!maybeSave()) return;
  doc_->newNetwork(true);
  updateTitle();
}

void MainWindow::openFile() {
  if (!maybeSave()) return;
  const QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("Open Network"), {},
      QStringLiteral("ANP Studio JSON (*.json);;All files (*)"));
  if (path.isEmpty()) return;
  QString err;
  if (!doc_->loadFromFile(path, &err)) {
    QMessageBox::warning(this, QStringLiteral("Open failed"), err);
  }
  updateTitle();
}

bool MainWindow::saveFile() {
  if (doc_->path().isEmpty()) return saveFileAs();
  QString err;
  if (!doc_->saveToFile(doc_->path(), &err)) {
    QMessageBox::warning(this, QStringLiteral("Save failed"), err);
    return false;
  }
  updateTitle();
  return true;
}

bool MainWindow::saveFileAs() {
  const QString path = QFileDialog::getSaveFileName(
      this, QStringLiteral("Save Network"), {},
      QStringLiteral("ANP Studio JSON (*.json);;All files (*)"));
  if (path.isEmpty()) return false;
  QString err;
  if (!doc_->saveToFile(path, &err)) {
    QMessageBox::warning(this, QStringLiteral("Save failed"), err);
    return false;
  }
  updateTitle();
  return true;
}

void MainWindow::calculate() {
  setStage(Stage::Synthesis);
  results_->calculate();
}

bool MainWindow::maybeSave() {
  if (!doc_->isDirty()) return true;
  const auto r = QMessageBox::warning(
      this, QStringLiteral("Unsaved changes"),
      QStringLiteral("The network has been modified.\nDo you want to save?"),
      QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
  if (r == QMessageBox::Save) return saveFile();
  if (r == QMessageBox::Cancel) return false;
  return true;
}

void MainWindow::closeEvent(QCloseEvent* event) {
  if (maybeSave()) event->accept();
  else event->ignore();
}
