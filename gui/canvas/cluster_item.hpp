#pragma once

#include <QGraphicsRectItem>
#include <QGraphicsSimpleTextItem>
#include <QString>

#include <functional>

class NodeItem;

class ClusterItem : public QGraphicsRectItem {
public:
  static constexpr int Type = UserType + 1;
  int type() const override { return Type; }

  ClusterItem(const QString& name, QGraphicsItem* parent = nullptr);

  [[nodiscard]] QString clusterName() const { return name_; }
  void setClusterName(const QString& name);

  void layoutNodes();

  // Called when this cluster (or a child node) moves, so links can refresh.
  void setLinkUpdateCallback(std::function<void()> cb) {
    linkUpdate_ = std::move(cb);
  }

protected:
  QVariant itemChange(GraphicsItemChange change,
                      const QVariant& value) override;

private:
  QString name_;
  QGraphicsSimpleTextItem* title_ = nullptr;
  std::function<void()> linkUpdate_;
};
