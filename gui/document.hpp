/**
 * @file document.hpp
 * @brief Application document: network model, undo, and file state.
 */

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUndoStack>
#include <QVector>
#include <memory>
#include <vector>

#include "anpcpp/network.hpp"

/**
 * @brief One Researcher notebook cell persisted with the Studio document.
 */
struct ResearcherCell {
  QString command;
  QString html;
  bool ok = false;
};

/**
 * @brief One named Researcher notebook (independent cell history).
 */
struct ResearcherNotebook {
  QString name;
  QVector<ResearcherCell> cells;
};

/**
 * @brief Owns the root @c anpcpp::AnpNetwork, undo stack, file path, and dirty flag.
 *
 * Subnet navigation maintains a stack of @c AnpNetwork* frames pointing into
 * nodes' owned subnetworks.
 *
 * File I/O wraps libanpcpp network JSON and may include an optional top-level
 * @c researcher notebook block (Studio extension; ignored by older tools).
 */
class Document : public QObject {
  Q_OBJECT
public:
  explicit Document(QObject* parent = nullptr);
  ~Document() override;

  [[nodiscard]] anpcpp::AnpNetwork& network();
  [[nodiscard]] const anpcpp::AnpNetwork& network() const;
  [[nodiscard]] anpcpp::AnpNetwork& root();
  [[nodiscard]] const anpcpp::AnpNetwork& root() const;
  [[nodiscard]] anpcpp::AnpNetwork* parentNetwork();
  [[nodiscard]] const anpcpp::AnpNetwork* parentNetwork() const;

  [[nodiscard]] QUndoStack* undoStack() { return &undo_; }

  [[nodiscard]] QString path() const { return path_; }
  [[nodiscard]] bool isDirty() const { return dirty_; }
  void setDirty(bool dirty);

  void newNetwork(bool create_alts = true);
  bool loadFromFile(const QString& path, QString* error = nullptr);
  bool saveToFile(const QString& path, QString* error = nullptr);
  void clearPath();

  /** @return All Researcher notebooks (may be empty). */
  [[nodiscard]] const QVector<ResearcherNotebook>& researcherNotebooks() const {
    return researcherNotebooks_;
  }
  /** @return Index of the active notebook (0 if empty). */
  [[nodiscard]] int researcherActiveIndex() const {
    return researcherActiveIndex_;
  }
  /**
   * @brief Replaces the Researcher session (does not emit session changed).
   *
   * Callers that edit notebooks should also @ref setDirty.
   */
  void setResearcherSession(QVector<ResearcherNotebook> notebooks,
                            int activeIndex);
  /** @brief Clears Researcher notebooks without emitting session changed. */
  void clearResearcherSession();

  void pushSubnet(const QString& nodeName);
  void popSubnet();
  void popToRoot();
  void popToDepth(int depth);
  [[nodiscard]] int subnetDepth() const;
  [[nodiscard]] QStringList breadcrumb() const;
  [[nodiscard]] QStringList networkPathOptions() const;
  [[nodiscard]] QString currentNetworkPath() const;
  bool navigateToNetworkPath(const QString& path);
  /**
   * @brief Returns the network at a breadcrumb path without changing the view.
   * @param path e.g. @c "Root" or @c "Root / Node" (@ref networkPathOptions).
   * @return Network pointer, or nullptr if @p path is invalid.
   */
  [[nodiscard]] anpcpp::AnpNetwork* networkAtPath(const QString& path) const;

  void notifyChanged();
  void flushModelChanged();

  void setSuppressLayoutPersist(bool suppress) {
    suppressLayoutPersist_ = suppress;
  }
  [[nodiscard]] bool suppressLayoutPersist() const {
    return suppressLayoutPersist_;
  }

  [[nodiscard]] QString selectedCluster() const { return selectedCluster_; }
  [[nodiscard]] QString selectedNode() const { return selectedNode_; }
  void setSelection(const QString& cluster, const QString& node);

  [[nodiscard]] bool hasResults() const { return hasResults_; }
  [[nodiscard]] bool resultsStale() const {
    return hasResults_ && resultsStale_;
  }
  void markResultsCurrent();
  void invalidateResults();

signals:
  void modelChanged();
  void dirtyChanged(bool dirty);
  void pathChanged(const QString& path);
  void viewNetworkChanged();
  void selectionChanged(const QString& cluster, const QString& node);
  void resultsFreshnessChanged();
  /**
   * @brief Emitted when the Researcher session is replaced (load / new).
   *
   * Not emitted for incremental edits via @ref setResearcherSession.
   */
  void researcherSessionChanged();

private:
  struct Frame {
    anpcpp::AnpNetwork* net = nullptr;
    QString hostNode;
  };

  void replaceRoot(std::unique_ptr<anpcpp::AnpNetwork> net);
  void clearSelectionIfInvalid();
  void queueModelChanged();
  void emitViewSwitch();

  struct ResearcherSessionData {
    QVector<ResearcherNotebook> notebooks;
    int activeIndex = 0;
  };

  [[nodiscard]] static ResearcherSessionData parseResearcherSession(
      const QByteArray& fileBytes);
  [[nodiscard]] QByteArray buildFileBytes() const;
  [[nodiscard]] bool researcherSessionIsTrivial() const;

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
  QVector<ResearcherNotebook> researcherNotebooks_;
  int researcherActiveIndex_ = 0;
};
