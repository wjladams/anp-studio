#include "commands/network_commands.hpp"

// Thin QUndoCommand wrappers: each command mutates doc_->network() and calls
// notifyChanged() so panels/canvas refresh from modelChanged.

AddClusterCmd::AddClusterCmd(Document* doc, QString name)
    : QUndoCommand(QStringLiteral("Add cluster %1").arg(name)),
      doc_(doc),
      name_(std::move(name)) {}

void AddClusterCmd::redo() {
  doc_->network().add_cluster(name_.toStdString());
  doc_->notifyChanged();
}

void AddClusterCmd::undo() {
  doc_->network().remove_cluster(name_.toStdString());
  doc_->notifyChanged();
}

AddNodeCmd::AddNodeCmd(Document* doc, QString cluster, QString name)
    : QUndoCommand(QStringLiteral("Add node %1").arg(name)),
      doc_(doc),
      cluster_(std::move(cluster)),
      name_(std::move(name)) {}

void AddNodeCmd::redo() {
  doc_->network().add_node(cluster_.toStdString(), name_.toStdString());
  doc_->notifyChanged();
}

void AddNodeCmd::undo() {
  doc_->network().remove_node(name_.toStdString());
  doc_->notifyChanged();
}

ConnectNodesCmd::ConnectNodesCmd(Document* doc, QString src, QString dest)
    : QUndoCommand(QStringLiteral("Connect %1 -> %2").arg(src, dest)),
      doc_(doc),
      src_(std::move(src)),
      dest_(std::move(dest)) {}

void ConnectNodesCmd::redo() {
  doc_->network().node_connect(src_.toStdString(), dest_.toStdString());
  doc_->notifyChanged();
}

void ConnectNodesCmd::undo() {
  doc_->network().node_disconnect(src_.toStdString(), dest_.toStdString());
  doc_->notifyChanged();
}

DisconnectNodesCmd::DisconnectNodesCmd(Document* doc, QString src, QString dest)
    : QUndoCommand(QStringLiteral("Disconnect %1 -> %2").arg(src, dest)),
      doc_(doc),
      src_(std::move(src)),
      dest_(std::move(dest)) {}

void DisconnectNodesCmd::redo() {
  doc_->network().node_disconnect(src_.toStdString(), dest_.toStdString());
  doc_->notifyChanged();
}

void DisconnectNodesCmd::undo() {
  doc_->network().node_connect(src_.toStdString(), dest_.toStdString());
  doc_->notifyChanged();
}

SetNodeComparisonCmd::SetNodeComparisonCmd(Document* doc,
                                           QString wrt,
                                           QString a,
                                           QString b,
                                           double value,
                                           double oldValue)
    : QUndoCommand(QStringLiteral("Set node comparison")),
      doc_(doc),
      wrt_(std::move(wrt)),
      a_(std::move(a)),
      b_(std::move(b)),
      value_(value),
      oldValue_(oldValue) {}

void SetNodeComparisonCmd::redo() {
  doc_->network().set_node_comparison(wrt_.toStdString(), a_.toStdString(),
                                      b_.toStdString(), value_);
  doc_->notifyChanged();
}

void SetNodeComparisonCmd::undo() {
  doc_->network().set_node_comparison(wrt_.toStdString(), a_.toStdString(),
                                      b_.toStdString(), oldValue_);
  doc_->notifyChanged();
}

SetClusterComparisonCmd::SetClusterComparisonCmd(Document* doc,
                                                 QString wrt,
                                                 QString a,
                                                 QString b,
                                                 double value,
                                                 double oldValue)
    : QUndoCommand(QStringLiteral("Set cluster comparison")),
      doc_(doc),
      wrt_(std::move(wrt)),
      a_(std::move(a)),
      b_(std::move(b)),
      value_(value),
      oldValue_(oldValue) {}

void SetClusterComparisonCmd::redo() {
  doc_->network().set_cluster_comparison(wrt_.toStdString(), a_.toStdString(),
                                         b_.toStdString(), value_);
  doc_->notifyChanged();
}

void SetClusterComparisonCmd::undo() {
  doc_->network().set_cluster_comparison(wrt_.toStdString(), a_.toStdString(),
                                         b_.toStdString(), oldValue_);
  doc_->notifyChanged();
}

