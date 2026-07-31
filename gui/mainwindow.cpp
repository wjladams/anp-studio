#include "mainwindow.hpp"

#include "canvas/network_canvas.hpp"
#include "commands/network_commands.hpp"
#include "document.hpp"
#include "panels/analysis_panel.hpp"
#include "panels/inspector_panel.hpp"
#include "panels/judgment_nav_panel.hpp"
#include "panels/judgment_priorities_panel.hpp"
#include "panels/pairwise_panel.hpp"
#include "panels/ratings_panel.hpp"

#include <QAction>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
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
  addStageBtn(QStringLiteral("Analysis"), Stage::Analysis);
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

  auto* fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
  fileMenu->addAction(QStringLiteral("&New"), this, &MainWindow::newFile,
                      QKeySequence::New);
  fileMenu->addAction(QStringLiteral("&Open…"), this, &MainWindow::openFile,
                      QKeySequence::Open);
  recentMenu_ = fileMenu->addMenu(QStringLiteral("Open &Recent"));
  connect(recentMenu_, &QMenu::aboutToShow, this, &MainWindow::rebuildRecentMenu);
  rebuildRecentMenu();
  sampleMenu_ = fileMenu->addMenu(QStringLiteral("Open &Sample…"));
  connect(sampleMenu_, &QMenu::aboutToShow, this, &MainWindow::rebuildSampleMenu);
  rebuildSampleMenu();
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
      netMenu->addAction(QStringLiteral("Connection Mode"), this, [this]() {
        canvas_->setConnectMode(connectModeAction_->isChecked());
      });
  connectModeAction_->setCheckable(true);
  netMenu->addAction(QStringLiteral("Up Subnetwork"), doc_,
                     &Document::popSubnet);
  netMenu->addAction(QStringLiteral("Root Network"), doc_,
                     &Document::popToRoot);

  auto* computeMenu = menuBar()->addMenu(QStringLiteral("&Compute"));
  computeMenu->addAction(QStringLiteral("Show Analysis"), this, [this]() {
    setStage(Stage::Analysis);
  }, QKeySequence(Qt::Key_F5));

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
      QStringLiteral("Structure → Judgments → Analysis"));
}

