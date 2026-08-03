#include "canvas/network_canvas.hpp"

/**
 * @file network_canvas.cpp
 * @brief Structure-stage canvas: rebuild, layout, rename, and connect modes.
 */

#include "canvas/cluster_item.hpp"
#include "canvas/cluster_layout.hpp"
#include "canvas/cluster_link_item.hpp"
#include "canvas/link_item.hpp"
#include "canvas/node_item.hpp"
#include "commands/network_commands.hpp"
#include "document.hpp"

#include <QContextMenuEvent>
#include <QEvent>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QResizeEvent>
#include <QShortcut>
#include <QTimer>
#include <QToolButton>
#include <QUndoStack>
#include <QVector>

#include <anpcpp/network.hpp>

#include <cmath>

namespace {

/** QLineEdit that reports Escape / Enter-release explicitly (proxy widgets often swallow filters). */
class InlineRenameEdit : public QLineEdit {
public:
  using QLineEdit::QLineEdit;
  std::function<void()> onEscape;
  std::function<void()> onEnterReleased;

protected:
  void keyPressEvent(QKeyEvent* event) override {
    if (event->key() == Qt::Key_Escape) {
      // Copy before invoke: finish() may clear onEscape while it runs.
      const std::function<void()> cb = onEscape;
      if (cb) cb();
      event->accept();
      return;
    }
    QLineEdit::keyPressEvent(event);
  }

  void keyReleaseEvent(QKeyEvent* event) override {
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
      const std::function<void()> cb = onEnterReleased;
      if (cb) cb();
    }
    QLineEdit::keyReleaseEvent(event);
  }
};

}  // namespace

NetworkCanvas::NetworkCanvas(Document* doc, QWidget* parent)
    : QGraphicsView(parent), doc_(doc) {
  scene_ = new QGraphicsScene(this);
  setScene(scene_);
  setRenderHint(QPainter::Antialiasing, true);
  setDragMode(QGraphicsView::RubberBandDrag);
  setMinimumSize(400, 300);
  setFocusPolicy(Qt::StrongFocus);

  addClusterBtn_ = new QToolButton(this);
  addClusterBtn_->setObjectName(QStringLiteral("canvasAddClusterBtn"));
  addClusterBtn_->setText(QStringLiteral("+"));
  addClusterBtn_->setToolTip(QStringLiteral("Add cluster"));
  addClusterBtn_->setCursor(Qt::PointingHandCursor);
  addClusterBtn_->setFixedSize(36, 36);
  addClusterBtn_->setAutoRaise(true);
  connect(addClusterBtn_, &QToolButton::clicked, this,
          &NetworkCanvas::promptAddCluster);

  auto* esc = new QShortcut(QKeySequence(Qt::Key_Escape), this);
  esc->setContext(Qt::WidgetWithChildrenShortcut);
  connect(esc, &QShortcut::activated, this, [this]() {
    if (renameProxy_ != nullptr) {
      finishInlineRename(false, false);
      return;
    }
    if (connectMode_) setConnectMode(false);
  });

  connect(doc_, &Document::modelChanged, this, &NetworkCanvas::rebuild);
  connect(doc_, &Document::viewNetworkChanged, this, &NetworkCanvas::rebuild);
  connect(scene_, &QGraphicsScene::selectionChanged, this, [this]() {
    if (connectMode_) return;
    const auto items = scene_->selectedItems();
    if (items.isEmpty()) {
      emit selectionChanged({}, {});
      return;
    }
    if (auto* n = qgraphicsitem_cast<NodeItem*>(items.first())) {
      emit selectionChanged({}, n->nodeName());
    } else if (auto* c = qgraphicsitem_cast<ClusterItem*>(items.first())) {
      emit selectionChanged(c->clusterName(), {});
    }
  });

  rebuild();
  positionAddClusterButton();
}

NetworkCanvas::~NetworkCanvas() {
  cancelInlineRename();
}

void NetworkCanvas::setConnectMode(bool on) {
  if (connectMode_ == on) {
    if (on) setFocus(Qt::OtherFocusReason);
    return;
  }
  connectMode_ = on;
  clearConnectionSources();
  setCursor(on ? Qt::CrossCursor : Qt::ArrowCursor);
  setDragMode(on ? QGraphicsView::NoDrag : QGraphicsView::RubberBandDrag);
  applyLinkVisibility();
  if (on) setFocus(Qt::OtherFocusReason);
  emit connectModeChanged(on);
}

