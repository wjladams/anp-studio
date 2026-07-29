#include "panels/judgment_nav_panel.hpp"

#include "commands/network_commands.hpp"
#include "document.hpp"

#include <QButtonGroup>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

namespace {

double pairwiseCoverage(const anpcpp::PairwiseJudgments& pw) {
  const std::size_t n = pw.size();
  if (n < 2) return n == 1 ? 1.0 : 0.0;
  std::size_t need = 0;
  std::size_t filled = 0;
  for (std::size_t i = 0; i < n; ++i) {
    for (std::size_t j = i + 1; j < n; ++j) {
      ++need;
      if (pw.comparison(i, j) != 0.0) ++filled;
    }
  }
  return need == 0 ? 0.0 : static_cast<double>(filled) / static_cast<double>(need);
}

double ratingsCoverage(const anpcpp::RatingsPrioritizer& rt) {
  if (rt.empty()) return 0.0;
  std::size_t filled = 0;
  for (const auto& alt : rt.alternatives()) {
    if (rt.mode() == anpcpp::RatingsPrioritizer::Mode::Categorical) {
      if (rt.rating(alt).has_value()) ++filled;
    } else if (rt.value(alt).has_value()) {
      ++filled;
    }
  }
  return static_cast<double>(filled) / static_cast<double>(rt.size());
}

bool nodeHasOutgoing(const anpcpp::AnpNode& n) {
  for (anpcpp::AnpCluster* dest : n.network()->clusters()) {
    const auto* slot = n.node_prioritizer(dest->name());
    if (slot != nullptr && !slot->empty()) return true;
  }
  return false;
}

bool clusterHasOutgoing(const anpcpp::AnpCluster& c) {
  // Cluster pairwise alternatives are destinations connected from this cluster.
  return c.cluster_pairwise().size() >= 2;
}

}  // namespace

JudgmentNavPanel::JudgmentNavPanel(Document* doc, QWidget* parent)
    : QWidget(parent), doc_(doc) {
  setObjectName(QStringLiteral("judgmentSelector"));
  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* modeLabel = new QLabel(QStringLiteral("Compare:"), this);
  modeLabel->setObjectName(QStringLiteral("selectorCaption"));
  layout->addWidget(modeLabel);

  modeGroup_ = new QButtonGroup(this);
  modeGroup_->setExclusive(true);
  nodeModeBtn_ = new QPushButton(QStringLiteral("Node"), this);
  clusterModeBtn_ = new QPushButton(QStringLiteral("Cluster"), this);
  for (auto* b : {nodeModeBtn_, clusterModeBtn_}) {
    b->setObjectName(QStringLiteral("selectorToggle"));
    b->setCheckable(true);
    b->setFlat(true);
    b->setCursor(Qt::PointingHandCursor);
    layout->addWidget(b);
  }
  modeGroup_->addButton(nodeModeBtn_, 0);
  modeGroup_->addButton(clusterModeBtn_, 1);
  nodeModeBtn_->setChecked(true);

  wrtLabel_ = new QLabel(QStringLiteral("Wrt Node:"), this);
  wrtLabel_->setObjectName(QStringLiteral("selectorCaption"));
  layout->addWidget(wrtLabel_);
  wrtBox_ = new QComboBox(this);
  wrtBox_->setMinimumWidth(140);
  layout->addWidget(wrtBox_);

  otherLabel_ = new QLabel(QStringLiteral("Other Cluster:"), this);
  otherLabel_->setObjectName(QStringLiteral("selectorCaption"));
  layout->addWidget(otherLabel_);
  otherBox_ = new QComboBox(this);
  otherBox_->setMinimumWidth(140);
  layout->addWidget(otherBox_);

  toPairwise_ = new QPushButton(QStringLiteral("Pairwise"), this);
  toRatings_ = new QPushButton(QStringLiteral("Ratings"), this);
  toPairwise_->setObjectName(QStringLiteral("selectorToggle"));
  toRatings_->setObjectName(QStringLiteral("selectorToggle"));
  toPairwise_->setCheckable(true);
  toRatings_->setCheckable(true);
  toPairwise_->setFlat(true);
  toRatings_->setFlat(true);
  toPairwise_->setCursor(Qt::PointingHandCursor);
  toRatings_->setCursor(Qt::PointingHandCursor);
  auto* kindGroup = new QButtonGroup(this);
  kindGroup->setExclusive(true);
  kindGroup->addButton(toPairwise_);
  kindGroup->addButton(toRatings_);
  toPairwise_->setChecked(true);
  layout->addWidget(toPairwise_);
  layout->addWidget(toRatings_);

  coverageLabel_ = new QLabel(this);
  coverageLabel_->setObjectName(QStringLiteral("selectorMuted"));
  layout->addWidget(coverageLabel_, 1);

  connect(modeGroup_, &QButtonGroup::idClicked, this,
          [this](int) { onModeChanged(); });
  connect(wrtBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int) { onWrtChanged(); });
  connect(otherBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int) { onOtherChanged(); });
  connect(toPairwise_, &QPushButton::clicked, this,
          &JudgmentNavPanel::onSwitchToPairwise);
  connect(toRatings_, &QPushButton::clicked, this,
          &JudgmentNavPanel::onSwitchToRatings);
  connect(doc_, &Document::modelChanged, this, &JudgmentNavPanel::refresh);
  connect(doc_, &Document::viewNetworkChanged, this, &JudgmentNavPanel::refresh);
  refresh();
}

