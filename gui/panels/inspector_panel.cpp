#include "panels/inspector_panel.hpp"

#include "commands/network_commands.hpp"
#include "document.hpp"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QFrame>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTableWidget>
#include <QTextEdit>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {

constexpr int kRoleSubnetPath = Qt::UserRole + 40;

QTableWidget* makeColumnTable(QWidget* parent) {
  auto* table = new QTableWidget(parent);
  table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table->setSelectionMode(QAbstractItemView::NoSelection);
  table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  table->verticalHeader()->setVisible(true);
  table->setMinimumHeight(120);
  return table;
}

void fillColumnTable(QTableWidget* table,
                     const std::vector<std::string>& rowNames,
                     const anpcpp::Vector& col) {
  table->clear();
  table->setColumnCount(1);
  table->setHorizontalHeaderLabels({QStringLiteral("Value")});
  table->setRowCount(static_cast<int>(rowNames.size()));
  for (int i = 0; i < static_cast<int>(rowNames.size()); ++i) {
    table->setVerticalHeaderItem(
        i, new QTableWidgetItem(QString::fromStdString(rowNames[static_cast<std::size_t>(i)])));
    const double v =
        i < static_cast<int>(col.size()) ? col[static_cast<std::size_t>(i)] : 0.0;
    auto* item = new QTableWidgetItem(QString::number(v, 'g', 6));
    table->setItem(i, 0, item);
  }
}

void addSubnetNodes(QTreeWidgetItem* parent,
                    const anpcpp::AnpNetwork& net,
                    const QStringList& pathPrefix) {
  for (const anpcpp::AnpNode* n : net.nodes()) {
    if (!n->has_subnetwork()) continue;
    QStringList path = pathPrefix;
    path << QString::fromStdString(n->name());
    auto* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem();
    item->setText(0, QString::fromStdString(n->name()));
    item->setData(0, kRoleSubnetPath, path);
    if (parent == nullptr) {
      // Caller adds top-level items.
    }
    addSubnetNodes(item, *n->subnetwork(), path);
  }
}

}  // namespace