void NetworkCanvas::select(const QString& cluster, const QString& node) {
  const bool blocked = scene_->blockSignals(true);
  scene_->clearSelection();
  if (!node.isEmpty()) {
    if (auto* ni = nodes_.value(node)) {
      ni->setSelected(true);
    }
  } else if (!cluster.isEmpty()) {
    if (auto* ci = clusters_.value(cluster)) {
      ci->setSelected(true);
    }
  }
  scene_->blockSignals(blocked);
}

void NetworkCanvas::cancelInlineRename() {
  if (renameProxy_ == nullptr) return;
  QGraphicsProxyWidget* proxy = renameProxy_;
  renameProxy_ = nullptr;
  renameClosing_ = true;
  renameOnCommit_ = {};
  renameOldName_.clear();

  auto hideCb = std::move(renameHideCb_);
  renameHideCb_ = {};

  if (QWidget* w = proxy->widget()) {
    if (auto* inlineEdit = dynamic_cast<InlineRenameEdit*>(w)) {
      inlineEdit->onEscape = {};
      inlineEdit->onEnterReleased = {};
    }
    w->disconnect();
    w->clearFocus();
  }
  // Detach before deleteLater so a parent NodeItem/ClusterItem rebuild
  // cannot double-delete the proxy.
  proxy->setParentItem(nullptr);
  if (scene_ != nullptr && proxy->scene() == scene_) {
    scene_->removeItem(proxy);
  }
  proxy->deleteLater();

  if (hideCb) hideCb();
}

void NetworkCanvas::finishInlineRename(bool accept, bool viaEnter) {
  if (renameProxy_ == nullptr || renameClosing_) return;
  renameClosing_ = true;

  QString neu;
  if (auto* edit = qobject_cast<QLineEdit*>(renameProxy_->widget())) {
    neu = edit->text().trimmed();
  }

  const QString oldName = renameOldName_;
  auto onCommit = std::move(renameOnCommit_);
  renameOnCommit_ = {};

  const bool isNode = renameIsNode_;
  const bool abortRemovesNode = renameAbortRemovesNode_;
  // Keep add-session cluster so we can restore flags if a rename is rejected.
  const QString addCluster = nodeCreateChainCluster_;

  // Snapshot then clear so a stale follow-up cannot chain again.
  const QString chainCluster =
      (viaEnter && accept && !neu.isEmpty()) ? nodeCreateChainCluster_
                                            : QString();
  nodeCreateChainCluster_.clear();
  renameAbortRemovesNode_ = false;

  const QPointer<NetworkCanvas> self(this);
  // Fully leave this call stack before destroying the editor or mutating the
  // model (avoids signal-stack teardown and ClusterItem/NodeItem UAF).
  QTimer::singleShot(0, this, [self, accept, viaEnter, neu, oldName, onCommit,
                               chainCluster, addCluster, abortRemovesNode,
                               isNode]() {
    if (!self) return;

    self->cancelInlineRename();

    // Escape (or empty accept): drop a just-created node so the create loop
    // does not leave an unwanted placeholder behind.
    if (!accept || neu.isEmpty()) {
      if (abortRemovesNode && !oldName.isEmpty()) {
        QUndoStack* stack = self->doc_->undoStack();
        const QString addText = QStringLiteral("Add node %1").arg(oldName);
        if (stack->canUndo() && stack->undoText() == addText) {
          stack->undo();
        } else {
          stack->push(new RemoveNodeCmd(self->doc_, oldName));
        }
      }
      return;
    }

    if (neu != oldName) {
      const bool available =
          isNode ? self->isNodeNameAvailable(neu, oldName)
                 : self->isClusterNameAvailable(neu, oldName);
      if (!available) {
        // Reject duplicate: warn, then reopen editor with the attempted name.
        QMessageBox::warning(
            self,
            QStringLiteral("Duplicate name"),
            isNode ? QStringLiteral(
                         "A node named \"%1\" already exists.\n"
                         "Please choose a unique name.")
                         .arg(neu)
                   : QStringLiteral(
                         "A cluster named \"%1\" already exists.\n"
                         "Please choose a unique name.")
                         .arg(neu));
        if (abortRemovesNode) {
          self->renameAbortRemovesNode_ = true;
          self->nodeCreateChainCluster_ = addCluster;
        }
        // Keep Enter-chain deferral if this confirm was via Enter.
        if (viaEnter && !addCluster.isEmpty()) {
          self->renameDeferReturnPressed_ = true;
        }
        QTimer::singleShot(0, self, [self, oldName, neu, isNode]() {
          if (!self) return;
          if (isNode) {
            self->startInlineRenameNode(oldName, neu);
          } else {
            self->startInlineRenameCluster(oldName, neu);
          }
        });
        return;
      }
      if (onCommit) onCommit(neu);
    }

    if (chainCluster.isEmpty()) return;

    // Next editor must not see the still-held Enter key. Defer wiring of
    // returnPressed until KeyRelease (see beginInlineRename).
    self->renameDeferReturnPressed_ = true;
    QTimer::singleShot(0, self, [self, chainCluster]() {
      if (self) self->promptAddNode(chainCluster);
    });
  });
}

