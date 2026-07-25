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

private:
  NodeItem* src_ = nullptr;
  NodeItem* dest_ = nullptr;
};