void MainWindow::buildStagePages() {
  // Structure: mode strip | canvas | inspector
  canvas_ = new NetworkCanvas(doc_, this);
  inspector_ = new InspectorPanel(doc_, this);
  auto* structurePage = new QWidget(stages_);
  auto* sSplit = new QSplitter(Qt::Horizontal, structurePage);
  sSplit->addWidget(canvas_);
  sSplit->addWidget(inspector_);
  sSplit->setStretchFactor(0, 4);
  sSplit->setStretchFactor(1, 1);

  auto* modeBar = new QWidget(structurePage);
  modeBar->setObjectName(QStringLiteral("structureModeBar"));
  auto* modeLay = new QHBoxLayout(modeBar);
  modeLay->setContentsMargins(8, 6, 8, 6);
  modeLay->setSpacing(0);
  modeLay->addStretch();
  structureModeButtons_ = new QButtonGroup(modeBar);
  structureModeButtons_->setExclusive(true);
  auto* normalBtn = new QPushButton(QStringLiteral("Normal"), modeBar);
  normalBtn->setObjectName(QStringLiteral("structureModeBtnNormal"));
  normalBtn->setCheckable(true);
  normalBtn->setChecked(true);
  normalBtn->setCursor(Qt::PointingHandCursor);
  auto* connectionBtn = new QPushButton(QStringLiteral("Connection"), modeBar);
  connectionBtn->setObjectName(QStringLiteral("structureModeBtnConnection"));
  connectionBtn->setCheckable(true);
  connectionBtn->setCursor(Qt::PointingHandCursor);
  structureModeButtons_->addButton(normalBtn, 0);
  structureModeButtons_->addButton(connectionBtn, 1);
  modeLay->addWidget(normalBtn);
  modeLay->addWidget(connectionBtn);
  modeLay->addStretch();

  connect(structureModeButtons_, &QButtonGroup::idClicked, this,
          [this](int id) {
            canvas_->setConnectMode(id == 1);
          });
  connect(canvas_, &NetworkCanvas::connectModeChanged, this,
          [this](bool on) {
            if (structureModeButtons_ != nullptr) {
              if (auto* b = structureModeButtons_->button(on ? 1 : 0)) {
                b->setChecked(true);
              }
            }
            if (connectModeAction_ != nullptr) {
              connectModeAction_->setChecked(on);
            }
            if (on) {
              statusBar()->showMessage(
                  QStringLiteral("Connection mode: click source(s), "
                                 "right-click destination(s). Esc = Normal."));
            } else if (stage_ == Stage::Structure) {
              statusBar()->showMessage(
                  QStringLiteral("Structure → Judgments → Analysis"));
            }
          });

  auto* sLay = new QVBoxLayout(structurePage);
  sLay->setContentsMargins(0, 0, 0, 0);
  sLay->setSpacing(0);
  sLay->addWidget(modeBar);
  sLay->addWidget(sSplit, 1);
  stages_->addWidget(structurePage);

  // Judgments: left config dock | pairwise/ratings | priorities chart
  judgmentNav_ = new JudgmentNavPanel(doc_, this);
  pairwise_ = new PairwisePanel(doc_, this);
  ratings_ = new RatingsPanel(doc_, this);
  judgmentPriorities_ = new JudgmentPrioritiesPanel(doc_, this);
  judgmentCenter_ = new QStackedWidget(this);
  judgmentCenter_->addWidget(pairwise_);
  judgmentCenter_->addWidget(ratings_);
  auto* judgmentsPage = new QWidget(stages_);
  auto* jLay = new QVBoxLayout(judgmentsPage);
  jLay->setContentsMargins(0, 0, 0, 0);
  jLay->setSpacing(0);
  auto* jSplit = new QSplitter(Qt::Horizontal, judgmentsPage);
  jSplit->addWidget(judgmentNav_);
  jSplit->addWidget(judgmentCenter_);
  jSplit->addWidget(judgmentPriorities_);
  jSplit->setStretchFactor(0, 0);
  jSplit->setStretchFactor(1, 4);
  jSplit->setStretchFactor(2, 1);
  jSplit->setCollapsible(0, false);
  jLay->addWidget(jSplit, 1);
  stages_->addWidget(judgmentsPage);

  // Analysis: left-tabbed Synthesis / Sensitivity / Influence
  analysis_ = new AnalysisPanel(doc_, this);
  stages_->addWidget(analysis_);
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
  if (stage != Stage::Structure && canvas_ != nullptr && canvas_->connectMode()) {
    canvas_->setConnectMode(false);
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
    judgmentPriorities_->showNodeRatings(parent, destCluster);
  } else {
    judgmentCenter_->setCurrentWidget(pairwise_);
    pairwise_->selectNodeLink(parent, destCluster);
    judgmentPriorities_->showNodePairwise(parent, destCluster);
  }
}

void MainWindow::onJudgmentClusterSelected(const QString& parent) {
  judgmentCenter_->setCurrentWidget(pairwise_);
  pairwise_->selectClusterParent(parent);
  judgmentPriorities_->showClusterPairwise(parent);
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

  networkPathCombo_ = new QComboBox(breadcrumbBar_);
  networkPathCombo_->setMinimumWidth(220);
  const QStringList options = doc_->networkPathOptions();
  networkPathCombo_->addItems(options);
  const int curIdx = networkPathCombo_->findText(doc_->currentNetworkPath());
  if (curIdx >= 0) networkPathCombo_->setCurrentIndex(curIdx);
  connect(networkPathCombo_, QOverload<int>::of(&QComboBox::activated), this,
          &MainWindow::onNetworkPathChosen);
  breadcrumbLay_->addWidget(networkPathCombo_);

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
      const int depth = i + 1;
      connect(link, &QPushButton::clicked, this, [this, depth]() {
        doc_->popToDepth(depth);
      });
      breadcrumbLay_->addWidget(link);
    }
  }
  breadcrumbLay_->addStretch();
}

