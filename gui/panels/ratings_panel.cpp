#include "panels/ratings_panel.hpp"

#include "commands/network_commands.hpp"
#include "document.hpp"
#include "ratings/rating_preset_dialogs.hpp"
#include "ratings/rating_preset_store.hpp"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStyle>
#include <QToolButton>
#include <QTableWidget>
#include <QUuid>
#include <QVBoxLayout>

#include <cmath>
#include <type_traits>
#include <variant>

namespace {

constexpr int kRolePresetId = Qt::UserRole;
constexpr int kRoleAction = Qt::UserRole + 1;

enum class ScaleAction {
  None = 0,
  Save,
  Manage,
  Import,
  Custom,
};

bool sameCategories(const std::vector<anpcpp::RatingCategory>& a,
                    const std::vector<anpcpp::RatingCategory>& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (a[i].id != b[i].id || a[i].label != b[i].label ||
        std::abs(a[i].value - b[i].value) > 1e-9) {
      return false;
    }
  }
  return true;
}

int indexOfScaleAction(QComboBox* box, ScaleAction action) {
  const int want = static_cast<int>(action);
  for (int i = 0; i < box->count(); ++i) {
    if (box->itemData(i, kRoleAction).toInt() == want) return i;
  }
  return -1;
}

void disableComboItem(QComboBox* box, int index) {
  if (auto* m = qobject_cast<QStandardItemModel*>(box->model())) {
    if (QStandardItem* it = m->item(index)) {
      it->setFlags(it->flags() & ~Qt::ItemIsEnabled);
    }
  }
}

}  // namespace

