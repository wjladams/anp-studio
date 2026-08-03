/**
 * @file link_item.hpp
 * @brief Graphics item for a directed node→node link on the Structure canvas.
 */

#pragma once

#include <QGraphicsPathItem>
#include <QPointer>

class NodeItem;

class LinkItem : public QGraphicsPathItem {
public:
  static constexpr int Type = UserType + 3;
  int type() const override { return Type; }

  LinkItem(NodeItem* src, NodeItem* dest, QGraphicsItem* parent = nullptr);

  void updatePath();
  [[nodiscard]] NodeItem* src() const { return src_; }
  [[nodiscard]] NodeItem* dest() const { return dest_; }

  // Track index for intra-cluster loops so several loops don't overlap.
  void setLoopLane(int lane);
  [[nodiscard]] bool isIntraCluster() const { return sameCluster(); }

private:
  [[nodiscard]] bool sameCluster() const;
  [[nodiscard]] QPainterPath loopPath() const;

  NodeItem* src_ = nullptr;
  NodeItem* dest_ = nullptr;
  int loopLane_ = 0;
};