InspectorPanel::InspectorPanel(Document* doc, QWidget* parent)
    : QWidget(parent), doc_(doc) {
  auto* outer = new QVBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);

  auto* scroll = new QScrollArea(this);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  auto* content = new QWidget(scroll);
  auto* layout = new QVBoxLayout(content);

  title_ = new QLabel(QStringLiteral("Inspector"), content);
  title_->setStyleSheet(QStringLiteral("font-weight: bold;"));
  layout->addWidget(title_);

  typeLabel_ = new QLabel(content);
  layout->addWidget(typeLabel_);

  layout->addWidget(new QLabel(QStringLiteral("Name"), content));
  nameEdit_ = new QLineEdit(content);
  layout->addWidget(nameEdit_);

  layout->addWidget(new QLabel(QStringLiteral("Description"), content));
  descriptionEdit_ = new QTextEdit(content);
  descriptionEdit_->setAcceptRichText(false);
  descriptionEdit_->setMaximumHeight(80);
  layout->addWidget(descriptionEdit_);

  invert_ = new QCheckBox(QStringLiteral("Invert (subnet synthesis)"), content);
  layout->addWidget(invert_);
  setAlts_ = new QPushButton(QStringLiteral("Set as alternatives cluster"), content);
  layout->addWidget(setAlts_);
  openSubnet_ = new QPushButton(QStringLiteral("Open Subnetwork"), content);
  layout->addWidget(openSubnet_);

  formulaLabel_ = new QLabel(QStringLiteral("Formula"), content);
  formulaLabel_->setStyleSheet(QStringLiteral("font-weight: bold; margin-top: 8px;"));
  layout->addWidget(formulaLabel_);
  synthKind_ = new QComboBox(content);
  synthKind_->addItem(QStringLiteral("Additive"),
                      static_cast<int>(anpcpp::SynthesisKind::Additive));
  synthKind_->addItem(QStringLiteral("Multiplicative"),
                      static_cast<int>(anpcpp::SynthesisKind::Multiplicative));
  synthKind_->addItem(QStringLiteral("Custom"),
                      static_cast<int>(anpcpp::SynthesisKind::Custom));
  layout->addWidget(synthKind_);
  customExpr_ = new QLineEdit(content);
  customExpr_->setPlaceholderText(QStringLiteral("e.g. Benefits / Costs"));
  layout->addWidget(customExpr_);

  subnetLabel_ = new QLabel(QStringLiteral("Subnetworks"), content);
  subnetLabel_->setStyleSheet(QStringLiteral("font-weight: bold; margin-top: 8px;"));
  layout->addWidget(subnetLabel_);
  subnetTree_ = new QTreeWidget(content);
  subnetTree_->setHeaderHidden(true);
  subnetTree_->setRootIsDecorated(true);
  subnetTree_->setMinimumHeight(100);
  layout->addWidget(subnetTree_);

  connectionsLabel_ = new QLabel(QStringLiteral("Connections"), content);
  connectionsLabel_->setStyleSheet(QStringLiteral("font-weight: bold; margin-top: 8px;"));
  layout->addWidget(connectionsLabel_);
  connectionsTree_ = new QTreeWidget(content);
  connectionsTree_->setHeaderHidden(true);
  connectionsTree_->setRootIsDecorated(true);
  connectionsTree_->setMinimumHeight(100);
  layout->addWidget(connectionsTree_);

  matricesLabel_ = new QLabel(QStringLiteral("Matrix columns"), content);
  matricesLabel_->setStyleSheet(QStringLiteral("font-weight: bold; margin-top: 8px;"));
  layout->addWidget(matricesLabel_);
  unscaledLabel_ = new QLabel(QStringLiteral("Unscaled"), content);
  layout->addWidget(unscaledLabel_);
  unscaledCol_ = makeColumnTable(content);
  layout->addWidget(unscaledCol_);
  scaledLabel_ = new QLabel(QStringLiteral("Scaled"), content);
  layout->addWidget(scaledLabel_);
  scaledCol_ = makeColumnTable(content);
  layout->addWidget(scaledCol_);
  limitLabel_ = new QLabel(QStringLiteral("Limit"), content);
  layout->addWidget(limitLabel_);
  limitCol_ = makeColumnTable(content);
  layout->addWidget(limitCol_);

  layout->addStretch();
  scroll->setWidget(content);
  outer->addWidget(scroll);

  connect(invert_, &QCheckBox::toggled, this, &InspectorPanel::onInvertToggled);
  connect(setAlts_, &QPushButton::clicked, this, &InspectorPanel::onSetAlternatives);
  connect(openSubnet_, &QPushButton::clicked, this, &InspectorPanel::onOpenSubnet);
  connect(synthKind_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &InspectorPanel::onSynthesisKindChanged);
  connect(customExpr_, &QLineEdit::editingFinished, this,
          &InspectorPanel::onCustomExprEdited);
  connect(nameEdit_, &QLineEdit::editingFinished, this,
          &InspectorPanel::onNameEdited);
  descriptionEdit_->installEventFilter(this);
  connect(subnetTree_, &QTreeWidget::itemActivated, this,
          &InspectorPanel::onSubnetItemActivated);
  connect(doc_, &Document::selectionChanged, this, &InspectorPanel::refresh);
  connect(doc_, &Document::modelChanged, this, &InspectorPanel::refresh);
  connect(doc_, &Document::viewNetworkChanged, this, &InspectorPanel::refresh);
  refresh();
}

bool InspectorPanel::eventFilter(QObject* watched, QEvent* event) {
  if (watched == descriptionEdit_ && event->type() == QEvent::FocusOut) {
    onDescriptionEdited();
  }
  return QWidget::eventFilter(watched, event);
}

QString InspectorPanel::displayNetworkName() const {
  const QString stored = QString::fromStdString(doc_->network().name());
  if (!stored.isEmpty()) return stored;
  if (&doc_->network() == &doc_->root()) return QStringLiteral("Root");
  const QStringList crumbs = doc_->breadcrumb();
  return crumbs.isEmpty() ? QStringLiteral("Network") : crumbs.last();
}

void InspectorPanel::refreshSynthesisControls() {
  const auto& opt = doc_->network().synthesis_options();
  const int idx = synthKind_->findData(static_cast<int>(opt.kind));
  if (idx >= 0) synthKind_->setCurrentIndex(idx);
  customExpr_->setText(QString::fromStdString(opt.custom_expr));
  customExpr_->setEnabled(opt.kind == anpcpp::SynthesisKind::Custom);
}

