#include "panels/ratings_panel.hpp"

#include "commands/network_commands.hpp"
#include "document.hpp"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <type_traits>
#include <variant>

RatingsPanel::RatingsPanel(Document* doc, QWidget* parent)
    : QWidget(parent), doc_(doc) {
  auto* layout = new QVBoxLayout(this);
  header_ = new QLabel(QStringLiteral("Select a Ratings link in the navigator"),
                       this);
  layout->addWidget(header_);

  modeBox_ = new QComboBox(this);
  modeBox_->addItem(QStringLiteral("Categorical"), 0);
  modeBox_->addItem(QStringLiteral("Numeric"), 1);
  layout->addWidget(new QLabel(QStringLiteral("Mode:"), this));
  layout->addWidget(modeBox_);

  scaleStack_ = new QStackedWidget(this);
  auto* catPage = new QWidget(scaleStack_);
  auto* catLay = new QVBoxLayout(catPage);
  catTable_ = new QTableWidget(0, 3, catPage);
  catTable_->setHorizontalHeaderLabels(
      {QStringLiteral("Id"), QStringLiteral("Label"), QStringLiteral("Value")});
  catTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  catLay->addWidget(catTable_);
  auto* catBtns = new QHBoxLayout;
  auto* addCat = new QPushButton(QStringLiteral("Add category"), catPage);
  applyCats_ = new QPushButton(QStringLiteral("Apply scale"), catPage);
  catBtns->addWidget(addCat);
  catBtns->addWidget(applyCats_);
  catLay->addLayout(catBtns);
  scaleStack_->addWidget(catPage);

  auto* numPage = new QWidget(scaleStack_);
  auto* numLay = new QVBoxLayout(numPage);
  interpreterBox_ = new QComboBox(numPage);
  interpreterBox_->addItem(QStringLiteral("Identity"), 0);
  interpreterBox_->addItem(QStringLiteral("Divide by max"), 1);
  interpreterBox_->addItem(QStringLiteral("Divide by constant"), 2);
  interpreterBox_->addItem(QStringLiteral("Min–max normalize"), 3);
  interpreterBox_->addItem(QStringLiteral("Piecewise linear"), 4);
  numLay->addWidget(new QLabel(QStringLiteral("Interpreter:"), numPage));
  numLay->addWidget(interpreterBox_);
  constantSpin_ = new QDoubleSpinBox(numPage);
  constantSpin_->setRange(1e-9, 1e9);
  constantSpin_->setValue(1.0);
  numLay->addWidget(new QLabel(QStringLiteral("Constant:"), numPage));
  numLay->addWidget(constantSpin_);
  knotTable_ = new QTableWidget(0, 2, numPage);
  knotTable_->setHorizontalHeaderLabels(
      {QStringLiteral("x"), QStringLiteral("y")});
  knotTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  numLay->addWidget(new QLabel(QStringLiteral("Piecewise knots:"), numPage));
  numLay->addWidget(knotTable_);
  applyKnots_ = new QPushButton(QStringLiteral("Apply interpreter"), numPage);
  auto* addKnot = new QPushButton(QStringLiteral("Add knot"), numPage);
  numLay->addWidget(addKnot);
  numLay->addWidget(applyKnots_);
  scaleStack_->addWidget(numPage);
  layout->addWidget(scaleStack_);

  layout->addWidget(new QLabel(QStringLiteral("Votes / values:"), this));
  votesTable_ = new QTableWidget(this);
  votesTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  layout->addWidget(votesTable_);
  readout_ = new QLabel(this);
  layout->addWidget(readout_);

  connect(modeBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &RatingsPanel::onModeChanged);
  connect(interpreterBox_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &RatingsPanel::onInterpreterChanged);
  connect(constantSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &RatingsPanel::onConstantChanged);
  connect(applyCats_, &QPushButton::clicked, this,
          &RatingsPanel::onApplyCategories);
  connect(addCat, &QPushButton::clicked, this, &RatingsPanel::onAddCategory);
  connect(applyKnots_, &QPushButton::clicked, this, &RatingsPanel::onApplyKnots);
  connect(addKnot, &QPushButton::clicked, this, [this]() {
    const int r = knotTable_->rowCount();
    knotTable_->insertRow(r);
    knotTable_->setItem(r, 0, new QTableWidgetItem(QStringLiteral("0")));
    knotTable_->setItem(r, 1, new QTableWidgetItem(QStringLiteral("0")));
  });
  connect(votesTable_, &QTableWidget::cellChanged, this,
          &RatingsPanel::onVotesChanged);
  connect(doc_, &Document::modelChanged, this, &RatingsPanel::refresh);
  connect(doc_, &Document::viewNetworkChanged, this, &RatingsPanel::refresh);
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

void RatingsPanel::selectLink(const QString& parent,
                              const QString& destCluster) {
  parent_ = parent;
  destCluster_ = destCluster;
  refresh();
}

void RatingsPanel::refresh() {
  updating_ = true;
  auto* rt = activeRatings();
  if (rt == nullptr) {
    header_->setText(
        QStringLiteral("No Ratings link selected — use “Use Ratings” in the "
                       "navigator for a node→cluster connection."));
    modeBox_->setEnabled(false);
    scaleStack_->setEnabled(false);
    votesTable_->setEnabled(false);
    updating_ = false;
    return;
  }
  header_->setText(
      QStringLiteral("Ratings: %1 → %2").arg(parent_, destCluster_));
  modeBox_->setEnabled(true);
  scaleStack_->setEnabled(true);
  votesTable_->setEnabled(true);
  modeBox_->setCurrentIndex(
      rt->mode() == anpcpp::RatingsPrioritizer::Mode::Categorical ? 0 : 1);
  rebuildScaleUi();
  rebuildVotes();
  updateReadout();
  updating_ = false;
}

void RatingsPanel::rebuildScaleUi() {
  auto* rt = activeRatings();
  if (rt == nullptr) return;
  const bool categorical =
      rt->mode() == anpcpp::RatingsPrioritizer::Mode::Categorical;
  scaleStack_->setCurrentIndex(categorical ? 0 : 1);
  if (categorical) {
    catTable_->setRowCount(0);
    for (const auto& c : rt->categories()) {
      const int r = catTable_->rowCount();
      catTable_->insertRow(r);
      catTable_->setItem(r, 0, new QTableWidgetItem(QString::fromStdString(c.id)));
      catTable_->setItem(
          r, 1, new QTableWidgetItem(QString::fromStdString(c.label)));
      catTable_->setItem(
          r, 2, new QTableWidgetItem(QString::number(c.value, 'f', 4)));
    }
  } else {
    int idx = 0;
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
          } else if constexpr (std::is_same_v<
                                   T, anpcpp::MinMaxNormalizeInterpreter>)
            idx = 3;
          else if constexpr (std::is_same_v<
                                 T, anpcpp::PiecewiseLinearInterpreter>) {
            idx = 4;
            knotTable_->setRowCount(0);
            for (const auto& [x, y] : interp.knots) {
              const int r = knotTable_->rowCount();
              knotTable_->insertRow(r);
              knotTable_->setItem(r, 0,
                                  new QTableWidgetItem(QString::number(x)));
              knotTable_->setItem(r, 1,
                                  new QTableWidgetItem(QString::number(y)));
            }
          }
        },
        rt->interpreter());
    interpreterBox_->setCurrentIndex(idx);
    constantSpin_->setEnabled(idx == 2);
    knotTable_->setEnabled(idx == 4);
  }
}

