#include "commands/network_commands.hpp"

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
                                               cppanp::SynthesisOptions neu,
                                               cppanp::SynthesisOptions old)
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
  if (created_) {
    doc_->network().clear_subnetwork(node_.toStdString());
    doc_->notifyChanged();
  }
}
