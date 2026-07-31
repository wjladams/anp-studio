/**
 * @file cluster_link_item.cpp
 * @brief Path geometry for cluster→cluster Structure edges.
 */

#include "canvas/cluster_link_item.hpp"

#include "canvas/cluster_item.hpp"

#include <QPainterPath>
#include <QPen>
#include <cmath>

namespace {

QPointF sideAnchor(const ClusterItem* cluster, bool preferRight) {
  const QRectF r = cluster->rect();
  const QPointF local = preferRight
                            ? QPointF(r.right(), r.center().y())
                            : QPointF(r.left(), r.center().y());
  return cluster->mapToScene(local);
}

void addArrowHead(QPainterPath& path, const QPointF& tipAt, const QPointF& u) {
  const QPointF n(-u.y(), u.x());
  const QPointF base = tipAt - u * 8;
  path.moveTo(base + n * 5);
  path.lineTo(tipAt);
  path.lineTo(base - n * 5);
  path.closeSubpath();
}

}  // namespace

ClusterLinkItem::ClusterLinkItem(ClusterItem* src,
                                 ClusterItem* dest,
                                 QGraphicsItem* parent)
    : QGraphicsPathItem(parent), src_(src), dest_(dest) {
  setPen(QPen(QColor(40, 70, 110), 2.0));
  setZValue(50);
  setFlag(QGraphicsItem::ItemIsSelectable, false);
  setAcceptedMouseButtons(Qt::NoButton);
  updatePath();
}

void ClusterLinkItem::updatePath() {
  if (src_ == nullptr || dest_ == nullptr) return;

  const QPointF srcC = src_->mapToScene(src_->rect().center());
  const QPointF destC = dest_->mapToScene(dest_->rect().center());

  const bool srcRight = srcC.x() <= destC.x();
  const bool destRight = destC.x() < srcC.x();
  const QPointF a = sideAnchor(src_, srcRight);
  const QPointF b = sideAnchor(dest_, destRight);

  QPainterPath path(a);
  path.lineTo(b);

  const QPointF dir = b - a;
  const qreal len = std::hypot(dir.x(), dir.y());
  if (len > 1e-6) {
    addArrowHead(path, b, dir / len);
  }
  setPath(path);
}
