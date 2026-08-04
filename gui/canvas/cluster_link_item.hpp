/**
 * @file cluster_link_item.hpp
 * @brief Directed edge between two cluster windows on the Structure canvas.
 */

#pragma once

#include <QGraphicsPathItem>

class ClusterItem;

/**
 * @brief Visual cluster→cluster link inferred from node connections.
 *
 * Anchors to the side midpoint (top, bottom, left, or right) of each cluster
 * that faces the other, chosen by the dominant center-to-center axis. Not
 * interactive (does not accept mouse buttons).
 */
class ClusterLinkItem : public QGraphicsPathItem {
public:
  static constexpr int Type = UserType + 4;
  int type() const override { return Type; }

  ClusterLinkItem(ClusterItem* src,
                  ClusterItem* dest,
                  QGraphicsItem* parent = nullptr);

  void updatePath();
  [[nodiscard]] ClusterItem* src() const { return src_; }
  [[nodiscard]] ClusterItem* dest() const { return dest_; }

private:
  ClusterItem* src_ = nullptr;
  ClusterItem* dest_ = nullptr;
};
