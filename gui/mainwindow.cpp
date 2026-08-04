#include "mainwindow.hpp"

/**
 * @file mainwindow.cpp
 * @brief Stage shell, menus, breadcrumb Scope, and multi-user I/O entry points.
 */

#include "canvas/network_canvas.hpp"
#include "commands/network_commands.hpp"
#include "document.hpp"
#include "panels/analysis_panel.hpp"
#include "panels/inspector_panel.hpp"
#include "panels/judgment_nav_panel.hpp"
#include "panels/judgment_priorities_panel.hpp"
#include "panels/pairwise_panel.hpp"
#include "panels/participants_roster_dialog.hpp"
#include "panels/collect_judgments_dialog.hpp"
#include "panels/ratings_panel.hpp"
#include "panels/researcher_panel.hpp"
#include "panels/session_panel.hpp"
#include "panels/settings_dialog.hpp"
#include "oauth/google_oauth.hpp"
#include "oauth/google_forms_client.hpp"
#include "io/judgment_template_io.hpp"

#include <QAction>
#include <QActionGroup>
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
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSizePolicy>
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

const QString kFilterAnpstudio = QStringLiteral("ANP Studio (*.anpstudio)");
const QString kFilterJson = QStringLiteral("ANP Studio JSON (*.json)");
const QString kFilterAll = QStringLiteral("All files (*)");

QString openNameFilters() {
  return kFilterAnpstudio + QStringLiteral(";;") + kFilterJson +
         QStringLiteral(";;") + kFilterAll;
}

/** @return True if @p dir contains model samples (.anpstudio or legacy .json). */
bool dirHasModelSamples(const QDir& d) {
  return !d.entryList({QStringLiteral("*.anpstudio"), QStringLiteral("*.json")},
                      QDir::Files)
              .isEmpty();
}

QStringList listModelSampleFiles(const QDir& d) {
  return d.entryList({QStringLiteral("*.anpstudio"), QStringLiteral("*.json")},
                     QDir::Files, QDir::Name);
}

/**
 * @brief Ensures Save As path has a model suffix based on the chosen filter.
 *
 * Leaves existing .anpstudio / .json paths alone (no forced rename).
 */
