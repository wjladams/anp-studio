#include "panels/pairwise_panel.hpp"

#include "commands/network_commands.hpp"
#include "document.hpp"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include <cmath>

namespace {

/** Map matrix a_{left,right} to a Saaty intensity on the left or right side. */
void decodeSaaty(double aLeftRight, int* intensityOut, bool* preferLeftOut) {
  if (aLeftRight <= 0.0 || !std::isfinite(aLeftRight)) {
    *intensityOut = 1;
    *preferLeftOut = true;
    return;
  }
  if (aLeftRight >= 1.0) {
    *preferLeftOut = true;
    *intensityOut = static_cast<int>(std::lround(aLeftRight));
  } else {
    *preferLeftOut = false;
    *intensityOut = static_cast<int>(std::lround(1.0 / aLeftRight));
  }
  if (*intensityOut < 1) *intensityOut = 1;
  if (*intensityOut > 9) *intensityOut = 9;
}

}  // namespace

PairwisePanel::PairwisePanel(Document* doc, QWidget* parent)
    : QWidget(parent), doc_(doc) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* viewRow = new QHBoxLayout;
  viewRow->setSpacing(8);
  auto* viewCaption = new QLabel(QStringLiteral("View:"), this);
  viewCaption->setObjectName(QStringLiteral("selectorCaption"));
  viewRow->addWidget(viewCaption);

  viewGroup_ = new QButtonGroup(this);
  viewGroup_->setExclusive(true);
  matrixBtn_ = new QPushButton(QStringLiteral("Matrix"), this);
  questionnaireBtn_ = new QPushButton(QStringLiteral("Questionnaire"), this);
  for (auto* b : {matrixBtn_, questionnaireBtn_}) {
    b->setObjectName(QStringLiteral("selectorToggle"));
    b->setCheckable(true);
    b->setFlat(true);
    b->setCursor(Qt::PointingHandCursor);
    viewRow->addWidget(b);
  }
  viewGroup_->addButton(matrixBtn_, 0);
  viewGroup_->addButton(questionnaireBtn_, 1);
  matrixBtn_->setChecked(true);
  viewRow->addStretch();
  layout->addLayout(viewRow);

  views_ = new QStackedWidget(this);
  table_ = new QTableWidget(views_);
  table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  views_->addWidget(table_);

  questionnaireScroll_ = new QScrollArea(views_);
  questionnaireScroll_->setWidgetResizable(true);
  questionnaireScroll_->setFrameShape(QFrame::NoFrame);
  questionnaireHost_ = new QWidget(questionnaireScroll_);
  questionnaireLay_ = new QVBoxLayout(questionnaireHost_);
  questionnaireLay_->setContentsMargins(0, 0, 0, 0);
  questionnaireLay_->setSpacing(6);
  questionnaireLay_->addStretch();
  questionnaireScroll_->setWidget(questionnaireHost_);
  views_->addWidget(questionnaireScroll_);
  layout->addWidget(views_, 1);

  info_ = new QLabel(this);
  layout->addWidget(info_);

  connect(viewGroup_, &QButtonGroup::idClicked, this,
          [this](int) { onViewModeChanged(); });
  connect(table_, &QTableWidget::cellChanged, this,
          &PairwisePanel::onCellChanged);
  connect(doc_, &Document::modelChanged, this, &PairwisePanel::refresh);
  connect(doc_, &Document::viewNetworkChanged, this, &PairwisePanel::refresh);
}

void PairwisePanel::selectNodeParent(const QString& name) {
  nodeMode_ = true;
  parent_ = name;
  rebuildViews();
}

void PairwisePanel::selectNodeLink(const QString& parent,
                                   const QString& destCluster) {
  nodeMode_ = true;
  parent_ = parent;
  destCluster_ = destCluster;
  rebuildViews();
}

void PairwisePanel::selectClusterParent(const QString& name) {
  nodeMode_ = false;
  parent_ = name;
  destCluster_.clear();
  rebuildViews();
}

void PairwisePanel::refresh() {
  rebuildViews();
}

void PairwisePanel::onViewModeChanged() {
  views_->setCurrentIndex(questionnaireBtn_->isChecked() ? 1 : 0);
}