RatingsPanel::RatingsPanel(Document* doc, QWidget* parent)
    : QWidget(parent), doc_(doc) {
  store_ = new RatingPresetStore(this);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(8);

  header_ = new QLabel(QStringLiteral("Select a Ratings link in the navigator"),
                       this);
  header_->setObjectName(QStringLiteral("ratingsHeader"));
  layout->addWidget(header_);

  scaleBlock_ = new QFrame(this);
  scaleBlock_->setObjectName(QStringLiteral("ratingsScaleBlock"));
  auto* scaleLay = new QVBoxLayout(scaleBlock_);
  scaleLay->setContentsMargins(0, 0, 0, 0);
  scaleLay->setSpacing(0);

  auto* scaleRow = new QWidget(scaleBlock_);
  scaleRow->setObjectName(QStringLiteral("ratingsScaleRow"));
  auto* scaleRowLay = new QHBoxLayout(scaleRow);
  scaleRowLay->setContentsMargins(10, 8, 10, 8);
  scaleRowLay->setSpacing(8);
  auto* scaleLabel = new QLabel(QStringLiteral("Scale"), scaleRow);
  scaleLabel->setObjectName(QStringLiteral("ratingsFieldLabel"));
  scaleBox_ = new QComboBox(scaleRow);
  scaleBox_->setMinimumWidth(200);
  modeHint_ = new QLabel(scaleRow);
  modeHint_->setObjectName(QStringLiteral("ratingsModeHint"));
  advancedBtn_ = new QToolButton(scaleRow);
  advancedBtn_->setObjectName(QStringLiteral("ratingsAdvancedBtn"));
  advancedBtn_->setText(QStringLiteral("Advanced ▾"));
  advancedBtn_->setCheckable(true);
  advancedBtn_->setAutoRaise(true);
  advancedBtn_->setToolButtonStyle(Qt::ToolButtonTextOnly);
  scaleRowLay->addWidget(scaleLabel);
  scaleRowLay->addWidget(scaleBox_);
  scaleRowLay->addWidget(modeHint_);
  scaleRowLay->addStretch(1);
  scaleRowLay->addWidget(advancedBtn_);
  scaleLay->addWidget(scaleRow);

  advancedPanel_ = new QWidget(scaleBlock_);
  advancedPanel_->setObjectName(QStringLiteral("ratingsAdvancedPop"));
  advancedPanel_->setVisible(false);
  auto* advLay = new QVBoxLayout(advancedPanel_);
  advLay->setContentsMargins(10, 10, 10, 10);
  advLay->setSpacing(6);

  auto* modeRow = new QHBoxLayout;
  advModeBox_ = new QComboBox(advancedPanel_);
  advModeBox_->addItem(QStringLiteral("Categorical"), 0);
  advModeBox_->addItem(QStringLiteral("Numeric"), 1);
  modeRow->addWidget(new QLabel(QStringLiteral("Mode"), advancedPanel_));
  modeRow->addWidget(advModeBox_);
  modeRow->addStretch(1);
  advLay->addLayout(modeRow);

  advStack_ = new QStackedWidget(advancedPanel_);
  auto* catPage = new QWidget(advStack_);
  auto* catLay = new QVBoxLayout(catPage);
  catLay->setContentsMargins(0, 0, 0, 0);
  catTable_ = new QTableWidget(0, 3, catPage);
  catTable_->setHorizontalHeaderLabels(
      {QStringLiteral("Id"), QStringLiteral("Label"), QStringLiteral("Value")});
  catTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  catLay->addWidget(catTable_);
  auto* addCat = new QPushButton(QStringLiteral("Add category"), catPage);
  catLay->addWidget(addCat, 0, Qt::AlignLeft);
  advStack_->addWidget(catPage);

  auto* numPage = new QWidget(advStack_);
  auto* numLay = new QVBoxLayout(numPage);
  numLay->setContentsMargins(0, 0, 0, 0);
  interpreterBox_ = new QComboBox(numPage);
  interpreterBox_->addItem(QStringLiteral("Identity"), 0);
  interpreterBox_->addItem(QStringLiteral("Divide by max"), 1);
  interpreterBox_->addItem(QStringLiteral("Divide by constant"), 2);
  interpreterBox_->addItem(QStringLiteral("Min–max normalize"), 3);
  interpreterBox_->addItem(QStringLiteral("Piecewise linear"), 4);
  numLay->addWidget(new QLabel(QStringLiteral("Interpreter"), numPage));
  numLay->addWidget(interpreterBox_);
  constantLabel_ = new QLabel(QStringLiteral("Constant"), numPage);
  constantSpin_ = new QDoubleSpinBox(numPage);
  constantSpin_->setRange(1e-9, 1e9);
  constantSpin_->setDecimals(6);
  constantSpin_->setValue(1.0);
  numLay->addWidget(constantLabel_);
  numLay->addWidget(constantSpin_);
  knotLabel_ = new QLabel(QStringLiteral("Piecewise knots"), numPage);
  knotTable_ = new QTableWidget(0, 2, numPage);
  knotTable_->setHorizontalHeaderLabels(
      {QStringLiteral("x"), QStringLiteral("y")});
  knotTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  addKnotBtn_ = new QPushButton(QStringLiteral("Add knot"), numPage);
  numLay->addWidget(knotLabel_);
  numLay->addWidget(knotTable_);
  numLay->addWidget(addKnotBtn_, 0, Qt::AlignLeft);
  advStack_->addWidget(numPage);
  advLay->addWidget(advStack_);

  auto* advFooter = new QHBoxLayout;
  auto* savePresetBtn =
      new QPushButton(QStringLiteral("Save as preset…"), advancedPanel_);
  auto* applyHint = new QLabel(QStringLiteral("Applies when you close Advanced"),
                               advancedPanel_);
  applyHint->setObjectName(QStringLiteral("ratingsModeHint"));
  advFooter->addWidget(savePresetBtn);
  advFooter->addStretch(1);
  advFooter->addWidget(applyHint);
  advLay->addLayout(advFooter);
  scaleLay->addWidget(advancedPanel_);
  layout->addWidget(scaleBlock_);

  votesLabel_ = new QLabel(QStringLiteral("Ratings"), this);
  votesLabel_->setObjectName(QStringLiteral("ratingsVotesLabel"));
  layout->addWidget(votesLabel_);
  votesTable_ = new QTableWidget(this);
  votesTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  layout->addWidget(votesTable_, 1);

  connect(scaleBox_, QOverload<int>::of(&QComboBox::activated), this,
          &RatingsPanel::onScaleActivated);
  connect(advancedBtn_, &QToolButton::toggled, this,
          &RatingsPanel::onAdvancedToggled);
  connect(advModeBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &RatingsPanel::onAdvModeChanged);
  connect(interpreterBox_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &RatingsPanel::onAdvInterpreterChanged);
  connect(addCat, &QPushButton::clicked, this, &RatingsPanel::onAddCategory);
  connect(addKnotBtn_, &QPushButton::clicked, this, &RatingsPanel::onAddKnot);
  connect(savePresetBtn, &QPushButton::clicked, this, &RatingsPanel::onSavePreset);
  connect(votesTable_, &QTableWidget::cellChanged, this,
          &RatingsPanel::onVoteValueChanged);
  connect(doc_, &Document::modelChanged, this, &RatingsPanel::refresh);
  connect(doc_, &Document::viewNetworkChanged, this, &RatingsPanel::refresh);
  connect(store_, &RatingPresetStore::changed, this,
          &RatingsPanel::rebuildScaleMenu);

  rebuildScaleMenu();
  setEnabledUi(false);
}