void InspectorPanel::setModeWidgets(Mode mode) {
  const bool net = mode == Mode::Network;
  const bool cluster = mode == Mode::Cluster;
  const bool node = mode == Mode::Node;

  formulaLabel_->setVisible(net);
  synthKind_->setVisible(net);
  customExpr_->setVisible(net);
  subnetLabel_->setVisible(net);
  subnetTree_->setVisible(net);

  setAlts_->setVisible(cluster);

  invert_->setVisible(node);
  openSubnet_->setVisible(node);
  connectionsLabel_->setVisible(node);
  connectionsTree_->setVisible(node);
  matricesLabel_->setVisible(node);
  unscaledLabel_->setVisible(node);
  scaledLabel_->setVisible(node);
  limitLabel_->setVisible(node);
  unscaledCol_->setVisible(node);
  scaledCol_->setVisible(node);
  limitCol_->setVisible(node);
}

void InspectorPanel::fillSubnetTree() {
  subnetTree_->clear();
  const anpcpp::AnpNetwork& net = doc_->network();
  for (const anpcpp::AnpNode* n : net.nodes()) {
    if (!n->has_subnetwork()) continue;
    QStringList path;
    path << QString::fromStdString(n->name());
    auto* item = new QTreeWidgetItem(subnetTree_);
    item->setText(0, QString::fromStdString(n->name()));
    item->setData(0, kRoleSubnetPath, path);
    addSubnetNodes(item, *n->subnetwork(), path);
  }
  subnetTree_->expandAll();
}

void InspectorPanel::fillConnections() {
  connectionsTree_->clear();
  const QString node = doc_->selectedNode();
  if (node.isEmpty()) return;
  auto* n = doc_->network().find_node(node.toStdString());
  if (!n) return;

  for (const anpcpp::AnpCluster* c : doc_->network().clusters()) {
    const auto* slot = n->node_prioritizer(c->name());
    if (!slot || slot->empty()) continue;
    auto* clusterItem = new QTreeWidgetItem(connectionsTree_);
    clusterItem->setText(0, QString::fromStdString(c->name()));
    for (const std::string& alt : slot->alternatives()) {
      auto* altItem = new QTreeWidgetItem(clusterItem);
      altItem->setText(0, QString::fromStdString(alt));
    }
  }
  connectionsTree_->expandAll();
}

void InspectorPanel::fillMatrixColumns(const QString& nodeName) {
  auto clear = [](QTableWidget* t) {
    t->clear();
    t->setRowCount(0);
    t->setColumnCount(0);
  };
  clear(unscaledCol_);
  clear(scaledCol_);
  clear(limitCol_);
  try {
    auto& net = doc_->network();
    auto* n = net.find_node(nodeName.toStdString());
    if (!n) return;
    const auto names = net.node_names();
    fillColumnTable(unscaledCol_, names, n->unscaled_column());

    std::size_t col = 0;
    bool found = false;
    for (std::size_t i = 0; i < names.size(); ++i) {
      if (names[i] == nodeName.toStdString()) {
        col = i;
        found = true;
        break;
      }
    }
    if (!found) return;

    const anpcpp::Matrix scaled = net.scaled_supermatrix();
    anpcpp::Vector scaledCol(names.size(), 0.0);
    for (std::size_t r = 0; r < names.size() && r < scaled.rows(); ++r) {
      scaledCol[r] = scaled(r, col);
    }
    fillColumnTable(scaledCol_, names, scaledCol);

    const anpcpp::Matrix limit = net.limit_matrix();
    anpcpp::Vector limitCol(names.size(), 0.0);
    for (std::size_t r = 0; r < names.size() && r < limit.rows(); ++r) {
      limitCol[r] = limit(r, col);
    }
    fillColumnTable(limitCol_, names, limitCol);
  } catch (...) {
  }
}

