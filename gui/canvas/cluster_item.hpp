/**
 * @file cluster_item.hpp
 * @brief Graphics item for a cluster window on the Structure canvas.
 */

#pragma once

#include <QGraphicsRectItem>
#include <QGraphicsSimpleTextItem>
#include <QString>

#include <functional>
#include <vector>

class NodeItem;

class ClusterItem : public QGraphicsRectItem {
public:
  static constexpr int Type = UserType + 1;
  int type() const override { return Type; }

  static constexpr qreal kWidth = 220.0;
  static constexpr qreal kTitleH = 28.0;
  static constexpr qreal kPad = 8.0;
  static constexpr qreal kRowH = 28.0;
  static constexpr qreal kRowGap = 4.0;
  static constexpr qreal kMinBodyH = 36.0;

  ClusterItem(const QString& name, QGraphicsItem* parent = nullptr);

  [[nodiscard]] QString clusterName() const { return name_; }
  void setClusterName(const QString& name);

  // Stack node rows under the title bar. Pass a dragging node + its preview Y
  // (in cluster coords) to leave a gap for live reorder feedback.
  void layoutNodes(NodeItem* dragging = nullptr, qreal dragY = 0.0);

  [[nodiscard]] qreal titleBarHeight() const { return kTitleH; }
  [[nodiscard]] int indexOf(const NodeItem* node) const;
  [[nodiscard]] int dropIndexForY(qreal y) const;
  [[nodiscard]] std::vector<NodeItem*> nodeItems() const;

  void setLinkUpdateCallback(std::function<void()> cb) {
    linkUpdate_ = std::move(cb);
  }

  /** @brief Invoked after a title-bar drag finishes (persist layout). */
  void setLayoutCommitCallback(std::function<void()> cb) {
    layoutCommit_ = std::move(cb);
  }

  /** @brief Invoked when the title-bar "+" is clicked (add node). */
  void setAddNodeCallback(std::function<void()> cb) {
    addNodeCb_ = std::move(cb);
  }

  /** @brief Invoked on title double-click to rename inline. */
  void setRenameCallback(std::function<void(const QString& cluster)> cb) {
    renameCb_ = std::move(cb);
  }

  /** @brief Shows or hides the title text (e.g. while editing). */
  void setTitleVisible(bool visible);

  /** @return Title text area excluding the "+" button. */
  [[nodiscard]] QRectF titleEditRect() const;

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
  void paint(QPainter* painter,
             const QStyleOptionGraphicsItem* option,
             QWidget* widget = nullptr) override;
  QVariant itemChange(GraphicsItemChange change,
                      const QVariant& value) override;

private:
  [[nodiscard]] QRectF addNodeButtonRect() const;

  QString name_;
  QGraphicsSimpleTextItem* title_ = nullptr;
  std::function<void()> linkUpdate_;
  std::function<void()> layoutCommit_;
  std::function<void()> addNodeCb_;
  std::function<void(const QString&)> renameCb_;
  bool draggingTitle_ = false;
  QPointF dragGrabOffset_;
};
