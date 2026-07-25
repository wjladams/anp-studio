#include "canvas/node_item.hpp"

#include <QBrush>
#include <QPen>

NodeItem::NodeItem(const QString& name, QGraphicsItem* parent)
    : QGraphicsEllipseItem(parent), name_(name) {
  setRect(0, 0, 140, 28);
  setFlags(ItemIsSelectable | ItemIsMovable | ItemSendsGeometryChanges);
  label_ = new QGraphicsSimpleTextItem(name_, this);
  label_->setPos(10, 6);
  refreshLook();
}

void NodeItem::setNodeName(const QString& name) {
  name_ = name;
  label_->setText(name_);
}

void NodeItem::setHasSubnet(bool has) {
  hasSubnet_ = has;
  refreshLook();
}

void NodeItem::setInvert(bool invert) {
  invert_ = invert;
  refreshLook();
}

void NodeItem::refreshLook() {
  QColor fill = QColor(220, 235, 255);
  if (hasSubnet_) fill = QColor(255, 236, 200);
  if (invert_) fill = QColor(255, 210, 210);
  setBrush(QBrush(fill));
  setPen(QPen(QColor(40, 70, 110), 1.5));
}

QVariant NodeItem::itemChange(GraphicsItemChange change,
                              const QVariant& value) {
  if (change == ItemPositionHasChanged && linkUpdate_) {
    linkUpdate_();
  }
  return QGraphicsEllipseItem::itemChange(change, value);
}
