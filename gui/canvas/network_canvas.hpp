/**
 * @file network_canvas.hpp
 * @brief Interactive graphics view for the ANP network diagram.
 */

#pragma once

#include <QGraphicsView>
#include <QHash>
#include <QSet>
#include <QString>

#include <functional>

class Document;
class ClusterItem;
class NodeItem;
class LinkItem;
class QToolButton;
class QGraphicsProxyWidget;
class QGraphicsItem;
class QKeyEvent;

/**
 * @brief Visual editor for clusters, nodes, and connections.
 *
 * Supports Connection mode for multi-source link editing and persists layout
 * positions back to the @ref Document model.
 */
class NetworkCanvas : public QGraphicsView {
  Q_OBJECT
public:
  /**
   * @param doc Document whose network is displayed.
   * @param parent Optional parent widget.
   */
  explicit NetworkCanvas(Document* doc, QWidget* parent = nullptr);
  ~NetworkCanvas() override;

  /** @brief Rebuilds scene items from the current network. */
  void rebuild();
  /** @brief Enables or disables Connection mode. */
  void setConnectMode(bool on);
  /** @return True when Connection mode is active. */
  [[nodiscard]] bool connectMode() const { return connectMode_; }
  /** @brief Highlights a cluster/node from Document selection. */
  void select(const QString& cluster, const QString& node);

signals:
  /** @brief Emitted to open a node's subnetwork (e.g. from context menu). */
  void nodeActivated(const QString& name);
  /** @brief Emitted when the user selects a cluster or node on the canvas. */
  void selectionChanged(const QString& cluster, const QString& node);
  /** @brief Emitted when Connection mode is entered or left. */
  void connectModeChanged(bool on);

protected:
  /** @brief Shows context menu for add/connect operations (Normal mode only). */
  void contextMenuEvent(QContextMenuEvent* event) override;
  /** @brief Handles Connection-mode clicks and Normal-mode interaction. */
  void mousePressEvent(QMouseEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;

private:
  void persistLayout();
  void updateLinks();
  void positionAddClusterButton();
  void promptAddCluster();
  void promptAddNode(const QString& clusterName);
  [[nodiscard]] QString uniqueClusterName() const;
  [[nodiscard]] QString uniqueNodeName() const;
  [[nodiscard]] bool isNodeNameAvailable(const QString& name,
                                         const QString& exceptName = {}) const;
  [[nodiscard]] bool isClusterNameAvailable(const QString& name,
                                            const QString& exceptName = {}) const;
  void startInlineRenameCluster(const QString& cluster);
  void startInlineRenameNode(const QString& node);
  void beginInlineRename(QGraphicsItem* parentItem,
                         const QRectF& localRect,
                         const QString& oldName,
                         std::function<void()> onShow,
                         std::function<void()> onHide,
                         std::function<void(const QString&)> onCommit);
  /** @param viaEnter True for Return/Enter; false for Escape or focus-loss. */
  void finishInlineRename(bool accept, bool viaEnter);
  void cancelInlineRename();

  [[nodiscard]] NodeItem* nodeItemAt(const QPoint& viewPos) const;
  [[nodiscard]] ClusterItem* clusterItemAt(const QPoint& viewPos) const;

  void clearConnectionSources();
  void setSoleConnectionSource(const QString& node);
  void toggleConnectionSource(const QString& node);
  void setConnectionSourcesFromCluster(ClusterItem* cluster, bool unionWith);
  void refreshConnectionVisuals();
  void syncDocumentSelectionFromSources();
  void batchToggleToDestinations(const QList<QString>& dests);
  void handleConnectionLeftClick(QMouseEvent* event);
  void handleConnectionRightClick(QMouseEvent* event);

  Document* doc_ = nullptr;
  QGraphicsScene* scene_ = nullptr;
  QToolButton* addClusterBtn_ = nullptr;
  QHash<QString, ClusterItem*> clusters_;
  QHash<QString, NodeItem*> nodes_;
  QList<LinkItem*> links_;
  bool connectMode_ = false;
  bool rebuilding_ = false;
  QSet<QString> connectionSources_;
  QString connectionPrimary_;
  QGraphicsProxyWidget* renameProxy_ = nullptr;
  std::function<void()> renameHideCb_;
  std::function<void(const QString&)> renameOnCommit_;
  QString renameOldName_;
  /** Non-empty while Add Node should chain another create after Enter. */
  QString nodeCreateChainCluster_;
  bool renameClosing_ = false;
  /** After Enter-chain, wait for KeyRelease before wiring returnPressed. */
  bool renameDeferReturnPressed_ = false;
  /** True once returnPressed is connected for the current editor. */
  bool renameReturnWired_ = false;
  /** True when this rename is for a node just created by promptAddNode. */
  bool renameAbortRemovesNode_ = false;
  /** True when the active inline rename target is a node (false = cluster). */
  bool renameIsNode_ = true;
};