void NetworkCanvas::beginInlineRename(
    QGraphicsItem* parentItem,
    const QRectF& localRect,
    const QString& oldName,
    std::function<void()> onShow,
    std::function<void()> onHide,
    std::function<void(const QString&)> onCommit,
    const QString& editText) {
  cancelInlineRename();
  if (parentItem == nullptr) return;

  auto* edit = new InlineRenameEdit(editText.isEmpty() ? oldName : editText);
  edit->setFrame(true);
  edit->selectAll();

  renameProxy_ = new QGraphicsProxyWidget(parentItem);
  renameProxy_->setWidget(edit);
  renameProxy_->setPos(localRect.topLeft());
  renameProxy_->resize(localRect.size());
  renameProxy_->setZValue(100);
  renameProxy_->setFlag(QGraphicsItem::ItemIsFocusable, true);
  renameHideCb_ = std::move(onHide);
  renameOnCommit_ = std::move(onCommit);
  renameOldName_ = oldName;
  renameClosing_ = false;
  renameReturnWired_ = false;

  const bool deferReturn = renameDeferReturnPressed_;
  renameDeferReturnPressed_ = false;

  if (onShow) onShow();

  edit->onEscape = [this]() {
    // Queued: do not tear down the editor from inside its keyPressEvent.
    QMetaObject::invokeMethod(
        this,
        [this]() { finishInlineRename(false, false); },
        Qt::QueuedConnection);
  };

  const auto wireReturnPressed = [this, edit]() {
    if (renameReturnWired_ || renameProxy_ == nullptr || renameClosing_) return;
    renameReturnWired_ = true;
    connect(edit, &QLineEdit::returnPressed, this,
            [this]() { finishInlineRename(true, true); }, Qt::QueuedConnection);
  };

  if (deferReturn) {
    // Chained add: do not connect returnPressed until Enter is released.
    // Also ignore editingFinished until then — QLineEdit emits it on each
    // Enter auto-repeat keypress, which would tear down the new editor.
    edit->onEnterReleased = [this, edit, wireReturnPressed]() {
      if (renameProxy_ == nullptr || renameClosing_) return;
      edit->onEnterReleased = {};
      wireReturnPressed();
    };
  } else {
    wireReturnPressed();
  }

  connect(edit, &QLineEdit::editingFinished, this,
          [this]() {
            if (renameProxy_ == nullptr || renameClosing_) return;
            // Blur-commit only after Enter is a live shortcut for this editor.
            if (!renameReturnWired_) return;
            finishInlineRename(true, false);
          },
          Qt::QueuedConnection);

  renameProxy_->setFocus(Qt::MouseFocusReason);
  edit->setFocus(Qt::MouseFocusReason);
}

void NetworkCanvas::keyPressEvent(QKeyEvent* event) {
  // Fallback: graphics view may receive Escape before the proxy widget.
  if (renameProxy_ != nullptr && !renameClosing_ &&
      event->key() == Qt::Key_Escape) {
    finishInlineRename(false, false);
    event->accept();
    return;
  }
  if (connectMode_ && event->key() == Qt::Key_Escape) {
    setConnectMode(false);
    event->accept();
    return;
  }
  QGraphicsView::keyPressEvent(event);
}

// --- Scene rebuild and layout -----------------------------------------------