anpcpp::RatingsPrioritizer* RatingsPanel::activeRatings() {
  if (parent_.isEmpty() || destCluster_.isEmpty()) return nullptr;
  try {
    return doc_->network()
        .node(parent_.toStdString())
        .node_ratings(destCluster_.toStdString());
  } catch (...) {
    return nullptr;
  }
}

void RatingsPanel::setEnabledUi(bool on) {
  scaleBlock_->setEnabled(on);
  votesTable_->setEnabled(on);
  votesLabel_->setEnabled(on);
}

void RatingsPanel::selectLink(const QString& parent,
                              const QString& destCluster) {
  if (advancedOpen_) {
    commitAdvancedIfNeeded();
    advancedBtn_->setChecked(false);
  }
  parent_ = parent;
  destCluster_ = destCluster;
  refresh();
}

void RatingsPanel::rebuildScaleMenu() {
  const QSignalBlocker block(scaleBox_);
  const QString keepId = currentPresetId_;
  const bool keepCustom = scaleIsCustom_;
  scaleBox_->clear();

  scaleBox_->addItem(QStringLiteral("Built-in"));
  disableComboItem(scaleBox_, scaleBox_->count() - 1);

  for (const RatingPreset& p : store_->builtIn()) {
    scaleBox_->addItem(p.name);
    scaleBox_->setItemData(scaleBox_->count() - 1, p.id, kRolePresetId);
  }
  scaleBox_->insertSeparator(scaleBox_->count());

  scaleBox_->addItem(QStringLiteral("My scales"));
  disableComboItem(scaleBox_, scaleBox_->count() - 1);
  if (store_->userPresets().isEmpty()) {
    scaleBox_->addItem(QStringLiteral("(none yet)"));
    disableComboItem(scaleBox_, scaleBox_->count() - 1);
  } else {
    for (const RatingPreset& p : store_->userPresets()) {
      scaleBox_->addItem(p.name);
      scaleBox_->setItemData(scaleBox_->count() - 1, p.id, kRolePresetId);
    }
  }
  scaleBox_->insertSeparator(scaleBox_->count());

  auto addAction = [&](const QString& text, ScaleAction a) {
    scaleBox_->addItem(text);
    scaleBox_->setItemData(scaleBox_->count() - 1, static_cast<int>(a),
                           kRoleAction);
  };
  addAction(QStringLiteral("Save current as preset…"), ScaleAction::Save);
  addAction(QStringLiteral("Manage scales…"), ScaleAction::Manage);
  addAction(QStringLiteral("Import…"), ScaleAction::Import);
  addAction(QStringLiteral("Custom"), ScaleAction::Custom);

  // Restore selection.
  int restore = -1;
  if (keepCustom) {
    restore = indexOfScaleAction(scaleBox_, ScaleAction::Custom);
  } else if (!keepId.isEmpty()) {
    for (int i = 0; i < scaleBox_->count(); ++i) {
      if (scaleBox_->itemData(i, kRolePresetId).toString() == keepId) {
        restore = i;
        break;
      }
    }
  }
  if (restore < 0) {
    // Prefer High/Medium/Low built-in as default display.
    for (int i = 0; i < scaleBox_->count(); ++i) {
      if (scaleBox_->itemData(i, kRolePresetId).toString() ==
          QLatin1String("high-medium-low")) {
        restore = i;
        break;
      }
    }
  }
  if (restore < 0) restore = 1;  // first built-in after header
  scaleBox_->setCurrentIndex(restore);
  scaleBoxGuardIndex_ = restore;
  if (keepCustom) {
    currentPresetId_.clear();
    scaleIsCustom_ = true;
  } else {
    currentPresetId_ = scaleBox_->itemData(restore, kRolePresetId).toString();
    scaleIsCustom_ = false;
  }
}

