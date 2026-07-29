#include "canvas/cluster_item.hpp"

#include "canvas/node_item.hpp"

#include <QBrush>
#include <QFont>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPen>
#include <QStyle>
#include <QStyleOptionGraphicsItem>
#include <algorithm>

namespace {
constexpr qreal kAddBtn = 20.0;
}  // namespace

ClusterItem::ClusterItem(const QString& name, QGraphicsItem* parent)
    : QGraphicsRectItem(parent), name_(name) {
  setRect(0, 0, kWidth, kTitleH + kMinBodyH);
  setBrush(QBrush(QColor(245, 248, 252)));
  setPen(QPen(QColor(60, 90, 130), 2));
  // Not ItemIsMovable — only the title bar initiates a drag.
  setFlags(ItemIsSelectable | ItemSendsGeometryChanges);
  title_ = new QGraphicsSimpleTextItem(name_, this);
  title_->setPos(10, 7);
  title_->setBrush(QColor(255, 255, 255));
  title_->setZValue(2);
  title_->setAcceptedMouseButtons(Qt::NoButton);
  layoutNodes();
}

QRectF ClusterItem::addNodeButtonRect() const {
  return QRectF(kWidth - kPad - kAddBtn, (kTitleH - kAddBtn) * 0.5, kAddBtn,
                kAddBtn);
}

void ClusterItem::setClusterName(const QString& name) {
  name_ = name;
  title_->setText(name_);
}

std::vector<NodeItem*> ClusterItem::nodeItems() const {
  // Child order matches model order (nodes are added in node_names order).
  std::vector<NodeItem*> out;
  for (QGraphicsItem* child : childItems()) {
    if (auto* node = qgraphicsitem_cast<NodeItem*>(child)) {
      out.push_back(node);
    }
  }
  return out;
}

int ClusterItem::indexOf(const NodeItem* node) const {
  const auto items = nodeItems();
  for (std::size_t i = 0; i < items.size(); ++i) {
    if (items[i] == node) return static_cast<int>(i);
  }
  return -1;
}

int ClusterItem::dropIndexForY(qreal y) const {
  const int n = static_cast<int>(nodeItems().size());
  if (n <= 0) return 0;
  const qreal bodyTop = kTitleH + kPad;
  const qreal stride = kRowH + kRowGap;
  if (y <= bodyTop) return 0;
  for (int i = 0; i < n; ++i) {
    const qreal mid = bodyTop + i * stride + kRowH * 0.5;
    if (y < mid) return i;
  }
  return n - 1;
}

void ClusterItem::layoutNodes(NodeItem* dragging, qreal dragY) {
  const auto items = nodeItems();
  const qreal rowW = kWidth - 2 * kPad;
  const qreal x = kPad;

  if (dragging == nullptr) {
    qreal y = kTitleH + kPad;
    for (NodeItem* n : items) {
      n->setRowGeometry(rowW, kRowH);
      n->setPos(x, y);
      y += kRowH + kRowGap;
    }
    const qreal bottom =
        items.empty() ? kTitleH + kMinBodyH : y - kRowGap + kPad;
    setRect(0, 0, kWidth, std::max(kTitleH + kMinBodyH, bottom));
    return;
  }

  // Live reorder preview: stack siblings, leave a gap at the drop slot,
  // and place the dragged row at the pointer Y.
  std::vector<NodeItem*> others;
  others.reserve(items.size());
  for (NodeItem* n : items) {
    if (n != dragging) others.push_back(n);
  }

  int drop = 0;
  {
    qreal probe = kTitleH + kPad;
    drop = static_cast<int>(others.size());
    for (int i = 0; i < static_cast<int>(others.size()); ++i) {
      if (dragY < probe + kRowH * 0.5) {
        drop = i;
        break;
      }
      probe += kRowH + kRowGap;
    }
  }

  qreal y = kTitleH + kPad;
  for (int i = 0; i <= static_cast<int>(others.size()); ++i) {
    if (i == drop) {
      y += kRowH + kRowGap;  // reserved gap for the dragged row
    }
    if (i < static_cast<int>(others.size())) {
      others[static_cast<std::size_t>(i)]->setRowGeometry(rowW, kRowH);
      others[static_cast<std::size_t>(i)]->setPos(x, y);
      y += kRowH + kRowGap;
    }
  }

  const qreal minY = kTitleH + kPad;
  const qreal maxY = std::max(minY, rect().height() - kPad - kRowH);
  const qreal dy = std::clamp(dragY, minY, maxY);
  dragging->setRowGeometry(rowW, kRowH);
  dragging->setPos(x, dy);

  const qreal contentBottom =
      std::max(y - kRowGap, dy + kRowH) + kPad;
  setRect(0, 0, kWidth, std::max(kTitleH + kMinBodyH, contentBottom));
}