void NetworkCanvas::rebuild() {
  // Preserve connection sources across scene teardown. Item destruction during
  // scene_->clear() can invoke link-update callbacks while nodes_ is empty;
  // without a guard those callbacks would prune connectionSources_ away.
  const QSet<QString> savedSources = connectionSources_;
  const QString savedPrimary = connectionPrimary_;

  rebuilding_ = true;
  cancelInlineRename();
  // Persist current item positions into the model before tearing the scene
  // down — but only for in-place edits of the same network. File load / subnet
  // navigation sets Document::suppressLayoutPersist so we do not copy the old
  // canvas onto a different AnpNetwork (same cluster names → "ghost" layout).
  if (!doc_->suppressLayoutPersist()) {
    persistLayout();
  }
  scene_->clear();
  clusters_.clear();
  nodes_.clear();
  links_.clear();
  clusterLinks_.clear();

  auto& net = doc_->network();
  qreal x = 40;
  int i = 0;
  for (anpcpp::AnpCluster* c : net.clusters()) {
    auto* item = new ClusterItem(QString::fromStdString(c->name()));
    double cx = 0, cy = 0;
    if (net.cluster_position(c->name(), cx, cy)) {
      item->setPos(cx, cy);
    } else {
      item->setPos(x, 40.0 + (i % 2) * 40);
      x += 260;
    }
    scene_->addItem(item);
    clusters_.insert(QString::fromStdString(c->name()), item);
    item->setLinkUpdateCallback([this]() { updateLinks(); });
    item->setLayoutCommitCallback([this]() {
      persistLayout();
      doc_->setDirty(true);
    });
    item->setAddNodeCallback([this, name = QString::fromStdString(c->name())]() {
      // Defer: AddNodeCmd rebuilds the scene and destroys this ClusterItem.
      // Calling promptAddNode synchronously from mousePressEvent would
      // return into a deleted item (segfault).
      const QPointer<NetworkCanvas> self(this);
      QTimer::singleShot(0, this, [self, name]() {
        if (self) self->promptAddNode(name);
      });
    });
    item->setRenameCallback([this](const QString& cluster) {
      nodeCreateChainCluster_.clear();
      renameAbortRemovesNode_ = false;
      startInlineRenameCluster(cluster);
    });

    for (anpcpp::AnpNode* n : c->nodes()) {
      auto* ni = new NodeItem(QString::fromStdString(n->name()), item);
      ni->setHasSubnet(n->has_subnetwork());
      ni->setInvert(n->invert());
      ni->setLinkUpdateCallback([this]() { updateLinks(); });
      ni->setReorderCallback(
          [this](const QString& node, int from, int to) {
            doc_->undoStack()->push(new ReorderNodeCmd(doc_, node, from, to));
          });
      ni->setRenameCallback([this](const QString& node) {
        nodeCreateChainCluster_.clear();
        renameAbortRemovesNode_ = false;
        startInlineRenameNode(node);
      });
      nodes_.insert(QString::fromStdString(n->name()), ni);
    }
    item->layoutNodes();
    ++i;
  }

  // Directed links from each node's prioritizer destinations (src -> dest).
  QHash<QString, int> loopLanes;
  for (anpcpp::AnpNode* src : net.nodes()) {
    for (anpcpp::AnpCluster* destC : net.clusters()) {
      const anpcpp::NodePrioritizerSlot* slot =
          src->node_prioritizer(destC->name());
      if (slot == nullptr || slot->empty()) continue;
      for (const auto& destName : slot->alternatives()) {
        NodeItem* a = nodes_.value(QString::fromStdString(src->name()));
        NodeItem* b = nodes_.value(QString::fromStdString(destName));
        if (a == nullptr || b == nullptr) continue;
        auto* link = new LinkItem(a, b);
        if (link->isIntraCluster()) {
          const QString key = QString::fromStdString(src->cluster()->name());
          link->setLoopLane(loopLanes[key]++);
        }
        scene_->addItem(link);
        links_.push_back(link);
      }
    }
  }
  for (LinkItem* link : links_) link->updatePath();

  // Implied cluster→cluster edges (same inference as Organize Clusters).
  const auto meta = cluster_layout::metaEdges(net);
  for (const auto& edge : meta) {
    ClusterItem* a = clusters_.value(edge.first);
    ClusterItem* b = clusters_.value(edge.second);
    if (a == nullptr || b == nullptr) continue;
    auto* link = new ClusterLinkItem(a, b);
    scene_->addItem(link);
    clusterLinks_.push_back(link);
  }
  for (ClusterLinkItem* link : clusterLinks_) link->updatePath();

  applyLinkVisibility();
  rebuilding_ = false;

  connectionSources_ = savedSources;
  connectionPrimary_ = savedPrimary;
  refreshConnectionVisuals();
  if (connectMode_ && !connectionSources_.isEmpty()) {
    syncDocumentSelectionFromSources();
  }
}

