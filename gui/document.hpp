/**
 * @file document.hpp
 * @brief Application document: network model, undo, and file state.
 */

#pragma once

#include <QByteArray>
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
 * @brief A Google Form created from this model (Studio extension; for reopen + import).
 *
 * @c structureFingerprint pins the form to the judgment question set at create
 * time. Changing nodes/links/alternatives/scales makes the form out of date
 * (compare via @c googleFormFingerprintMatches). Judgment *values* do not.
 */
struct LinkedGoogleForm {
  QString formId;
  QString title;
  QString responderUrl;
  QString editUrl;
  QString createdAtIso;  // Qt::ISODate
  /** SHA-256 hex of sorted [anp:…] judgment tags at create time. */
  QString structureFingerprint;
  /** Judgment tags present on the form (for diagnostics / matching). */
  QStringList questionTags;
  /**
   * Forms questionIds parallel to @c mappedTags (identity + judgments).
   * Empty for legacy forms — import falls back to [anp:] tags in titles.
   */
  QStringList questionIds;
  /** Tags aligned with @c questionIds (includes respondent / email tags). */
  QStringList mappedTags;
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
   * Callers that edit notebooks should also call @c setDirty().
   */
  void setResearcherSession(QVector<ResearcherNotebook> notebooks,
                            int activeIndex);
  /** @brief Clears Researcher notebooks without emitting session changed. */
  void clearResearcherSession();

  /** @return Google Forms linked to this model (newest last). */
  [[nodiscard]] const QVector<LinkedGoogleForm>& linkedGoogleForms() const {
    return linkedGoogleForms_;
  }
  /** @brief Appends a linked form and marks dirty. Emits @ref linkedFormsChanged. */
  void addLinkedGoogleForm(const LinkedGoogleForm& form);
  /** @brief Removes by form id. */
  void removeLinkedGoogleForm(const QString& formId);
  /** @return Most recently linked form, or nullptr. */
  [[nodiscard]] const LinkedGoogleForm* latestLinkedGoogleForm() const;

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
   * @param path e.g. @c "Root" or @c "Root / Node" (same form as
   *        @c networkPathOptions()).
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

  // --- Multi-user judgment session -----------------------------------------

  /** @return True if the model has any judgment participants defined. */
  [[nodiscard]] bool hasParticipants() const {
    return !root().participants().empty();
  }
  /** @return Model participants (shared roster owned by the root network). */
  [[nodiscard]] const std::vector<anpcpp::JudgmentParticipant>& participants()
      const {
    return root().participants();
  }
  /** @return Named judgment groups. */
  [[nodiscard]] const std::vector<anpcpp::JudgmentGroup>& judgmentGroups()
      const {
    return root().judgment_groups();
  }
  /** @return Current document-wide judgment session scope. */
  [[nodiscard]] anpcpp::JudgmentSession judgmentSession() const {
    return root().judgment_session();
  }
  /**
   * @brief Sets the session scope, rebuilds effective judgments on the root
   *        network, and marks the document dirty. Emits @ref sessionChanged.
   */
  void setJudgmentSession(const anpcpp::JudgmentSession& session);

  /**
   * @brief Adds (or renames, if @p id already exists) a participant.
   * Ensures per-user judgment tables exist and rebuilds effective judgments.
   */
  anpcpp::JudgmentParticipant& addParticipant(const QString& id,
                                              const QString& name,
                                              const QString& email = {});
  /** @brief Removes a participant and their judgment tables. */
  void removeParticipant(const QString& id);
  /** @brief Adds or updates a named group of participant ids. */
  anpcpp::JudgmentGroup& setJudgmentGroup(const QString& id,
                                          const QString& name,
                                          const QStringList& memberIds);
  /** @brief Removes a named group. */
  void removeJudgmentGroup(const QString& id);

  /**
   * @brief Serializes the root ANP network only (no researcher / google_forms).
   *
   * Used for undo snapshots of remove-participant and bulk judgment imports.
   */
  [[nodiscard]] QByteArray snapshotNetworkJson() const;
  /**
   * @brief Replaces the root network from @ref snapshotNetworkJson bytes.
   *
   * Preserves the undo stack, file path, Researcher notebooks, and linked
   * Google Forms. Resets the subnet view stack to root and refreshes UI.
   */
  void applyNetworkJson(const QByteArray& bytes);

  /**
   * @brief Rebuilds effective judgments from the root network downward.
   *
   * Call after any per-participant judgment edit (via @c *_for network
   * calls). Marks the document dirty and triggers a UI refresh.
   */
  void rebuildEffectiveJudgments();

  /**
   * @return Active participant id for editing, or empty when the session is
   *         an aggregate (Average / Group) — those views are read-only.
   */
  [[nodiscard]] QString activeParticipantId() const;
  /**
   * @return True if judgment editors should be read-only: participants exist
   *         and the session scope is an aggregate (Average or Group).
   */
  [[nodiscard]] bool judgmentReadOnly() const;

signals:
  /** @brief Emitted when the judgment session scope or roster changes. */
  void sessionChanged();
  void modelChanged();
  void dirtyChanged(bool dirty);
  void pathChanged(const QString& path);
  void viewNetworkChanged();
  void selectionChanged(const QString& cluster, const QString& node);
  void resultsFreshnessChanged();
  /**
   * @brief Emitted when the Researcher session is replaced (load / new).
   *
   * Not emitted for incremental edits via @c setResearcherSession().
   */
  void researcherSessionChanged();
  /** @brief Emitted when the linked Google Forms list changes. */
  void linkedFormsChanged();

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
  [[nodiscard]] static QVector<LinkedGoogleForm> parseLinkedGoogleForms(
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
  QVector<LinkedGoogleForm> linkedGoogleForms_;
};