SetSynthesisOptionsCmd::SetSynthesisOptionsCmd(Document* doc,
                                               anpcpp::SynthesisOptions neu,
                                               anpcpp::SynthesisOptions old)
    : QUndoCommand(QStringLiteral("Set synthesis options")),
      doc_(doc),
      neu_(std::move(neu)),
      old_(std::move(old)) {}

void SetSynthesisOptionsCmd::redo() {
  doc_->network().set_synthesis_options(neu_);
  doc_->notifyChanged();
}

void SetSynthesisOptionsCmd::undo() {
  doc_->network().set_synthesis_options(old_);
  doc_->notifyChanged();
}

SetInvertCmd::SetInvertCmd(Document* doc, QString node, bool value)
    : QUndoCommand(QStringLiteral("Set invert")),
      doc_(doc),
      node_(std::move(node)),
      value_(value) {
  oldValue_ = doc_->network().node(node_.toStdString()).invert();
}

void SetInvertCmd::redo() {
  doc_->network().node(node_.toStdString()).set_invert(value_);
  doc_->notifyChanged();
}

void SetInvertCmd::undo() {
  doc_->network().node(node_.toStdString()).set_invert(oldValue_);
  doc_->notifyChanged();
}

SetAlternativesClusterCmd::SetAlternativesClusterCmd(Document* doc,
                                                     QString name,
                                                     QString oldName)
    : QUndoCommand(QStringLiteral("Set alternatives cluster")),
      doc_(doc),
      name_(std::move(name)),
      oldName_(std::move(oldName)) {}

void SetAlternativesClusterCmd::redo() {
  doc_->network().set_alternatives_cluster(name_.toStdString());
  doc_->notifyChanged();
}

void SetAlternativesClusterCmd::undo() {
  if (!oldName_.isEmpty()) {
    doc_->network().set_alternatives_cluster(oldName_.toStdString());
  }
  doc_->notifyChanged();
}

EnsureSubnetCmd::EnsureSubnetCmd(Document* doc, QString node)
    : QUndoCommand(QStringLiteral("Attach subnetwork")),
      doc_(doc),
      node_(std::move(node)) {}

void EnsureSubnetCmd::redo() {
  auto& n = doc_->network().node(node_.toStdString());
  created_ = !n.has_subnetwork();
  n.ensure_subnetwork();
  doc_->notifyChanged();
}

void EnsureSubnetCmd::undo() {
  // Only remove the subnetwork if this command created it (not if it pre-existed).
  if (created_) {
    doc_->network().clear_subnetwork(node_.toStdString());
    doc_->notifyChanged();
  }
}

ReorderNodeCmd::ReorderNodeCmd(Document* doc,
                               QString node,
                               int fromIndex,
                               int toIndex)
    : QUndoCommand(QStringLiteral("Reorder node %1").arg(node)),
      doc_(doc),
      node_(std::move(node)),
      fromIndex_(fromIndex),
      toIndex_(toIndex) {}

void ReorderNodeCmd::redo() {
  doc_->network().move_node(node_.toStdString(),
                            static_cast<std::size_t>(toIndex_));
  doc_->notifyChanged();
}

void ReorderNodeCmd::undo() {
  doc_->network().move_node(node_.toStdString(),
                            static_cast<std::size_t>(fromIndex_));
  doc_->notifyChanged();
}

RemoveNodeCmd::RemoveNodeCmd(Document* doc, QString name)
    : QUndoCommand(QStringLiteral("Remove node %1").arg(name)),
      doc_(doc),
      name_(std::move(name)) {
  if (auto* n = doc_->network().find_node(name_.toStdString())) {
    hadNode_ = true;
    cluster_ = QString::fromStdString(n->cluster()->name());
  }
}

void RemoveNodeCmd::redo() {
  if (!hadNode_) return;
  doc_->network().remove_node(name_.toStdString());
  doc_->notifyChanged();
}

void RemoveNodeCmd::undo() {
  if (!hadNode_) return;
  doc_->network().add_node(cluster_.toStdString(), name_.toStdString());
  doc_->notifyChanged();
}

RemoveClusterCmd::RemoveClusterCmd(Document* doc, QString name)
    : QUndoCommand(QStringLiteral("Remove cluster %1").arg(name)),
      doc_(doc),
      name_(std::move(name)) {
  hadCluster_ = doc_->network().find_cluster(name_.toStdString()) != nullptr;
}

void RemoveClusterCmd::redo() {
  if (!hadCluster_) return;
  doc_->network().remove_cluster(name_.toStdString());
  doc_->notifyChanged();
}

