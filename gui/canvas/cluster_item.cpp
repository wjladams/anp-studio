#include "canvas/cluster_item.hpp"

#include "canvas/node_item.hpp"

#include <QBrush>
#include <QPen>
#include <algorithm>

ClusterItem::ClusterItem(const QString& name, QGraphicsItem* parent)
    : QGraphicsRectItem(parent), name_(name) {
  setRect(0, 0, 220, 160);
  setBrush(QBrush(QColor(245, 248, 252)));
  setPen(QPen(QColor(60, 90, 130), 2));
  setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
  title_ = new QGraphicsSimpleTextItem(name_, this);
  title_->setPos(10, 8);
  title_->setBrush(QColor(30, 50, 80));
}

void ClusterItem::setClusterName(const QString& name) {
  name_ = name;
  title_->setText(name_);
}

void ClusterItem::layoutNodes() {
  qreal y = 36;
  for (QGraphicsItem* child : childItems()) {
    auto* node = qgraphicsitem_cast<NodeItem*>(child);
    if (node == nullptr) continue;
    node->setPos(16, y);
    y += 36;
  }
  const qreal h = std::max<qreal>(160.0, y + 16);
  setRect(0, 0, 220, h);
}

QVariant ClusterItem::itemChange(GraphicsItemChange change,
                                 const QVariant& value) {
  if (change == ItemPositionHasChanged && linkUpdate_) {
    linkUpdate_();
  }
  return QGraphicsRectItem::itemChange(change, value);
}