void ClusterItem::paint(QPainter* painter,
                        const QStyleOptionGraphicsItem* option,
                        QWidget* widget) {
  Q_UNUSED(widget);
  const QRectF r = rect();
  const bool selected =
      (option->state & QStyle::State_Selected) || isSelected();

  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setPen(pen());
  painter->setBrush(brush());
  painter->drawRoundedRect(r, 4, 4);

  // Title bar strip (drag handle for the whole cluster).
  QColor titleFill = selected ? QColor(30, 80, 140) : QColor(60, 100, 150);
  QRectF titleRect(r.left(), r.top(), r.width(), kTitleH);
  painter->save();
  painter->setPen(Qt::NoPen);
  painter->setBrush(titleFill);
  painter->setClipRect(titleRect.adjusted(0, 0, 0, 1));
  painter->drawRoundedRect(r, 4, 4);
  painter->fillRect(QRectF(r.left(), r.top() + 4, r.width(), kTitleH - 4),
                    titleFill);
  painter->restore();

  // Title-bar "+" to add a node (right-justified).
  const QRectF addRect = addNodeButtonRect();
  painter->save();
  painter->setPen(QPen(QColor(255, 255, 255, 220), 1.2));
  painter->setBrush(QColor(255, 255, 255, 35));
  painter->drawRoundedRect(addRect, 4, 4);
  QFont plusFont = painter->font();
  plusFont.setBold(true);
  plusFont.setPointSizeF(12);
  painter->setFont(plusFont);
  painter->setPen(QColor(255, 255, 255));
  painter->drawText(addRect, Qt::AlignCenter, QStringLiteral("+"));
  painter->restore();

  painter->setBrush(Qt::NoBrush);
  painter->setPen(pen());
  painter->drawRoundedRect(r, 4, 4);
}

void ClusterItem::mousePressEvent(QGraphicsSceneMouseEvent* event) {
  if (event->button() == Qt::LeftButton &&
      addNodeButtonRect().contains(event->pos())) {
    if (addNodeCb_) {
      addNodeCb_();
    }
    event->accept();
    return;
  }
  if (event->button() == Qt::LeftButton && event->pos().y() <= kTitleH) {
    draggingTitle_ = true;
    dragGrabOffset_ = event->pos();
    setSelected(true);
    event->accept();
    return;
  }
  draggingTitle_ = false;
  QGraphicsRectItem::mousePressEvent(event);
}

void ClusterItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
  if (draggingTitle_) {
    setPos(event->scenePos() - dragGrabOffset_);
    event->accept();
    return;
  }
  QGraphicsRectItem::mouseMoveEvent(event);
}

void ClusterItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
  if (draggingTitle_) {
    draggingTitle_ = false;
    event->accept();
    return;
  }
  QGraphicsRectItem::mouseReleaseEvent(event);
}

QVariant ClusterItem::itemChange(GraphicsItemChange change,
                                 const QVariant& value) {
  if (change == ItemPositionHasChanged && linkUpdate_) {
    linkUpdate_();
  }
  return QGraphicsRectItem::itemChange(change, value);
}
