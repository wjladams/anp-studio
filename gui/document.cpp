#include "document.hpp"

#include "anpcpp/json_io.hpp"

#include <QFile>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMetaObject>
#include <QtGlobal>

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <utility>

namespace {

constexpr int kResearcherFormatVersion = 2;

QVector<ResearcherCell> cellsFromJsonArray(const QJsonArray& arr) {
  QVector<ResearcherCell> cells;
  cells.reserve(arr.size());
  for (const QJsonValue& v : arr) {
    if (!v.isObject()) continue;
    const QJsonObject o = v.toObject();
    ResearcherCell cell;
    cell.command = o.value(QStringLiteral("command")).toString();
    cell.html = o.value(QStringLiteral("html")).toString();
    cell.ok = o.value(QStringLiteral("ok")).toBool(true);
    if (cell.command.isEmpty() && cell.html.isEmpty()) continue;
    cells.push_back(std::move(cell));
  }
  return cells;
}

QJsonArray cellsToJsonArray(const QVector<ResearcherCell>& cells) {
  QJsonArray arr;
  for (const ResearcherCell& cell : cells) {
    QJsonObject o;
    o.insert(QStringLiteral("command"), cell.command);
    o.insert(QStringLiteral("html"), cell.html);
    o.insert(QStringLiteral("ok"), cell.ok);
    arr.push_back(o);
  }
  return arr;
}

}  // namespace

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

anpcpp::AnpNetwork* Document::parentNetwork() {
  if (stack_.size() < 2) return nullptr;
  return stack_[stack_.size() - 2].net;
}

