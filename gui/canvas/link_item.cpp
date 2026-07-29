#include "canvas/link_item.hpp"

#include "canvas/cluster_item.hpp"
#include "canvas/node_item.hpp"

#include <QPainterPath>
#include <QPen>
#include <algorithm>
#include <cmath>

namespace {

// How far an intra-cluster link bows out past the cluster edge.
constexpr qreal kLoopMinBulge = 22.0;
constexpr qreal kLoopMaxBulge = 50.0;
constexpr qreal kLoopLaneStep = 13.0;
constexpr int kLoopLanes = 3;
constexpr qreal kLoopCorner = 8.0;

QPointF sideAnchor(const NodeItem* node, bool preferRight, qreal yOffset = 0.0) {
  const QRectF r = node->rect();
  const QPointF local = preferRight
                            ? QPointF(r.right(), r.center().y() + yOffset)
                            : QPointF(r.left(), r.center().y() + yOffset);
  return node->mapToScene(local);
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

void LinkItem::setLoopLane(int lane) {
  loopLane_ = lane;
  updatePath();
}

bool LinkItem::sameCluster() const {
  return src_ != nullptr && dest_ != nullptr &&
         src_->parentItem() != nullptr &&
         src_->parentItem() == dest_->parentItem();
}

QPainterPath LinkItem::loopPath() const {
  // Rounded-rectangle detour out the right edge of the cluster, so links
  // between rows of the same cluster don't run through the node boxes.
  const bool self = (src_ == dest_);
  const qreal selfOffset = self ? src_->rect().height() * 0.25 : 0.0;
  const QPointF a = sideAnchor(src_, /*preferRight=*/true, -selfOffset);
  const QPointF b = sideAnchor(dest_, /*preferRight=*/true, selfOffset);

  const qreal dy = b.y() - a.y();
  const qreal span = std::abs(dy);
  const int lane = ((loopLane_ % kLoopLanes) + kLoopLanes) % kLoopLanes;
  const qreal bulge = std::clamp(kLoopMinBulge + lane * kLoopLaneStep,
                                 kLoopMinBulge, kLoopMaxBulge);
  // Measure the loop track from the cluster border so it reads as an
  // external detour rather than a line grazing the node boxes.
  qreal edgeX = std::max(a.x(), b.x());
  if (auto* cluster = qgraphicsitem_cast<ClusterItem*>(src_->parentItem())) {
    edgeX = std::max(
        edgeX, cluster->mapToScene(cluster->rect().topRight()).x());
  }
  const qreal outX = edgeX + bulge;
  const qreal r = std::min({kLoopCorner, bulge * 0.5, span * 0.5});
  const qreal sgn = (dy >= 0.0) ? 1.0 : -1.0;

  QPainterPath path(a);
  path.lineTo(outX - r, a.y());
  path.quadTo(QPointF(outX, a.y()), QPointF(outX, a.y() + sgn * r));
  path.lineTo(outX, b.y() - sgn * r);
  path.quadTo(QPointF(outX, b.y()), QPointF(outX - r, b.y()));
  path.lineTo(b);
  // Arrow enters the destination row from the right, pointing left.
  addArrowHead(path, b, QPointF(-1.0, 0.0));
  return path;
}

void LinkItem::updatePath() {
  if (src_ == nullptr || dest_ == nullptr) return;

  if (sameCluster()) {
    setPath(loopPath());
    return;
  }

  const QPointF srcC = src_->mapToScene(src_->rect().center());
  const QPointF destC = dest_->mapToScene(dest_->rect().center());

  // Attach to facing side midpoints (box-within-box rows).
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