void RatingsPanel::setScaleDisplay(const QString& name,
                                   const QString& presetId) {
  Q_UNUSED(name);
  currentPresetId_ = presetId;
  scaleIsCustom_ = false;
  const QSignalBlocker block(scaleBox_);
  int idx = -1;
  for (int i = 0; i < scaleBox_->count(); ++i) {
    if (scaleBox_->itemData(i, kRolePresetId).toString() == presetId) {
      idx = i;
      break;
    }
  }
  if (idx >= 0) {
    scaleBox_->setCurrentIndex(idx);
    scaleBoxGuardIndex_ = idx;
  } else {
    showCustomScaleInCombo();
  }
}

void RatingsPanel::showCustomScaleInCombo() {
  currentPresetId_.clear();
  scaleIsCustom_ = true;
  const QSignalBlocker block(scaleBox_);
  const int idx = indexOfScaleAction(scaleBox_, ScaleAction::Custom);
  if (idx >= 0) {
    scaleBox_->setCurrentIndex(idx);
    scaleBoxGuardIndex_ = idx;
  }
}

void RatingsPanel::refresh() {
  // Undo child commands notify on each redo; skip until the scale macro ends
  // so we do not re-enter applyPreset while categories are still empty.
  if (applyingScale_) return;

  updating_ = true;
  auto* rt = activeRatings();
  if (rt == nullptr) {
    header_->setText(
        QStringLiteral("No Ratings link selected — choose Ratings in the "
                       "navigator for a node→cluster connection."));
    setEnabledUi(false);
    votesTable_->setRowCount(0);
    updating_ = false;
    return;
  }
  header_->setText(
      QStringLiteral("%1 → %2").arg(parent_, destCluster_));
  setEnabledUi(true);
  modeHint_->setText(rt->mode() == anpcpp::RatingsPrioritizer::Mode::Categorical
                         ? QStringLiteral("Categorical")
                         : QStringLiteral("Numeric"));

  // Match display name to a preset when possible.
  bool matched = false;
  for (const RatingPreset& p : store_->all()) {
    if (p.mode != rt->mode()) continue;
    if (p.mode == anpcpp::RatingsPrioritizer::Mode::Categorical) {
      if (sameCategories(p.categories, rt->categories())) {
        setScaleDisplay(p.name, p.id);
        matched = true;
        break;
      }
    }
  }
  if (!matched) {
    if (rt->mode() == anpcpp::RatingsPrioritizer::Mode::Numeric) {
      // Match percent preset loosely.
      bool isPercent = false;
      std::visit(
          [&](const auto& interp) {
            using T = std::decay_t<decltype(interp)>;
            if constexpr (std::is_same_v<T,
                                         anpcpp::DivideByConstantInterpreter>) {
              isPercent = std::abs(interp.constant - 100.0) < 1e-9;
            }
          },
          rt->interpreter());
      if (isPercent) {
        setScaleDisplay(QStringLiteral("Percent (0–100)"),
                        QStringLiteral("percent-0-100"));
        matched = true;
      }
    }
  }
  if (!matched) {
    showCustomScaleInCombo();
  }

  if (rt->categories().empty() &&
      rt->mode() == anpcpp::RatingsPrioritizer::Mode::Categorical) {
    // Seed default scale once for empty categorical links.
    if (auto p = store_->findById(QStringLiteral("high-medium-low"))) {
      updating_ = false;
      applyPreset(*p);
      return;
    }
  }

  if (advancedOpen_) {
    loadAdvancedFromActive();
  }
  rebuildVotes();
  updating_ = false;
}