const anpcpp::PairwiseJudgments* PairwisePanel::currentPairwise() const {
  if (parent_.isEmpty()) return nullptr;
  auto& net = doc_->network();
  if (nodeMode()) {
    if (destCluster_.isEmpty()) return nullptr;
    if (net.find_node(parent_.toStdString()) == nullptr) return nullptr;
    return net.node(parent_.toStdString())
        .node_pairwise(destCluster_.toStdString());
  }
  if (net.find_cluster(parent_.toStdString()) == nullptr) return nullptr;
  return &net.cluster(parent_.toStdString()).cluster_pairwise();
}

void PairwisePanel::rebuildViews() {
  updating_ = true;
  table_->clear();
  table_->setRowCount(0);
  table_->setColumnCount(0);
  info_->clear();

  while (QLayoutItem* item = questionnaireLay_->takeAt(0)) {
    if (QWidget* w = item->widget()) {
      w->deleteLater();
    }
    delete item;
  }

  if (parent_.isEmpty()) {
    info_->setText(
        QStringLiteral("Select a Wrt parent above to edit comparisons."));
    questionnaireLay_->addStretch();
    updating_ = false;
    return;
  }
  if (nodeMode() && destCluster_.isEmpty()) {
    info_->setText(QStringLiteral("Select an Other Cluster above."));
    questionnaireLay_->addStretch();
    updating_ = false;
    return;
  }

  const anpcpp::PairwiseJudgments* pw = currentPairwise();
  if (pw == nullptr) {
    info_->setText(QStringLiteral("Selected parent is not in this network."));
    questionnaireLay_->addStretch();
    updating_ = false;
    return;
  }
  if (pw->size() == 0) {
    info_->setText(QStringLiteral("No comparisons yet. Connect nodes first."));
    questionnaireLay_->addStretch();
    updating_ = false;
    return;
  }

  rebuildMatrix(pw);
  rebuildQuestionnaire(pw);
  updateInfo(pw);
  views_->setCurrentIndex(questionnaireBtn_->isChecked() ? 1 : 0);
  updating_ = false;
}

void PairwisePanel::rebuildMatrix(const anpcpp::PairwiseJudgments* pw) {
  const auto& names = pw->alternatives();
  const int n = static_cast<int>(names.size());
  table_->setRowCount(n);
  table_->setColumnCount(n);
  QStringList labels;
  for (const auto& s : names) labels << QString::fromStdString(s);
  table_->setHorizontalHeaderLabels(labels);
  table_->setVerticalHeaderLabels(labels);

  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) {
      auto* item = new QTableWidgetItem(
          QString::number(pw->comparison(static_cast<std::size_t>(i),
                                         static_cast<std::size_t>(j)),
                          'g', 6));
      if (i == j) item->setFlags(item->flags() & ~Qt::ItemIsEditable);
      table_->setItem(i, j, item);
    }
  }
}