const anpcpp::AnpNetwork* Document::parentNetwork() const {
  if (stack_.size() < 2) return nullptr;
  return stack_[stack_.size() - 2].net;
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

void Document::setResearcherSession(QVector<ResearcherNotebook> notebooks,
                                    int activeIndex) {
  researcherNotebooks_ = std::move(notebooks);
  if (researcherNotebooks_.isEmpty()) {
    researcherActiveIndex_ = 0;
  } else {
    researcherActiveIndex_ =
        qBound(0, activeIndex, researcherNotebooks_.size() - 1);
  }
}

void Document::clearResearcherSession() {
  researcherNotebooks_.clear();
  researcherActiveIndex_ = 0;
}

void Document::addLinkedGoogleForm(const LinkedGoogleForm& form) {
  if (form.formId.isEmpty()) return;
  for (int i = 0; i < linkedGoogleForms_.size(); ++i) {
    if (linkedGoogleForms_[i].formId == form.formId) {
      linkedGoogleForms_[i] = form;
      setDirty(true);
      emit linkedFormsChanged();
      return;
    }
  }
  linkedGoogleForms_.push_back(form);
  setDirty(true);
  emit linkedFormsChanged();
}

void Document::removeLinkedGoogleForm(const QString& formId) {
  const auto before = linkedGoogleForms_.size();
  linkedGoogleForms_.erase(
      std::remove_if(linkedGoogleForms_.begin(), linkedGoogleForms_.end(),
                     [&](const LinkedGoogleForm& f) {
                       return f.formId == formId;
                     }),
      linkedGoogleForms_.end());
  if (linkedGoogleForms_.size() != before) {
    setDirty(true);
    emit linkedFormsChanged();
  }
}

const LinkedGoogleForm* Document::latestLinkedGoogleForm() const {
  if (linkedGoogleForms_.isEmpty()) return nullptr;
  return &linkedGoogleForms_.back();
}

QVector<LinkedGoogleForm> Document::parseLinkedGoogleForms(
    const QByteArray& fileBytes) {
  QVector<LinkedGoogleForm> out;
  QJsonParseError err;
  const QJsonDocument doc = QJsonDocument::fromJson(fileBytes, &err);
  if (err.error != QJsonParseError::NoError || !doc.isObject()) return out;
  const QJsonObject root = doc.object();
  if (!root.contains(QStringLiteral("google_forms"))) return out;
  const QJsonValue gf = root.value(QStringLiteral("google_forms"));
  QJsonArray arr;
  if (gf.isArray()) {
    arr = gf.toArray();
  } else if (gf.isObject()) {
    arr = gf.toObject().value(QStringLiteral("forms")).toArray();
  }
  for (const QJsonValue& v : arr) {
    if (!v.isObject()) continue;
    const QJsonObject o = v.toObject();
    LinkedGoogleForm f;
    f.formId = o.value(QStringLiteral("formId")).toString();
    f.title = o.value(QStringLiteral("title")).toString();
    f.responderUrl = o.value(QStringLiteral("responderUrl")).toString();
    f.editUrl = o.value(QStringLiteral("editUrl")).toString();
    f.createdAtIso = o.value(QStringLiteral("createdAt")).toString();
    f.structureFingerprint =
        o.value(QStringLiteral("structureFingerprint")).toString();
    const QJsonArray tags = o.value(QStringLiteral("questionTags")).toArray();
    for (const QJsonValue& tv : tags) {
      const QString t = tv.toString();
      if (!t.isEmpty()) f.questionTags.push_back(t);
    }
    const QJsonArray ids = o.value(QStringLiteral("questionIds")).toArray();
    for (const QJsonValue& iv : ids) {
      const QString id = iv.toString();
      if (!id.isEmpty()) f.questionIds.push_back(id);
    }
    const QJsonArray mapped = o.value(QStringLiteral("mappedTags")).toArray();
    for (const QJsonValue& mv : mapped) {
      const QString t = mv.toString();
      if (!t.isEmpty()) f.mappedTags.push_back(t);
    }
    if (!f.formId.isEmpty()) out.push_back(f);
  }
  return out;
}

bool Document::researcherSessionIsTrivial() const {
  if (researcherNotebooks_.isEmpty()) return true;
  if (researcherNotebooks_.size() != 1) return false;
  const ResearcherNotebook& nb = researcherNotebooks_.front();
  if (!nb.cells.isEmpty()) return false;
  return nb.name.isEmpty() || nb.name == QStringLiteral("Notebook 1");
}

Document::ResearcherSessionData Document::parseResearcherSession(
    const QByteArray& fileBytes) {
  ResearcherSessionData out;
  QJsonParseError err;
  const QJsonDocument doc = QJsonDocument::fromJson(fileBytes, &err);
  if (err.error != QJsonParseError::NoError || !doc.isObject()) {
    return out;
  }
  const QJsonObject root = doc.object();
  if (!root.contains(QStringLiteral("researcher"))) return out;
  const QJsonValue researcherVal = root.value(QStringLiteral("researcher"));
  if (!researcherVal.isObject()) return out;
  const QJsonObject researcher = researcherVal.toObject();
  const int version = researcher.value(QStringLiteral("version")).toInt(1);

  if (version == 1) {
    // Legacy flat cells → single notebook.
    ResearcherNotebook nb;
    nb.name = QStringLiteral("Notebook 1");
    nb.cells =
        cellsFromJsonArray(researcher.value(QStringLiteral("cells")).toArray());
    if (!nb.cells.isEmpty()) {
      out.notebooks.push_back(std::move(nb));
      out.activeIndex = 0;
    }
    return out;
  }
  if (version != kResearcherFormatVersion) return out;

  const QJsonArray notebooks =
      researcher.value(QStringLiteral("notebooks")).toArray();
  out.notebooks.reserve(notebooks.size());
  for (const QJsonValue& v : notebooks) {
    if (!v.isObject()) continue;
    const QJsonObject o = v.toObject();
    ResearcherNotebook nb;
    nb.name = o.value(QStringLiteral("name")).toString();
    if (nb.name.isEmpty()) nb.name = QStringLiteral("Notebook");
    nb.cells = cellsFromJsonArray(o.value(QStringLiteral("cells")).toArray());
    out.notebooks.push_back(std::move(nb));
  }
  out.activeIndex = researcher.value(QStringLiteral("active")).toInt(0);
  if (!out.notebooks.isEmpty()) {
    out.activeIndex =
        qBound(0, out.activeIndex, out.notebooks.size() - 1);
  } else {
    out.activeIndex = 0;
  }
  return out;
}

QByteArray Document::buildFileBytes() const {
  // Start from libanpcpp's canonical network document, then merge Studio extras.
  const std::string netText = anpcpp::network_to_json(*root_);
  QJsonParseError err;
  QJsonDocument doc =
      QJsonDocument::fromJson(QByteArray::fromStdString(netText), &err);
  if (err.error != QJsonParseError::NoError || !doc.isObject()) {
    throw std::runtime_error(
        std::string("failed to parse network JSON for save: ") +
        err.errorString().toStdString());
  }

  QJsonObject root = doc.object();
  if (researcherSessionIsTrivial()) {
    root.remove(QStringLiteral("researcher"));
  } else {
    QJsonArray notebooks;
    for (const ResearcherNotebook& nb : researcherNotebooks_) {
      QJsonObject o;
      o.insert(QStringLiteral("name"), nb.name);
      o.insert(QStringLiteral("cells"), cellsToJsonArray(nb.cells));
      notebooks.push_back(o);
    }
    QJsonObject researcher;
    researcher.insert(QStringLiteral("version"), kResearcherFormatVersion);
    researcher.insert(QStringLiteral("active"), researcherActiveIndex_);
    researcher.insert(QStringLiteral("notebooks"), notebooks);
    root.insert(QStringLiteral("researcher"), researcher);
  }
  if (!linkedGoogleForms_.isEmpty()) {
    QJsonArray forms;
    for (const LinkedGoogleForm& f : linkedGoogleForms_) {
      QJsonObject o;
      o.insert(QStringLiteral("formId"), f.formId);
      o.insert(QStringLiteral("title"), f.title);
      o.insert(QStringLiteral("responderUrl"), f.responderUrl);
      o.insert(QStringLiteral("editUrl"), f.editUrl);
      o.insert(QStringLiteral("createdAt"), f.createdAtIso);
      if (!f.structureFingerprint.isEmpty()) {
        o.insert(QStringLiteral("structureFingerprint"),
                 f.structureFingerprint);
      }
      if (!f.questionTags.isEmpty()) {
        QJsonArray tags;
        for (const QString& t : f.questionTags) tags.append(t);
        o.insert(QStringLiteral("questionTags"), tags);
      }
      if (!f.questionIds.isEmpty()) {
        QJsonArray ids;
        for (const QString& id : f.questionIds) ids.append(id);
        o.insert(QStringLiteral("questionIds"), ids);
      }
      if (!f.mappedTags.isEmpty()) {
        QJsonArray mapped;
        for (const QString& t : f.mappedTags) mapped.append(t);
        o.insert(QStringLiteral("mappedTags"), mapped);
      }
      forms.push_back(o);
    }
    QJsonObject block;
    block.insert(QStringLiteral("version"), 2);
    block.insert(QStringLiteral("forms"), forms);
    root.insert(QStringLiteral("google_forms"), block);
  } else {
    root.remove(QStringLiteral("google_forms"));
  }
  doc.setObject(root);
  return doc.toJson(QJsonDocument::Indented);
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
  researcherNotebooks_.clear();
  researcherActiveIndex_ = 0;
  linkedGoogleForms_.clear();
  setDirty(false);
  emit selectionChanged(selectedCluster_, selectedNode_);
  emit resultsFreshnessChanged();
  emitViewSwitch();
  emit researcherSessionChanged();
  emit linkedFormsChanged();
}

void Document::emitViewSwitch() {
  // See declaration: suppress canvas→model layout write across network swaps.
  setSuppressLayoutPersist(true);
  emit viewNetworkChanged();
  notifyChanged();
  flushModelChanged();
  setSuppressLayoutPersist(false);
}

void Document::newNetwork(bool create_alts) {
  path_.clear();
  emit pathChanged(path_);
  replaceRoot(std::make_unique<anpcpp::AnpNetwork>(create_alts));
}

bool Document::loadFromFile(const QString& path, QString* error) {
  try {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
      if (error) {
        *error = QStringLiteral("Could not open file: %1").arg(file.errorString());
      }
      return false;
    }
    const QByteArray bytes = file.readAll();
    file.close();

    auto net = anpcpp::network_from_json(bytes.toStdString());
    const ResearcherSessionData session = parseResearcherSession(bytes);
    const QVector<LinkedGoogleForm> forms = parseLinkedGoogleForms(bytes);

    path_ = path;
    emit pathChanged(path_);
    replaceRoot(std::move(net));
    researcherNotebooks_ = session.notebooks;
    researcherActiveIndex_ = session.activeIndex;
    linkedGoogleForms_ = forms;
    emit researcherSessionChanged();
    emit linkedFormsChanged();
    setDirty(false);
    return true;
  } catch (const std::exception& e) {
    if (error) *error = QString::fromUtf8(e.what());
    return false;
  }
}