void NetworkCanvas::persistLayout() {
  if (doc_ == nullptr) return;
  auto& net = doc_->network();
  for (auto it = clusters_.begin(); it != clusters_.end(); ++it) {
    const QPointF p = it.value()->pos();
    try {
      net.set_cluster_position(it.key().toStdString(), p.x(), p.y());
    } catch (...) {
    }
  }
}

void NetworkCanvas::organizeClusters() {
  if (doc_ == nullptr || clusters_.isEmpty()) return;

  QHash<QString, QSizeF> sizes;
  QHash<QString, QPointF> oldPositions;
  sizes.reserve(clusters_.size());
  oldPositions.reserve(clusters_.size());
  for (auto it = clusters_.begin(); it != clusters_.end(); ++it) {
    sizes.insert(it.key(), it.value()->rect().size());
    oldPositions.insert(it.key(), it.value()->pos());
  }

  const QHash<QString, QPointF> newPositions =
      cluster_layout::organize(doc_->network(), sizes);
  if (newPositions.isEmpty()) return;

  bool changed = false;
  for (auto it = newPositions.begin(); it != newPositions.end(); ++it) {
    const QPointF old = oldPositions.value(it.key());
    if (std::abs(old.x() - it.value().x()) > 0.5 ||
        std::abs(old.y() - it.value().y()) > 0.5) {
      changed = true;
      break;
    }
  }
  if (!changed) return;

  doc_->undoStack()->push(
      new SetClusterPositionsCmd(doc_, oldPositions, newPositions));
  fitClustersInView();
}

void NetworkCanvas::fitClustersInView() {
  if (scene_ == nullptr || clusters_.isEmpty()) return;
  QRectF bounds;
  bool first = true;
  for (ClusterItem* item : clusters_) {
    if (item == nullptr) continue;
    const QRectF r = item->mapToScene(item->rect()).boundingRect();
    if (first) {
      bounds = r;
      first = false;
    } else {
      bounds |= r;
    }
  }
  if (first) return;
  bounds.adjust(-40, -40, 40, 40);
  fitInView(bounds, Qt::KeepAspectRatio);
  // Avoid extreme zoom-out on tiny nets; keep a readable default scale.
  if (transform().m11() > 1.25) {
    resetTransform();
    centerOn(bounds.center());
  }
}

void NetworkCanvas::updateLinks() {
  if (rebuilding_) return;
  for (LinkItem* link : links_) link->updatePath();
  for (ClusterLinkItem* link : clusterLinks_) link->updatePath();
}

void NetworkCanvas::applyLinkVisibility() {
  // Normal: cluster meta-edges only. Connection: full node→node links.
  const bool showNodes = connectMode_;
  for (LinkItem* link : links_) {
    link->setVisible(showNodes);
  }
  for (ClusterLinkItem* link : clusterLinks_) {
    link->setVisible(!showNodes);
  }
}

void NetworkCanvas::positionAddClusterButton() {
  if (addClusterBtn_ == nullptr) return;
  const int margin = 12;
  addClusterBtn_->move(width() - addClusterBtn_->width() - margin, margin);
  addClusterBtn_->raise();
}

void NetworkCanvas::resizeEvent(QResizeEvent* event) {
  QGraphicsView::resizeEvent(event);
  positionAddClusterButton();
}

void NetworkCanvas::promptAddCluster() {
  const QString name = uniqueClusterName();
  doc_->undoStack()->push(new AddClusterCmd(doc_, name));
  doc_->flushModelChanged();
  select(name, {});
  // Defer past the add/rebuild stack so the new item exists and can take focus.
  const QPointer<NetworkCanvas> self(this);
  QTimer::singleShot(0, this, [self, name]() {
    if (!self) return;
    self->nodeCreateChainCluster_.clear();
    self->renameAbortRemovesNode_ = false;
    self->startInlineRenameCluster(name);
  });
}

