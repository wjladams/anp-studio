#include "panels/pairwise_panel.hpp"

#include "commands/network_commands.hpp"
#include "document.hpp"

#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

PairwisePanel::PairwisePanel(Document* doc, QWidget* parent)
    : QWidget(parent), doc_(doc) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  table_ = new QTableWidget(this);
  table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  layout->addWidget(table_);
  info_ = new QLabel(this);
  layout->addWidget(info_);

  connect(table_, &QTableWidget::cellChanged, this,
          &PairwisePanel::onCellChanged);
  connect(doc_, &Document::modelChanged, this, &PairwisePanel::refresh);
  connect(doc_, &Document::viewNetworkChanged, this, &PairwisePanel::refresh);
}

void PairwisePanel::selectNodeParent(const QString& name) {
  nodeMode_ = true;
  parent_ = name;
  rebuildTable();
}

void PairwisePanel::selectNodeLink(const QString& parent,
                                   const QString& destCluster) {
  nodeMode_ = true;
  parent_ = parent;
  destCluster_ = destCluster;
  rebuildTable();
}

void PairwisePanel::selectClusterParent(const QString& name) {
  nodeMode_ = false;
  parent_ = name;
  destCluster_.clear();
  rebuildTable();
}

void PairwisePanel::refresh() {
  rebuildTable();
}

void PairwisePanel::rebuildTable() {
  updating_ = true;
  table_->clear();
  table_->setRowCount(0);
  table_->setColumnCount(0);
  info_->clear();

  if (parent_.isEmpty()) {
    info_->setText(
        QStringLiteral("Select a Wrt parent above to edit comparisons."));
    updating_ = false;
    return;
  }

  auto& net = doc_->network();
  const anpcpp::PairwiseJudgments* pw = nullptr;

  if (nodeMode()) {
    if (destCluster_.isEmpty()) {
      info_->setText(QStringLiteral("Select an Other Cluster above."));
      updating_ = false;
      return;
    }
    if (net.find_node(parent_.toStdString()) == nullptr) {
      info_->setText(QStringLiteral("Selected node is not in this network."));
      updating_ = false;
      return;
    }
    anpcpp::AnpNode& node = net.node(parent_.toStdString());
    pw = node.node_pairwise(destCluster_.toStdString());
  } else {
    if (net.find_cluster(parent_.toStdString()) == nullptr) {
      info_->setText(
          QStringLiteral("Selected cluster is not in this network."));
      updating_ = false;
      return;
    }
    pw = &net.cluster(parent_.toStdString()).cluster_pairwise();
  }

  if (pw == nullptr || pw->size() == 0) {
    info_->setText(QStringLiteral("No comparisons yet. Connect nodes first."));
    updating_ = false;
    return;
  }

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

  if (pw->size() >= 2) {
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
  updating_ = false;
}

void PairwisePanel::onCellChanged(int row, int col) {
  if (updating_ || row == col) return;
  if (table_->item(row, col) == nullptr) return;
  bool ok = false;
  const double value = table_->item(row, col)->text().toDouble(&ok);
  if (!ok) return;

  auto& net = doc_->network();
  if (nodeMode()) {
    anpcpp::AnpNode& node = net.node(parent_.toStdString());
    auto* pw = node.node_pairwise(destCluster_.toStdString());
    if (pw == nullptr || pw->size() == 0) return;
    const auto& names = pw->alternatives();
    const double old =
        pw->comparison(static_cast<std::size_t>(row),
                       static_cast<std::size_t>(col));
    doc_->undoStack()->push(new SetNodeComparisonCmd(
        doc_, parent_,
        QString::fromStdString(names[static_cast<std::size_t>(row)]),
        QString::fromStdString(names[static_cast<std::size_t>(col)]), value,
        old));
  } else {
    auto& pw = net.cluster(parent_.toStdString()).cluster_pairwise();
    if (pw.size() == 0) return;
    const auto& names = pw.alternatives();
    const double old =
        pw.comparison(static_cast<std::size_t>(row),
                      static_cast<std::size_t>(col));
    doc_->undoStack()->push(new SetClusterComparisonCmd(
        doc_, parent_,
        QString::fromStdString(names[static_cast<std::size_t>(row)]),
        QString::fromStdString(names[static_cast<std::size_t>(col)]), value,
        old));
  }
}