void RemoveClusterCmd::undo() {
  if (!hadCluster_) return;
  doc_->network().add_cluster(name_.toStdString());
  doc_->notifyChanged();
}

RenameNodeCmd::RenameNodeCmd(Document* doc, QString oldName, QString newName)
    : QUndoCommand(QStringLiteral("Rename node %1 → %2").arg(oldName, newName)),
      doc_(doc),
      oldName_(std::move(oldName)),
      newName_(std::move(newName)) {}

void RenameNodeCmd::redo() {
  doc_->network().rename_node(oldName_.toStdString(), newName_.toStdString());
  if (doc_->selectedNode() == oldName_) {
    doc_->setSelection(doc_->selectedCluster(), newName_);
  }
  doc_->notifyChanged();
}

void RenameNodeCmd::undo() {
  doc_->network().rename_node(newName_.toStdString(), oldName_.toStdString());
  if (doc_->selectedNode() == newName_) {
    doc_->setSelection(doc_->selectedCluster(), oldName_);
  }
  doc_->notifyChanged();
}

RenameClusterCmd::RenameClusterCmd(Document* doc, QString oldName, QString newName)
    : QUndoCommand(
          QStringLiteral("Rename cluster %1 → %2").arg(oldName, newName)),
      doc_(doc),
      oldName_(std::move(oldName)),
      newName_(std::move(newName)) {}

void RenameClusterCmd::redo() {
  doc_->network().rename_cluster(oldName_.toStdString(), newName_.toStdString());
  if (doc_->selectedCluster() == oldName_) {
    doc_->setSelection(newName_, doc_->selectedNode());
  }
  doc_->notifyChanged();
}

void RenameClusterCmd::undo() {
  doc_->network().rename_cluster(newName_.toStdString(), oldName_.toStdString());
  if (doc_->selectedCluster() == newName_) {
    doc_->setSelection(oldName_, doc_->selectedNode());
  }
  doc_->notifyChanged();
}

SetNetworkNameCmd::SetNetworkNameCmd(Document* doc, QString name)
    : QUndoCommand(QStringLiteral("Set network name")),
      doc_(doc),
      name_(std::move(name)) {
  oldName_ = QString::fromStdString(doc_->network().name());
}

void SetNetworkNameCmd::redo() {
  doc_->network().set_name(name_.toStdString());
  doc_->notifyChanged();
}

void SetNetworkNameCmd::undo() {
  doc_->network().set_name(oldName_.toStdString());
  doc_->notifyChanged();
}

SetNetworkDescriptionCmd::SetNetworkDescriptionCmd(Document* doc,
                                                   QString description)
    : QUndoCommand(QStringLiteral("Set network description")),
      doc_(doc),
      description_(std::move(description)) {
  oldDescription_ = QString::fromStdString(doc_->network().description());
}

void SetNetworkDescriptionCmd::redo() {
  doc_->network().set_description(description_.toStdString());
  doc_->notifyChanged();
}

void SetNetworkDescriptionCmd::undo() {
  doc_->network().set_description(oldDescription_.toStdString());
  doc_->notifyChanged();
}

SetNodeDescriptionCmd::SetNodeDescriptionCmd(Document* doc,
                                             QString node,
                                             QString description)
    : QUndoCommand(QStringLiteral("Set node description")),
      doc_(doc),
      node_(std::move(node)),
      description_(std::move(description)) {
  if (auto* n = doc_->network().find_node(node_.toStdString())) {
    oldDescription_ = QString::fromStdString(n->description());
  }
}

void SetNodeDescriptionCmd::redo() {
  if (auto* n = doc_->network().find_node(node_.toStdString())) {
    n->set_description(description_.toStdString());
    doc_->notifyChanged();
  }
}

void SetNodeDescriptionCmd::undo() {
  if (auto* n = doc_->network().find_node(node_.toStdString())) {
    n->set_description(oldDescription_.toStdString());
    doc_->notifyChanged();
  }
}

SetClusterDescriptionCmd::SetClusterDescriptionCmd(Document* doc,
                                                   QString cluster,
                                                   QString description)
    : QUndoCommand(QStringLiteral("Set cluster description")),
      doc_(doc),
      cluster_(std::move(cluster)),
      description_(std::move(description)) {
  if (auto* c = doc_->network().find_cluster(cluster_.toStdString())) {
    oldDescription_ = QString::fromStdString(c->description());
  }
}

