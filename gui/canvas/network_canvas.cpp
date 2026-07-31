#include "canvas/network_canvas.hpp"

#include "canvas/cluster_item.hpp"
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

namespace {

/** QLineEdit that reports Escape explicitly (proxy widgets often swallow filters). */
class InlineRenameEdit : public QLineEdit {
public:
  using QLineEdit::QLineEdit;
  std::function<void()> onEscape;

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
    if (renameProxy_ != nullptr) return;
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
  renameClosing_ = false;

  auto hideCb = std::move(renameHideCb_);
  renameHideCb_ = {};

  if (QWidget* w = proxy->widget()) {
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

void NetworkCanvas::beginInlineRename(
    QGraphicsItem* parentItem,
    const QRectF& localRect,
    const QString& oldName,
    std::function<void()> onShow,
    std::function<void()> onHide,
    std::function<void(const QString&)> onCommit) {
  cancelInlineRename();
  if (parentItem == nullptr) return;

  auto* edit = new InlineRenameEdit(oldName);
  edit->setFrame(true);
  edit->selectAll();

  renameProxy_ = new QGraphicsProxyWidget(parentItem);
  renameProxy_->setWidget(edit);
  renameProxy_->setPos(localRect.topLeft());
  renameProxy_->resize(localRect.size());
  renameProxy_->setZValue(100);
  renameProxy_->setFlag(QGraphicsItem::ItemIsFocusable, true);
  renameHideCb_ = std::move(onHide);
  if (onShow) onShow();

  // Never destroy the editor from inside its own key/signal handlers.
  const auto finish = [this, edit, oldName, onCommit](bool accept) {
    if (renameProxy_ == nullptr || renameClosing_) return;
    renameClosing_ = true;
    const QString neu = edit->text().trimmed();
    edit->disconnect();
    const QPointer<NetworkCanvas> self(this);
    QTimer::singleShot(0, this, [self, accept, neu, oldName, onCommit]() {
      if (!self) return;
      self->cancelInlineRename();
      if (accept && !neu.isEmpty() && neu != oldName && onCommit) {
        onCommit(neu);
      }
    });
  };

  edit->onEscape = [finish]() { finish(false); };
  connect(edit, &QLineEdit::returnPressed, this, [finish]() { finish(true); });
  connect(edit, &QLineEdit::editingFinished, this, [this, finish]() {
    if (renameProxy_ == nullptr || renameClosing_) return;
    finish(true);
  });

  renameProxy_->setFocus(Qt::MouseFocusReason);
  edit->setFocus(Qt::MouseFocusReason);
}

void NetworkCanvas::keyPressEvent(QKeyEvent* event) {
  // Fallback: graphics view may receive Escape before the proxy widget.
  if (renameProxy_ != nullptr && !renameClosing_ &&
      event->key() == Qt::Key_Escape) {
    if (auto* edit = static_cast<InlineRenameEdit*>(renameProxy_->widget())) {
      // Copy before invoke: finish() must not destroy a running std::function.
      const std::function<void()> cb = edit->onEscape;
      if (cb) {
        cb();
      } else {
        renameClosing_ = true;
        const QPointer<NetworkCanvas> self(this);
        QTimer::singleShot(0, this, [self]() {
          if (self) self->cancelInlineRename();
        });
      }
    }
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

void NetworkCanvas::rebuild() {
  // Preserve connection sources across scene teardown. Item destruction during
  // scene_->clear() can invoke link-update callbacks while nodes_ is empty;
  // without a guard those callbacks would prune connectionSources_ away.
  const QSet<QString> savedSources = connectionSources_;
  const QString savedPrimary = connectionPrimary_;

  rebuilding_ = true;
  cancelInlineRename();
  persistLayout();
  scene_->clear();
  clusters_.clear();
  nodes_.clear();
  links_.clear();

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
    item->setAddNodeCallback([this, name = QString::fromStdString(c->name())]() {
      promptAddNode(name);
    });
    item->setRenameCallback([this](const QString& cluster) {
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

void NetworkCanvas::updateLinks() {
  if (rebuilding_) return;
  for (LinkItem* link : links_) link->updatePath();
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
  select(name, {});
  // Defer past the add/rebuild stack so the new item exists and can take focus.
  const QPointer<NetworkCanvas> self(this);
  QTimer::singleShot(0, this, [self, name]() {
    if (self) self->startInlineRenameCluster(name);
  });
}

void NetworkCanvas::promptAddNode(const QString& clusterName) {
  const QString name = uniqueNodeName();
  doc_->undoStack()->push(new AddNodeCmd(doc_, clusterName, name));
  select({}, name);
  const QPointer<NetworkCanvas> self(this);
  QTimer::singleShot(0, this, [self, name]() {
    if (self) self->startInlineRenameNode(name);
  });
}

QString NetworkCanvas::uniqueClusterName() const {
  const auto& net = doc_->network();
  for (int i = 1;; ++i) {
    const QString name = (i == 1)
                             ? QStringLiteral("New Cluster")
                             : QStringLiteral("New Cluster %1").arg(i);
    if (net.find_cluster(name.toStdString()) == nullptr) return name;
  }
}

QString NetworkCanvas::uniqueNodeName() const {
  const auto& net = doc_->network();
  for (int i = 1;; ++i) {
    const QString name =
        (i == 1) ? QStringLiteral("New Node")
                 : QStringLiteral("New Node %1").arg(i);
    if (net.find_node(name.toStdString()) == nullptr) return name;
  }
}

void NetworkCanvas::startInlineRenameCluster(const QString& cluster) {
  ClusterItem* ci = clusters_.value(cluster);
  if (ci == nullptr) return;
  ci->setSelected(true);
  beginInlineRename(
      ci, ci->titleEditRect(), cluster,
      [ci]() { ci->setTitleVisible(false); },
      [ci]() { ci->setTitleVisible(true); },
      [this, cluster](const QString& neu) {
        doc_->undoStack()->push(new RenameClusterCmd(doc_, cluster, neu));
      });
}

void NetworkCanvas::startInlineRenameNode(const QString& node) {
  NodeItem* item = nodes_.value(node);
  if (item == nullptr) return;
  item->setSelected(true);
  beginInlineRename(
      item, item->rect().adjusted(4, 2, -4, -2), node,
      [item]() { item->setLabelVisible(false); },
      [item]() { item->setLabelVisible(true); },
      [this, node](const QString& neu) {
        doc_->undoStack()->push(new RenameNodeCmd(doc_, node, neu));
      });
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