void InspectorPanel::refresh() {
  updating_ = true;
  const QString node = doc_->selectedNode();
  const QString cluster = doc_->selectedCluster();

  if (!node.isEmpty() && doc_->network().find_node(node.toStdString())) {
    setModeWidgets(Mode::Node);
    typeLabel_->setText(QStringLiteral("Node"));
    nameEdit_->setText(node);
    descriptionEdit_->setPlainText(
        QString::fromStdString(doc_->network().node(node.toStdString()).description()));
    invert_->setChecked(doc_->network().node(node.toStdString()).invert());
    fillConnections();
    fillMatrixColumns(node);
  } else if (!cluster.isEmpty() &&
             doc_->network().find_cluster(cluster.toStdString())) {
    setModeWidgets(Mode::Cluster);
    typeLabel_->setText(QStringLiteral("Cluster"));
    nameEdit_->setText(cluster);
    descriptionEdit_->setPlainText(QString::fromStdString(
        doc_->network().cluster(cluster.toStdString()).description()));
    auto* alts = doc_->network().alternatives_cluster();
    setAlts_->setEnabled(alts == nullptr ||
                         QString::fromStdString(alts->name()) != cluster);
  } else {
    setModeWidgets(Mode::Network);
    typeLabel_->setText(QStringLiteral("Network"));
    nameEdit_->setText(displayNetworkName());
    descriptionEdit_->setPlainText(
        QString::fromStdString(doc_->network().description()));
    refreshSynthesisControls();
    fillSubnetTree();
  }
  updating_ = false;
}

void InspectorPanel::onNameEdited() {
  if (updating_) return;
  const QString neu = nameEdit_->text().trimmed();
  if (neu.isEmpty()) {
    refresh();
    return;
  }
  const QString node = doc_->selectedNode();
  const QString cluster = doc_->selectedCluster();
  if (!node.isEmpty()) {
    if (neu == node) return;
    if (doc_->network().find_node(neu.toStdString())) {
      refresh();
      return;
    }
    doc_->undoStack()->push(new RenameNodeCmd(doc_, node, neu));
  } else if (!cluster.isEmpty()) {
    if (neu == cluster) return;
    if (doc_->network().find_cluster(neu.toStdString())) {
      refresh();
      return;
    }
    doc_->undoStack()->push(new RenameClusterCmd(doc_, cluster, neu));
  } else {
    const QString stored = QString::fromStdString(doc_->network().name());
    QString toStore = neu;
    const bool atRoot = &doc_->network() == &doc_->root();
    if (atRoot && neu == QStringLiteral("Root") && stored.isEmpty()) {
      return;
    }
    if (toStore == stored) return;
    if (atRoot && toStore == QStringLiteral("Root")) {
      toStore.clear();
    }
    doc_->undoStack()->push(new SetNetworkNameCmd(doc_, toStore));
  }
}

void InspectorPanel::onDescriptionEdited() {
  if (updating_) return;
  const QString neu = descriptionEdit_->toPlainText();
  const QString node = doc_->selectedNode();
  const QString cluster = doc_->selectedCluster();
  if (!node.isEmpty()) {
    const QString old =
        QString::fromStdString(doc_->network().node(node.toStdString()).description());
    if (neu == old) return;
    doc_->undoStack()->push(new SetNodeDescriptionCmd(doc_, node, neu));
  } else if (!cluster.isEmpty()) {
    const QString old = QString::fromStdString(
        doc_->network().cluster(cluster.toStdString()).description());
    if (neu == old) return;
    doc_->undoStack()->push(new SetClusterDescriptionCmd(doc_, cluster, neu));
  } else {
    const QString old = QString::fromStdString(doc_->network().description());
    if (neu == old) return;
    doc_->undoStack()->push(new SetNetworkDescriptionCmd(doc_, neu));
  }
}

void InspectorPanel::onSubnetItemActivated(QTreeWidgetItem* item, int /*column*/) {
  if (!item) return;
  const QStringList rel = item->data(0, kRoleSubnetPath).toStringList();
  if (rel.isEmpty()) return;
  QString path = doc_->currentNetworkPath();
  for (const QString& host : rel) {
    path += QStringLiteral(" / ") + host;
  }
  doc_->navigateToNetworkPath(path);
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

void InspectorPanel::onSynthesisKindChanged(int) {
  if (updating_) return;
  anpcpp::SynthesisOptions old = doc_->network().synthesis_options();
  anpcpp::SynthesisOptions neu = old;
  neu.kind = static_cast<anpcpp::SynthesisKind>(synthKind_->currentData().toInt());
  if (neu.kind == old.kind) return;
  doc_->undoStack()->push(new SetSynthesisOptionsCmd(doc_, neu, old));
}

void InspectorPanel::onCustomExprEdited() {
  if (updating_) return;
  anpcpp::SynthesisOptions old = doc_->network().synthesis_options();
  anpcpp::SynthesisOptions neu = old;
  neu.custom_expr = customExpr_->text().toStdString();
  if (neu.custom_expr == old.custom_expr) return;
  doc_->undoStack()->push(new SetSynthesisOptionsCmd(doc_, neu, old));
}
