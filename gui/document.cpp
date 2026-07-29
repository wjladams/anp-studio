#include "document.hpp"

#include "anpcpp/json_io.hpp"

Document::Document(QObject* parent) : QObject(parent) {
  newNetwork(true);
  connect(&undo_, &QUndoStack::indexChanged, this, [this](int) {
    setDirty(true);
    notifyChanged();
  });
}

anpcpp::AnpNetwork& Document::network() {
  return *stack_.back().net;
}

const anpcpp::AnpNetwork& Document::network() const {
  return *stack_.back().net;
}

anpcpp::AnpNetwork& Document::root() {
  return *root_;
}

const anpcpp::AnpNetwork& Document::root() const {
  return *root_;
}

void Document::setDirty(bool dirty) {
  if (dirty_ == dirty) return;
  dirty_ = dirty;
  emit dirtyChanged(dirty_);
}

void Document::replaceRoot(std::unique_ptr<anpcpp::AnpNetwork> net) {
  const bool blocked = undo_.blockSignals(true);
  undo_.clear();
  undo_.blockSignals(blocked);
  root_ = std::move(net);
  stack_.clear();
  stack_.push_back(Frame{root_.get(), {}});
  selectedCluster_.clear();
  selectedNode_.clear();
  hasResults_ = false;
  resultsStale_ = false;
  setDirty(false);
  emit selectionChanged(selectedCluster_, selectedNode_);
  emit resultsFreshnessChanged();
  emit viewNetworkChanged();
  notifyChanged();
}

void Document::newNetwork(bool create_alts) {
  path_.clear();
  emit pathChanged(path_);
  replaceRoot(std::make_unique<anpcpp::AnpNetwork>(create_alts));
}

bool Document::loadFromFile(const QString& path, QString* error) {
  try {
    auto net = anpcpp::load_network_file(path.toStdString());
    path_ = path;
    emit pathChanged(path_);
    replaceRoot(std::move(net));
    return true;
  } catch (const std::exception& e) {
    if (error) *error = QString::fromUtf8(e.what());
    return false;
  }
}

bool Document::saveToFile(const QString& path, QString* error) {
  try {
    // Persist the root network (includes all nested subnets).
    anpcpp::save_network_file(*root_, path.toStdString());
    path_ = path;
    emit pathChanged(path_);
    setDirty(false);
    return true;
  } catch (const std::exception& e) {
    if (error) *error = QString::fromUtf8(e.what());
    return false;
  }
}

void Document::pushSubnet(const QString& nodeName) {
  anpcpp::AnpNetwork& cur = network();
  anpcpp::AnpNode* node = cur.find_node(nodeName.toStdString());
  if (node == nullptr) return;
  anpcpp::AnpNetwork& sub = node->ensure_subnetwork();
  // Stack frame records the host node for breadcrumbs when editing nested nets.
  stack_.push_back(Frame{&sub, nodeName});
  selectedCluster_.clear();
  selectedNode_.clear();
  emit selectionChanged(selectedCluster_, selectedNode_);
  emit viewNetworkChanged();
  notifyChanged();
}

void Document::popSubnet() {
  if (stack_.size() <= 1) return;
  stack_.pop_back();
  selectedCluster_.clear();
  selectedNode_.clear();
  emit selectionChanged(selectedCluster_, selectedNode_);
  emit viewNetworkChanged();
  notifyChanged();
}

void Document::popToRoot() {
  if (stack_.size() <= 1) return;
  stack_.resize(1);
  selectedCluster_.clear();
  selectedNode_.clear();
  emit selectionChanged(selectedCluster_, selectedNode_);
  emit viewNetworkChanged();
  notifyChanged();
}

void Document::popToDepth(int depth) {
  if (depth < 1) depth = 1;
  const auto target = static_cast<std::size_t>(depth);
  if (target >= stack_.size()) return;
  stack_.resize(target);
  selectedCluster_.clear();
  selectedNode_.clear();
  emit selectionChanged(selectedCluster_, selectedNode_);
  emit viewNetworkChanged();
  notifyChanged();
}

int Document::subnetDepth() const {
  return static_cast<int>(stack_.size());
}

QStringList Document::breadcrumb() const {
  QStringList parts;
  parts << QStringLiteral("Root");
  for (std::size_t i = 1; i < stack_.size(); ++i) {
    parts << stack_[i].hostNode;
  }
  return parts;
}

void Document::notifyChanged() {
  invalidateResults();
  clearSelectionIfInvalid();
  emit modelChanged();
}

void Document::setSelection(const QString& cluster, const QString& node) {
  if (selectedCluster_ == cluster && selectedNode_ == node) return;
  selectedCluster_ = cluster;
  selectedNode_ = node;
  emit selectionChanged(selectedCluster_, selectedNode_);
}

void Document::markResultsCurrent() {
  hasResults_ = true;
  resultsStale_ = false;
  emit resultsFreshnessChanged();
}

void Document::invalidateResults() {
  if (!hasResults_) return;
  if (resultsStale_) return;
  resultsStale_ = true;
  emit resultsFreshnessChanged();
}

void Document::clearSelectionIfInvalid() {
  bool changed = false;
  if (!selectedNode_.isEmpty() &&
      network().find_node(selectedNode_.toStdString()) == nullptr) {
    selectedNode_.clear();
    changed = true;
  }
  if (!selectedCluster_.isEmpty() &&
      network().find_cluster(selectedCluster_.toStdString()) == nullptr) {
    selectedCluster_.clear();
    changed = true;
  }
  if (changed) {
    emit selectionChanged(selectedCluster_, selectedNode_);
  }
}
