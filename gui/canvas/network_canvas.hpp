#pragma once

#include <QGraphicsView>
#include <QHash>
#include <QString>

class Document;
class ClusterItem;
class NodeItem;
class LinkItem;

class NetworkCanvas : public QGraphicsView {
  Q_OBJECT
public:
  explicit NetworkCanvas(Document* doc, QWidget* parent = nullptr);

  void rebuild();
  void setConnectMode(bool on);
  [[nodiscard]] bool connectMode() const { return connectMode_; }

signals:
  void nodeActivated(const QString& name);
  void selectionChanged(const QString& cluster, const QString& node);

protected:
  void contextMenuEvent(QContextMenuEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;

private:
  void persistLayout();
  void updateLinks();
  NodeItem* nodeItemAt(const QPoint& viewPos) const;

  Document* doc_ = nullptr;
  QGraphicsScene* scene_ = nullptr;
  QHash<QString, ClusterItem*> clusters_;
  QHash<QString, NodeItem*> nodes_;
  QList<LinkItem*> links_;
  bool connectMode_ = false;
  QString connectSrc_;
};