void RatingsPanel::applyPreset(const RatingPreset& preset) {
  if (activeRatings() == nullptr || applyingScale_) return;
  applyingScale_ = true;
  doc_->undoStack()->beginMacro(
      QStringLiteral("Apply scale “%1”").arg(preset.name));
  doc_->undoStack()->push(
      new SetRatingsModeCmd(doc_, parent_, destCluster_, preset.mode));
  if (preset.mode == anpcpp::RatingsPrioritizer::Mode::Categorical) {
    doc_->undoStack()->push(new SetRatingsCategoriesCmd(
        doc_, parent_, destCluster_, preset.categories));
  } else {
    doc_->undoStack()->push(new SetRatingsInterpreterCmd(
        doc_, parent_, destCluster_, preset.interpreter));
  }
  doc_->undoStack()->endMacro();
  currentPresetId_ = preset.id;
  scaleIsCustom_ = false;
  applyingScale_ = false;
  refresh();
}

void RatingsPanel::onScaleActivated(int index) {
  if (updating_ || index < 0) return;
  const QVariant actionVar = scaleBox_->itemData(index, kRoleAction);
  if (actionVar.isValid()) {
    const auto action = static_cast<ScaleAction>(actionVar.toInt());
    if (action == ScaleAction::Custom) {
      // Stay on Custom in the closed combo; open Advanced to edit.
      scaleBoxGuardIndex_ = index;
      scaleIsCustom_ = true;
      currentPresetId_.clear();
      advancedBtn_->setChecked(true);
      return;
    }
    const QSignalBlocker block(scaleBox_);
    scaleBox_->setCurrentIndex(scaleBoxGuardIndex_);
    switch (action) {
      case ScaleAction::Save:
        onSavePreset();
        break;
      case ScaleAction::Manage:
        onManagePresets();
        break;
      case ScaleAction::Import:
        onImportPresets();
        break;
      default:
        break;
    }
    return;
  }
  const QString id = scaleBox_->itemData(index, kRolePresetId).toString();
  if (id.isEmpty()) {
    const QSignalBlocker block(scaleBox_);
    scaleBox_->setCurrentIndex(scaleBoxGuardIndex_);
    return;
  }
  scaleBoxGuardIndex_ = index;
  scaleIsCustom_ = false;
  if (auto p = store_->findById(id)) {
    if (advancedOpen_) {
      advancedBtn_->setChecked(false);
    }
    applyPreset(*p);
  }
}

void RatingsPanel::onAdvancedToggled(bool on) {
  if (on) {
    loadAdvancedFromActive();
    advancedPanel_->setVisible(true);
    scaleBlock_->setProperty("advancedOpen", true);
    scaleBlock_->style()->unpolish(scaleBlock_);
    scaleBlock_->style()->polish(scaleBlock_);
    advancedBtn_->setText(QStringLiteral("Advanced ▴"));
    advancedOpen_ = true;
  } else {
    if (advancedOpen_) {
      commitAdvancedIfNeeded();
    }
    advancedPanel_->setVisible(false);
    scaleBlock_->setProperty("advancedOpen", false);
    scaleBlock_->style()->unpolish(scaleBlock_);
    scaleBlock_->style()->polish(scaleBlock_);
    advancedBtn_->setText(QStringLiteral("Advanced ▾"));
    advancedOpen_ = false;
  }
}