void NetworkCanvas::promptAddNode(const QString& clusterName) {
  const QString name = uniqueNodeName();
  doc_->undoStack()->push(new AddNodeCmd(doc_, clusterName, name));
  doc_->flushModelChanged();
  select({}, name);
  const QPointer<NetworkCanvas> self(this);
  QTimer::singleShot(0, this, [self, name, clusterName]() {
    if (!self) return;
    // Mark this rename as an add-node session so Enter chains another create,
    // and Escape removes this placeholder node.
    self->nodeCreateChainCluster_ = clusterName;
    self->renameAbortRemovesNode_ = true;
    self->startInlineRenameNode(name);
  });
}

QString NetworkCanvas::uniqueClusterName() const {
  const auto& net = doc_->network();
  for (int i = 1;; ++i) {
    const QString name = QStringLiteral("New Cluster #%1").arg(i);
    if (net.find_cluster(name.toStdString()) == nullptr) return name;
  }
}

QString NetworkCanvas::uniqueNodeName() const {
  const auto& net = doc_->network();
  for (int i = 1;; ++i) {
    const QString name = QStringLiteral("New Node #%1").arg(i);
    if (net.find_node(name.toStdString()) == nullptr) return name;
  }
}

bool NetworkCanvas::isNodeNameAvailable(const QString& name,
                                        const QString& exceptName) const {
  if (name.isEmpty()) return false;
  if (name == exceptName) return true;
  return doc_->network().find_node(name.toStdString()) == nullptr;
}

bool NetworkCanvas::isClusterNameAvailable(const QString& name,
                                           const QString& exceptName) const {
  if (name.isEmpty()) return false;
  if (name == exceptName) return true;
  return doc_->network().find_cluster(name.toStdString()) == nullptr;
}

void NetworkCanvas::startInlineRenameCluster(const QString& cluster,
                                             const QString& editText) {
  ClusterItem* ci = clusters_.value(cluster);
  if (ci == nullptr) return;
  ci->setSelected(true);
  renameIsNode_ = false;
  beginInlineRename(
      ci, ci->titleEditRect(), cluster,
      [this, cluster]() {
        if (auto* c = clusters_.value(cluster)) c->setTitleVisible(false);
      },
      [this, cluster]() {
        if (auto* c = clusters_.value(cluster)) c->setTitleVisible(true);
      },
      [this, cluster](const QString& neu) {
        doc_->undoStack()->push(new RenameClusterCmd(doc_, cluster, neu));
      },
      editText);
}

void NetworkCanvas::startInlineRenameNode(const QString& node,
                                          const QString& editText) {
  NodeItem* item = nodes_.value(node);
  if (item == nullptr) return;
  item->setSelected(true);
  renameIsNode_ = true;
  beginInlineRename(
      item, item->rect().adjusted(4, 2, -4, -2), node,
      [this, node]() {
        if (auto* n = nodes_.value(node)) n->setLabelVisible(false);
      },
      [this, node]() {
        if (auto* n = nodes_.value(node)) n->setLabelVisible(true);
      },
      [this, node](const QString& neu) {
        doc_->undoStack()->push(new RenameNodeCmd(doc_, node, neu));
      },
      editText);
}

NodeItem* NetworkCanvas::nodeItemAt(const QPoint& viewPos) const {
  for (QGraphicsItem* it : items(viewPos)) {
    if (auto* n = qgraphicsitem_cast<NodeItem*>(it)) return n;
  }
  return nullptr;
}

ClusterItem* NetworkCanvas::clusterItemAt(const QPoint& viewPos) const {
  for (QGraphicsItem* it : items(viewPos)) {
    if (auto* c = qgraphicsitem_cast<ClusterItem*>(it)) return c;
  }
  return nullptr;
}

// --- Connect mode (batch link / unlink) -------------------------------------

void NetworkCanvas::clearConnectionSources() {
  connectionSources_.clear();
  connectionPrimary_.clear();
  refreshConnectionVisuals();
}

void NetworkCanvas::setSoleConnectionSource(const QString& node) {
  connectionSources_.clear();
  connectionSources_.insert(node);
  connectionPrimary_ = node;
  refreshConnectionVisuals();
  syncDocumentSelectionFromSources();
}

