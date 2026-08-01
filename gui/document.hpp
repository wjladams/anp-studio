/**
 * @file document.hpp
 * @brief Application document: network model, undo, and file state.
 */

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUndoStack>
#include <memory>
#include <vector>

#include "anpcpp/network.hpp"

/**
 * @brief Owns the root @c anpcpp::AnpNetwork, undo stack, file path, and dirty flag.
 *
 * Subnet navigation maintains a stack of @c AnpNetwork* frames pointing into
 * nodes' owned subnetworks.
 */
class Document : public QObject {
  Q_OBJECT
public:
  /**
   * @param parent Optional Qt parent object.
   */
  explicit Document(QObject* parent = nullptr);
  ~Document() override;

  /** @return The network currently being edited (may be a subnetwork). */
  [[nodiscard]] anpcpp::AnpNetwork& network();
  /** @brief Const overload of @ref network. */
  [[nodiscard]] const anpcpp::AnpNetwork& network() const;
  /** @return The root (top-level) network. */
  [[nodiscard]] anpcpp::AnpNetwork& root();
  /** @brief Const overload of @ref root. */
  [[nodiscard]] const anpcpp::AnpNetwork& root() const;
  /**
   * @return Parent network in the subnet stack, or @c nullptr at root.
   *
   * Used by the Researcher stage as @c parentModel.
   */
  [[nodiscard]] anpcpp::AnpNetwork* parentNetwork();
  /** @brief Const overload of @ref parentNetwork. */
  [[nodiscard]] const anpcpp::AnpNetwork* parentNetwork() const;

  /** @return Undo/redo command stack for model edits. */
  [[nodiscard]] QUndoStack* undoStack() { return &undo_; }

  /** @return Current file path (empty if unsaved). */
  [[nodiscard]] QString path() const { return path_; }
  /** @return True if there are unsaved changes. */
  [[nodiscard]] bool isDirty() const { return dirty_; }
  /** @brief Sets the dirty flag and emits @ref dirtyChanged when it changes. */
  void setDirty(bool dirty);

  /** @brief Replaces the model with a new empty network. */
  void newNetwork(bool create_alts = true);
  /**
   * @brief Loads a network from JSON file.
   * @param path File to read.
   * @param error Optional error message output.
   * @return False on failure.
   */
  bool loadFromFile(const QString& path, QString* error = nullptr);
  /**
   * @brief Saves the root network to JSON.
   * @param path Output path.
   * @param error Optional error message output.
   * @return False on failure.
   */
  bool saveToFile(const QString& path, QString* error = nullptr);
  /**
   * @brief Clears the associated file path (e.g. after opening a sample).
   *
   * Leaves the in-memory model unchanged so the next Save prompts Save As.
   */
  void clearPath();

  /** @brief Descends into a node's subnetwork for editing. */
  void pushSubnet(const QString& nodeName);
  /** @brief Returns to the parent network in the subnet stack. */
  void popSubnet();
  /** @brief Clears the subnet stack back to the root network. */
  void popToRoot();
  /**
   * @brief Pops the subnet stack to @p depth frames (1 = root only).
   * @param depth Target stack size; values &lt; 1 are treated as 1.
   */
  void popToDepth(int depth);
  /** @return Number of subnet frames above root (0 at root). */
  [[nodiscard]] int subnetDepth() const;
  /** @return Breadcrumb trail of host node names for the current view. */
  [[nodiscard]] QStringList breadcrumb() const;

  /**
   * @return All reachable network paths ("Root", "Root / Host", …).
   */
  [[nodiscard]] QStringList networkPathOptions() const;
  /** @return Current path joined with " / ". */
  [[nodiscard]] QString currentNetworkPath() const;
  /**
   * @brief Navigates the subnet stack to @p path.
   * @return False if the path is invalid.
   */
  bool navigateToNetworkPath(const QString& path);

  /**
   * @brief Invalidates results and schedules @ref modelChanged.
   *
   * Multiple calls in the same event-loop turn coalesce into one emission so
   * undo macros and command+indexChanged pairs do not thrash the UI.
   */
  void notifyChanged();

  /**
   * @brief Emits any pending coalesced @ref modelChanged immediately.
   *
   * Use after an undo push when the caller must read updated canvas/UI state
   * before returning to the event loop.
   */
  void flushModelChanged();

  /**
   * @brief When true, NetworkCanvas::rebuild skips persisting item positions
   *        into the model (used when positions were just written by a command).
   */
  void setSuppressLayoutPersist(bool suppress) {
    suppressLayoutPersist_ = suppress;
  }
  /** @return True when canvas rebuild should not persist layout. */
  [[nodiscard]] bool suppressLayoutPersist() const {
    return suppressLayoutPersist_;
  }

  /** @return Selected cluster name (may be empty). */
  [[nodiscard]] QString selectedCluster() const { return selectedCluster_; }
  /** @return Selected node name (may be empty). */
  [[nodiscard]] QString selectedNode() const { return selectedNode_; }
  /**
   * @brief Soft-persists structure selection across stages.
   * @param cluster Selected cluster (empty if node-only).
   * @param node Selected node (empty if cluster-only).
   */
  void setSelection(const QString& cluster, const QString& node);

  /** @return True if Calculate has produced a results snapshot. */
  [[nodiscard]] bool hasResults() const { return hasResults_; }
  /** @return True if the model changed since the last successful Calculate. */
  [[nodiscard]] bool resultsStale() const {
    return hasResults_ && resultsStale_;
  }
  /** @brief Marks results current after a successful Calculate. */
  void markResultsCurrent();
  /** @brief Marks results stale (or clears hasResults if never calculated). */
  void invalidateResults();

signals:
  /** @brief Emitted when the model structure or data changes. */
  void modelChanged();
  /** @brief Emitted when the dirty flag changes. */
  void dirtyChanged(bool dirty);
  /** @brief Emitted when the file path changes. */
  void pathChanged(const QString& path);
  /** @brief Emitted when the active network view changes (subnet navigation). */
  void viewNetworkChanged();
  /** @brief Emitted when soft selection changes. */
  void selectionChanged(const QString& cluster, const QString& node);
  /** @brief Emitted when calc freshness changes. */
  void resultsFreshnessChanged();

private:
  struct Frame {
    anpcpp::AnpNetwork* net = nullptr;
    QString hostNode;
  };

  void replaceRoot(std::unique_ptr<anpcpp::AnpNetwork> net);
  void clearSelectionIfInvalid();
  void queueModelChanged();

  std::unique_ptr<anpcpp::AnpNetwork> root_;
  std::vector<Frame> stack_;
  QUndoStack undo_;
  QString path_;
  bool dirty_ = false;
  bool suppressLayoutPersist_ = false;
  bool modelChangedQueued_ = false;
  QString selectedCluster_;
  QString selectedNode_;
  bool hasResults_ = false;
  bool resultsStale_ = false;
};
