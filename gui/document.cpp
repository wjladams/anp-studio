#include "document.hpp"

#include "anpcpp/json_io.hpp"

#include <QMetaObject>

#include <functional>

Document::Document(QObject* parent) : QObject(parent) {
  newNetwork(true);
  // Commands already call notifyChanged() in redo/undo. Only mark dirty here
  // so each push does not refresh the UI twice.
  connect(&undo_, &QUndoStack::indexChanged, this, [this](int) {
    setDirty(true);
  });
}

Document::~Document() {
  // Disconnect undo hooks before members tear down. indexChanged used to call
  // notifyChanged(); keep the disconnect so future handlers stay safe.
  disconnect(&undo_, nullptr, this, nullptr);
  blockSignals(true);
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

void Document::clearPath() {
  if (path_.isEmpty()) return;
  path_.clear();
  emit pathChanged(path_);
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

QString Document::currentNetworkPath() const {
  return breadcrumb().join(QStringLiteral(" / "));
}

QStringList Document::networkPathOptions() const {
  QStringList out;
  out << QStringLiteral("Root");

  std::function<void(const anpcpp::AnpNetwork&, const QString&)> walk;
  walk = [&](const anpcpp::AnpNetwork& net, const QString& prefix) {
    for (const anpcpp::AnpNode* n : net.nodes()) {
      if (!n->has_subnetwork()) continue;
      const QString path =
          prefix + QStringLiteral(" / ") + QString::fromStdString(n->name());
      out << path;
      walk(*n->subnetwork(), path);
    }
  };
  walk(*root_, QStringLiteral("Root"));
  return out;
}

bool Document::navigateToNetworkPath(const QString& path) {
  const QStringList parts = path.split(QStringLiteral(" / "), Qt::SkipEmptyParts);
  if (parts.isEmpty() || parts.first() != QStringLiteral("Root")) {
    return false;
  }

  // Rebuild stack from root along host names.
  std::vector<Frame> neu;
  neu.push_back(Frame{root_.get(), {}});
  anpcpp::AnpNetwork* cur = root_.get();
  for (int i = 1; i < parts.size(); ++i) {
    anpcpp::AnpNode* node = cur->find_node(parts[i].toStdString());
    if (node == nullptr || !node->has_subnetwork()) {
      return false;
    }
    anpcpp::AnpNetwork* sub = node->subnetwork();
    neu.push_back(Frame{sub, parts[i]});
    cur = sub;
  }

  if (neu.size() == stack_.size()) {
    bool same = true;
    for (std::size_t i = 0; i < neu.size(); ++i) {
      if (neu[i].net != stack_[i].net || neu[i].hostNode != stack_[i].hostNode) {
        same = false;
        break;
      }
    }
    if (same) return true;
  }

  stack_ = std::move(neu);
  selectedCluster_.clear();
  selectedNode_.clear();
  emit selectionChanged(selectedCluster_, selectedNode_);
  emit viewNetworkChanged();
  notifyChanged();
  return true;
}

void Document::notifyChanged() {
  invalidateResults();
  clearSelectionIfInvalid();
  queueModelChanged();
}

void Document::queueModelChanged() {
  if (modelChangedQueued_) return;
  modelChangedQueued_ = true;
  // Coalesce bursty updates (macros, rapid edits) into one UI refresh.
  QMetaObject::invokeMethod(
      this,
      [this]() {
        if (!modelChangedQueued_) return;
        modelChangedQueued_ = false;
        emit modelChanged();
      },
      Qt::QueuedConnection);
}

void Document::flushModelChanged() {
  if (!modelChangedQueued_) return;
  modelChangedQueued_ = false;
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
