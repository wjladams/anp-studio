#include "canvas/node_item.hpp"

#include "canvas/cluster_item.hpp"

#include <QBrush>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPen>
#include <QStyle>
#include <QStyleOptionGraphicsItem>
#include <algorithm>
#include <cmath>

NodeItem::NodeItem(const QString& name, QGraphicsItem* parent)
    : QGraphicsRectItem(parent), name_(name) {
  setRect(0, 0, 200, ClusterItem::kRowH);
  // Not freely movable — only vertical reorder within the parent cluster.
  setFlags(ItemIsSelectable | ItemSendsGeometryChanges);
  label_ = new QGraphicsSimpleTextItem(name_, this);
  label_->setPos(10, 6);
  label_->setZValue(1);
  label_->setAcceptedMouseButtons(Qt::NoButton);
  refreshLook();
}

void NodeItem::setNodeName(const QString& name) {
  name_ = name;
  refreshLook();
}

void NodeItem::setHasSubnet(bool has) {
  hasSubnet_ = has;
  refreshLook();
}

void NodeItem::setInvert(bool invert) {
  invert_ = invert;
  refreshLook();
}

void NodeItem::setRowGeometry(qreal width, qreal height) {
  setRect(0, 0, width, height);
  label_->setPos(10, (height - label_->boundingRect().height()) * 0.5);
}

ClusterItem* NodeItem::parentCluster() const {
  return qgraphicsitem_cast<ClusterItem*>(parentItem());
}

void NodeItem::refreshLook() {
  QString text = name_;
  if (hasSubnet_) text += QStringLiteral("  [subnet]");
  if (invert_) text += QStringLiteral("  (inv)");
  label_->setText(text);

  QColor fill = QColor(255, 255, 255);
  if (hasSubnet_) fill = QColor(255, 244, 220);
  if (invert_) fill = QColor(255, 228, 228);
  setBrush(QBrush(fill));
  setPen(QPen(QColor(70, 100, 140), 1.2));
}

void NodeItem::paint(QPainter* painter,
                     const QStyleOptionGraphicsItem* option,
                     QWidget* widget) {
  Q_UNUSED(widget);
  painter->setRenderHint(QPainter::Antialiasing, true);
  QPen p = pen();
  if (option->state & QStyle::State_Selected) {
    p.setColor(QColor(20, 90, 180));
    p.setWidthF(2.0);
  }
  painter->setPen(p);
  painter->setBrush(brush());
  painter->drawRoundedRect(rect(), 3, 3);
}

QVariant NodeItem::itemChange(GraphicsItemChange change,
                              const QVariant& value) {
  if (change == ItemPositionHasChanged && linkUpdate_) {
    linkUpdate_();
  }
  return QGraphicsRectItem::itemChange(change, value);
}

void NodeItem::mousePressEvent(QGraphicsSceneMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    reordering_ = false;
    fromIndex_ = -1;
    pressPos_ = event->pos();
    if (ClusterItem* c = parentCluster()) {
      fromIndex_ = c->indexOf(this);
    }
  }
  QGraphicsRectItem::mousePressEvent(event);
}

void NodeItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
  if (!(event->buttons() & Qt::LeftButton) || fromIndex_ < 0) {
    QGraphicsRectItem::mouseMoveEvent(event);
    return;
  }
  ClusterItem* c = parentCluster();
  if (c == nullptr) {
    QGraphicsRectItem::mouseMoveEvent(event);
    return;
  }

  const QPointF delta = event->pos() - pressPos_;
  if (!reordering_ && std::abs(delta.y()) > 4.0) {
    reordering_ = true;
  }
  if (!reordering_) {
    QGraphicsRectItem::mouseMoveEvent(event);
    return;
  }

  // event->pos() is in item coords; convert intended top-left into cluster.
  const QPointF clusterPos = mapToParent(QPointF(0, 0)) +
                             QPointF(0, event->pos().y() - pressPos_.y());
  c->layoutNodes(this, clusterPos.y());
  if (linkUpdate_) linkUpdate_();
  event->accept();
}

void NodeItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
  ClusterItem* c = parentCluster();
  if (reordering_ && c != nullptr && fromIndex_ >= 0) {
    const int toIndex = c->dropIndexForY(pos().y() + rect().height() * 0.5);
    if (toIndex != fromIndex_ && reorderCb_) {
      reorderCb_(name_, fromIndex_, toIndex);
    } else {
      c->layoutNodes();
      if (linkUpdate_) linkUpdate_();
    }
  }
  reordering_ = false;
  fromIndex_ = -1;
  QGraphicsRectItem::mouseReleaseEvent(event);
}

void NodeItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) {
  event->accept();
  if (activateCb_) {
    activateCb_(name_);
  }
}