QString ensureModelSaveSuffix(QString path, const QString& selectedFilter) {
  const QFileInfo fi(path);
  const QString suffix = fi.suffix().toLower();
  if (suffix == QLatin1String("anpstudio") || suffix == QLatin1String("json")) {
    return path;
  }
  const bool wantJson =
      selectedFilter.contains(QStringLiteral("*.json"), Qt::CaseInsensitive);
  if (wantJson) {
    return path + QStringLiteral(".json");
  }
  return path + QStringLiteral(".anpstudio");
}
}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  doc_ = new Document(this);
  googleOAuth_ = new GoogleOAuth(this);

  auto* central = new QWidget(this);
  auto* rootLay = new QVBoxLayout(central);
  rootLay->setContentsMargins(12, 8, 12, 8);
  rootLay->setSpacing(8);

  auto* stageRow = new QHBoxLayout;
  stageRow->setContentsMargins(0, 0, 0, 0);
  stageRow->setSpacing(0);
  auto* stageTrack = new QWidget(central);
  stageTrack->setObjectName(QStringLiteral("stageTabTrack"));
  auto* trackLay = new QHBoxLayout(stageTrack);
  trackLay->setContentsMargins(3, 3, 3, 3);
  trackLay->setSpacing(2);
  stageButtons_ = new QButtonGroup(this);
  stageButtons_->setExclusive(true);
  auto addStageBtn = [&](const QString& text, Stage s) {
    auto* b = new QPushButton(text, stageTrack);
    b->setObjectName(QStringLiteral("stageTab"));
    b->setCheckable(true);
    b->setFlat(true);
    b->setCursor(Qt::PointingHandCursor);
    stageButtons_->addButton(b, static_cast<int>(s));
    trackLay->addWidget(b);
  };
  addStageBtn(QStringLiteral("Structure"), Stage::Structure);
  addStageBtn(QStringLiteral("Judgments"), Stage::Judgments);
  addStageBtn(QStringLiteral("Analysis"), Stage::Analysis);
  addStageBtn(QStringLiteral("Researcher"), Stage::Researcher);
  stageRow->addWidget(stageTrack, 0, Qt::AlignVCenter);

  breadcrumbBar_ = new QWidget(central);
  breadcrumbBar_->setObjectName(QStringLiteral("breadcrumbBar"));
  breadcrumbBar_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
  breadcrumbLay_ = new QHBoxLayout(breadcrumbBar_);
  breadcrumbLay_->setContentsMargins(0, 0, 0, 0);
  breadcrumbLay_->setSpacing(4);
  stageRow->addStretch(1);
  stageRow->addWidget(breadcrumbBar_, 0, Qt::AlignVCenter);
  rootLay->addLayout(stageRow);

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
  recentShortcutActions_.reserve(kRecentShortcutCount);
  for (int i = 0; i < kRecentShortcutCount; ++i) {
    auto* act = new QAction(this);
    act->setShortcut(QKeySequence(Qt::CTRL | static_cast<Qt::Key>(Qt::Key_1 + i)));
    act->setShortcutContext(Qt::WindowShortcut);
    act->setEnabled(false);
    connect(act, &QAction::triggered, this, &MainWindow::openRecentFile);
    addAction(act);
    recentShortcutActions_.append(act);
  }
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
  auto* settingsAction = fileMenu->addAction(
      QStringLiteral("Setti&ngs…"), this, &MainWindow::onSettings);
  settingsAction->setShortcut(QKeySequence::Preferences);
  settingsAction->setMenuRole(QAction::PreferencesRole);
  fileMenu->addSeparator();
  fileMenu->addAction(QStringLiteral("&Close"), this, &QWidget::close,
                      QKeySequence::Close);
  auto* quitAction = fileMenu->addAction(QStringLiteral("&Quit"), this,
                                         &QWidget::close, QKeySequence::Quit);
  quitAction->setMenuRole(QAction::QuitRole);

  auto* editMenu = menuBar()->addMenu(QStringLiteral("&Edit"));
  editMenu->addAction(doc_->undoStack()->createUndoAction(
      this, QStringLiteral("Undo")));
  editMenu->addAction(doc_->undoStack()->createRedoAction(
      this, QStringLiteral("Redo")));

  auto* viewMenu = menuBar()->addMenu(QStringLiteral("&View"));
  stageActionGroup_ = new QActionGroup(this);
  stageActionGroup_->setExclusive(true);
  auto addStageAction = [&](const QString& text, Stage stage, Qt::Key digit) {
    auto* act = viewMenu->addAction(text);
    act->setCheckable(true);
    act->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | digit));
    act->setShortcutContext(Qt::WindowShortcut);
    stageActionGroup_->addAction(act);
    connect(act, &QAction::triggered, this, [this, stage]() { setStage(stage); });
    return act;
  };
  stageStructureAction_ =
      addStageAction(QStringLiteral("&Structure"), Stage::Structure, Qt::Key_1);
  stageJudgmentsAction_ =
      addStageAction(QStringLiteral("&Judgments"), Stage::Judgments, Qt::Key_2);
  stageAnalysisAction_ =
      addStageAction(QStringLiteral("&Analysis"), Stage::Analysis, Qt::Key_3);
  stageResearcherAction_ =
      addStageAction(QStringLiteral("&Researcher"), Stage::Researcher, Qt::Key_4);

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
  participantsMenu->addAction(QStringLiteral("&Collect judgments…"), this,
                              &MainWindow::onCollectJudgments);

  auto* computeMenu = menuBar()->addMenu(QStringLiteral("&Compute"));
  computeMenu->addAction(QStringLiteral("Show Analysis"), this, [this]() {
    setStage(Stage::Analysis);
  }, QKeySequence(Qt::Key_F5));
  computeMenu->addAction(QStringLiteral("Show Researcher"), this, [this]() {
    setStage(Stage::Researcher);
  });

  auto* helpMenu = menuBar()->addMenu(QStringLiteral("&Help"));
  auto* guideAction = helpMenu->addAction(QStringLiteral("User Guide"), this,
                                          &MainWindow::openUserGuide);
#ifndef Q_OS_MACOS
  guideAction->setShortcuts(
      {QKeySequence(QKeySequence::HelpContents),
       QKeySequence(Qt::CTRL | Qt::Key_H)});
#else
  guideAction->setShortcut(QKeySequence::HelpContents);