void RatingsPanel::rebuildVotes() {
  auto* rt = activeRatings();
  if (rt == nullptr) return;
  votesTable_->blockSignals(true);
  votesTable_->clear();
  const bool categorical =
      rt->mode() == anpcpp::RatingsPrioritizer::Mode::Categorical;
  votesTable_->setColumnCount(categorical ? 2 : 2);
  votesTable_->setHorizontalHeaderLabels(
      {QStringLiteral("Alternative"),
       categorical ? QStringLiteral("Category") : QStringLiteral("Value")});
  votesTable_->setRowCount(static_cast<int>(rt->size()));
  for (std::size_t i = 0; i < rt->size(); ++i) {
    const QString alt = QString::fromStdString(rt->alternatives()[i]);
    votesTable_->setItem(static_cast<int>(i), 0,
                         new QTableWidgetItem(alt));
    votesTable_->item(static_cast<int>(i), 0)->setFlags(Qt::ItemIsEnabled);
    if (categorical) {
      QString id;
      if (auto r = rt->rating(rt->alternatives()[i])) {
        id = QString::fromStdString(*r);
      }
      votesTable_->setItem(static_cast<int>(i), 1, new QTableWidgetItem(id));
    } else {
      QString v;
      if (auto raw = rt->value(rt->alternatives()[i])) {
        v = QString::number(*raw);
      }
      votesTable_->setItem(static_cast<int>(i), 1, new QTableWidgetItem(v));
    }
  }
  votesTable_->blockSignals(false);
}