void RatingsPanel::loadAdvancedFromActive() {
  auto* rt = activeRatings();
  if (rt == nullptr) return;
  const QSignalBlocker b1(advModeBox_);
  const QSignalBlocker b2(interpreterBox_);
  const QSignalBlocker b3(constantSpin_);
  advModeBox_->setCurrentIndex(
      rt->mode() == anpcpp::RatingsPrioritizer::Mode::Categorical ? 0 : 1);
  advStack_->setCurrentIndex(advModeBox_->currentIndex());

  catTable_->setRowCount(0);
  for (const auto& c : rt->categories()) {
    const int r = catTable_->rowCount();
    catTable_->insertRow(r);
    catTable_->setItem(r, 0, new QTableWidgetItem(QString::fromStdString(c.id)));
    catTable_->setItem(r, 1,
                       new QTableWidgetItem(QString::fromStdString(c.label)));
    catTable_->setItem(r, 2,
                       new QTableWidgetItem(QString::number(c.value, 'f', 4)));
  }

  int idx = 0;
  knotTable_->setRowCount(0);
  std::visit(
      [&](const auto& interp) {
        using T = std::decay_t<decltype(interp)>;
        if constexpr (std::is_same_v<T, anpcpp::IdentityInterpreter>) idx = 0;
        else if constexpr (std::is_same_v<T, anpcpp::DivideByMaxInterpreter>)
          idx = 1;
        else if constexpr (std::is_same_v<T,
                                          anpcpp::DivideByConstantInterpreter>) {
          idx = 2;
          constantSpin_->setValue(interp.constant);
        } else if constexpr (std::is_same_v<T,
                                            anpcpp::MinMaxNormalizeInterpreter>)
          idx = 3;
        else if constexpr (std::is_same_v<T,
                                          anpcpp::PiecewiseLinearInterpreter>) {
          idx = 4;
          for (const auto& [x, y] : interp.knots) {
            const int r = knotTable_->rowCount();
            knotTable_->insertRow(r);
            knotTable_->setItem(r, 0, new QTableWidgetItem(QString::number(x)));
            knotTable_->setItem(r, 1, new QTableWidgetItem(QString::number(y)));
          }
        }
      },
      rt->interpreter());
  interpreterBox_->setCurrentIndex(idx);
  updateNumericAdvancedVisibility();
}

void RatingsPanel::updateNumericAdvancedVisibility() {
  const int idx = interpreterBox_->currentIndex();
  const bool showConst = idx == 2;
  const bool showKnots = idx == 4;
  constantLabel_->setVisible(showConst);
  constantSpin_->setVisible(showConst);
  knotLabel_->setVisible(showKnots);
  knotTable_->setVisible(showKnots);
  addKnotBtn_->setVisible(showKnots);
}

void RatingsPanel::onAdvModeChanged() {
  if (updating_) return;
  advStack_->setCurrentIndex(advModeBox_->currentIndex());
}

void RatingsPanel::onAdvInterpreterChanged() {
  if (updating_) return;
  updateNumericAdvancedVisibility();
}

void RatingsPanel::onAddCategory() {
  const int r = catTable_->rowCount();
  catTable_->insertRow(r);
  catTable_->setItem(r, 0, new QTableWidgetItem(QStringLiteral("C%1").arg(r + 1)));
  catTable_->setItem(
      r, 1, new QTableWidgetItem(QStringLiteral("Category %1").arg(r + 1)));
  catTable_->setItem(r, 2, new QTableWidgetItem(QStringLiteral("0.5")));
}

void RatingsPanel::onAddKnot() {
  const int r = knotTable_->rowCount();
  knotTable_->insertRow(r);
  knotTable_->setItem(r, 0, new QTableWidgetItem(QStringLiteral("0")));
  knotTable_->setItem(r, 1, new QTableWidgetItem(QStringLiteral("0")));
}

std::vector<anpcpp::RatingCategory> RatingsPanel::readCategoryTable() const {
  std::vector<anpcpp::RatingCategory> cats;
  for (int r = 0; r < catTable_->rowCount(); ++r) {
    auto* idItem = catTable_->item(r, 0);
    auto* labelItem = catTable_->item(r, 1);
    auto* valItem = catTable_->item(r, 2);
    if (idItem == nullptr || idItem->text().trimmed().isEmpty()) continue;
    anpcpp::RatingCategory c;
    c.id = idItem->text().trimmed().toStdString();
    c.label = labelItem ? labelItem->text().toStdString() : c.id;
    bool ok = false;
    c.value = valItem ? valItem->text().toDouble(&ok) : 0.0;
    if (!ok) c.value = 0.0;
    cats.push_back(std::move(c));
  }
  return cats;
}

