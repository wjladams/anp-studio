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

#include "anpcpp/limit_matrix.hpp"
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
 *
 * Matrix / globals commands accept:
 * - @c on &lt;target&gt; — handle (@c thisModel, …) or breadcrumb path
 *   (@c "Root", @c "Root / Node", …); for a handle with subnets,
 *   @c on &lt;handle&gt; "Root / Node"
 * - @c method &lt;calculus|newhierarchy|sinks&gt; — limit / globals only
 * - flags: @c with_limit (newhierarchy), @c no_straight (sinks)
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

  /**
   * @brief Completions for the token being typed at the end of @p line.
   * @return Candidate strings (already filtered by the current token prefix).
   */
  [[nodiscard]] QStringList completions(const QString& line) const;

  /** @return Known handle names (reserved + loaded). */
  [[nodiscard]] QStringList handleNames() const;

private:
  struct CalcTarget {
    anpcpp::AnpNetwork* net = nullptr;
    QString label;
  };

  struct LimitParse {
    anpcpp::LimitMatrixOptions options;
    QString methodLabel = QStringLiteral("calculus");
    QString error;
    bool ok = true;
  };

  [[nodiscard]] anpcpp::AnpNetwork* resolve(const QString& name,
                                            QString* error) const;
  [[nodiscard]] anpcpp::AnpNetwork* active(QString* error) const;
  [[nodiscard]] bool isReservedName(const QString& name) const;
  [[nodiscard]] ResearcherEvalResult okHtml(const QString& body) const;
  [[nodiscard]] ResearcherEvalResult errHtml(const QString& message) const;

  [[nodiscard]] CalcTarget resolveOnTarget(const QString& target,
                                           QString* error) const;
  [[nodiscard]] CalcTarget resolveOnHandlePath(const QString& handle,
                                               const QString& path,
                                               QString* error) const;
  [[nodiscard]] static anpcpp::AnpNetwork* networkAtRelativePath(
      anpcpp::AnpNetwork* root, const QString& path, QString* error);
  [[nodiscard]] static QStringList pathOptionsUnder(anpcpp::AnpNetwork* root);
  [[nodiscard]] LimitParse parseLimitOptions(const QStringList& args,
                                             int startIndex) const;

  /**
   * @brief Parses optional @c on / limit options for matrix and globals.
   * @param allowLimit When false, method/flags are rejected.
   */
  [[nodiscard]] ResearcherEvalResult parseCalcTarget(
      const QStringList& args, bool allowLimit, CalcTarget* target,
      LimitParse* limit) const;

  [[nodiscard]] ResearcherEvalResult cmdMatrix(const QString& kind,
                                               const QStringList& args);
  [[nodiscard]] ResearcherEvalResult cmdGlobals(const QStringList& args);

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
  [[nodiscard]] ResearcherEvalResult cmdAltScores();

  /** Resolve relative load paths against cwd, document dir, then app samples. */
  [[nodiscard]] QString resolveLoadPath(const QString& path) const;

  Document* doc_ = nullptr;
  QString activeName_ = QStringLiteral("thisModel");
  std::map<QString, std::unique_ptr<anpcpp::AnpNetwork>> loaded_;
};
