#include "mainwindow.hpp"

#include "canvas/network_canvas.hpp"
#include "commands/network_commands.hpp"
#include "document.hpp"
#include "io/judgment_template_io.hpp"
#include "panels/analysis_panel.hpp"
#include "panels/inspector_panel.hpp"
#include "panels/judgment_nav_panel.hpp"
#include "panels/judgment_priorities_panel.hpp"
#include "panels/pairwise_panel.hpp"
#include "panels/participants_roster_dialog.hpp"
#include "panels/ratings_panel.hpp"
#include "panels/researcher_panel.hpp"
#include "panels/session_panel.hpp"

#include <QAction>
#include <QAbstractButton>
#include <QApplication>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
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
#include <QSysInfo>
#include <QUndoStack>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

namespace {
constexpr auto kDocsBaseUrl = "https://bamath.org/anp-studio";
}  // namespace

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
  addStageBtn(QStringLiteral("Researcher"), Stage::Researcher);
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
  connect(doc_, &Document::sessionChanged, this, &MainWindow::updateBreadcrumb);

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
  netMenu->addAction(QStringLiteral("Organize Clusters"), this, [this]() {
    canvas_->organizeClusters();
  });
  netMenu->addAction(QStringLiteral("Up Subnetwork"), doc_,
                     &Document::popSubnet);
  netMenu->addAction(QStringLiteral("Root Network"), doc_,
                     &Document::popToRoot);

  auto* participantsMenu = menuBar()->addMenu(QStringLiteral("&Participants"));
  participantsMenu->addAction(QStringLiteral("&Manage participants…"), this,
                              &MainWindow::onManageParticipants);
  participantsMenu->addSeparator();
  participantsMenu->addAction(QStringLiteral("Export Excel &templates…"),
                              this, &MainWindow::onExportJudgmentTemplates);
  participantsMenu->addAction(QStringLiteral("Import judgment t&emplates…"),
                              this, &MainWindow::onImportJudgmentTemplates);
  auto* helpMenu = menuBar()->addMenu(QStringLiteral("&Help"));
  helpMenu->addAction(QStringLiteral("User Guide"), this,
                      &MainWindow::openUserGuide);
  helpMenu->addAction(QStringLiteral("Glossary"), this,
                      &MainWindow::openGlossary);
  helpMenu->addSeparator();
  auto* aboutAction = helpMenu->addAction(
      QStringLiteral("About ANP Studio…"), this, &MainWindow::showAbout);
  aboutAction->setMenuRole(QAction::AboutRole);

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
      QStringLiteral("Structure → Judgments → Analysis → Researcher"));
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
                  QStringLiteral(
                      "Structure → Judgments → Analysis → Researcher"));
            }
          });

  auto* sLay = new QVBoxLayout(structurePage);
  sLay->setContentsMargins(0, 0, 0, 0);
  sLay->setSpacing(0);
  sLay->addWidget(modeBar);
  sLay->addWidget(sSplit, 1);
  stages_->addWidget(structurePage);

  // Judgments: left config dock | pairwise/ratings | session + priorities chart
  judgmentNav_ = new JudgmentNavPanel(doc_, this);
  pairwise_ = new PairwisePanel(doc_, this);
  ratings_ = new RatingsPanel(doc_, this);
  sessionPanel_ = new SessionPanel(doc_, this);
  judgmentPriorities_ = new JudgmentPrioritiesPanel(doc_, this);
  judgmentCenter_ = new QStackedWidget(this);
  judgmentCenter_->addWidget(pairwise_);
  judgmentCenter_->addWidget(ratings_);

  auto* rightSplit = new QSplitter(Qt::Vertical, this);
  rightSplit->addWidget(sessionPanel_);
  rightSplit->addWidget(judgmentPriorities_);
  rightSplit->setStretchFactor(0, 2);
  rightSplit->setStretchFactor(1, 1);

  auto* judgmentsPage = new QWidget(stages_);
  auto* jLay = new QVBoxLayout(judgmentsPage);
  jLay->setContentsMargins(0, 0, 0, 0);
  jLay->setSpacing(0);
  auto* jSplit = new QSplitter(Qt::Horizontal, judgmentsPage);
  jSplit->addWidget(judgmentNav_);
  jSplit->addWidget(judgmentCenter_);
  jSplit->addWidget(rightSplit);
  jSplit->setStretchFactor(0, 0);
  jSplit->setStretchFactor(1, 4);
  jSplit->setStretchFactor(2, 1);
  jSplit->setCollapsible(0, false);
  jLay->addWidget(jSplit, 1);
  stages_->addWidget(judgmentsPage);

  // Analysis: left-tabbed Synthesis / Sensitivity / Influence
  analysis_ = new AnalysisPanel(doc_, this);
  stages_->addWidget(analysis_);

  // Researcher: DSL notebook over thisModel / parentModel
  researcher_ = new ResearcherPanel(doc_, this);
  stages_->addWidget(researcher_);
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
  // Apply any coalesced model refresh before stage-specific UI reads state.
  doc_->flushModelChanged();
  updateBreadcrumb();
  if (stage == Stage::Structure) {
    canvas_->select(doc_->selectedCluster(), doc_->selectedNode());
  } else if (stage == Stage::Judgments) {
    judgmentNav_->refresh();
  } else if (stage == Stage::Researcher) {
    researcher_->refreshBindings();
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

  // Phase 2 (light): Scope combo beside the network chooser on Analysis and
  // Researcher — same document session as Judgments, no separate sync.
  scopeCombo_ = nullptr;
  if ((stage_ == Stage::Analysis || stage_ == Stage::Researcher) &&
      doc_->hasParticipants()) {
    auto* sep = new QLabel(QStringLiteral("·"), breadcrumbBar_);
    sep->setObjectName(QStringLiteral("breadcrumbSep"));
    breadcrumbLay_->addWidget(sep);
    auto* scopeCaption = new QLabel(QStringLiteral("Scope:"), breadcrumbBar_);
    scopeCaption->setObjectName(QStringLiteral("breadcrumbCaption"));
    breadcrumbLay_->addWidget(scopeCaption);

    scopeCombo_ = new QComboBox(breadcrumbBar_);
    scopeCombo_->setMinimumWidth(160);
    const anpcpp::JudgmentSession session = doc_->judgmentSession();
    int curIdx = 0;
    scopeCombo_->addItem(QStringLiteral("Group average"));
    scopeCombo_->setItemData(0, QString(), Qt::UserRole);
    scopeCombo_->setItemData(0, QStringLiteral("average"), Qt::UserRole + 1);
    if (session.kind == anpcpp::JudgmentScopeKind::Average) curIdx = 0;
    for (const auto& p : doc_->participants()) {
      const int i = scopeCombo_->count();
      scopeCombo_->addItem(QString::fromStdString(p.name));
      scopeCombo_->setItemData(i, QString::fromStdString(p.id), Qt::UserRole);
      scopeCombo_->setItemData(i, QStringLiteral("participant"),
                              Qt::UserRole + 1);
      if (session.kind == anpcpp::JudgmentScopeKind::Participant &&
          session.id == p.id) {
        curIdx = i;
      }
    }
    for (const auto& g : doc_->judgmentGroups()) {
      const int i = scopeCombo_->count();
      scopeCombo_->addItem(QString::fromStdString(g.name));
      scopeCombo_->setItemData(i, QString::fromStdString(g.id), Qt::UserRole);
      scopeCombo_->setItemData(i, QStringLiteral("group"), Qt::UserRole + 1);
      if (session.kind == anpcpp::JudgmentScopeKind::Group &&
          session.id == g.id) {
        curIdx = i;
      }
    }
    scopeCombo_->setCurrentIndex(curIdx);
    connect(scopeCombo_, QOverload<int>::of(&QComboBox::activated), this,
            &MainWindow::onScopeChosen);
    breadcrumbLay_->addWidget(scopeCombo_);
    breadcrumbLay_->addStretch();
  }
}