bool Document::saveToFile(const QString& path, QString* error) {
  try {
    const QByteArray bytes = buildFileBytes();
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
      if (error) {
        *error =
            QStringLiteral("Could not write file: %1").arg(file.errorString());
      }
      return false;
    }
    if (file.write(bytes) != bytes.size()) {
      if (error) *error = QStringLiteral("Incomplete write to file.");
      return false;
    }
    file.close();
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
  emitViewSwitch();
}

void Document::popSubnet() {
  if (stack_.size() <= 1) return;
  stack_.pop_back();
  selectedCluster_.clear();
  selectedNode_.clear();
  emit selectionChanged(selectedCluster_, selectedNode_);
  emitViewSwitch();
}

void Document::popToRoot() {
  if (stack_.size() <= 1) return;
  stack_.resize(1);
  selectedCluster_.clear();
  selectedNode_.clear();
  emit selectionChanged(selectedCluster_, selectedNode_);
  emitViewSwitch();
}

void Document::popToDepth(int depth) {
  if (depth < 1) depth = 1;
  const auto target = static_cast<std::size_t>(depth);
  if (target >= stack_.size()) return;
  stack_.resize(target);
  selectedCluster_.clear();
  selectedNode_.clear();
  emit selectionChanged(selectedCluster_, selectedNode_);
  emitViewSwitch();
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

anpcpp::AnpNetwork* Document::networkAtPath(const QString& path) const {
  const QStringList parts = path.split(QStringLiteral(" / "), Qt::SkipEmptyParts);
  if (parts.isEmpty() || parts.first() != QStringLiteral("Root") ||
      root_ == nullptr) {
    return nullptr;
  }
  anpcpp::AnpNetwork* cur = root_.get();
  for (int i = 1; i < parts.size(); ++i) {
    anpcpp::AnpNode* node = cur->find_node(parts[i].toStdString());
    if (node == nullptr || !node->has_subnetwork()) {
      return nullptr;
    }
    cur = node->subnetwork();
  }
  return cur;
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
  emitViewSwitch();
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

void Document::setJudgmentSession(const anpcpp::JudgmentSession& session) {
  if (root_ == nullptr) return;
  const anpcpp::JudgmentSession cur = root_->judgment_session();
  if (cur.kind == session.kind && cur.id == session.id) return;
  root_->set_judgment_session(session);
  root_->rebuild_effective_judgments();
  setDirty(true);
  emit sessionChanged();
  notifyChanged();
}

anpcpp::JudgmentParticipant& Document::addParticipant(const QString& id,
                                                      const QString& name,
                                                      const QString& email) {
  anpcpp::JudgmentParticipant& p = root_->add_participant(
      id.toStdString(), name.toStdString(), email.toStdString());
  root_->ensure_multiuser_initialized();
  root_->rebuild_effective_judgments();
  setDirty(true);
  emit sessionChanged();
  notifyChanged();
  return p;
}

void Document::removeParticipant(const QString& id) {
  if (root_ == nullptr) return;
  root_->remove_participant(id.toStdString());
  root_->rebuild_effective_judgments();
  setDirty(true);
  emit sessionChanged();
  notifyChanged();
}

anpcpp::JudgmentGroup& Document::setJudgmentGroup(const QString& id,
                                                  const QString& name,
                                                  const QStringList& memberIds) {
  std::vector<std::string> members;
  members.reserve(static_cast<std::size_t>(memberIds.size()));
  for (const QString& m : memberIds) members.push_back(m.toStdString());
  anpcpp::JudgmentGroup& g = root_->add_judgment_group(
      id.toStdString(), name.toStdString(), std::move(members));
  root_->rebuild_effective_judgments();
  setDirty(true);
  emit sessionChanged();
  notifyChanged();
  return g;
}

void Document::removeJudgmentGroup(const QString& id) {
  if (root_ == nullptr) return;
  root_->remove_judgment_group(id.toStdString());
  root_->rebuild_effective_judgments();
  setDirty(true);
  emit sessionChanged();
  notifyChanged();
}

void Document::rebuildEffectiveJudgments() {
  if (root_ == nullptr) return;
  root_->rebuild_effective_judgments();
  setDirty(true);
  notifyChanged();
}

QString Document::activeParticipantId() const {
  if (root_ == nullptr) return {};
  const anpcpp::JudgmentSession session = root_->judgment_session();
  if (session.kind == anpcpp::JudgmentScopeKind::Participant &&
      !session.id.empty()) {
    return QString::fromStdString(session.id);
  }
  return {};
}

bool Document::judgmentReadOnly() const {
  if (root_ == nullptr || root_->participants().empty()) return false;
  return root_->judgment_session().kind != anpcpp::JudgmentScopeKind::Participant;
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
