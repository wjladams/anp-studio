#include "canvas/link_item.hpp"

#include "canvas/node_item.hpp"

#include <QPainterPath>
#include <QPen>
#include <cmath>

LinkItem::LinkItem(NodeItem* src, NodeItem* dest, QGraphicsItem* parent)
    : QGraphicsPathItem(parent), src_(src), dest_(dest) {
  setPen(QPen(QColor(40, 70, 110), 2.0));
  // Above cluster backgrounds so directed edges stay visible (including
  // within a cluster). Do not steal mouse clicks from nodes underneath.
  setZValue(50);
  setFlag(QGraphicsItem::ItemIsSelectable, false);
  setAcceptedMouseButtons(Qt::NoButton);
  updatePath();
}

void LinkItem::updatePath() {
  if (src_ == nullptr || dest_ == nullptr) return;
  // Item coordinates equal scene coords for top-level links at (0,0).
  const QPointF a = src_->mapToScene(src_->rect().center());
  const QPointF b = dest_->mapToScene(dest_->rect().center());
  QPainterPath path(a);
  path.lineTo(b);
  // Arrow head at the destination end (directed connection).
  const QPointF dir = b - a;
  const qreal len = std::hypot(dir.x(), dir.y());
  if (len > 1e-6) {
    const QPointF u = dir / len;
    const QPointF n(-u.y(), u.x());
    const QPointF tip = b - u * 10;
    path.moveTo(tip + n * 6);
    path.lineTo(b - u * 2);
    path.lineTo(tip - n * 6);
    path.closeSubpath();
  }
  setPath(path);
}