void MainWindow::onNetworkPathChosen(int index) {
  if (networkPathCombo_ == nullptr || index < 0) return;
  doc_->navigateToNetworkPath(networkPathCombo_->itemText(index));
}

void MainWindow::onScopeChosen(int index) {
  if (scopeCombo_ == nullptr || index < 0) return;
  const QString kind = scopeCombo_->itemData(index, Qt::UserRole + 1).toString();
  const QString id = scopeCombo_->itemData(index, Qt::UserRole).toString();
  anpcpp::JudgmentSession session;
  if (kind == QLatin1String("participant")) {
    session.kind = anpcpp::JudgmentScopeKind::Participant;
    session.id = id.toStdString();
  } else if (kind == QLatin1String("group")) {
    session.kind = anpcpp::JudgmentScopeKind::Group;
    session.id = id.toStdString();
  } else {
    session.kind = anpcpp::JudgmentScopeKind::Average;
  }
  doc_->setJudgmentSession(session);
}

void MainWindow::onManageParticipants() {
  showParticipantsRosterDialog(this, doc_);
}








void MainWindow::onExportJudgmentTemplates() {
  if (doc_->root().participants().empty()) {
    QMessageBox::information(
        this, QStringLiteral("Export Excel templates"),
        QStringLiteral(
            "Add participants first (Participants → Manage participants…)."));
    return;
  }

  JudgmentTemplateExportOptions options;
  {
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("Export Excel templates"));
    box.setIcon(QMessageBox::Question);
    box.setText(QStringLiteral(
        "Export one Excel workbook per participant into a folder."));
    box.setInformativeText(QStringLiteral(
        "Each file is named ANP_judgments_<Name>.xlsx for sharing. "
        "Respondents fill the yellow \"Your rating\" column on the "
        "Your judgments sheet. Identity is stored in a hidden _meta sheet "
        "(not the filename).\n\n"
        "Leave values blank for respondents to fill, or include their "
        "current votes."));
    auto* blankBtn =
        box.addButton(QStringLiteral("Blank templates"), QMessageBox::AcceptRole);
    auto* filledBtn = box.addButton(QStringLiteral("Include existing votes"),
                                    QMessageBox::ActionRole);
    box.addButton(QMessageBox::Cancel);
    box.exec();
    if (box.clickedButton() == filledBtn) {
      options.includeExistingVotes = true;
    } else if (box.clickedButton() != blankBtn) {
      return;
    }
  }

  const QString dir = QFileDialog::getExistingDirectory(
      this, QStringLiteral("Folder for Excel templates"));
  if (dir.isEmpty()) return;

  const JudgmentTemplateExportResult result =
      exportJudgmentTemplates(*doc_, dir, options);
  if (!result.ok) {
    QMessageBox::warning(this, QStringLiteral("Export Excel templates"),
                         result.error);
    return;
  }

  QMessageBox::information(
      this, QStringLiteral("Export complete"),
      QStringLiteral("Wrote %1 Excel template(s) to:\n%2\n\n"
                     "Share each file with that person. When they return it, "
                     "use Participants → Import judgment templates…")
          .arg(result.filesWritten)
          .arg(dir));
}

