#include "panels/judgment_nav_panel.hpp"

#include "commands/network_commands.hpp"
#include "document.hpp"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

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

}  // namespace

JudgmentNavPanel::JudgmentNavPanel(Document* doc, QWidget* parent)
    : QWidget(parent), doc_(doc) {
  auto* layout = new QVBoxLayout(this);
  layout->addWidget(new QLabel(QStringLiteral("Judgments"), this));

  filter_ = new QComboBox(this);
  filter_->addItem(QStringLiteral("All"), 0);
  filter_->addItem(QStringLiteral("Node parents"), 1);
  filter_->addItem(QStringLiteral("Cluster parents"), 2);
  filter_->addItem(QStringLiteral("Alternatives dest only"), 3);
  layout->addWidget(filter_);

  coverageLabel_ = new QLabel(this);
  layout->addWidget(coverageLabel_);

  tree_ = new QTreeWidget(this);
  tree_->setHeaderLabels({QStringLiteral("Parent"), QStringLiteral("Dest"),
                          QStringLiteral("Kind"), QStringLiteral("Cover"),
                          QStringLiteral("CR")});
  layout->addWidget(tree_);

  auto* switchRow = new QHBoxLayout;
  toPairwise_ = new QPushButton(QStringLiteral("Use Pairwise"), this);
  toRatings_ = new QPushButton(QStringLiteral("Use Ratings"), this);
  switchRow->addWidget(toPairwise_);
  switchRow->addWidget(toRatings_);
  layout->addLayout(switchRow);

  connect(filter_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &JudgmentNavPanel::refresh);
  connect(tree_, &QTreeWidget::itemClicked, this,
          &JudgmentNavPanel::onItemClicked);
  connect(toPairwise_, &QPushButton::clicked, this,
          &JudgmentNavPanel::onSwitchToPairwise);
  connect(toRatings_, &QPushButton::clicked, this,
          &JudgmentNavPanel::onSwitchToRatings);
  connect(doc_, &Document::modelChanged, this, &JudgmentNavPanel::refresh);
  connect(doc_, &Document::viewNetworkChanged, this, &JudgmentNavPanel::refresh);
  refresh();
}

void JudgmentNavPanel::refresh() {
  tree_->clear();
  auto& net = doc_->network();
  const int filter = filter_->currentData().toInt();
  QString altsName;
  if (auto* a = net.alternatives_cluster()) {
    altsName = QString::fromStdString(a->name());
  }

  double coverSum = 0.0;
  int coverCount = 0;

  if (filter != 2) {
    for (anpcpp::AnpNode* n : net.nodes()) {
      for (anpcpp::AnpCluster* dest : net.clusters()) {
        const auto* slot = n->node_prioritizer(dest->name());
        if (slot == nullptr || slot->empty()) continue;
        const QString destName = QString::fromStdString(dest->name());
        if (filter == 3 && !altsName.isEmpty() && destName != altsName) continue;

        const bool ratings =
            slot->kind == anpcpp::NodePrioritizerKind::Ratings;
        double cover = 0.0;
        QString crText = QStringLiteral("—");
        if (ratings) {
          cover = ratingsCoverage(slot->ratings);
        } else {
          cover = pairwiseCoverage(slot->pairwise);
          if (slot->pairwise.size() >= 3) {
            const double cr = slot->pairwise.consistency_ratio();
            crText = QString::number(cr, 'f', 3);
            if (cr > 0.1) crText += QStringLiteral(" ⚠");
          }
        }
        coverSum += cover;
        ++coverCount;

        auto* item = new QTreeWidgetItem(tree_);
        item->setText(0, QString::fromStdString(n->name()));
        item->setText(1, destName);
        item->setText(2, ratings ? QStringLiteral("Ratings")
                                 : QStringLiteral("Pairwise"));
        item->setText(3, QString::number(cover * 100.0, 'f', 0) +
                             QStringLiteral("%"));
        item->setText(4, crText);
        item->setData(0, Qt::UserRole, 1);  // node
        item->setData(0, Qt::UserRole + 1, ratings);
      }
    }
  }

  if (filter == 0 || filter == 2) {
    for (anpcpp::AnpCluster* c : net.clusters()) {
      const auto& pw = c->cluster_pairwise();
      if (pw.size() < 2) continue;
      const double cover = pairwiseCoverage(pw);
      coverSum += cover;
      ++coverCount;
      QString crText = QStringLiteral("—");
      if (pw.size() >= 3) {
        const double cr = pw.consistency_ratio();
        crText = QString::number(cr, 'f', 3);
        if (cr > 0.1) crText += QStringLiteral(" ⚠");
      }
      auto* item = new QTreeWidgetItem(tree_);
      item->setText(0, QString::fromStdString(c->name()));
      item->setText(1, QStringLiteral("(clusters)"));
      item->setText(2, QStringLiteral("Pairwise"));
      item->setText(3, QString::number(cover * 100.0, 'f', 0) +
                           QStringLiteral("%"));
      item->setText(4, crText);
      item->setData(0, Qt::UserRole, 0);  // cluster
    }
  }

  if (coverCount > 0) {
    coverageLabel_->setText(
        QStringLiteral("Overall coverage: %1%")
            .arg(QString::number(100.0 * coverSum / coverCount, 'f', 0)));
  } else {
    coverageLabel_->setText(
        QStringLiteral("No judgment parents — connect nodes in Structure."));
  }

  const bool canSwitch = currentIsNode_ && !currentParent_.isEmpty() &&
                         !currentDest_.isEmpty();
  toPairwise_->setEnabled(canSwitch);
  toRatings_->setEnabled(canSwitch);
}

void JudgmentNavPanel::onItemClicked() {
  auto* item = tree_->currentItem();
  if (item == nullptr) return;
  currentParent_ = item->text(0);
  currentDest_ = item->text(1);
  currentIsNode_ = item->data(0, Qt::UserRole).toInt() == 1;
  const bool ratings = item->data(0, Qt::UserRole + 1).toBool();

  if (currentIsNode_) {
    doc_->setSelection({}, currentParent_);
    emit nodeJudgmentSelected(currentParent_, currentDest_, ratings);
  } else {
    doc_->setSelection(currentParent_, {});
    emit clusterJudgmentSelected(currentParent_);
  }
  toPairwise_->setEnabled(currentIsNode_);
  toRatings_->setEnabled(currentIsNode_);
}

void JudgmentNavPanel::onSwitchToPairwise() {
  if (!currentIsNode_ || currentParent_.isEmpty() || currentDest_.isEmpty())
    return;
  doc_->undoStack()->push(new SetPrioritizerKindCmd(
      doc_, currentParent_, currentDest_,
      anpcpp::NodePrioritizerKind::Pairwise));
  emit nodeJudgmentSelected(currentParent_, currentDest_, false);
}

void JudgmentNavPanel::onSwitchToRatings() {
  if (!currentIsNode_ || currentParent_.isEmpty() || currentDest_.isEmpty())
    return;
  doc_->undoStack()->push(new SetPrioritizerKindCmd(
      doc_, currentParent_, currentDest_,
      anpcpp::NodePrioritizerKind::Ratings));
  emit nodeJudgmentSelected(currentParent_, currentDest_, true);
}
