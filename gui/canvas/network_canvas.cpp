#include "canvas/network_canvas.hpp"

#include "canvas/cluster_item.hpp"
#include "canvas/link_item.hpp"
#include "canvas/node_item.hpp"
#include "commands/network_commands.hpp"
#include "document.hpp"

#include <QContextMenuEvent>
#include <QGraphicsScene>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QTimer>
#include <QToolButton>

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

void NetworkCanvas::rebuild() {
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

    for (anpcpp::AnpNode* n : c->nodes()) {
      auto* ni = new NodeItem(QString::fromStdString(n->name()), item);
      ni->setHasSubnet(n->has_subnetwork());
      ni->setInvert(n->invert());
      ni->setLinkUpdateCallback([this]() { updateLinks(); });
      ni->setReorderCallback(
          [this](const QString& node, int from, int to) {
            doc_->undoStack()->push(new ReorderNodeCmd(doc_, node, from, to));
          });
      ni->setActivateCallback([this](const QString& node) {
        // Defer: EnsureSubnet/pushSubnet rebuilds the scene and would delete
        // this NodeItem while mouseDoubleClickEvent is still on the stack.
        QTimer::singleShot(0, this, [this, node]() {
          emit nodeActivated(node);
        });
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
  bool ok = false;
  const QString name = QInputDialog::getText(
      this, QStringLiteral("Add Cluster"), QStringLiteral("Name:"),
      QLineEdit::Normal, {}, &ok);
  if (ok && !name.trimmed().isEmpty()) {
    doc_->undoStack()->push(new AddClusterCmd(doc_, name.trimmed()));
  }
}

void NetworkCanvas::promptAddNode(const QString& clusterName) {
  bool ok = false;
  const QString name = QInputDialog::getText(
      this, QStringLiteral("Add Node"), QStringLiteral("Name:"),
      QLineEdit::Normal, {}, &ok);
  if (ok && !name.trimmed().isEmpty()) {
    doc_->undoStack()->push(
        new AddNodeCmd(doc_, clusterName, name.trimmed()));
  }
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
  menu.addAction(QStringLiteral("Add Cluster…"), this,
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
    menu.addAction(QStringLiteral("Add Node…"), this, [this, clusterName]() {
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
                     doc_->undoStack()->push(
                         new EnsureSubnetCmd(doc_, node->nodeName()));
                     doc_->pushSubnet(node->nodeName());
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
