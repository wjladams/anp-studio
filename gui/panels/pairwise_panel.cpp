#include "panels/pairwise_panel.hpp"

#include "commands/network_commands.hpp"
#include "document.hpp"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QRadioButton>
#include <QTableWidget>
#include <QVBoxLayout>

PairwisePanel::PairwisePanel(Document* doc, QWidget* parent)
    : QWidget(parent), doc_(doc) {
  auto* layout = new QVBoxLayout(this);
  auto* modeRow = new QHBoxLayout;
  nodeMode_ = new QRadioButton(QStringLiteral("Node parent"), this);
  clusterMode_ = new QRadioButton(QStringLiteral("Cluster parent"), this);
  nodeMode_->setChecked(true);
  modeRow->addWidget(nodeMode_);
  modeRow->addWidget(clusterMode_);
  layout->addLayout(modeRow);

  parentBox_ = new QComboBox(this);
  destClusterBox_ = new QComboBox(this);
  layout->addWidget(new QLabel(QStringLiteral("Parent:"), this));
  layout->addWidget(parentBox_);
  layout->addWidget(new QLabel(QStringLiteral("Destination cluster:"), this));
  layout->addWidget(destClusterBox_);

  table_ = new QTableWidget(this);
  table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  layout->addWidget(table_);
  info_ = new QLabel(this);
  layout->addWidget(info_);

  connect(nodeMode_, &QRadioButton::toggled, this,
          &PairwisePanel::onParentModeChanged);
  connect(parentBox_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &PairwisePanel::onParentChanged);
  connect(destClusterBox_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &PairwisePanel::onDestClusterChanged);
  connect(table_, &QTableWidget::cellChanged, this,
          &PairwisePanel::onCellChanged);
  connect(doc_, &Document::modelChanged, this, &PairwisePanel::refresh);
  connect(doc_, &Document::viewNetworkChanged, this, &PairwisePanel::refresh);

  refresh();
}

bool PairwisePanel::nodeMode() const {
  return nodeMode_->isChecked();
}

void PairwisePanel::selectNodeParent(const QString& name) {
  nodeMode_->setChecked(true);
  refresh();
  const int idx = parentBox_->findText(name);
  if (idx >= 0) parentBox_->setCurrentIndex(idx);
}

void PairwisePanel::selectClusterParent(const QString& name) {
  clusterMode_->setChecked(true);
  refresh();
  const int idx = parentBox_->findText(name);
  if (idx >= 0) parentBox_->setCurrentIndex(idx);
}

void PairwisePanel::onParentModeChanged() {
  refresh();
}

void PairwisePanel::onParentChanged(int) {
  rebuildTable();
}

void PairwisePanel::onDestClusterChanged(int) {
  rebuildTable();
}

void PairwisePanel::refresh() {
  // Guard table/cell handlers while repopulating combo boxes.
  updating_ = true;
  const QString curParent = parentBox_->currentText();
  const QString curDest = destClusterBox_->currentText();
  parentBox_->clear();
  destClusterBox_->clear();

  auto& net = doc_->network();
  if (nodeMode()) {
    // Node parent: compare nodes in a chosen destination cluster.
    for (const auto& n : net.node_names()) {
      parentBox_->addItem(QString::fromStdString(n));
    }
    for (const auto& c : net.cluster_names()) {
      destClusterBox_->addItem(QString::fromStdString(c));
    }
    destClusterBox_->setEnabled(true);
  } else {
    // Cluster parent: compare clusters (no destination selector).
    for (const auto& c : net.cluster_names()) {
      parentBox_->addItem(QString::fromStdString(c));
    }
    destClusterBox_->setEnabled(false);
  }

  int p = parentBox_->findText(curParent);
  if (p >= 0) parentBox_->setCurrentIndex(p);
  int d = destClusterBox_->findText(curDest);
  if (d >= 0) destClusterBox_->setCurrentIndex(d);
  updating_ = false;
  rebuildTable();
}

void PairwisePanel::rebuildTable() {
  updating_ = true;
  table_->clear();
  table_->setRowCount(0);
  table_->setColumnCount(0);
  info_->clear();

  if (parentBox_->currentText().isEmpty()) {
    updating_ = false;
    return;
  }

  auto& net = doc_->network();
  const anpcpp::PairwiseJudgments* pw = nullptr;

  // Resolve the pairwise table for the current parent + mode selection.
  if (nodeMode()) {
    if (destClusterBox_->currentText().isEmpty()) {
      updating_ = false;
      return;
    }
    anpcpp::AnpNode& node =
        net.node(parentBox_->currentText().toStdString());
    pw = node.node_pairwise(destClusterBox_->currentText().toStdString());
  } else {
    pw = &net.cluster(parentBox_->currentText().toStdString())
              .cluster_pairwise();
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
      // Diagonal is always 1 and not user-editable.
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
  // Ignore programmatic fills and reciprocal diagonal updates.
  if (updating_ || row == col) return;
  bool ok = false;
  const double value = table_->item(row, col)->text().toDouble(&ok);
  if (!ok) return;

  auto& net = doc_->network();
  if (nodeMode()) {
    anpcpp::AnpNode& node =
        net.node(parentBox_->currentText().toStdString());
    auto* pw =
        node.node_pairwise(destClusterBox_->currentText().toStdString());
    if (pw == nullptr || pw->size() == 0) return;
    const auto& names = pw->alternatives();
    const double old =
        pw->comparison(static_cast<std::size_t>(row),
                       static_cast<std::size_t>(col));
    // Route edits through undo so the table refresh stays in sync with the model.
    doc_->undoStack()->push(new SetNodeComparisonCmd(
        doc_, parentBox_->currentText(),
        QString::fromStdString(names[static_cast<std::size_t>(row)]),
        QString::fromStdString(names[static_cast<std::size_t>(col)]), value,
        old));
  } else {
    auto& pw =
        net.cluster(parentBox_->currentText().toStdString()).cluster_pairwise();
    if (pw.size() == 0) return;
    const auto& names = pw.alternatives();
    const double old =
        pw.comparison(static_cast<std::size_t>(row),
                      static_cast<std::size_t>(col));
    doc_->undoStack()->push(new SetClusterComparisonCmd(
        doc_, parentBox_->currentText(),
        QString::fromStdString(names[static_cast<std::size_t>(row)]),
        QString::fromStdString(names[static_cast<std::size_t>(col)]), value,
        old));
  }
}