void MainWindow::onImportJudgmentTemplates() {
  QFileDialog dlg(this, QStringLiteral("Import judgment templates"));
  dlg.setFileMode(QFileDialog::ExistingFiles);  // multi-select
  dlg.setNameFilter(
      QStringLiteral("Excel templates (*.xlsx);;"
                     "Judgment templates (*.xlsx *.csv);;"
                     "CSV (legacy) (*.csv);;All files (*)"));
  dlg.setLabelText(QFileDialog::Accept,
                   QStringLiteral("Import"));
  if (dlg.exec() != QDialog::Accepted) return;
  const QStringList paths = dlg.selectedFiles();
  if (paths.isEmpty()) return;

  const JudgmentTemplateImportResult result =
      importJudgmentTemplates(*doc_, paths);
  if (!result.ok) {
    QMessageBox::warning(this, QStringLiteral("Import judgment templates"),
                         result.error);
    return;
  }

  QString msg =
      QStringLiteral(
          "Processed %1 template(s).\n"
          "Created %2 participant(s).\n"
          "Set %3 judgment value(s).\n")
          .arg(result.filesProcessed)
          .arg(result.participantsCreated)
          .arg(result.judgmentsSet);
  if (result.judgmentsSkipped > 0) {
    msg +=
        QStringLiteral("Skipped %1 row(s).\n").arg(result.judgmentsSkipped);
  }
  if (!result.createdParticipantNames.isEmpty()) {
    msg += QStringLiteral("\nNew participants:\n- ") +
           result.createdParticipantNames.join(QStringLiteral("\n- "));
  }
  if (!result.notes.isEmpty()) {
    msg += QStringLiteral("\n\n") + result.notes.join(QLatin1Char('\n'));
  }
  QMessageBox::information(this, QStringLiteral("Import complete"), msg);
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
  canvas_->persistLayout();
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
  canvas_->persistLayout();
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

void MainWindow::openUserGuide() {
  QDesktopServices::openUrl(
      QUrl(QStringLiteral("%1/guide/").arg(QLatin1String(kDocsBaseUrl))));
}

void MainWindow::openGlossary() {
  QDesktopServices::openUrl(QUrl(
      QStringLiteral("%1/guide/glossary/").arg(QLatin1String(kDocsBaseUrl))));
}

void MainWindow::showAbout() {
  const QString version = QCoreApplication::applicationVersion();
  const QString text = QStringLiteral(
      "<h3>ANP Studio %1</h3>"
      "<p>Analytic Network Process desktop modeling.</p>"
      "<p>"
      "<b>Operating system:</b> %2<br>"
      "<b>CPU architecture:</b> %3<br>"
      "<b>Qt:</b> %4"
      "</p>"
      "<p>"
      "<a href=\"%5/\">Website</a> · "
      "<a href=\"%5/guide/\">User guide</a> · "
      "<a href=\"https://github.com/wjladams/anp-studio/releases\">Releases</a>"
      "</p>"
      "<p>Copyright © 2026 William Adams<br>"
      "MIT License — see the repository for details.</p>")
                           .arg(version,
                                QSysInfo::prettyProductName(),
                                QSysInfo::currentCpuArchitecture(),
                                QString::fromLatin1(qVersion()),
                                QLatin1String(kDocsBaseUrl));
  QMessageBox about(this);
  about.setWindowTitle(QStringLiteral("About ANP Studio"));
  about.setTextFormat(Qt::RichText);
  about.setText(text);
  about.setIconPixmap(windowIcon().pixmap(64, 64));
  about.exec();
}
