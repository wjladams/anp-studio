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

enum class Side { Left, Right, Top, Bottom };

/** Side of @p fromC that faces @p towardC (dominant axis of the vector). */
Side facingSide(const QPointF& fromC, const QPointF& towardC) {
  const qreal dx = towardC.x() - fromC.x();
  const qreal dy = towardC.y() - fromC.y();
  if (std::abs(dx) >= std::abs(dy)) {
    return dx >= 0.0 ? Side::Right : Side::Left;
  }
  return dy >= 0.0 ? Side::Bottom : Side::Top;
}

QPointF sideAnchor(const ClusterItem* cluster, Side side) {
  const QRectF r = cluster->rect();
  QPointF local;
  switch (side) {
    case Side::Left:
      local = QPointF(r.left(), r.center().y());
      break;
    case Side::Right:
      local = QPointF(r.right(), r.center().y());
      break;
    case Side::Top:
      local = QPointF(r.center().x(), r.top());
      break;
    case Side::Bottom:
      local = QPointF(r.center().x(), r.bottom());
      break;
  }
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

  const QPointF a = sideAnchor(src_, facingSide(srcC, destC));
  const QPointF b = sideAnchor(dest_, facingSide(destC, srcC));

  QPainterPath path(a);
  path.lineTo(b);

  const QPointF dir = b - a;
  const qreal len = std::hypot(dir.x(), dir.y());
  if (len > 1e-6) {
    addArrowHead(path, b, dir / len);
  }
  setPath(path);
}
