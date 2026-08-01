/**
 * @file researcher_dsl.hpp
 * @brief Small command language for the Researcher stage.
 */

#pragma once

#include <QString>
#include <QStringList>
#include <map>
#include <memory>
#include <vector>

#include "anpcpp/network.hpp"

class Document;

/**
 * @brief Result of evaluating one Researcher DSL command.
 */
struct ResearcherEvalResult {
  bool ok = false;
  QString html;
};

/**
 * @brief Session state: live document handles plus optionally loaded models.
 *
 * Built-in names:
 * - @c thisModel — @ref Document::network (current subnet view)
 * - @c parentModel — @ref Document::parentNetwork (nullptr at root)
 *
 * Extra models are owned here via @c load … as &lt;name&gt;.
 */
class ResearcherSession {
public:
  explicit ResearcherSession(Document* doc);

  [[nodiscard]] Document* document() const { return doc_; }

  /** @return Active handle name (default @c thisModel). */
  [[nodiscard]] QString activeName() const { return activeName_; }

  /**
   * @brief Evaluates a single-line command.
   * @param line Raw user input (leading/trailing whitespace ignored).
   */
  [[nodiscard]] ResearcherEvalResult eval(const QString& line);

  /** @return Binding summary lines for the side panel. */
  [[nodiscard]] QStringList bindingLines() const;

  /** @return Starter snippets shown in the left rail. */
  [[nodiscard]] static QStringList starterCommands();

  /** @return HTML help page. */
  [[nodiscard]] static QString helpHtml();

private:
  [[nodiscard]] anpcpp::AnpNetwork* resolve(const QString& name,
                                            QString* error) const;
  [[nodiscard]] anpcpp::AnpNetwork* active(QString* error) const;
  [[nodiscard]] bool isReservedName(const QString& name) const;
  [[nodiscard]] ResearcherEvalResult okHtml(const QString& body) const;
  [[nodiscard]] ResearcherEvalResult errHtml(const QString& message) const;

  [[nodiscard]] ResearcherEvalResult cmdHelp() const;
  [[nodiscard]] ResearcherEvalResult cmdVars() const;
  [[nodiscard]] ResearcherEvalResult cmdUse(const QStringList& args);
  [[nodiscard]] ResearcherEvalResult cmdLoad(const QStringList& tokens);
  [[nodiscard]] ResearcherEvalResult cmdDrop(const QStringList& args);
  [[nodiscard]] ResearcherEvalResult cmdInfo();
  [[nodiscard]] ResearcherEvalResult cmdPath() const;
  [[nodiscard]] ResearcherEvalResult cmdWhich() const;
  [[nodiscard]] ResearcherEvalResult cmdClusters();
  [[nodiscard]] ResearcherEvalResult cmdNodes();
  [[nodiscard]] ResearcherEvalResult cmdAlts();
  [[nodiscard]] ResearcherEvalResult cmdMatrix(const QString& kind);
  [[nodiscard]] ResearcherEvalResult cmdGlobals();
  [[nodiscard]] ResearcherEvalResult cmdAltScores();

  /** Resolve relative load paths against cwd, document dir, then app samples. */
  [[nodiscard]] QString resolveLoadPath(const QString& path) const;

  Document* doc_ = nullptr;
  QString activeName_ = QStringLiteral("thisModel");
  std::map<QString, std::unique_ptr<anpcpp::AnpNetwork>> loaded_;
};
