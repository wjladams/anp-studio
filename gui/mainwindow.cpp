#include "mainwindow.hpp"

#include "canvas/network_canvas.hpp"
#include "document.hpp"
#include "panels/pairwise_panel.hpp"
#include "panels/results_panel.hpp"
#include "panels/structure_panel.hpp"

#include <QAction>
#include <QCloseEvent>
#include <QDockWidget>
#include <QFileDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QToolBar>
#include <QUndoStack>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  doc_ = new Document(this);
  canvas_ = new NetworkCanvas(doc_, this);
  setCentralWidget(canvas_);

  structure_ = new StructurePanel(doc_, this);
  pairwise_ = new PairwisePanel(doc_, this);
  results_ = new ResultsPanel(doc_, this);

  auto* structDock = new QDockWidget(QStringLiteral("Structure"), this);
  structDock->setWidget(structure_);
  addDockWidget(Qt::LeftDockWidgetArea, structDock);

  auto* pairDock = new QDockWidget(QStringLiteral("Pairwise"), this);
  pairDock->setWidget(pairwise_);
  addDockWidget(Qt::RightDockWidgetArea, pairDock);

  auto* resultsDock = new QDockWidget(QStringLiteral("Results"), this);
  resultsDock->setWidget(results_);
  addDockWidget(Qt::BottomDockWidgetArea, resultsDock);

  connect(structure_, &StructurePanel::nodeSelected, pairwise_,
          &PairwisePanel::selectNodeParent);
  connect(structure_, &StructurePanel::clusterSelected, pairwise_,
          &PairwisePanel::selectClusterParent);
  connect(canvas_, &NetworkCanvas::selectionChanged, this,
          [this](const QString& cluster, const QString& node) {
            if (!node.isEmpty()) pairwise_->selectNodeParent(node);
            else if (!cluster.isEmpty()) pairwise_->selectClusterParent(cluster);
          });
  connect(canvas_, &NetworkCanvas::nodeActivated, doc_, &Document::pushSubnet);

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

  resize(1280, 800);
  updateTitle();
  statusBar()->showMessage(
      QStringLiteral("Ready — right-click canvas to add clusters/nodes"));
}

void MainWindow::updateTitle() {
  QString title = QStringLiteral("cppanp");
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
      QStringLiteral("cppanp JSON (*.json);;All files (*)"));
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
      QStringLiteral("cppanp JSON (*.json);;All files (*)"));
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
