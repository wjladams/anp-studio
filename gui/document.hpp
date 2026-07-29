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

  /** @return The network currently being edited (may be a subnetwork). */
  [[nodiscard]] anpcpp::AnpNetwork& network();
  /** @brief Const overload of @ref network. */
  [[nodiscard]] const anpcpp::AnpNetwork& network() const;
  /** @return The root (top-level) network. */
  [[nodiscard]] anpcpp::AnpNetwork& root();
  /** @brief Const overload of @ref root. */
  [[nodiscard]] const anpcpp::AnpNetwork& root() const;

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

  /** @brief Descends into a node's subnetwork for editing. */
  void pushSubnet(const QString& nodeName);
  /** @brief Returns to the parent network in the subnet stack. */
  void popSubnet();
  /** @brief Clears the subnet stack back to the root network. */
  void popToRoot();
  /** @return Number of subnet frames above root (0 at root). */
  [[nodiscard]] int subnetDepth() const;
  /** @return Breadcrumb trail of host node names for the current view. */
  [[nodiscard]] QStringList breadcrumb() const;

  /** @brief Marks the document dirty and emits @ref modelChanged. */
  void notifyChanged();

signals:
  /** @brief Emitted when the model structure or data changes. */
  void modelChanged();
  /** @brief Emitted when the dirty flag changes. */
  void dirtyChanged(bool dirty);
  /** @brief Emitted when the file path changes. */
  void pathChanged(const QString& path);
  /** @brief Emitted when the active network view changes (subnet navigation). */
  void viewNetworkChanged();

private:
  struct Frame {
    anpcpp::AnpNetwork* net = nullptr;
    QString hostNode;
  };

  void replaceRoot(std::unique_ptr<anpcpp::AnpNetwork> net);

  std::unique_ptr<anpcpp::AnpNetwork> root_;
  std::vector<Frame> stack_;
  QUndoStack undo_;
  QString path_;
  bool dirty_ = false;
};