anpcpp::ScoreInterpreter RatingsPanel::readInterpreter() const {
  switch (interpreterBox_->currentIndex()) {
    case 1:
      return anpcpp::DivideByMaxInterpreter{};
    case 2:
      return anpcpp::DivideByConstantInterpreter{constantSpin_->value()};
    case 3:
      return anpcpp::MinMaxNormalizeInterpreter{};
    case 4: {
      anpcpp::PiecewiseLinearInterpreter pl;
      for (int r = 0; r < knotTable_->rowCount(); ++r) {
        auto* xItem = knotTable_->item(r, 0);
        auto* yItem = knotTable_->item(r, 1);
        if (xItem == nullptr || yItem == nullptr) continue;
        pl.knots.emplace_back(xItem->text().toDouble(),
                              yItem->text().toDouble());
      }
      return pl;
    }
    default:
      return anpcpp::IdentityInterpreter{};
  }
}

RatingPreset RatingsPanel::draftPresetFromAdvanced() const {
  RatingPreset p;
  p.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  p.name = QStringLiteral("Custom scale");
  p.source = RatingPresetSource::User;
  p.mode = advModeBox_->currentIndex() == 0
               ? anpcpp::RatingsPrioritizer::Mode::Categorical
               : anpcpp::RatingsPrioritizer::Mode::Numeric;
  if (p.mode == anpcpp::RatingsPrioritizer::Mode::Categorical) {
    p.categories = readCategoryTable();
  } else {
    p.interpreter = readInterpreter();
  }
  return p;
}

void RatingsPanel::commitAdvancedIfNeeded() {
  auto* rt = activeRatings();
  if (rt == nullptr) return;

  const auto mode = advModeBox_->currentIndex() == 0
                        ? anpcpp::RatingsPrioritizer::Mode::Categorical
                        : anpcpp::RatingsPrioritizer::Mode::Numeric;
  const auto cats = readCategoryTable();
  const auto interp = readInterpreter();

  const bool modeChanged = rt->mode() != mode;
  if (!modeChanged && mode == anpcpp::RatingsPrioritizer::Mode::Categorical &&
      sameCategories(rt->categories(), cats)) {
    return;
  }

  try {
    applyingScale_ = true;
    doc_->undoStack()->beginMacro(QStringLiteral("Update ratings scale"));
    if (modeChanged) {
      doc_->undoStack()->push(
          new SetRatingsModeCmd(doc_, parent_, destCluster_, mode));
    }
    if (mode == anpcpp::RatingsPrioritizer::Mode::Categorical) {
      doc_->undoStack()->push(
          new SetRatingsCategoriesCmd(doc_, parent_, destCluster_, cats));
    } else {
      doc_->undoStack()->push(
          new SetRatingsInterpreterCmd(doc_, parent_, destCluster_, interp));
    }
    doc_->undoStack()->endMacro();
    applyingScale_ = false;
    currentPresetId_.clear();
    refresh();
  } catch (const std::exception& e) {
    applyingScale_ = false;
    QMessageBox::warning(this, QStringLiteral("Invalid scale"),
                         QString::fromUtf8(e.what()));
  }
}

void RatingsPanel::rebuildVotes() {
  auto* rt = activeRatings();
  if (rt == nullptr) return;
  const QSignalBlocker block(votesTable_);
  votesTable_->clear();
  votesTable_->clearSpans();
  // Clear cell widgets.
  votesTable_->setRowCount(0);

  const bool categorical =
      rt->mode() == anpcpp::RatingsPrioritizer::Mode::Categorical;
  votesLabel_->setText(categorical ? QStringLiteral("Ratings")
                                   : QStringLiteral("Ratings (raw values)"));
  votesTable_->setColumnCount(categorical ? 2 : 3);
  if (categorical) {
    votesTable_->setHorizontalHeaderLabels(
        {QStringLiteral("Alternative"), QStringLiteral("Rating")});
  } else {
    votesTable_->setHorizontalHeaderLabels(
        {QStringLiteral("Alternative"), QStringLiteral("Value"),
         QStringLiteral("Score")});
  }
  votesTable_->setRowCount(static_cast<int>(rt->size()));
  const auto scores = rt->scores();

  for (std::size_t i = 0; i < rt->size(); ++i) {
    const int row = static_cast<int>(i);
    const QString alt = QString::fromStdString(rt->alternatives()[i]);
    auto* altItem = new QTableWidgetItem(alt);
    altItem->setFlags(Qt::ItemIsEnabled);
    votesTable_->setItem(row, 0, altItem);

    if (categorical) {
      auto* box = new QComboBox(votesTable_);
      box->addItem(QStringLiteral("(none)"), QString());
      for (const auto& c : rt->categories()) {
        box->addItem(
            QStringLiteral("%1 (%2)")
                .arg(QString::fromStdString(c.label))
                .arg(c.value, 0, 'g', 4),
            QString::fromStdString(c.id));
      }
      QString curId;
      if (auto r = rt->rating(rt->alternatives()[i])) {
        curId = QString::fromStdString(*r);
      }
      const int idx = box->findData(curId);
      box->setCurrentIndex(idx >= 0 ? idx : 0);
      votesTable_->setCellWidget(row, 1, box);
      connect(box, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
              [this, row](int) { onVoteComboChanged(row); });
    } else {
      QString v;
      if (auto raw = rt->value(rt->alternatives()[i])) {
        v = QString::number(*raw);
      }
      votesTable_->setItem(row, 1, new QTableWidgetItem(v));
      const double sc = i < scores.size() ? scores[i] : 0.0;
      auto* scoreItem =
          new QTableWidgetItem(QString::number(sc, 'f', 3));
      scoreItem->setFlags(Qt::ItemIsEnabled);
      votesTable_->setItem(row, 2, scoreItem);
    }
  }
}