void RatingsPanel::updateReadout() {
  auto* rt = activeRatings();
  if (rt == nullptr) {
    readout_->clear();
    return;
  }
  const auto scores = rt->scores();
  const auto pris = rt->priorities();
  QStringList parts;
  for (std::size_t i = 0; i < rt->size(); ++i) {
    parts << QStringLiteral("%1: score=%2 pri=%3")
                 .arg(QString::fromStdString(rt->alternatives()[i]))
                 .arg(scores[i], 0, 'f', 3)
                 .arg(pris[i], 0, 'f', 3);
  }
  readout_->setText(parts.join(QStringLiteral("  |  ")));
}

void RatingsPanel::onModeChanged() {
  if (updating_) return;
  auto* rt = activeRatings();
  if (rt == nullptr) return;
  const auto mode = modeBox_->currentIndex() == 0
                        ? anpcpp::RatingsPrioritizer::Mode::Categorical
                        : anpcpp::RatingsPrioritizer::Mode::Numeric;
  if (rt->mode() == mode) return;
  doc_->undoStack()->push(
      new SetRatingsModeCmd(doc_, parent_, destCluster_, mode));
}

void RatingsPanel::onInterpreterChanged() {
  if (updating_) return;
  constantSpin_->setEnabled(interpreterBox_->currentIndex() == 2);
  knotTable_->setEnabled(interpreterBox_->currentIndex() == 4);
}

void RatingsPanel::onConstantChanged() {
  // Applied via Apply interpreter button.
}

void RatingsPanel::onApplyCategories() {
  if (activeRatings() == nullptr) return;
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
  try {
    doc_->undoStack()->push(
        new SetRatingsCategoriesCmd(doc_, parent_, destCluster_, cats));
  } catch (const std::exception& e) {
    QMessageBox::warning(this, QStringLiteral("Invalid scale"),
                         QString::fromUtf8(e.what()));
  }
}

void RatingsPanel::onAddCategory() {
  const int r = catTable_->rowCount();
  catTable_->insertRow(r);
  catTable_->setItem(r, 0, new QTableWidgetItem(QStringLiteral("C%1").arg(r + 1)));
  catTable_->setItem(r, 1, new QTableWidgetItem(QStringLiteral("Category %1").arg(r + 1)));
  catTable_->setItem(r, 2, new QTableWidgetItem(QStringLiteral("0.5")));
}

void RatingsPanel::onApplyKnots() {
  if (activeRatings() == nullptr) return;
  anpcpp::ScoreInterpreter interp;
  switch (interpreterBox_->currentIndex()) {
    case 0:
      interp = anpcpp::IdentityInterpreter{};
      break;
    case 1:
      interp = anpcpp::DivideByMaxInterpreter{};
      break;
    case 2:
      interp = anpcpp::DivideByConstantInterpreter{constantSpin_->value()};
      break;
    case 3:
      interp = anpcpp::MinMaxNormalizeInterpreter{};
      break;
    case 4: {
      anpcpp::PiecewiseLinearInterpreter pl;
      for (int r = 0; r < knotTable_->rowCount(); ++r) {
        auto* xItem = knotTable_->item(r, 0);
        auto* yItem = knotTable_->item(r, 1);
        if (xItem == nullptr || yItem == nullptr) continue;
        pl.knots.emplace_back(xItem->text().toDouble(),
                              yItem->text().toDouble());
      }
      interp = std::move(pl);
      break;
    }
    default:
      interp = anpcpp::IdentityInterpreter{};
      break;
  }
  doc_->undoStack()->push(
      new SetRatingsInterpreterCmd(doc_, parent_, destCluster_, interp));
}

void RatingsPanel::onVotesChanged(int row, int col) {
  if (updating_ || col != 1) return;
  auto* rt = activeRatings();
  if (rt == nullptr) return;
  auto* altItem = votesTable_->item(row, 0);
  auto* valItem = votesTable_->item(row, 1);
  if (altItem == nullptr) return;
  const QString alt = altItem->text();
  const QString text = valItem ? valItem->text().trimmed() : QString();
  if (rt->mode() == anpcpp::RatingsPrioritizer::Mode::Categorical) {
    try {
      doc_->undoStack()->push(
          new SetRatingVoteCmd(doc_, parent_, destCluster_, alt, text));
    } catch (const std::exception& e) {
      QMessageBox::warning(this, QStringLiteral("Invalid vote"),
                           QString::fromUtf8(e.what()));
      refresh();
    }
  } else {
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
}
