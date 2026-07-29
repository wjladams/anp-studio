/**
 * @file network_canvas.hpp
 * @brief Interactive graphics view for the ANP network diagram.
 */

#pragma once

#include <QGraphicsView>
#include <QHash>
#include <QString>

class Document;
class ClusterItem;
class NodeItem;
class LinkItem;
class QToolButton;

/**
 * @brief Visual editor for clusters, nodes, and connections.
 *
 * Supports connect mode for drawing links and persists layout positions
 * back to the @ref Document model.
 */
class NetworkCanvas : public QGraphicsView {
  Q_OBJECT
public:
  /**
   * @param doc Document whose network is displayed.
   * @param parent Optional parent widget.
   */
  explicit NetworkCanvas(Document* doc, QWidget* parent = nullptr);

  /** @brief Rebuilds scene items from the current network. */
  void rebuild();
  /** @brief Enables or disables click-to-connect link mode. */
  void setConnectMode(bool on);
  /** @return True when connect mode is active. */
  [[nodiscard]] bool connectMode() const { return connectMode_; }
  /** @brief Highlights a cluster/node from Document selection. */
  void select(const QString& cluster, const QString& node);

signals:
  /** @brief Emitted when a node is double-clicked (e.g. to open subnetwork). */
  void nodeActivated(const QString& name);
  /** @brief Emitted when the user selects a cluster or node on the canvas. */
  void selectionChanged(const QString& cluster, const QString& node);

protected:
  /** @brief Shows context menu for add/connect operations. */
  void contextMenuEvent(QContextMenuEvent* event) override;
  /** @brief Handles node selection and connect-mode clicks. */
  void mousePressEvent(QMouseEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;

private:
  void persistLayout();
  void updateLinks();
  void positionAddClusterButton();
  void promptAddCluster();
  void promptAddNode(const QString& clusterName);
  NodeItem* nodeItemAt(const QPoint& viewPos) const;

  Document* doc_ = nullptr;
  QGraphicsScene* scene_ = nullptr;
  QToolButton* addClusterBtn_ = nullptr;
  QHash<QString, ClusterItem*> clusters_;
  QHash<QString, NodeItem*> nodes_;
  QList<LinkItem*> links_;
  bool connectMode_ = false;
  QString connectSrc_;
};
