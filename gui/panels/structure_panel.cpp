#include "panels/structure_panel.hpp"

#include "commands/network_commands.hpp"
#include "document.hpp"

#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

StructurePanel::StructurePanel(Document* doc, QWidget* parent)
    : QWidget(parent), doc_(doc) {
  auto* layout = new QVBoxLayout(this);
  tree_ = new QTreeWidget(this);
  tree_->setHeaderLabels({QStringLiteral("Name"), QStringLiteral("Kind")});
  tree_->setContextMenuPolicy(Qt::CustomContextMenu);
  layout->addWidget(tree_);

  connect(tree_, &QTreeWidget::itemClicked, this,
          &StructurePanel::onItemClicked);
  connect(tree_, &QTreeWidget::customContextMenuRequested, this,
          &StructurePanel::onContextMenu);
  connect(doc_, &Document::modelChanged, this, &StructurePanel::refresh);
  connect(doc_, &Document::viewNetworkChanged, this, &StructurePanel::refresh);
  connect(doc_, &Document::selectionChanged, this,
          &StructurePanel::syncSelectionFromDoc);
  refresh();
}

void StructurePanel::refresh() {
  updating_ = true;
  tree_->clear();
  for (anpcpp::AnpCluster* c : doc_->network().clusters()) {
    auto* ci = new QTreeWidgetItem(
        tree_, {QString::fromStdString(c->name()), QStringLiteral("Cluster")});
    for (anpcpp::AnpNode* n : c->nodes()) {
      QString label = QString::fromStdString(n->name());
      if (n->has_subnetwork()) label += QStringLiteral(" [subnet]");
      if (n->invert()) label += QStringLiteral(" [inv]");
      auto* ni = new QTreeWidgetItem(
          ci, {QString::fromStdString(n->name()), QStringLiteral("Node")});
      ni->setToolTip(0, label);
    }
    ci->setExpanded(true);
  }
  updating_ = false;
  syncSelectionFromDoc();
}

void StructurePanel::syncSelectionFromDoc() {
  if (updating_) return;
  const QString node = doc_->selectedNode();
  const QString cluster = doc_->selectedCluster();
  tree_->clearSelection();
  for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
    auto* ci = tree_->topLevelItem(i);
    if (!cluster.isEmpty() && ci->text(0) == cluster && node.isEmpty()) {
      tree_->setCurrentItem(ci);
      return;
    }
    for (int j = 0; j < ci->childCount(); ++j) {
      auto* ni = ci->child(j);
      if (!node.isEmpty() && ni->text(0) == node) {
        tree_->setCurrentItem(ni);
        return;
      }
    }
  }
}

void StructurePanel::onItemClicked() {
  auto* item = tree_->currentItem();
  if (item == nullptr) return;
  const QString kind = item->text(1);
  if (kind == QStringLiteral("Node")) {
    doc_->setSelection(item->parent() ? item->parent()->text(0) : QString(),
                       item->text(0));
    emit nodeSelected(item->text(0));
  } else if (kind == QStringLiteral("Cluster")) {
    doc_->setSelection(item->text(0), {});
    emit clusterSelected(item->text(0));
  }
}

void StructurePanel::onContextMenu(const QPoint& pos) {
  auto* item = tree_->itemAt(pos);
  QMenu menu(this);
  menu.addAction(QStringLiteral("Add cluster…"), this, [this]() {
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, QStringLiteral("Add cluster"), QStringLiteral("Name:"),
        QLineEdit::Normal, {}, &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    doc_->undoStack()->push(new AddClusterCmd(doc_, name.trimmed()));
  });
  if (item != nullptr && item->text(1) == QStringLiteral("Cluster")) {
    const QString cluster = item->text(0);
    menu.addAction(QStringLiteral("Add node…"), this, [this, cluster]() {
      bool ok = false;
      const QString name = QInputDialog::getText(
          this, QStringLiteral("Add node"), QStringLiteral("Name:"),
          QLineEdit::Normal, {}, &ok);
      if (!ok || name.trimmed().isEmpty()) return;
      doc_->undoStack()->push(
          new AddNodeCmd(doc_, cluster, name.trimmed()));
    });
    menu.addAction(QStringLiteral("Delete cluster"), this, [this, cluster]() {
      doc_->undoStack()->push(new RemoveClusterCmd(doc_, cluster));
    });
  }
  if (item != nullptr && item->text(1) == QStringLiteral("Node")) {
    const QString node = item->text(0);
    menu.addAction(QStringLiteral("Delete node"), this, [this, node]() {
      doc_->undoStack()->push(new RemoveNodeCmd(doc_, node));
    });
  }
  menu.exec(tree_->viewport()->mapToGlobal(pos));
}
