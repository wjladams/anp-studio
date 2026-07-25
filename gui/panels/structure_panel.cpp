#include "panels/structure_panel.hpp"

#include "document.hpp"

#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

StructurePanel::StructurePanel(Document* doc, QWidget* parent)
    : QWidget(parent), doc_(doc) {
  auto* layout = new QVBoxLayout(this);
  crumb_ = new QLabel(this);
  layout->addWidget(crumb_);

  auto* up = new QPushButton(QStringLiteral("Up to Parent"), this);
  connect(up, &QPushButton::clicked, this, [this]() { doc_->popSubnet(); });
  layout->addWidget(up);
  auto* root = new QPushButton(QStringLiteral("Root Network"), this);
  connect(root, &QPushButton::clicked, this, [this]() { doc_->popToRoot(); });
  layout->addWidget(root);

  tree_ = new QTreeWidget(this);
  tree_->setHeaderLabels({QStringLiteral("Name"), QStringLiteral("Kind")});
  layout->addWidget(tree_);

  connect(tree_, &QTreeWidget::itemClicked, this,
          [this](QTreeWidgetItem* item, int) {
            const QString kind = item->text(1);
            if (kind == QStringLiteral("Node")) {
              emit nodeSelected(item->text(0));
            } else if (kind == QStringLiteral("Cluster")) {
              emit clusterSelected(item->text(0));
            }
          });

  connect(doc_, &Document::modelChanged, this, &StructurePanel::refresh);
  connect(doc_, &Document::viewNetworkChanged, this, &StructurePanel::refresh);
  refresh();
}

void StructurePanel::refresh() {
  crumb_->setText(doc_->breadcrumb().join(QStringLiteral(" / ")));
  tree_->clear();
  for (cppanp::AnpCluster* c : doc_->network().clusters()) {
    auto* ci = new QTreeWidgetItem(
        tree_, {QString::fromStdString(c->name()), QStringLiteral("Cluster")});
    for (cppanp::AnpNode* n : c->nodes()) {
      QString label = QString::fromStdString(n->name());
      if (n->has_subnetwork()) label += QStringLiteral(" [subnet]");
      if (n->invert()) label += QStringLiteral(" [inv]");
      new QTreeWidgetItem(ci,
                          {QString::fromStdString(n->name()),
                           QStringLiteral("Node")});
      (void)label;
    }
    ci->setExpanded(true);
  }
}