#endif
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

// --- Stage pages (Structure / Judgments / Analysis / Researcher) ------------

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

  // Judgments: left config dock | pairwise/ratings | Scope rail + priorities chart
  judgmentNav_ = new JudgmentNavPanel(doc_, this);
  pairwise_ = new PairwisePanel(doc_, this);
  ratings_ = new RatingsPanel(doc_, this);
  sessionPanel_ = new SessionPanel(doc_, this);
  connect(sessionPanel_, &SessionPanel::collectJudgmentsRequested, this,
          &MainWindow::onCollectJudgments);
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
  if (stageStructureAction_ != nullptr) {
    QAction* checked = nullptr;
    switch (stage) {
      case Stage::Structure:
        checked = stageStructureAction_;
        break;
      case Stage::Judgments:
        checked = stageJudgmentsAction_;
        break;
      case Stage::Analysis:
        checked = stageAnalysisAction_;
        break;
      case Stage::Researcher:
        checked = stageResearcherAction_;
        break;
    }
    if (checked != nullptr) {
      checked->setChecked(true);
    }
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

// --- Breadcrumb: Network path + shared Scope combo --------------------------

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

  // Shared Scope combo beside Network on Judgments, Analysis, and Researcher.
  // Same JudgmentSession as the Judgments Scope rail (SessionPanel).
  scopeCombo_ = nullptr;
  if ((stage_ == Stage::Judgments || stage_ == Stage::Analysis ||
       stage_ == Stage::Researcher) &&
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

    if (stage_ == Stage::Judgments) {
      auto* modeChip = new QLabel(breadcrumbBar_);
      if (session.kind == anpcpp::JudgmentScopeKind::Participant) {
        modeChip->setObjectName(QStringLiteral("scopeModeChipEditable"));
        modeChip->setText(QStringLiteral("Editable"));
      } else {
        modeChip->setObjectName(QStringLiteral("scopeModeChipReadonly"));
        modeChip->setText(QStringLiteral("Read-only"));
      }
      breadcrumbLay_->addWidget(modeChip);
    }
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
  const anpcpp::JudgmentSession old = doc_->judgmentSession();
  if (old.kind == session.kind && old.id == session.id) return;
  doc_->undoStack()->push(new SetJudgmentSessionCmd(doc_, session, old));
}

// --- Participants: roster, Collect hub, Excel, Google Forms -----------------

void MainWindow::onManageParticipants() {
  showParticipantsRosterDialog(this, doc_);
}

void MainWindow::onCollectJudgments() {
  CollectJudgmentsDialog dlg(doc_, googleOAuth_, this);
  connect(&dlg, &CollectJudgmentsDialog::exportExcelRequested, this,
          [this](const QString& participantId) {
            onExportJudgmentTemplates(participantId);
          });
  connect(&dlg, &CollectJudgmentsDialog::importExcelRequested, this,
          [this]() { onImportJudgmentTemplates(false); });
  connect(&dlg, &CollectJudgmentsDialog::importCsvRequested, this,
          [this]() { onImportJudgmentTemplates(true); });
  connect(&dlg, &CollectJudgmentsDialog::createGoogleFormRequested, this,
          &MainWindow::onCreateGoogleForm);
  connect(&dlg, &CollectJudgmentsDialog::importGoogleFormRequested, this,
          &MainWindow::onImportGoogleFormResults);
  connect(&dlg, &CollectJudgmentsDialog::openLinkedFormRequested, this,
          &MainWindow::onOpenLinkedGoogleForm);
  connect(&dlg, &CollectJudgmentsDialog::connectGoogleRequested, this,
          &MainWindow::onSettings);
  dlg.exec();
}

void MainWindow::onExportJudgmentTemplates(const QString& participantId) {
  if (doc_->root().participants().empty()) {
    QMessageBox::information(
        this, QStringLiteral("Export Excel templates"),
        QStringLiteral(
            "Add participants first (Participants → Manage participants…)."));
    return;
  }

  JudgmentTemplateExportOptions options;
  if (!participantId.isEmpty()) {
    options.participantIds << participantId;
  }

  {
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("Export Excel templates"));
    box.setIcon(QMessageBox::Question);
    if (participantId.isEmpty()) {
      box.setText(QStringLiteral(
          "Export one Excel workbook per participant into a folder."));
    } else {
      QString name = participantId;
      for (const auto& p : doc_->participants()) {
        if (QString::fromStdString(p.id) == participantId) {
          name = QString::fromStdString(p.name);
          break;
        }
      }
      box.setText(
          QStringLiteral("Export an Excel workbook for %1.").arg(name));
    }
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

  if (!participantId.isEmpty()) {
    const anpcpp::JudgmentParticipant* target = nullptr;
    for (const auto& p : doc_->participants()) {
      if (QString::fromStdString(p.id) == participantId) {
        target = &p;
        break;
      }
    }
    if (target == nullptr) {
      QMessageBox::warning(this, QStringLiteral("Export Excel templates"),
                           QStringLiteral("Selected participant was not found."));
      return;
    }
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Save Excel template"),
        judgmentTemplateFileName(*target),
        QStringLiteral("Excel workbook (*.xlsx)"));
    if (path.isEmpty()) return;
    QString err;
    if (!writeJudgmentTemplateXlsx(doc_->root(), *target, path, options,
                                   &err)) {
      QMessageBox::warning(this, QStringLiteral("Export Excel templates"), err);
      return;
    }
    QMessageBox::information(
        this, QStringLiteral("Export complete"),
        QStringLiteral("Wrote Excel template to:\n%1\n\n"
                       "When they return it, use Collect judgments… → "
                       "Import .xlsx…")
            .arg(path));
    return;
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
                     "use Collect judgments… → Import .xlsx…")
          .arg(result.filesWritten)
          .arg(dir));
}