void PairwisePanel::rebuildQuestionnaire(const anpcpp::PairwiseJudgments* pw) {
  const auto& names = pw->alternatives();
  const int n = static_cast<int>(names.size());

  auto* hint = new QLabel(
      QStringLiteral(
          "Mark toward the alternative that is more important / preferred."),
      questionnaireHost_);
  hint->setObjectName(QStringLiteral("selectorMuted"));
  hint->setWordWrap(true);
  questionnaireLay_->addWidget(hint);

  for (int i = 0; i < n; ++i) {
    for (int j = i + 1; j < n; ++j) {
      const QString left = QString::fromStdString(names[static_cast<std::size_t>(i)]);
      const QString right =
          QString::fromStdString(names[static_cast<std::size_t>(j)]);
      const double aij =
          pw->comparison(static_cast<std::size_t>(i),
                         static_cast<std::size_t>(j));

      auto* row = new QWidget(questionnaireHost_);
      row->setObjectName(QStringLiteral("questionnaireRow"));
      auto* rowLay = new QHBoxLayout(row);
      rowLay->setContentsMargins(4, 4, 4, 4);
      rowLay->setSpacing(4);

      auto* leftLabel = new QLabel(left, row);
      leftLabel->setMinimumWidth(100);
      leftLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
      leftLabel->setWordWrap(true);
      rowLay->addWidget(leftLabel, 1);

      auto* scaleGroup = new QButtonGroup(row);
      scaleGroup->setExclusive(true);

      int intensity = 1;
      bool preferLeft = true;
      decodeSaaty(aij, &intensity, &preferLeft);

      // Left side: 9..2 (prefer left), then 1, then right side 2..9 (prefer right).
      auto addScaleBtn = [&](int id, const QString& text) {
        auto* btn = new QRadioButton(text, row);
        btn->setObjectName(QStringLiteral("saatyScaleBtn"));
        btn->setCursor(Qt::PointingHandCursor);
        scaleGroup->addButton(btn, id);
        rowLay->addWidget(btn);
      };

      for (int k = 9; k >= 2; --k) {
        addScaleBtn(-k, QString::number(k));  // negative id => prefer left
      }
      addScaleBtn(1, QStringLiteral("1"));
      for (int k = 2; k <= 9; ++k) {
        addScaleBtn(k, QString::number(k));  // positive id => prefer right
      }

      const int checkedId = preferLeft ? (intensity == 1 ? 1 : -intensity)
                                       : intensity;
      if (auto* checked = scaleGroup->button(checkedId)) {
        checked->setChecked(true);
      } else if (auto* eq = scaleGroup->button(1)) {
        eq->setChecked(true);
      }

      auto* rightLabel = new QLabel(right, row);
      rightLabel->setMinimumWidth(100);
      rightLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
      rightLabel->setWordWrap(true);
      rowLay->addWidget(rightLabel, 1);

      connect(scaleGroup, &QButtonGroup::idClicked, this,
              [this, i, j](int id) {
                double value = 1.0;
                if (id == 1) {
                  value = 1.0;
                } else if (id < 0) {
                  value = static_cast<double>(-id);  // prefer left
                } else {
                  value = 1.0 / static_cast<double>(id);  // prefer right
                }
                onQuestionnaireAnswered(i, j, value);
              });

      questionnaireLay_->addWidget(row);
    }
  }
  questionnaireLay_->addStretch();
}

void PairwisePanel::updateInfo(const anpcpp::PairwiseJudgments* pw) {
  if (pw == nullptr || pw->size() < 2) {
    info_->clear();
    return;
  }
  const auto& names = pw->alternatives();
  const auto pri = pw->priorities();
  QStringList parts;
  for (std::size_t i = 0; i < pri.size(); ++i) {
    parts << QStringLiteral("%1=%2")
                 .arg(QString::fromStdString(names[i]))
                 .arg(pri[i], 0, 'f', 4);
  }
  info_->setText(QStringLiteral("Priorities: %1\nCR: %2")
                     .arg(parts.join(QStringLiteral(", ")))
                     .arg(pw->consistency_ratio(), 0, 'f', 4));
}

void PairwisePanel::applyComparison(const QString& a, const QString& b,
                                    double value) {
  const anpcpp::PairwiseJudgments* pw = currentPairwise();
  if (pw == nullptr || pw->size() == 0) return;
  const double old = pw->comparison(a.toStdString(), b.toStdString());
  if (std::abs(old - value) < 1e-12) return;

  if (nodeMode()) {
    doc_->undoStack()->push(
        new SetNodeComparisonCmd(doc_, parent_, a, b, value, old));
  } else {
    doc_->undoStack()->push(
        new SetClusterComparisonCmd(doc_, parent_, a, b, value, old));
  }
}

void PairwisePanel::onQuestionnaireAnswered(int row, int col, double value) {
  if (updating_) return;
  const anpcpp::PairwiseJudgments* pw = currentPairwise();
  if (pw == nullptr || pw->size() == 0) return;
  const auto& names = pw->alternatives();
  applyComparison(QString::fromStdString(names[static_cast<std::size_t>(row)]),
                  QString::fromStdString(names[static_cast<std::size_t>(col)]),
                  value);
}

void PairwisePanel::onCellChanged(int row, int col) {
  if (updating_ || row == col) return;
  if (table_->item(row, col) == nullptr) return;
  bool ok = false;
  const double value = table_->item(row, col)->text().toDouble(&ok);
  if (!ok || value <= 0.0) return;

  const anpcpp::PairwiseJudgments* pw = currentPairwise();
  if (pw == nullptr || pw->size() == 0) return;
  const auto& names = pw->alternatives();
  applyComparison(QString::fromStdString(names[static_cast<std::size_t>(row)]),
                  QString::fromStdString(names[static_cast<std::size_t>(col)]),
                  value);
}
