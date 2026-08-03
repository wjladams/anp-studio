/**
 * @file node_item.hpp
 * @brief Graphics item for a node row inside a cluster on the Structure canvas.
 */

#pragma once

#include <QGraphicsRectItem>
#include <QGraphicsSimpleTextItem>
#include <QString>

#include <functional>

class ClusterItem;

class NodeItem : public QGraphicsRectItem {
public:
  static constexpr int Type = UserType + 2;
  int type() const override { return Type; }

  NodeItem(const QString& name, QGraphicsItem* parent = nullptr);

  [[nodiscard]] QString nodeName() const { return name_; }
  void setNodeName(const QString& name);
  void setHasSubnet(bool has);
  void setInvert(bool invert);

  void setRowGeometry(qreal width, qreal height);

  void setLinkUpdateCallback(std::function<void()> cb) {
    linkUpdate_ = std::move(cb);
  }

  // Called when a vertical reorder completes (fromIndex -> toIndex).
  void setReorderCallback(
      std::function<void(const QString& node, int from, int to)> cb) {
    reorderCb_ = std::move(cb);
  }

  /** @brief Invoked on double-click to rename inline. */
  void setRenameCallback(std::function<void(const QString& node)> cb) {
    renameCb_ = std::move(cb);
  }

  /** @brief Shows or hides the painted label (e.g. while editing). */
  void setLabelVisible(bool visible);

  /** @brief Marks this node as a Connection-mode source (multi-select highlight). */
  void setConnectionSource(bool on);
  [[nodiscard]] bool isConnectionSource() const { return connectionSource_; }

protected:
  void paint(QPainter* painter,
             const QStyleOptionGraphicsItem* option,
             QWidget* widget = nullptr) override;
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
  QVariant itemChange(GraphicsItemChange change,
                      const QVariant& value) override;

private:
  [[nodiscard]] ClusterItem* parentCluster() const;
  void refreshLook();

  QString name_;
  QGraphicsSimpleTextItem* label_ = nullptr;
  bool hasSubnet_ = false;
  bool invert_ = false;
  bool connectionSource_ = false;
  std::function<void()> linkUpdate_;
  std::function<void(const QString&, int, int)> reorderCb_;
  std::function<void(const QString&)> renameCb_;

  bool reordering_ = false;
  int fromIndex_ = -1;
  QPointF pressPos_;
};