void NetworkCanvas::toggleConnectionSource(const QString& node) {
  if (connectionSources_.contains(node)) {
    connectionSources_.remove(node);
    if (connectionPrimary_ == node) {
      connectionPrimary_ =
          connectionSources_.isEmpty() ? QString()
                                       : *connectionSources_.constBegin();
    }
  } else {
    connectionSources_.insert(node);
    connectionPrimary_ = node;
  }
  refreshConnectionVisuals();
  syncDocumentSelectionFromSources();
}

void NetworkCanvas::setConnectionSourcesFromCluster(ClusterItem* cluster,
                                                    bool unionWith) {
  if (cluster == nullptr) return;
  if (!unionWith) connectionSources_.clear();
  const auto members = cluster->nodeItems();
  for (NodeItem* n : members) {
    connectionSources_.insert(n->nodeName());
  }
  if (!members.empty()) {
    connectionPrimary_ = members.front()->nodeName();
  } else if (!unionWith) {
    connectionPrimary_.clear();
  }
  refreshConnectionVisuals();
  syncDocumentSelectionFromSources();
}

void NetworkCanvas::refreshConnectionVisuals() {
  if (rebuilding_) return;

  // Prune against the document model, not the transient canvas node map —
  // during rebuild nodes_ may be empty while sources are still valid.
  QSet<QString> alive;
  for (const QString& s : connectionSources_) {
    if (doc_->network().find_node(s.toStdString()) != nullptr) {
      alive.insert(s);
    }
  }
  connectionSources_ = alive;
  if (!connectionPrimary_.isEmpty() &&
      !connectionSources_.contains(connectionPrimary_)) {
    connectionPrimary_ = connectionSources_.isEmpty()
                             ? QString()
                             : *connectionSources_.constBegin();
  }

  for (auto it = nodes_.begin(); it != nodes_.end(); ++it) {
    it.value()->setConnectionSource(connectionSources_.contains(it.key()));
  }

  const bool dim = connectMode_ && !connectionSources_.isEmpty();
  for (LinkItem* link : links_) {
    if (!dim) {
      link->setOpacity(1.0);
      continue;
    }
    const bool relevant =
        link->src() != nullptr &&
        connectionSources_.contains(link->src()->nodeName());
    link->setOpacity(relevant ? 1.0 : 0.2);
  }
}

void NetworkCanvas::syncDocumentSelectionFromSources() {
  if (connectionSources_.isEmpty()) {
    emit selectionChanged({}, {});
    return;
  }
  const QString node = connectionPrimary_.isEmpty()
                           ? *connectionSources_.constBegin()
                           : connectionPrimary_;
  emit selectionChanged({}, node);
}

void NetworkCanvas::batchToggleToDestinations(const QList<QString>& dests) {
  if (connectionSources_.isEmpty() || dests.isEmpty()) return;

  struct Pair {
    QString src;
    QString dest;
  };
  QVector<Pair> pairs;
  for (const QString& src : connectionSources_) {
    for (const QString& dest : dests) {
      if (src == dest) continue;
      if (doc_->network().find_node(src.toStdString()) == nullptr) continue;
      if (doc_->network().find_node(dest.toStdString()) == nullptr) continue;
      pairs.push_back({src, dest});
    }
  }
  if (pairs.isEmpty()) return;

  auto& net = doc_->network();
  auto isConnected = [&net](const QString& src, const QString& dest) {
    try {
      return net.node(src.toStdString())
          .is_connected_to(&net.node(dest.toStdString()));
    } catch (...) {
      return false;
    }
  };

  bool anyMissing = false;
  for (const Pair& p : pairs) {
    if (!isConnected(p.src, p.dest)) {
      anyMissing = true;
      break;
    }
  }

  if (anyMissing) {
    QVector<Pair> toConnect;
    for (const Pair& p : pairs) {
      if (!isConnected(p.src, p.dest)) toConnect.push_back(p);
    }
    if (toConnect.isEmpty()) return;
    if (toConnect.size() == 1) {
      doc_->undoStack()->push(
          new ConnectNodesCmd(doc_, toConnect.front().src, toConnect.front().dest));
    } else {
      doc_->undoStack()->beginMacro(
          QStringLiteral("Connect %1 link(s)").arg(toConnect.size()));
      for (const Pair& p : toConnect) {
        doc_->undoStack()->push(new ConnectNodesCmd(doc_, p.src, p.dest));
      }
      doc_->undoStack()->endMacro();
    }
  } else {
    if (pairs.size() == 1) {
      doc_->undoStack()->push(
          new DisconnectNodesCmd(doc_, pairs.front().src, pairs.front().dest));
    } else {
      doc_->undoStack()->beginMacro(
          QStringLiteral("Disconnect %1 link(s)").arg(pairs.size()));
      for (const Pair& p : pairs) {
        doc_->undoStack()->push(new DisconnectNodesCmd(doc_, p.src, p.dest));
      }
      doc_->undoStack()->endMacro();
    }
  }
}