bool JudgmentNavPanel::nodeMode() const {
  return nodeModeBtn_->isChecked();
}

void JudgmentNavPanel::onModeChanged() {
  preferRatings_ = false;
  toPairwise_->setChecked(true);
  refresh();
}

void JudgmentNavPanel::onWrtChanged() {
  if (updating_) return;
  rebuildOtherList();
  emitCurrent();
}

void JudgmentNavPanel::onOtherChanged() {
  if (updating_) return;
  emitCurrent();
}

void JudgmentNavPanel::onSwitchToPairwise() {
  preferRatings_ = false;
  toPairwise_->setChecked(true);
  if (!nodeMode() || wrtBox_->currentText().isEmpty() ||
      otherBox_->currentText().isEmpty()) {
    emitCurrent();
    return;
  }
  doc_->undoStack()->push(new SetPrioritizerKindCmd(
      doc_, wrtBox_->currentText(), otherBox_->currentText(),
      anpcpp::NodePrioritizerKind::Pairwise));
  emit nodeJudgmentSelected(wrtBox_->currentText(), otherBox_->currentText(),
                            false);
}

void JudgmentNavPanel::onSwitchToRatings() {
  preferRatings_ = true;
  toRatings_->setChecked(true);
  if (!nodeMode() || wrtBox_->currentText().isEmpty() ||
      otherBox_->currentText().isEmpty()) {
    return;
  }
  doc_->undoStack()->push(new SetPrioritizerKindCmd(
      doc_, wrtBox_->currentText(), otherBox_->currentText(),
      anpcpp::NodePrioritizerKind::Ratings));
  emit nodeJudgmentSelected(wrtBox_->currentText(), otherBox_->currentText(),
                            true);
}