void MainWindow::onNetworkPathChosen(int index) {
  if (networkPathCombo_ == nullptr || index < 0) return;
  doc_->navigateToNetworkPath(networkPathCombo_->itemText(index));
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
  (void)openPath(path);
}

bool MainWindow::openPath(const QString& path) {
  QString err;
  if (!doc_->loadFromFile(path, &err)) {
    QMessageBox::warning(this, QStringLiteral("Open failed"), err);
    return false;
  }
  rememberRecentFile(path);
  updateTitle();
  return true;
}

bool MainWindow::openSamplePath(const QString& path) {
  QString err;
  if (!doc_->loadFromFile(path, &err)) {
    QMessageBox::warning(this, QStringLiteral("Open Sample failed"), err);
    return false;
  }
  // Do not bind Save to the distribution/copy path.
  doc_->clearPath();
  doc_->setDirty(false);
  updateTitle();
  statusBar()->showMessage(
      QStringLiteral("Opened sample “%1” — Save As to keep a copy.")
          .arg(sampleDisplayName(QFileInfo(path).fileName())),
      8000);
  return true;
}

QString MainWindow::samplesDirectory() {
  const QString appDir = QCoreApplication::applicationDirPath();
  const QStringList candidates = {
      appDir + QStringLiteral("/samples"),
      appDir + QStringLiteral("/../Resources/samples"),
      appDir + QStringLiteral("/../share/anpstudio/samples"),
  };
  for (const QString& dir : candidates) {
    const QFileInfo fi(dir);
    if (!fi.isDir()) continue;
    const QDir d(fi.absoluteFilePath());
    if (!d.entryList({QStringLiteral("*.json")}, QDir::Files).isEmpty()) {
      return fi.absoluteFilePath();
    }
  }

  // Dev fallback: walk up looking for repo-root samples/ next to CMakeLists.txt.
  QDir walk(appDir);
  for (int i = 0; i < 6; ++i) {
    const QString samples = walk.filePath(QStringLiteral("samples"));
    const QString cmake = walk.filePath(QStringLiteral("CMakeLists.txt"));
    if (QFileInfo::exists(cmake) && QFileInfo(samples).isDir()) {
      const QDir d(samples);
      if (!d.entryList({QStringLiteral("*.json")}, QDir::Files).isEmpty()) {
        return QFileInfo(samples).absoluteFilePath();
      }
    }
    if (!walk.cdUp()) break;
  }
  return {};
}

QString MainWindow::sampleDisplayName(const QString& fileName) {
  QString base = QFileInfo(fileName).completeBaseName();
  // Strip leading catalog index: "02_ahp_best_car" -> "ahp_best_car"
  if (base.size() > 3 && base[0].isDigit() && base[1].isDigit() &&
      base[2] == QLatin1Char('_')) {
    base = base.mid(3);
  }
  base.replace(QLatin1Char('_'), QLatin1Char(' '));
  if (!base.isEmpty()) {
    base[0] = base[0].toUpper();
  }
  return base;
}

void MainWindow::rebuildSampleMenu() {
  if (sampleMenu_ == nullptr) return;
  sampleMenu_->clear();

  const QString dirPath = samplesDirectory();
  if (dirPath.isEmpty()) {
    auto* empty = sampleMenu_->addAction(QStringLiteral("(No samples found)"));
    empty->setEnabled(false);
    return;
  }

  QDir dir(dirPath);
  const QStringList files =
      dir.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
  if (files.isEmpty()) {
    auto* empty = sampleMenu_->addAction(QStringLiteral("(No samples found)"));
    empty->setEnabled(false);
    return;
  }

  for (const QString& file : files) {
    auto* act = sampleMenu_->addAction(sampleDisplayName(file));
    act->setData(dir.absoluteFilePath(file));
    act->setToolTip(dir.absoluteFilePath(file));
    connect(act, &QAction::triggered, this, &MainWindow::openSampleFile);
  }
}