void NetworkCanvas::handleConnectionLeftClick(QMouseEvent* event) {
  const bool shift = event->modifiers() & Qt::ShiftModifier;
  if (NodeItem* n = nodeItemAt(event->pos())) {
    if (shift) {
      toggleConnectionSource(n->nodeName());
    } else {
      setSoleConnectionSource(n->nodeName());
    }
    event->accept();
    return;
  }
  if (ClusterItem* c = clusterItemAt(event->pos())) {
    setConnectionSourcesFromCluster(c, shift);
    event->accept();
    return;
  }
  clearConnectionSources();
  syncDocumentSelectionFromSources();
  event->accept();
}

void NetworkCanvas::handleConnectionRightClick(QMouseEvent* event) {
  if (connectionSources_.isEmpty()) {
    event->accept();
    return;
  }
  if (NodeItem* n = nodeItemAt(event->pos())) {
    batchToggleToDestinations({n->nodeName()});
    event->accept();
    return;
  }
  if (ClusterItem* c = clusterItemAt(event->pos())) {
    QList<QString> dests;
    for (NodeItem* n : c->nodeItems()) {
      dests.push_back(n->nodeName());
    }
    batchToggleToDestinations(dests);
    event->accept();
    return;
  }
  event->accept();
}

void NetworkCanvas::mousePressEvent(QMouseEvent* event) {
  if (connectMode_) {
    if (event->button() == Qt::LeftButton) {
      handleConnectionLeftClick(event);
      return;
    }
    if (event->button() == Qt::RightButton) {
      handleConnectionRightClick(event);
      return;
    }
  }
  QGraphicsView::mousePressEvent(event);
  updateLinks();
}

void NetworkCanvas::contextMenuEvent(QContextMenuEvent* event) {
  if (connectMode_) {
    event->accept();
    return;
  }

  QMenu menu(this);
  menu.addAction(QStringLiteral("Add Cluster"), this,
                 &NetworkCanvas::promptAddCluster);
  menu.addAction(QStringLiteral("Organize Clusters"), this,
                 &NetworkCanvas::organizeClusters);

  NodeItem* node = nodeItemAt(event->pos());
  ClusterItem* cluster = clusterItemAt(event->pos());

  if (cluster != nullptr) {
    const QString clusterName = cluster->clusterName();
    menu.addAction(QStringLiteral("Add Node"), this, [this, clusterName]() {
      promptAddNode(clusterName);
    });
    menu.addAction(QStringLiteral("Set as Alternatives Cluster"), this,
                   [this, clusterName]() {
                     QString old;
                     if (auto* ac = doc_->network().alternatives_cluster()) {
                       old = QString::fromStdString(ac->name());
                     }
                     doc_->undoStack()->push(
                         new SetAlternativesClusterCmd(doc_, clusterName, old));
                   });
  }

  if (node != nullptr) {
    menu.addAction(QStringLiteral("Open / Create Subnetwork"), this,
                   [this, node]() {
                     emit nodeActivated(node->nodeName());
                   });
    menu.addAction(QStringLiteral("Toggle Invert"), this, [this, node]() {
      const bool cur =
          doc_->network().node(node->nodeName().toStdString()).invert();
      doc_->undoStack()->push(
          new SetInvertCmd(doc_, node->nodeName(), !cur));
    });
    menu.addAction(QStringLiteral("Connect From Here…"), this, [this, node]() {
      setConnectMode(true);
      setSoleConnectionSource(node->nodeName());
    });
  }

  menu.addSeparator();
  auto* modeAct = menu.addAction(QStringLiteral("Connection Mode"), this, [this]() {
    setConnectMode(!connectMode_);
  });
  modeAct->setCheckable(true);
  modeAct->setChecked(connectMode_);

  menu.exec(event->globalPos());
}
