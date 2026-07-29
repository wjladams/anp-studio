#include "panels/inspector_panel.hpp"

#include "commands/network_commands.hpp"
#include "document.hpp"

#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

InspectorPanel::InspectorPanel(Document* doc, QWidget* parent)
    : QWidget(parent), doc_(doc) {
  auto* layout = new QVBoxLayout(this);
  title_ = new QLabel(QStringLiteral("Inspector"), this);
  title_->setStyleSheet(QStringLiteral("font-weight: bold;"));
  layout->addWidget(title_);
  nameLabel_ = new QLabel(QStringLiteral("Nothing selected"), this);
  layout->addWidget(nameLabel_);
  invert_ = new QCheckBox(QStringLiteral("Invert (subnet synthesis)"), this);
  layout->addWidget(invert_);
  setAlts_ = new QPushButton(QStringLiteral("Set as alternatives cluster"), this);
  layout->addWidget(setAlts_);
  openSubnet_ = new QPushButton(QStringLiteral("Open Subnetwork"), this);
  layout->addWidget(openSubnet_);
  layout->addStretch();

  connect(invert_, &QCheckBox::toggled, this, &InspectorPanel::onInvertToggled);
  connect(setAlts_, &QPushButton::clicked, this, &InspectorPanel::onSetAlternatives);
  connect(openSubnet_, &QPushButton::clicked, this, &InspectorPanel::onOpenSubnet);
  connect(doc_, &Document::selectionChanged, this, &InspectorPanel::refresh);
  connect(doc_, &Document::modelChanged, this, &InspectorPanel::refresh);
  connect(doc_, &Document::viewNetworkChanged, this, &InspectorPanel::refresh);
  refresh();
}

void InspectorPanel::refresh() {
  updating_ = true;
  const QString node = doc_->selectedNode();
  const QString cluster = doc_->selectedCluster();
  invert_->setVisible(false);
  setAlts_->setVisible(false);
  openSubnet_->setVisible(false);

  if (!node.isEmpty() && doc_->network().find_node(node.toStdString())) {
    nameLabel_->setText(QStringLiteral("Node: %1").arg(node));
    auto& n = doc_->network().node(node.toStdString());
    invert_->setVisible(true);
    invert_->setChecked(n.invert());
    openSubnet_->setVisible(true);
  } else if (!cluster.isEmpty() &&
             doc_->network().find_cluster(cluster.toStdString())) {
    nameLabel_->setText(QStringLiteral("Cluster: %1").arg(cluster));
    setAlts_->setVisible(true);
    auto* alts = doc_->network().alternatives_cluster();
    setAlts_->setEnabled(alts == nullptr ||
                         QString::fromStdString(alts->name()) != cluster);
  } else {
    nameLabel_->setText(QStringLiteral("Nothing selected"));
  }
  updating_ = false;
}

void InspectorPanel::onInvertToggled(bool checked) {
  if (updating_) return;
  const QString node = doc_->selectedNode();
  if (node.isEmpty()) return;
  doc_->undoStack()->push(new SetInvertCmd(doc_, node, checked));
}

void InspectorPanel::onSetAlternatives() {
  const QString cluster = doc_->selectedCluster();
  if (cluster.isEmpty()) return;
  QString old;
  if (auto* a = doc_->network().alternatives_cluster()) {
    old = QString::fromStdString(a->name());
  }
  doc_->undoStack()->push(new SetAlternativesClusterCmd(doc_, cluster, old));
}

void InspectorPanel::onOpenSubnet() {
  const QString node = doc_->selectedNode();
  if (node.isEmpty()) return;
  doc_->undoStack()->push(new EnsureSubnetCmd(doc_, node));
  doc_->pushSubnet(node);
}