void SetClusterDescriptionCmd::redo() {
  if (auto* c = doc_->network().find_cluster(cluster_.toStdString())) {
    c->set_description(description_.toStdString());
    doc_->notifyChanged();
  }
}

void SetClusterDescriptionCmd::undo() {
  if (auto* c = doc_->network().find_cluster(cluster_.toStdString())) {
    c->set_description(oldDescription_.toStdString());
    doc_->notifyChanged();
  }
}

SetPrioritizerKindCmd::SetPrioritizerKindCmd(Document* doc,
                                             QString wrt,
                                             QString destCluster,
                                             anpcpp::NodePrioritizerKind kind)
    : QUndoCommand(QStringLiteral("Set prioritizer kind")),
      doc_(doc),
      wrt_(std::move(wrt)),
      destCluster_(std::move(destCluster)),
      kind_(kind) {
  oldKind_ = doc_->network()
                 .node(wrt_.toStdString())
                 .node_prioritizer_kind(destCluster_.toStdString());
}

void SetPrioritizerKindCmd::redo() {
  doc_->network().set_node_prioritizer_kind(
      wrt_.toStdString(), destCluster_.toStdString(), kind_);
  doc_->notifyChanged();
}

void SetPrioritizerKindCmd::undo() {
  doc_->network().set_node_prioritizer_kind(
      wrt_.toStdString(), destCluster_.toStdString(), oldKind_);
  doc_->notifyChanged();
}

SetRatingsModeCmd::SetRatingsModeCmd(Document* doc,
                                     QString wrt,
                                     QString destCluster,
                                     anpcpp::RatingsPrioritizer::Mode mode)
    : QUndoCommand(QStringLiteral("Set ratings mode")),
      doc_(doc),
      wrt_(std::move(wrt)),
      destCluster_(std::move(destCluster)),
      mode_(mode) {
  oldMode_ = doc_->network()
                 .node(wrt_.toStdString())
                 .node_ratings(destCluster_.toStdString())
                 ->mode();
}

void SetRatingsModeCmd::redo() {
  doc_->network()
      .node(wrt_.toStdString())
      .node_ratings(destCluster_.toStdString())
      ->set_mode(mode_);
  doc_->notifyChanged();
}

void SetRatingsModeCmd::undo() {
  doc_->network()
      .node(wrt_.toStdString())
      .node_ratings(destCluster_.toStdString())
      ->set_mode(oldMode_);
  doc_->notifyChanged();
}

SetRatingsCategoriesCmd::SetRatingsCategoriesCmd(
    Document* doc,
    QString wrt,
    QString destCluster,
    std::vector<anpcpp::RatingCategory> cats)
    : QUndoCommand(QStringLiteral("Set rating categories")),
      doc_(doc),
      wrt_(std::move(wrt)),
      destCluster_(std::move(destCluster)),
      cats_(std::move(cats)) {
  oldCats_ = doc_->network()
                 .node(wrt_.toStdString())
                 .node_ratings(destCluster_.toStdString())
                 ->categories();
}

void SetRatingsCategoriesCmd::redo() {
  doc_->network()
      .node(wrt_.toStdString())
      .node_ratings(destCluster_.toStdString())
      ->set_categories(cats_);
  doc_->notifyChanged();
}

void SetRatingsCategoriesCmd::undo() {
  doc_->network()
      .node(wrt_.toStdString())
      .node_ratings(destCluster_.toStdString())
      ->set_categories(oldCats_);
  doc_->notifyChanged();
}

SetRatingVoteCmd::SetRatingVoteCmd(Document* doc,
                                   QString wrt,
                                   QString destCluster,
                                   QString alt,
                                   QString categoryId)
    : QUndoCommand(QStringLiteral("Set rating vote")),
      doc_(doc),
      wrt_(std::move(wrt)),
      destCluster_(std::move(destCluster)),
      alt_(std::move(alt)),
      categoryId_(std::move(categoryId)) {
  const auto old = doc_->network()
                       .node(wrt_.toStdString())
                       .node_ratings(destCluster_.toStdString())
                       ->rating(alt_.toStdString());
  hadOld_ = old.has_value();
  if (hadOld_) oldCategoryId_ = QString::fromStdString(*old);
}