void MainWindow::openSampleFile() {
  auto* act = qobject_cast<QAction*>(sender());
  if (act == nullptr) return;
  const QString path = act->data().toString();
  if (path.isEmpty()) return;
  if (!maybeSave()) return;
  if (!QFileInfo::exists(path)) {
    QMessageBox::warning(
        this, QStringLiteral("Open Sample"),
        QStringLiteral("Sample file no longer exists:\n%1").arg(path));
    return;
  }
  (void)openSamplePath(path);
}

bool MainWindow::saveFile() {
  if (doc_->path().isEmpty()) return saveFileAs();
  QString err;
  if (!doc_->saveToFile(doc_->path(), &err)) {
    QMessageBox::warning(this, QStringLiteral("Save failed"), err);
    return false;
  }
  rememberRecentFile(doc_->path());
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
  rememberRecentFile(path);
  updateTitle();
  return true;
}

void MainWindow::rememberRecentFile(const QString& path) {
  const QString abs = QFileInfo(path).absoluteFilePath();
  if (abs.isEmpty()) return;
  QSettings settings;
  QStringList recent = settings.value(QStringLiteral("recentFiles")).toStringList();
  recent.removeAll(abs);
  recent.prepend(abs);
  while (recent.size() > kMaxRecentFiles) {
    recent.removeLast();
  }
  settings.setValue(QStringLiteral("recentFiles"), recent);
}

void MainWindow::rebuildRecentMenu() {
  if (recentMenu_ == nullptr) return;
  recentMenu_->clear();

  QSettings settings;
  QStringList recent = settings.value(QStringLiteral("recentFiles")).toStringList();
  QStringList existing;
  existing.reserve(recent.size());
  for (const QString& path : recent) {
    if (QFileInfo::exists(path)) {
      existing.append(path);
    }
  }
  if (existing.size() != recent.size()) {
    settings.setValue(QStringLiteral("recentFiles"), existing);
  }

  if (existing.isEmpty()) {
    auto* empty = recentMenu_->addAction(QStringLiteral("(No recent files)"));
    empty->setEnabled(false);
    return;
  }

  int i = 0;
  for (const QString& path : existing) {
    QString label = QFileInfo(path).fileName();
    if (i < 9) {
      label = QStringLiteral("&%1 %2").arg(i + 1).arg(label);
    }
    auto* act = recentMenu_->addAction(label);
    act->setData(path);
    act->setToolTip(path);
    connect(act, &QAction::triggered, this, &MainWindow::openRecentFile);
    ++i;
  }
  recentMenu_->addSeparator();
  recentMenu_->addAction(QStringLiteral("Clear Recent"), this,
                         &MainWindow::clearRecentFiles);
}

void MainWindow::openRecentFile() {
  auto* act = qobject_cast<QAction*>(sender());
  if (act == nullptr) return;
  const QString path = act->data().toString();
  if (path.isEmpty()) return;
  if (!maybeSave()) return;
  if (!QFileInfo::exists(path)) {
    QMessageBox::warning(
        this, QStringLiteral("Open Recent"),
        QStringLiteral("File no longer exists:\n%1").arg(path));
    QSettings settings;
    QStringList recent =
        settings.value(QStringLiteral("recentFiles")).toStringList();
    recent.removeAll(QFileInfo(path).absoluteFilePath());
    recent.removeAll(path);
    settings.setValue(QStringLiteral("recentFiles"), recent);
    return;
  }
  (void)openPath(path);
}

void MainWindow::clearRecentFiles() {
  QSettings settings;
  settings.remove(QStringLiteral("recentFiles"));
  rebuildRecentMenu();
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