void JudgmentNavPanel::refresh() {
  updating_ = true;
  const QString curWrt = wrtBox_->currentText();
  const QString curOther = otherBox_->currentText();

  wrtLabel_->setText(nodeMode() ? QStringLiteral("Wrt Node:")
                                : QStringLiteral("Wrt Cluster:"));
  otherLabel_->setVisible(nodeMode());
  otherBox_->setVisible(nodeMode());
  toPairwise_->setVisible(nodeMode());
  toRatings_->setVisible(nodeMode());

  rebuildWrtList();
  int wi = wrtBox_->findText(curWrt);
  if (wi >= 0) wrtBox_->setCurrentIndex(wi);
  else if (wrtBox_->count() > 0) wrtBox_->setCurrentIndex(0);

  rebuildOtherList();
  int oi = otherBox_->findText(curOther);
  if (oi >= 0) otherBox_->setCurrentIndex(oi);
  else if (otherBox_->count() > 0) otherBox_->setCurrentIndex(0);

  // Coverage for the current selection (or hint when empty).
  if (wrtBox_->count() == 0) {
    coverageLabel_->setText(
        QStringLiteral("No connected parents — connect nodes in Structure."));
  } else if (nodeMode() && !wrtBox_->currentText().isEmpty() &&
             !otherBox_->currentText().isEmpty()) {
    auto& n = doc_->network().node(wrtBox_->currentText().toStdString());
    const auto* slot =
        n.node_prioritizer(otherBox_->currentText().toStdString());
    if (slot != nullptr && !slot->empty()) {
      const bool ratings =
          slot->kind == anpcpp::NodePrioritizerKind::Ratings;
      preferRatings_ = ratings;
      toRatings_->setChecked(ratings);
      toPairwise_->setChecked(!ratings);
      const double cover = ratings ? ratingsCoverage(slot->ratings)
                                   : pairwiseCoverage(slot->pairwise);
      QString text = QStringLiteral("Coverage: %1%")
                         .arg(QString::number(cover * 100.0, 'f', 0));
      if (!ratings && slot->pairwise.size() >= 3) {
        const double cr = slot->pairwise.consistency_ratio();
        text += QStringLiteral("   CR: %1").arg(QString::number(cr, 'f', 3));
        if (cr > 0.1) text += QStringLiteral(" ⚠");
      }
      coverageLabel_->setText(text);
    } else {
      coverageLabel_->clear();
    }
  } else if (!nodeMode() && !wrtBox_->currentText().isEmpty()) {
    const auto& pw =
        doc_->network().cluster(wrtBox_->currentText().toStdString())
            .cluster_pairwise();
    const double cover = pairwiseCoverage(pw);
    QString text = QStringLiteral("Coverage: %1%")
                       .arg(QString::number(cover * 100.0, 'f', 0));
    if (pw.size() >= 3) {
      const double cr = pw.consistency_ratio();
      text += QStringLiteral("   CR: %1").arg(QString::number(cr, 'f', 3));
      if (cr > 0.1) text += QStringLiteral(" ⚠");
    }
    coverageLabel_->setText(text);
  } else {
    coverageLabel_->clear();
  }

  updating_ = false;
  emitCurrent();
}

void JudgmentNavPanel::rebuildWrtList() {
  wrtBox_->clear();
  auto& net = doc_->network();
  if (nodeMode()) {
    for (anpcpp::AnpNode* n : net.nodes()) {
      if (nodeHasOutgoing(*n)) {
        wrtBox_->addItem(QString::fromStdString(n->name()));
      }
    }
  } else {
    for (anpcpp::AnpCluster* c : net.clusters()) {
      if (clusterHasOutgoing(*c)) {
        wrtBox_->addItem(QString::fromStdString(c->name()));
      }
    }
  }
}

void JudgmentNavPanel::rebuildOtherList() {
  otherBox_->clear();
  if (!nodeMode() || wrtBox_->currentText().isEmpty()) return;
  auto& n = doc_->network().node(wrtBox_->currentText().toStdString());
  for (anpcpp::AnpCluster* dest : doc_->network().clusters()) {
    const auto* slot = n.node_prioritizer(dest->name());
    if (slot != nullptr && !slot->empty()) {
      otherBox_->addItem(QString::fromStdString(dest->name()));
    }
  }
}

void JudgmentNavPanel::emitCurrent() {
  if (updating_) return;
  if (nodeMode()) {
    if (wrtBox_->currentText().isEmpty() || otherBox_->currentText().isEmpty())
      return;
    auto& n = doc_->network().node(wrtBox_->currentText().toStdString());
    const auto* slot =
        n.node_prioritizer(otherBox_->currentText().toStdString());
    const bool ratings =
        preferRatings_ ||
        (slot != nullptr &&
         slot->kind == anpcpp::NodePrioritizerKind::Ratings);
    doc_->setSelection({}, wrtBox_->currentText());
    emit nodeJudgmentSelected(wrtBox_->currentText(), otherBox_->currentText(),
                              ratings);
  } else {
    if (wrtBox_->currentText().isEmpty()) return;
    doc_->setSelection(wrtBox_->currentText(), {});
    emit clusterJudgmentSelected(wrtBox_->currentText());
  }
}