void SetRatingVoteCmd::redo() {
  auto* rt = doc_->network()
                 .node(wrt_.toStdString())
                 .node_ratings(destCluster_.toStdString());
  if (categoryId_.isEmpty()) rt->clear_rating(alt_.toStdString());
  else rt->set_rating(alt_.toStdString(), categoryId_.toStdString());
  doc_->notifyChanged();
}

void SetRatingVoteCmd::undo() {
  auto* rt = doc_->network()
                 .node(wrt_.toStdString())
                 .node_ratings(destCluster_.toStdString());
  if (!hadOld_) rt->clear_rating(alt_.toStdString());
  else rt->set_rating(alt_.toStdString(), oldCategoryId_.toStdString());
  doc_->notifyChanged();
}

SetRatingValueCmd::SetRatingValueCmd(Document* doc,
                                     QString wrt,
                                     QString destCluster,
                                     QString alt,
                                     bool clear,
                                     double value)
    : QUndoCommand(QStringLiteral("Set rating value")),
      doc_(doc),
      wrt_(std::move(wrt)),
      destCluster_(std::move(destCluster)),
      alt_(std::move(alt)),
      clear_(clear),
      value_(value) {
  const auto old = doc_->network()
                       .node(wrt_.toStdString())
                       .node_ratings(destCluster_.toStdString())
                       ->value(alt_.toStdString());
  hadOld_ = old.has_value();
  if (hadOld_) oldValue_ = *old;
}

void SetRatingValueCmd::redo() {
  auto* rt = doc_->network()
                 .node(wrt_.toStdString())
                 .node_ratings(destCluster_.toStdString());
  if (clear_) rt->clear_value(alt_.toStdString());
  else rt->set_value(alt_.toStdString(), value_);
  doc_->notifyChanged();
}

void SetRatingValueCmd::undo() {
  auto* rt = doc_->network()
                 .node(wrt_.toStdString())
                 .node_ratings(destCluster_.toStdString());
  if (!hadOld_) rt->clear_value(alt_.toStdString());
  else rt->set_value(alt_.toStdString(), oldValue_);
  doc_->notifyChanged();
}

SetRatingsInterpreterCmd::SetRatingsInterpreterCmd(
    Document* doc,
    QString wrt,
    QString destCluster,
    anpcpp::ScoreInterpreter interpreter)
    : QUndoCommand(QStringLiteral("Set ratings interpreter")),
      doc_(doc),
      wrt_(std::move(wrt)),
      destCluster_(std::move(destCluster)),
      interpreter_(std::move(interpreter)) {
  oldInterpreter_ = doc_->network()
                        .node(wrt_.toStdString())
                        .node_ratings(destCluster_.toStdString())
                        ->interpreter();
}

void SetRatingsInterpreterCmd::redo() {
  doc_->network()
      .node(wrt_.toStdString())
      .node_ratings(destCluster_.toStdString())
      ->set_interpreter(interpreter_);
  doc_->notifyChanged();
}

void SetRatingsInterpreterCmd::undo() {
  doc_->network()
      .node(wrt_.toStdString())
      .node_ratings(destCluster_.toStdString())
      ->set_interpreter(oldInterpreter_);
  doc_->notifyChanged();
}

SetClusterPositionsCmd::SetClusterPositionsCmd(
    Document* doc,
    QHash<QString, QPointF> oldPositions,
    QHash<QString, QPointF> newPositions)
    : QUndoCommand(QStringLiteral("Organize clusters")),
      doc_(doc),
      oldPositions_(std::move(oldPositions)),
      newPositions_(std::move(newPositions)) {}

void SetClusterPositionsCmd::apply(const QHash<QString, QPointF>& positions) {
  auto& net = doc_->network();
  for (auto it = positions.begin(); it != positions.end(); ++it) {
    try {
      net.set_cluster_position(it.key().toStdString(), it.value().x(),
                               it.value().y());
    } catch (...) {
    }
  }
  // Skip canvas persist-of-old-items during the rebuild this triggers.
  // Flush synchronously so rebuild runs while suppress is still true;
  // coalesced notify would clear the flag before the queued rebuild.
  doc_->setSuppressLayoutPersist(true);
  doc_->notifyChanged();
  doc_->flushModelChanged();
  doc_->setSuppressLayoutPersist(false);
}

void SetClusterPositionsCmd::redo() { apply(newPositions_); }

void SetClusterPositionsCmd::undo() { apply(oldPositions_); }
