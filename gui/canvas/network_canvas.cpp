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
#include <QTimer>
#include <QToolButton>

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

  addClusterBtn_ = new QToolButton(this);
  addClusterBtn_->setObjectName(QStringLiteral("canvasAddClusterBtn"));
  addClusterBtn_->setText(QStringLiteral("+"));
  addClusterBtn_->setToolTip(QStringLiteral("Add cluster"));
  addClusterBtn_->setCursor(Qt::PointingHandCursor);
  addClusterBtn_->setFixedSize(36, 36);
  addClusterBtn_->setAutoRaise(true);
  connect(addClusterBtn_, &QToolButton::clicked, this,
          &NetworkCanvas::promptAddCluster);

  connect(doc_, &Document::modelChanged, this, &NetworkCanvas::rebuild);
  connect(doc_, &Document::viewNetworkChanged, this, &NetworkCanvas::rebuild);
  connect(scene_, &QGraphicsScene::selectionChanged, this, [this]() {
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

void NetworkCanvas::setConnectMode(bool on) {
  connectMode_ = on;
  connectSrc_.clear();
  setCursor(on ? Qt::CrossCursor : Qt::ArrowCursor);
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
  QGraphicsView::keyPressEvent(event);
}

void NetworkCanvas::rebuild() {
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
  updateLinks();
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

void NetworkCanvas::mousePressEvent(QMouseEvent* event) {
  if (connectMode_ && event->button() == Qt::LeftButton) {
    if (NodeItem* n = nodeItemAt(event->pos())) {
      if (connectSrc_.isEmpty()) {
        connectSrc_ = n->nodeName();
      } else if (connectSrc_ != n->nodeName()) {
        doc_->undoStack()->push(
            new ConnectNodesCmd(doc_, connectSrc_, n->nodeName()));
        connectSrc_.clear();
        setConnectMode(false);
      }
      return;
    }
  }
  QGraphicsView::mousePressEvent(event);
  updateLinks();
}

void NetworkCanvas::contextMenuEvent(QContextMenuEvent* event) {
  QMenu menu(this);
  menu.addAction(QStringLiteral("Add Cluster"), this,
                 &NetworkCanvas::promptAddCluster);

  NodeItem* node = nodeItemAt(event->pos());
  ClusterItem* cluster = nullptr;
  for (QGraphicsItem* it : items(event->pos())) {
    if (auto* c = qgraphicsitem_cast<ClusterItem*>(it)) {
      cluster = c;
      break;
    }
  }

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
      connectSrc_ = node->nodeName();
      setConnectMode(true);
    });
  }

  menu.addSeparator();
  menu.addAction(QStringLiteral("Connect Mode"), this, [this]() {
    setConnectMode(!connectMode_);
  });

  menu.exec(event->globalPos());
}