void MainWindow::onImportJudgmentTemplates(bool preferCsv) {
  QFileDialog dlg(this, preferCsv ? QStringLiteral("Import Forms CSV")
                                  : QStringLiteral("Import judgment templates"));
  dlg.setFileMode(QFileDialog::ExistingFiles);  // multi-select
  if (preferCsv) {
    dlg.setNameFilter(
        QStringLiteral("CSV (*.csv);;"
                       "Excel templates (*.xlsx);;"
                       "Judgment templates (*.xlsx *.csv);;"
                       "All files (*)"));
  } else {
    dlg.setNameFilter(
        QStringLiteral("Excel templates (*.xlsx);;"
                       "Judgment templates (*.xlsx *.csv);;"
                       "CSV (legacy) (*.csv);;All files (*)"));
  }
  dlg.setLabelText(QFileDialog::Accept, QStringLiteral("Import"));
  if (dlg.exec() != QDialog::Accepted) return;
  const QStringList paths = dlg.selectedFiles();
  if (paths.isEmpty()) return;

  const QByteArray before = doc_->snapshotNetworkJson();
  const JudgmentTemplateImportResult result =
      importJudgmentTemplates(*doc_, paths);
  if (!result.ok) {
    if (doc_->snapshotNetworkJson() != before) {
      doc_->applyNetworkJson(before);
    }
    QMessageBox::warning(this, QStringLiteral("Import judgment templates"),
                         result.error);
    return;
  }
  const QByteArray after = doc_->snapshotNetworkJson();
  if (before != after) {
    doc_->undoStack()->push(new ApplyNetworkSnapshotCmd(
        doc_, before, after, QStringLiteral("Import judgment templates")));
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

void MainWindow::onSettings() {
  SettingsDialog dlg(googleOAuth_, this);
  dlg.setCurrentPage(SettingsDialog::Page::ConnectedAccounts);
  dlg.exec();
}

void MainWindow::refreshLinkedFormUi() {
  const LinkedGoogleForm* linked = doc_->latestLinkedGoogleForm();
  const bool hasForm = linked != nullptr && !linked->formId.isEmpty();
  const bool current =
      hasForm &&
      googleFormFingerprintMatches(linked->structureFingerprint, doc_->root());

  if (openLinkedFormAction_ != nullptr) {
    openLinkedFormAction_->setEnabled(hasForm);
    openLinkedFormAction_->setText(
        current ? QStringLiteral("Open linked Google Form…")
                : (hasForm
                       ? QStringLiteral(
                             "Open linked Google Form… (out of date)")
                       : QStringLiteral("Open linked Google Form…")));
  }
  if (importGoogleFormAction_ != nullptr) {
    importGoogleFormAction_->setEnabled(hasForm);
    importGoogleFormAction_->setText(
        current
            ? QStringLiteral("&Import Google Form results…")
            : (hasForm
                   ? QStringLiteral(
                         "&Import Google Form results… (out of date)")
                   : QStringLiteral("&Import Google Form results…")));
  }
}

void MainWindow::onCreateGoogleForm() {
  if (googleOAuth_ == nullptr || !googleOAuth_->isConnected()) {
    QMessageBox::information(
        this, QStringLiteral("Google Form"),
        QStringLiteral(
            "Connect a Google account first:\nFile → Settings… → Connected "
            "accounts"));
    return;
  }

  const LinkedGoogleForm* existing = doc_->latestLinkedGoogleForm();
  if (existing != nullptr) {
    const bool current = googleFormFingerprintMatches(
        existing->structureFingerprint, doc_->root());
    const auto reply = QMessageBox::question(
        this, QStringLiteral("Create Google Form"),
        current
            ? QStringLiteral(
                  "A Google Form is already linked to this model and matches "
                  "the current structure.\n\n"
                  "Create a new form anyway? The previous link will be kept "
                  "(archived) on the model.")
            : QStringLiteral(
                  "The linked Google Form is out of date with the current "
                  "model structure.\n\n"
                  "Create a new form? The previous link will be kept "
                  "(archived) on the model."),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (reply != QMessageBox::Yes) return;
  }

  QString title = QStringLiteral("ANP Studio judgments");
  if (!doc_->path().isEmpty()) {
    title = QFileInfo(doc_->path()).completeBaseName() +
            QStringLiteral(" — judgments");
  } else if (!doc_->root().name().empty()) {
    title = QString::fromStdString(doc_->root().name()) +
            QStringLiteral(" — judgments");
  }

  statusBar()->showMessage(QStringLiteral("Creating Google Form…"));
  QApplication::setOverrideCursor(Qt::WaitCursor);
  const GoogleFormCreateResult result =
      createGoogleFormForNetwork(*googleOAuth_, doc_->root(), title);
  QApplication::restoreOverrideCursor();
  statusBar()->clearMessage();

  if (!result.ok) {
    QMessageBox::warning(this, QStringLiteral("Create Google Form"),
                         result.error);
    return;
  }

  LinkedGoogleForm linked;
  linked.formId = result.formId;
  linked.title = title;
  linked.responderUrl = result.responderUrl;
  linked.editUrl = result.editUrl;
  linked.createdAtIso =
      QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
  linked.structureFingerprint = result.structureFingerprint;
  linked.questionTags = result.questionTags;
  linked.questionIds = result.questionIds;
  linked.mappedTags = result.mappedTags;
  doc_->addLinkedGoogleForm(linked);

  QString msg =
      QStringLiteral("Created form with %1 question(s).\n\n"
                     "The form id and structure fingerprint are saved with "
                     "this model (Save the file to keep the link).\n\n")
          .arg(result.questionCount);
  if (!result.error.isEmpty()) {
    msg += result.error + QStringLiteral("\n\n");
  }
  msg += QStringLiteral("Responder link:\n%1\n\nEdit link:\n%2")
             .arg(result.responderUrl, result.editUrl);
  QMessageBox::information(this, QStringLiteral("Google Form created"), msg);

  if (!result.editUrl.isEmpty()) {
    QDesktopServices::openUrl(QUrl(result.editUrl));
  } else if (!result.responderUrl.isEmpty()) {
    QDesktopServices::openUrl(QUrl(result.responderUrl));
  }
}

void MainWindow::onImportGoogleFormResults() {
  if (googleOAuth_ == nullptr || !googleOAuth_->isConnected()) {
    QMessageBox::information(
        this, QStringLiteral("Import Google Form"),
        QStringLiteral(
            "Connect a Google account first:\nFile → Settings… → Connected "
            "accounts"));
    return;
  }
  const LinkedGoogleForm* linked = doc_->latestLinkedGoogleForm();
  if (linked == nullptr || linked->formId.isEmpty()) {
    QMessageBox::information(
        this, QStringLiteral("Import Google Form"),
        QStringLiteral(
            "No Google Form is linked to this model.\n"
            "Create one with Participants → Create Google Form… first."));
    return;
  }

  const bool current = googleFormFingerprintMatches(
      linked->structureFingerprint, doc_->root());
  bool matchingOnly = false;
  if (!current) {
    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(QStringLiteral("Form out of date"));
    box.setText(QStringLiteral(
        "The linked Google Form does not match the current model structure."));
    box.setInformativeText(QStringLiteral(
        "Nodes, connections, alternatives, or rating scales may have changed "
        "since the form was created (or this link has no fingerprint).\n\n"
        "You can import answers that still match, or cancel and create a new "
        "form with Participants → Create Google Form…"));
    auto* matchingBtn = box.addButton(QStringLiteral("Import matching only"),
                                      QMessageBox::AcceptRole);
    box.addButton(QMessageBox::Cancel);
    box.exec();
    if (box.clickedButton() != matchingBtn) return;
    matchingOnly = true;
  }

  statusBar()->showMessage(QStringLiteral("Importing Google Form responses…"));
  QApplication::setOverrideCursor(Qt::WaitCursor);
  const QByteArray before = doc_->snapshotNetworkJson();
  const GoogleFormImportResult result = importGoogleFormResponses(
      *googleOAuth_, *doc_, linked->formId, matchingOnly);
  QApplication::restoreOverrideCursor();
  statusBar()->clearMessage();

  if (!result.ok) {
    if (doc_->snapshotNetworkJson() != before) {
      doc_->applyNetworkJson(before);
    }
    QMessageBox::warning(this, QStringLiteral("Import Google Form"),
                         result.error);
    return;
  }
  const QByteArray after = doc_->snapshotNetworkJson();
  if (before != after) {
    doc_->undoStack()->push(new ApplyNetworkSnapshotCmd(
        doc_, before, after, QStringLiteral("Import Google Form responses")));
  }

  QString msg =
      QStringLiteral(
          "Processed %1 response(s).\n"
          "Created %2 participant(s).\n"
          "Set %3 judgment value(s).\n")
          .arg(result.responsesProcessed)
          .arg(result.participantsCreated)
          .arg(result.judgmentsSet);
  if (result.judgmentsSkipped > 0) {
    msg +=
        QStringLiteral("Skipped %1 judgment(s).\n").arg(result.judgmentsSkipped);
  }
  if (matchingOnly) {
    msg += QStringLiteral(
        "\n(Matching-only import: outdated questions were skipped.)\n");
  }
  if (!result.createdParticipantNames.isEmpty()) {
    msg += QStringLiteral("\nNew participants:\n- ") +
           result.createdParticipantNames.join(QStringLiteral("\n- "));
  }
  if (!result.skippedNotes.isEmpty()) {
    msg += QStringLiteral("\n\n") + result.skippedNotes.join(QLatin1Char('\n'));
  }
  QMessageBox::information(this, QStringLiteral("Import complete"), msg);
}

void MainWindow::onOpenLinkedGoogleForm() {
  const LinkedGoogleForm* f = doc_->latestLinkedGoogleForm();
  if (f == nullptr) {
    QMessageBox::information(
        this, QStringLiteral("Linked Google Form"),
        QStringLiteral("No Google Form is linked to this model yet.\n"
                       "Use Participants → Create Google Form…"));
    return;
  }

  const bool current =
      googleFormFingerprintMatches(f->structureFingerprint, doc_->root());

  auto* box = new QMessageBox(this);
  box->setAttribute(Qt::WA_DeleteOnClose);
  box->setWindowTitle(QStringLiteral("Linked Google Form"));
  box->setIcon(current ? QMessageBox::Information : QMessageBox::Warning);
  box->setText(current ? QStringLiteral("Latest linked form (up to date)")
                       : QStringLiteral("Latest linked form (out of date)"));
  QString info =
      QStringLiteral("%1\n\nForm id: %2\nCreated: %3\n"
                     "Structure: %4\n\n"
                     "%5 form(s) linked to this model.")
          .arg(f->title, f->formId, f->createdAtIso,
               current ? QStringLiteral("matches current model")
                       : QStringLiteral(
                             "does not match — create a new form after "
                             "structural changes"),
               QString::number(doc_->linkedGoogleForms().size()));
  if (!current) {
    info += QStringLiteral(
        "\n\nImport can still apply answers for questions that remain; "
        "or create a new form for the current structure.");
  }
  box->setInformativeText(info);
  auto* editBtn = box->addButton(QStringLiteral("Open editor"),
                                 QMessageBox::AcceptRole);
  auto* fillBtn = box->addButton(QStringLiteral("Open fill-out link"),
                                 QMessageBox::ActionRole);
  box->addButton(QMessageBox::Cancel);
  box->exec();
  const QAbstractButton* clicked = box->clickedButton();
  if (clicked == editBtn && !f->editUrl.isEmpty()) {
    QDesktopServices::openUrl(QUrl(f->editUrl));
  } else if (clicked == fillBtn && !f->responderUrl.isEmpty()) {
    QDesktopServices::openUrl(QUrl(f->responderUrl));
  }
}

// --- Document file I/O and samples ------------------------------------------

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
      this, QStringLiteral("Open Network"), {}, openNameFilters());
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

bool MainWindow::openDocument(const QString& path) { return openPath(path); }

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
    if (dirHasModelSamples(d)) {
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
      if (dirHasModelSamples(d)) {
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
  const QStringList files = listModelSampleFiles(dir);
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
  QFileDialog dlg(this, QStringLiteral("Save Network"));
  dlg.setAcceptMode(QFileDialog::AcceptSave);
  dlg.setNameFilters(
      QStringList{kFilterAnpstudio, kFilterJson, kFilterAll});
  dlg.selectNameFilter(kFilterAnpstudio);
  dlg.setDefaultSuffix(QStringLiteral("anpstudio"));
  QObject::connect(&dlg, &QFileDialog::filterSelected, &dlg,
                   [&dlg](const QString& filter) {
                     if (filter.contains(QStringLiteral("*.json"),
                                         Qt::CaseInsensitive)) {
                       dlg.setDefaultSuffix(QStringLiteral("json"));
                     } else if (filter.contains(QStringLiteral("*.anpstudio"),
                                                Qt::CaseInsensitive)) {
                       dlg.setDefaultSuffix(QStringLiteral("anpstudio"));
                     }
                   });
  if (dlg.exec() != QDialog::Accepted) return false;
  QString path = dlg.selectedFiles().value(0);
  if (path.isEmpty()) return false;
  path = ensureModelSaveSuffix(path, dlg.selectedNameFilter());

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
  syncRecentShortcutActions();
}

QStringList MainWindow::loadExistingRecentFiles() {
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
  return existing;
}

void MainWindow::syncRecentShortcutActions() {
  if (recentShortcutActions_.size() != kRecentShortcutCount) return;
  const QStringList existing = loadExistingRecentFiles();
  for (int i = 0; i < kRecentShortcutCount; ++i) {
    QAction* act = recentShortcutActions_[i];
    if (i < existing.size()) {
      const QString& path = existing.at(i);
      act->setText(QStringLiteral("&%1 %2")
                       .arg(i + 1)
                       .arg(QFileInfo(path).fileName()));
      act->setData(path);
      act->setToolTip(path);
      act->setEnabled(true);
    } else {
      act->setText(QStringLiteral("&%1").arg(i + 1));
      act->setData(QString());
      act->setToolTip(QString());
      act->setEnabled(false);
    }
  }
}

void MainWindow::rebuildRecentMenu() {
  if (recentMenu_ == nullptr) return;
  recentMenu_->clear();
  syncRecentShortcutActions();

  const QStringList existing = loadExistingRecentFiles();
  if (existing.isEmpty()) {
    auto* empty = recentMenu_->addAction(QStringLiteral("(No recent files)"));
    empty->setEnabled(false);
    return;
  }

  for (int i = 0; i < existing.size(); ++i) {
    if (i < kRecentShortcutCount) {
      recentMenu_->addAction(recentShortcutActions_[i]);
    } else {
      const QString& path = existing.at(i);
      auto* act = recentMenu_->addAction(QFileInfo(path).fileName());
      act->setData(path);
      act->setToolTip(path);
      connect(act, &QAction::triggered, this, &MainWindow::openRecentFile);
    }
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
    syncRecentShortcutActions();
    return;
  }
  (void)openPath(path);
}

void MainWindow::clearRecentFiles() {
  QSettings settings;
  settings.remove(QStringLiteral("recentFiles"));
  syncRecentShortcutActions();
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