void RatingsPanel::onVoteComboChanged(int row) {
  if (updating_) return;
  auto* rt = activeRatings();
  if (rt == nullptr) return;
  auto* box = qobject_cast<QComboBox*>(votesTable_->cellWidget(row, 1));
  auto* altItem = votesTable_->item(row, 0);
  if (box == nullptr || altItem == nullptr) return;
  const QString alt = altItem->text();
  const QString id = box->currentData().toString();
  try {
    doc_->undoStack()->push(
        new SetRatingVoteCmd(doc_, parent_, destCluster_, alt, id));
  } catch (const std::exception& e) {
    QMessageBox::warning(this, QStringLiteral("Invalid vote"),
                         QString::fromUtf8(e.what()));
    refresh();
  }
}

void RatingsPanel::onVoteValueChanged(int row, int col) {
  if (updating_ || col != 1) return;
  auto* rt = activeRatings();
  if (rt == nullptr) return;
  if (rt->mode() == anpcpp::RatingsPrioritizer::Mode::Categorical) return;
  auto* altItem = votesTable_->item(row, 0);
  auto* valItem = votesTable_->item(row, 1);
  if (altItem == nullptr) return;
  const QString alt = altItem->text();
  const QString text = valItem ? valItem->text().trimmed() : QString();
  if (text.isEmpty()) {
    doc_->undoStack()->push(
        new SetRatingValueCmd(doc_, parent_, destCluster_, alt, true, 0.0));
  } else {
    bool ok = false;
    const double v = text.toDouble(&ok);
    if (!ok) {
      refresh();
      return;
    }
    doc_->undoStack()->push(
        new SetRatingValueCmd(doc_, parent_, destCluster_, alt, false, v));
  }
}

void RatingsPanel::onSavePreset() {
  RatingPreset draft;
  if (advancedOpen_) {
    draft = draftPresetFromAdvanced();
  } else if (auto* rt = activeRatings()) {
    draft = ratingPresetFromPrioritizer(
        *rt, QUuid::createUuid().toString(QUuid::WithoutBraces),
        QStringLiteral("Custom scale"), {});
  } else {
    return;
  }
  promptSaveRatingPreset(this, store_, draft);
}

void RatingsPanel::onManagePresets() {
  showManageRatingPresetsDialog(this, store_);
}

void RatingsPanel::onImportPresets() {
  const QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("Import rating presets"), {},
      QStringLiteral("JSON (*.json);;All files (*)"));
  if (path.isEmpty()) return;
  QString err;
  const int n = store_->importFromFile(path, &err);
  if (n <= 0) {
    QMessageBox::warning(this, QStringLiteral("Import"),
                         err.isEmpty() ? QStringLiteral("No presets imported.")
                                       : err);
    return;
  }
  QMessageBox::information(
      this, QStringLiteral("Import"),
      QStringLiteral("Imported %1 preset(s) into My scales.").arg(n));
}
