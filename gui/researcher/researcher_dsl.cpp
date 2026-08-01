/**
 * @file researcher_dsl.cpp
 * @brief Researcher DSL evaluation against Document + loaded models.
 */

#include "researcher/researcher_dsl.hpp"

#include "document.hpp"
#include "html_report.hpp"

#include "anpcpp/json_io.hpp"
#include "anpcpp/matrix.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

#include <functional>
#include <stdexcept>
#include <utility>

namespace {

QStringList tokenize(const QString& line) {
  QStringList out;
  QString cur;
  QChar quote;
  for (int i = 0; i < line.size(); ++i) {
    const QChar c = line.at(i);
    if (!quote.isNull()) {
      if (c == quote) {
        quote = QChar();
      } else {
        cur += c;
      }
      continue;
    }
    if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
      quote = c;
      continue;
    }
    if (c.isSpace()) {
      if (!cur.isEmpty()) {
        out << cur;
        cur.clear();
      }
      continue;
    }
    cur += c;
  }
  if (!quote.isNull()) {
    cur.prepend(quote);
  }
  if (!cur.isEmpty()) out << cur;
  return out;
}

bool isMatrixCommand(const QString& cmd) {
  return cmd == QLatin1String("unscaled") || cmd == QLatin1String("scaled") ||
         cmd == QLatin1String("cluster_matrix") ||
         cmd == QLatin1String("cluster") || cmd == QLatin1String("limit");
}

bool isGlobalsCommand(const QString& cmd) {
  return cmd == QLatin1String("globals") || cmd == QLatin1String("global");
}

bool isLimitMethodCommand(const QString& cmd) {
  return cmd == QLatin1String("limit") || isGlobalsCommand(cmd);
}

bool isKeyword(const QString& tok) {
  return tok.compare(QStringLiteral("on"), Qt::CaseInsensitive) == 0 ||
         tok.compare(QStringLiteral("method"), Qt::CaseInsensitive) == 0 ||
         tok.compare(QStringLiteral("with_limit"), Qt::CaseInsensitive) == 0 ||
         tok.compare(QStringLiteral("withlimit"), Qt::CaseInsensitive) == 0 ||
         tok.compare(QStringLiteral("no_straight"), Qt::CaseInsensitive) == 0 ||
         tok.compare(QStringLiteral("straight"), Qt::CaseInsensitive) == 0 ||
         tok.compare(QStringLiteral("straight_normalizer"),
                     Qt::CaseInsensitive) == 0 ||
         tok.compare(QStringLiteral("no_straight_normalizer"),
                     Qt::CaseInsensitive) == 0;
}

QStringList methodNames() {
  return {QStringLiteral("calculus"), QStringLiteral("newhierarchy"),
          QStringLiteral("sinks")};
}

QStringList limitFlags() {
  return {QStringLiteral("with_limit"), QStringLiteral("no_straight"),
          QStringLiteral("straight")};
}

}  // namespace

ResearcherSession::ResearcherSession(Document* doc) : doc_(doc) {}

QStringList ResearcherSession::starterCommands() {
  return {
      QStringLiteral("help"),
      QStringLiteral("info"),
      QStringLiteral("vars"),
      QStringLiteral("path"),
      QStringLiteral("clusters"),
      QStringLiteral("nodes"),
      QStringLiteral("alts"),
      QStringLiteral("unscaled"),
      QStringLiteral("scaled"),
      QStringLiteral("cluster_matrix"),
      QStringLiteral("limit"),
      QStringLiteral("globals"),
      QStringLiteral("altscores"),
  };
}

QString ResearcherSession::helpHtml() {
  QString body = QStringLiteral(
      "<h2>Researcher commands</h2>"
      "<p>Inspect the open document (and optionally other JSON models) with a "
      "small command language. Output is HTML tables, similar to Analysis.</p>"
      "<h3>Handles</h3>"
      "<ul>"
      "<li><code>thisModel</code> — currently viewed network (subnet stack "
      "top)</li>"
      "<li><code>parentModel</code> — parent network, or unavailable at "
      "root</li>"
      "<li>Extra names from <code>load … as name</code></li>"
      "</ul>"
      "<h3>Commands</h3>");
  body += HtmlReport::tableBegin();
  body += QStringLiteral(
      "<tr><th>Command</th><th>Meaning</th></tr>"
      "<tr><td style='background-color:#ffffff;'><code>help</code></td>"
      "<td style='background-color:#ffffff;'>This page</td></tr>"
      "<tr><td style='background-color:#f8f9fa;'><code>vars</code></td>"
      "<td style='background-color:#f8f9fa;'>List handles</td></tr>"
      "<tr><td style='background-color:#ffffff;'><code>which</code></td>"
      "<td style='background-color:#ffffff;'>Active handle</td></tr>"
      "<tr><td style='background-color:#f8f9fa;'><code>use &lt;name&gt;</code></td>"
      "<td style='background-color:#f8f9fa;'>Select active handle</td></tr>"
      "<tr><td style='background-color:#ffffff;'>"
      "<code>load &lt;path&gt; [as &lt;name&gt;]</code></td>"
      "<td style='background-color:#ffffff;'>Load a JSON model into a named "
      "handle</td></tr>"
      "<tr><td style='background-color:#f8f9fa;'><code>drop &lt;name&gt;</code></td>"
      "<td style='background-color:#f8f9fa;'>Unload a loaded handle</td></tr>"
      "<tr><td style='background-color:#ffffff;'><code>info</code></td>"
      "<td style='background-color:#ffffff;'>Summary of the active model</td></tr>"
      "<tr><td style='background-color:#f8f9fa;'><code>path</code></td>"
      "<td style='background-color:#f8f9fa;'>Document subnet path</td></tr>"
      "<tr><td style='background-color:#ffffff;'>"
      "<code>clusters</code> / <code>nodes</code> / <code>alts</code></td>"
      "<td style='background-color:#ffffff;'>Name lists</td></tr>"
      "<tr><td style='background-color:#f8f9fa;'>"
      "<code>unscaled</code> / <code>scaled</code> / "
      "<code>cluster_matrix</code> / <code>limit</code></td>"
      "<td style='background-color:#f8f9fa;'>Matrices (see options below)</td></tr>"
      "<tr><td style='background-color:#ffffff;'><code>globals</code></td>"
      "<td style='background-color:#ffffff;'>Global priorities (see options "
      "below)</td></tr>"
      "<tr><td style='background-color:#f8f9fa;'><code>altscores</code></td>"
      "<td style='background-color:#f8f9fa;'>Alternative scores</td></tr>");
  body += HtmlReport::tableEnd();
  body += QStringLiteral(
      "<h3>Matrix / globals options</h3>"
      "<p>Press <b>Tab</b> in the input line to complete commands, handles, "
      "paths, and methods.</p>"
      "<ul>"
      "<li><code>on &lt;target&gt;</code> — network for the calculation. "
      "Target is a handle (<code>thisModel</code>, <code>parentModel</code>, "
      "loaded name) or a document breadcrumb path "
      "(<code>Root</code>, <code>\"Root / Node\"</code>). "
      "For a handle with subnets: "
      "<code>on h \"Root / Child\"</code>.</li>"
      "<li><code>method &lt;name&gt;</code> — for <code>limit</code> / "
      "<code>globals</code> only: "
      "<code>calculus</code> (default), <code>newhierarchy</code>, "
      "<code>sinks</code>.</li>"
      "<li>Flags: <code>with_limit</code> (newhierarchy), "
      "<code>no_straight</code> / <code>straight</code> (sinks).</li>"
      "</ul>"
      "<p>Examples:</p>"
      "<ul>"
      "<li><code>scaled on thisModel</code></li>"
      "<li><code>cluster_matrix on \"Root / Criteria\"</code></li>"
      "<li><code>limit on parentModel method sinks</code></li>"
      "<li><code>globals method newhierarchy with_limit</code></li>"
      "</ul>"
      "<p>Paths with spaces must be quoted: "
      "<code>load \"/path/to/my model.json\" as m2</code></p>");
  return HtmlReport::wrapDocument(body);
}

ResearcherEvalResult ResearcherSession::okHtml(const QString& body) const {
  return {true, HtmlReport::wrapDocument(body)};
}

ResearcherEvalResult ResearcherSession::errHtml(const QString& message) const {
  return {false, HtmlReport::wrapDocument(HtmlReport::errorParagraph(message))};
}

bool ResearcherSession::isReservedName(const QString& name) const {
  return name == QStringLiteral("thisModel") ||
         name == QStringLiteral("parentModel");
}

QStringList ResearcherSession::handleNames() const {
  QStringList names;
  names << QStringLiteral("thisModel") << QStringLiteral("parentModel");
  for (const auto& kv : loaded_) names << kv.first;
  return names;
}

anpcpp::AnpNetwork* ResearcherSession::resolve(const QString& name,
                                               QString* error) const {
  if (doc_ == nullptr) {
    if (error) *error = QStringLiteral("No document.");
    return nullptr;
  }
  if (name == QStringLiteral("thisModel")) {
    return &doc_->network();
  }
  if (name == QStringLiteral("parentModel")) {
    anpcpp::AnpNetwork* p = doc_->parentNetwork();
    if (p == nullptr) {
      if (error)
        *error = QStringLiteral(
            "parentModel is unavailable (already at the root network).");
      return nullptr;
    }
    return p;
  }
  const auto it = loaded_.find(name);
  if (it == loaded_.end() || it->second == nullptr) {
    if (error) *error = QStringLiteral("Unknown handle '%1'.").arg(name);
    return nullptr;
  }
  return it->second.get();
}

anpcpp::AnpNetwork* ResearcherSession::active(QString* error) const {
  return resolve(activeName_, error);
}

anpcpp::AnpNetwork* ResearcherSession::networkAtRelativePath(
    anpcpp::AnpNetwork* root, const QString& path, QString* error) {
  if (root == nullptr) {
    if (error) *error = QStringLiteral("No network.");
    return nullptr;
  }
  const QStringList parts =
      path.split(QStringLiteral(" / "), Qt::SkipEmptyParts);
  if (parts.isEmpty() || parts.first() != QStringLiteral("Root")) {
    if (error)
      *error =
          QStringLiteral("Subnet path must start with Root (got '%1').").arg(path);
    return nullptr;
  }
  anpcpp::AnpNetwork* cur = root;
  for (int i = 1; i < parts.size(); ++i) {
    anpcpp::AnpNode* node = cur->find_node(parts[i].toStdString());
    if (node == nullptr || !node->has_subnetwork()) {
      if (error)
        *error = QStringLiteral("No subnetwork path '%1'.").arg(path);
      return nullptr;
    }
    cur = node->subnetwork();
  }
  return cur;
}

QStringList ResearcherSession::pathOptionsUnder(anpcpp::AnpNetwork* root) {
  QStringList out;
  if (root == nullptr) return out;
  out << QStringLiteral("Root");
  std::function<void(const anpcpp::AnpNetwork&, const QString&)> walk;
  walk = [&](const anpcpp::AnpNetwork& net, const QString& prefix) {
    for (const anpcpp::AnpNode* n : net.nodes()) {
      if (!n->has_subnetwork()) continue;
      const QString path =
          prefix + QStringLiteral(" / ") + QString::fromStdString(n->name());
      out << path;
      walk(*n->subnetwork(), path);
    }
  };
  walk(*root, QStringLiteral("Root"));
  return out;
}

ResearcherSession::CalcTarget ResearcherSession::resolveOnHandlePath(
    const QString& handle, const QString& path, QString* error) const {
  CalcTarget t;
  anpcpp::AnpNetwork* base = resolve(handle, error);
  if (base == nullptr) return t;

  if ((handle == QStringLiteral("thisModel") ||
       handle == QStringLiteral("parentModel")) &&
      doc_ != nullptr && path.startsWith(QStringLiteral("Root"))) {
    anpcpp::AnpNetwork* net = doc_->networkAtPath(path);
    if (net == nullptr) {
      if (error)
        *error = QStringLiteral("Unknown network path '%1'.").arg(path);
      return t;
    }
    t.net = net;
    t.label = QStringLiteral("%1 @ %2").arg(handle, path);
    return t;
  }

  anpcpp::AnpNetwork* net = networkAtRelativePath(base, path, error);
  if (net == nullptr) return t;
  t.net = net;
  t.label = QStringLiteral("%1 @ %2").arg(handle, path);
  return t;
}

ResearcherSession::CalcTarget ResearcherSession::resolveOnTarget(
    const QString& target, QString* error) const {
  CalcTarget t;
  if (target == QStringLiteral("Root") ||
      target.startsWith(QStringLiteral("Root /"))) {
    if (doc_ == nullptr) {
      if (error) *error = QStringLiteral("No document.");
      return t;
    }
    anpcpp::AnpNetwork* net = doc_->networkAtPath(target);
    if (net == nullptr) {
      if (error)
        *error = QStringLiteral("Unknown network path '%1'.").arg(target);
      return t;
    }
    t.net = net;
    t.label = target;
    return t;
  }

  anpcpp::AnpNetwork* net = resolve(target, error);
  if (net == nullptr) return t;
  t.net = net;
  t.label = target;
  return t;
}

ResearcherSession::LimitParse ResearcherSession::parseLimitOptions(
    const QStringList& args, int startIndex) const {
  LimitParse parsed;
  for (int i = startIndex; i < args.size(); ++i) {
    const QString& tok = args.at(i);
    if (tok.compare(QStringLiteral("method"), Qt::CaseInsensitive) == 0) {
      if (i + 1 >= args.size()) {
        parsed.ok = false;
        parsed.error =
            QStringLiteral("Usage: method <calculus|newhierarchy|sinks>");
        return parsed;
      }
      const QString m = args.at(++i).toLower();
      if (m == QLatin1String("calculus") || m == QLatin1String("calc")) {
        parsed.options.method = anpcpp::LimitMatrixMethod::Calculus;
        parsed.methodLabel = QStringLiteral("calculus");
      } else if (m == QLatin1String("newhierarchy") ||
                 m == QLatin1String("new_hierarchy") ||
                 m == QLatin1String("hierarchy") || m == QLatin1String("nh")) {
        parsed.options.method = anpcpp::LimitMatrixMethod::NewHierarchy;
        parsed.methodLabel = QStringLiteral("newhierarchy");
      } else if (m == QLatin1String("sinks") || m == QLatin1String("sink")) {
        parsed.options.method = anpcpp::LimitMatrixMethod::Sinks;
        parsed.methodLabel = QStringLiteral("sinks");
      } else {
        parsed.ok = false;
        parsed.error = QStringLiteral(
                           "Unknown limit method '%1' "
                           "(calculus, newhierarchy, sinks).")
                           .arg(args.at(i));
        return parsed;
      }
      continue;
    }
    if (tok.compare(QStringLiteral("with_limit"), Qt::CaseInsensitive) == 0 ||
        tok.compare(QStringLiteral("withlimit"), Qt::CaseInsensitive) == 0) {
      parsed.options.with_limit = true;
      continue;
    }
    if (tok.compare(QStringLiteral("no_straight"), Qt::CaseInsensitive) == 0 ||
        tok.compare(QStringLiteral("no_straight_normalizer"),
                    Qt::CaseInsensitive) == 0) {
      parsed.options.straight_normalizer = false;
      continue;
    }
    if (tok.compare(QStringLiteral("straight"), Qt::CaseInsensitive) == 0 ||
        tok.compare(QStringLiteral("straight_normalizer"),
                    Qt::CaseInsensitive) == 0) {
      parsed.options.straight_normalizer = true;
      continue;
    }
    parsed.ok = false;
    parsed.error = QStringLiteral("Unexpected argument '%1'.").arg(tok);
    return parsed;
  }
  return parsed;
}

ResearcherEvalResult ResearcherSession::parseCalcTarget(
    const QStringList& args, bool allowLimit, CalcTarget* target,
    LimitParse* limit) const {
  QString error;
  anpcpp::AnpNetwork* def = active(&error);
  if (def == nullptr) return errHtml(error);
  target->net = def;
  target->label = activeName_;
  *limit = LimitParse{};

  int i = 0;
  if (i < args.size() && !isKeyword(args.at(i))) {
    // Positional target, or handle + optional Root path.
    const QString& a0 = args.at(i);
    if (i + 1 < args.size() &&
        (args.at(i + 1) == QStringLiteral("Root") ||
         args.at(i + 1).startsWith(QStringLiteral("Root /")))) {
      *target = resolveOnHandlePath(a0, args.at(i + 1), &error);
      if (target->net == nullptr) return errHtml(error);
      i += 2;
    } else {
      *target = resolveOnTarget(a0, &error);
      if (target->net == nullptr) return errHtml(error);
      ++i;
    }
  }

  while (i < args.size()) {
    const QString& tok = args.at(i);
    if (tok.compare(QStringLiteral("on"), Qt::CaseInsensitive) == 0) {
      if (i + 1 >= args.size()) {
        return errHtml(QStringLiteral(
            "Usage: on <handle|Root path>  or  on <handle> \"Root / …\""));
      }
      const QString& a1 = args.at(i + 1);
      if (i + 2 < args.size() &&
          (args.at(i + 2) == QStringLiteral("Root") ||
           args.at(i + 2).startsWith(QStringLiteral("Root /")))) {
        *target = resolveOnHandlePath(a1, args.at(i + 2), &error);
        if (target->net == nullptr) return errHtml(error);
        i += 3;
      } else {
        *target = resolveOnTarget(a1, &error);
        if (target->net == nullptr) return errHtml(error);
        i += 2;
      }
      continue;
    }

    if (!allowLimit) {
      return errHtml(QStringLiteral("Unexpected argument '%1'.").arg(tok));
    }

    // Remaining tokens are limit options (method / flags).
    *limit = parseLimitOptions(args, i);
    if (!limit->ok) return errHtml(limit->error);
    return {};
  }
  return {};
}

QStringList ResearcherSession::bindingLines() const {
  QStringList lines;
  const QString path =
      doc_ != nullptr ? doc_->currentNetworkPath() : QStringLiteral("—");
  lines << QStringLiteral("thisModel  [%1]").arg(path);
  if (doc_ != nullptr && doc_->parentNetwork() != nullptr) {
    lines << QStringLiteral("parentModel  (available)");
  } else {
    lines << QStringLiteral("parentModel  (at root)");
  }
  for (const auto& kv : loaded_) {
    lines << QStringLiteral("%1  (loaded)").arg(kv.first);
  }
  lines << QStringLiteral("active → %1").arg(activeName_);
  return lines;
}

ResearcherEvalResult ResearcherSession::eval(const QString& line) {
  const QString trimmed = line.trimmed();
  if (trimmed.isEmpty()) {
    return errHtml(QStringLiteral("Empty command. Try 'help'."));
  }

  const QStringList tokens = tokenize(trimmed);
  if (tokens.isEmpty()) {
    return errHtml(QStringLiteral("Empty command. Try 'help'."));
  }
  if (tokens.first().startsWith(QLatin1Char('"')) ||
      tokens.first().startsWith(QLatin1Char('\''))) {
    return errHtml(QStringLiteral("Unclosed quote in command."));
  }

  const QString cmd = tokens.first().toLower();
  const QStringList args = tokens.mid(1);

  try {
    if (cmd == QLatin1String("help") || cmd == QLatin1String("?")) {
      return cmdHelp();
    }
    if (cmd == QLatin1String("vars")) return cmdVars();
    if (cmd == QLatin1String("which")) return cmdWhich();
    if (cmd == QLatin1String("use")) return cmdUse(args);
    if (cmd == QLatin1String("load")) return cmdLoad(tokens);
    if (cmd == QLatin1String("drop")) return cmdDrop(args);
    if (cmd == QLatin1String("info")) return cmdInfo();
    if (cmd == QLatin1String("path")) return cmdPath();
    if (cmd == QLatin1String("clusters")) return cmdClusters();
    if (cmd == QLatin1String("nodes")) return cmdNodes();
    if (cmd == QLatin1String("alts") || cmd == QLatin1String("alternatives")) {
      return cmdAlts();
    }
    if (isMatrixCommand(cmd)) {
      const QString kind =
          cmd == QLatin1String("cluster") ? QStringLiteral("cluster_matrix")
                                          : cmd;
      return cmdMatrix(kind, args);
    }
    if (isGlobalsCommand(cmd)) return cmdGlobals(args);
    if (cmd == QLatin1String("altscores") ||
        cmd == QLatin1String("priorities") ||
        cmd == QLatin1String("priority")) {
      return cmdAltScores();
    }
    return errHtml(
        QStringLiteral("Unknown command '%1'. Try 'help'.").arg(tokens.first()));
  } catch (const std::exception& e) {
    return errHtml(QString::fromUtf8(e.what()));
  }
}

QStringList ResearcherSession::completions(const QString& line) const {
  const bool trailingSpace = line.endsWith(QLatin1Char(' '));
  const QStringList tokens = tokenize(line.trimmed());
  QStringList candidates;
  QString prefix;

  auto stripQuotes = [](QString s) {
    if (s.size() >= 2 &&
        ((s.startsWith(QLatin1Char('"')) && s.endsWith(QLatin1Char('"'))) ||
         (s.startsWith(QLatin1Char('\'')) && s.endsWith(QLatin1Char('\''))))) {
      return s.mid(1, s.size() - 2);
    }
    if (s.startsWith(QLatin1Char('"')) || s.startsWith(QLatin1Char('\''))) {
      return s.mid(1);
    }
    return s;
  };

  auto quotePath = [](const QString& path) {
    QString esc = path;
    esc.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QStringLiteral("\"%1\"").arg(esc);
  };

  auto quotePaths = [&](const QStringList& paths) {
    QStringList out;
    for (const QString& p : paths) out << quotePath(p);
    return out;
  };

  auto filter = [&](const QStringList& opts) {
    const QString bare = stripQuotes(prefix);
    QStringList out;
    for (const QString& o : opts) {
      const QString ob = stripQuotes(o);
      const bool isNetworkPath =
          ob == QLatin1String("Root") ||
          ob.startsWith(QStringLiteral("Root /"));
      if (bare.isEmpty()) {
        out << o;
        continue;
      }
      if (isNetworkPath) {
        // Substring match so "bene" completes "Root / Benefits".
        if (ob.contains(bare, Qt::CaseInsensitive) ||
            o.startsWith(prefix, Qt::CaseInsensitive)) {
          out << o;
        }
      } else if (ob.startsWith(bare, Qt::CaseInsensitive) ||
                 o.startsWith(prefix, Qt::CaseInsensitive)) {
        out << o;
      }
    }
    out.removeDuplicates();
    return out;
  };

  if (tokens.isEmpty() || (tokens.size() == 1 && !trailingSpace)) {
    prefix = tokens.isEmpty() ? QString() : tokens.first();
    QStringList cmds = starterCommands();
    cmds << QStringLiteral("which") << QStringLiteral("use")
         << QStringLiteral("load") << QStringLiteral("drop")
         << QStringLiteral("cluster") << QStringLiteral("global")
         << QStringLiteral("alternatives") << QStringLiteral("priorities");
    return filter(cmds);
  }

  const QString cmd = tokens.first().toLower();
  prefix = trailingSpace ? QString() : tokens.last();
  // Prefer the raw trailing token (with quotes) when the line ends mid-quote.
  if (!trailingSpace) {
    int tokenStart = 0;
    bool inQuote = false;
    QChar quote;
    for (int i = 0; i < line.size(); ++i) {
      const QChar c = line.at(i);
      if (inQuote) {
        if (c == quote) inQuote = false;
        continue;
      }
      if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
        inQuote = true;
        quote = c;
        continue;
      }
      if (c.isSpace()) tokenStart = i + 1;
    }
    prefix = line.mid(tokenStart);
  }

  const QStringList finished =
      trailingSpace ? tokens.mid(1) : tokens.mid(1, tokens.size() - 2);

  if (cmd == QLatin1String("use") || cmd == QLatin1String("drop")) {
    return filter(handleNames());
  }

  if (!isMatrixCommand(cmd) && !isGlobalsCommand(cmd)) return {};

  const bool wantMethod = isLimitMethodCommand(cmd);
  bool sawOn = false;
  bool sawMethod = false;
  for (int i = 0; i < finished.size(); ++i) {
    const QString& t = finished.at(i);
    if (t.compare(QStringLiteral("on"), Qt::CaseInsensitive) == 0) {
      sawOn = true;
      if (i + 1 < finished.size()) ++i;
      continue;
    }
    if (wantMethod &&
        t.compare(QStringLiteral("method"), Qt::CaseInsensitive) == 0) {
      sawMethod = true;
      if (i + 1 < finished.size()) ++i;
      continue;
    }
  }

  if (!finished.isEmpty()) {
    const QString& prev = finished.last();
    if (prev.compare(QStringLiteral("on"), Qt::CaseInsensitive) == 0) {
      QStringList targets = handleNames();
      if (doc_ != nullptr) targets << quotePaths(doc_->networkPathOptions());
      return filter(targets);
    }
    if (wantMethod &&
        prev.compare(QStringLiteral("method"), Qt::CaseInsensitive) == 0) {
      return filter(methodNames());
    }
    if (finished.size() >= 2) {
      const QString& maybeOn = finished.at(finished.size() - 2);
      const QString& handleTok = finished.last();
      if (maybeOn.compare(QStringLiteral("on"), Qt::CaseInsensitive) == 0) {
        if (handleTok == QStringLiteral("thisModel") ||
            handleTok == QStringLiteral("parentModel")) {
          if (doc_ != nullptr) {
            QStringList opts = quotePaths(doc_->networkPathOptions());
            if (wantMethod) {
              opts << QStringLiteral("method");
              opts << limitFlags();
            }
            return filter(opts);
          }
        } else {
          QString err;
          anpcpp::AnpNetwork* base = resolve(handleTok, &err);
          if (base != nullptr) {
            QStringList opts = quotePaths(pathOptionsUnder(base));
            if (wantMethod) {
              opts << QStringLiteral("method");
              opts << limitFlags();
            }
            return filter(opts);
          }
        }
      }
    }
  }

  if (!sawOn) candidates << QStringLiteral("on");
  if (wantMethod && !sawMethod) candidates << QStringLiteral("method");
  if (wantMethod) candidates << limitFlags();
  if (!sawOn) {
    candidates << handleNames();
    if (doc_ != nullptr) candidates << quotePaths(doc_->networkPathOptions());
  }
  return filter(candidates);
}

ResearcherEvalResult ResearcherSession::cmdHelp() const {
  return {true, helpHtml()};
}

QString ResearcherSession::resolveLoadPath(const QString& path) const {
  const QFileInfo direct(path);
  if (direct.isAbsolute() && direct.exists()) return direct.absoluteFilePath();
  if (direct.exists()) return direct.absoluteFilePath();

  if (doc_ != nullptr && !doc_->path().isEmpty()) {
    const QDir docDir = QFileInfo(doc_->path()).absoluteDir();
    const QFileInfo besideDoc(docDir.filePath(path));
    if (besideDoc.exists()) return besideDoc.absoluteFilePath();
  }

  const QDir appDir(QCoreApplication::applicationDirPath());
  const QFileInfo besideApp(appDir.filePath(path));
  if (besideApp.exists()) return besideApp.absoluteFilePath();
  const QFileInfo inSamples(appDir.filePath(QStringLiteral("samples/") + path));
  if (inSamples.exists()) return inSamples.absoluteFilePath();
  return path;
}

ResearcherEvalResult ResearcherSession::cmdVars() const {
  QString body = QStringLiteral("<h3>Handles</h3><ul>");
  for (const QString& line : bindingLines()) {
    body += QStringLiteral("<li><code>") + HtmlReport::escape(line) +
            QStringLiteral("</code></li>");
  }
  body += QStringLiteral("</ul>");
  return okHtml(body);
}

ResearcherEvalResult ResearcherSession::cmdWhich() const {
  return okHtml(QStringLiteral("<p>Active handle: <code>") +
                HtmlReport::escape(activeName_) + QStringLiteral("</code></p>"));
}

ResearcherEvalResult ResearcherSession::cmdUse(const QStringList& args) {
  if (args.size() != 1) {
    return errHtml(QStringLiteral("Usage: use <name>"));
  }
  QString error;
  if (resolve(args.at(0), &error) == nullptr) return errHtml(error);
  activeName_ = args.at(0);
  return okHtml(QStringLiteral("<p>Active handle is now <code>") +
                HtmlReport::escape(activeName_) + QStringLiteral("</code>.</p>"));
}

ResearcherEvalResult ResearcherSession::cmdLoad(const QStringList& tokens) {
  if (tokens.size() < 2) {
    return errHtml(QStringLiteral("Usage: load <path> [as <name>]"));
  }
  const QString path = tokens.at(1);
  QString name;
  if (tokens.size() == 2) {
    name = QFileInfo(path).completeBaseName();
    if (name.isEmpty()) name = QStringLiteral("model");
  } else if (tokens.size() == 4 &&
             tokens.at(2).compare(QStringLiteral("as"), Qt::CaseInsensitive) ==
                 0) {
    name = tokens.at(3);
  } else {
    return errHtml(QStringLiteral("Usage: load <path> [as <name>]"));
  }

  if (isReservedName(name)) {
    return errHtml(
        QStringLiteral("Cannot overwrite reserved handle '%1'.").arg(name));
  }
  if (!name.contains(
          QRegularExpression(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$")))) {
    return errHtml(QStringLiteral(
        "Handle name must be an identifier (letters, digits, underscore)."));
  }

  try {
    auto net = anpcpp::load_network_file(resolveLoadPath(path).toStdString());
    loaded_[name] = std::move(net);
    activeName_ = name;
    return okHtml(QStringLiteral("<p>Loaded <code>") + HtmlReport::escape(path) +
                  QStringLiteral("</code> as <code>") +
                  HtmlReport::escape(name) +
                  QStringLiteral("</code> (now active).</p>"));
  } catch (const std::exception& e) {
    return errHtml(QString::fromUtf8(e.what()));
  }
}

ResearcherEvalResult ResearcherSession::cmdDrop(const QStringList& args) {
  if (args.size() != 1) {
    return errHtml(QStringLiteral("Usage: drop <name>"));
  }
  const QString& name = args.at(0);
  if (isReservedName(name)) {
    return errHtml(
        QStringLiteral("Cannot drop reserved handle '%1'.").arg(name));
  }
  if (loaded_.erase(name) == 0) {
    return errHtml(QStringLiteral("No loaded handle '%1'.").arg(name));
  }
  if (activeName_ == name) activeName_ = QStringLiteral("thisModel");
  return okHtml(QStringLiteral("<p>Dropped <code>") + HtmlReport::escape(name) +
                QStringLiteral("</code>.</p>"));
}

ResearcherEvalResult ResearcherSession::cmdInfo() {
  QString error;
  anpcpp::AnpNetwork* net = active(&error);
  if (net == nullptr) return errHtml(error);

  QString body = QStringLiteral("<h3>info — ") + HtmlReport::escape(activeName_) +
                 QStringLiteral("</h3><ul>");
  body += QStringLiteral("<li>Clusters: ") + QString::number(net->nclusters()) +
          QStringLiteral("</li>");
  body += QStringLiteral("<li>Nodes: ") +
          QString::number(net->node_names().size()) + QStringLiteral("</li>");
  body += QStringLiteral("<li>Alternatives: ") +
          QString::number(net->alt_names().size()) + QStringLiteral("</li>");
  body += QStringLiteral("<li>Has subnetworks: ") +
          QString(net->has_subnet() ? QStringLiteral("yes")
                                    : QStringLiteral("no")) +
          QStringLiteral("</li>");
  if (activeName_ == QStringLiteral("thisModel") && doc_ != nullptr) {
    body += QStringLiteral("<li>Document path: ") +
            HtmlReport::escape(doc_->currentNetworkPath()) +
            QStringLiteral("</li>");
    if (!doc_->path().isEmpty()) {
      body += QStringLiteral("<li>File: ") + HtmlReport::escape(doc_->path()) +
              QStringLiteral("</li>");
    }
  }
  body += QStringLiteral("</ul>");
  return okHtml(body);
}

ResearcherEvalResult ResearcherSession::cmdPath() const {
  if (doc_ == nullptr) return errHtml(QStringLiteral("No document."));
  return okHtml(QStringLiteral("<p><code>") +
                HtmlReport::escape(doc_->currentNetworkPath()) +
                QStringLiteral("</code></p>"));
}

ResearcherEvalResult ResearcherSession::cmdClusters() {
  QString error;
  anpcpp::AnpNetwork* net = active(&error);
  if (net == nullptr) return errHtml(error);
  return okHtml(HtmlReport::nameList(net->cluster_names(),
                                     QStringLiteral("Clusters — ") + activeName_));
}

ResearcherEvalResult ResearcherSession::cmdNodes() {
  QString error;
  anpcpp::AnpNetwork* net = active(&error);
  if (net == nullptr) return errHtml(error);
  return okHtml(HtmlReport::nameList(net->node_names(),
                                     QStringLiteral("Nodes — ") + activeName_));
}

ResearcherEvalResult ResearcherSession::cmdAlts() {
  QString error;
  anpcpp::AnpNetwork* net = active(&error);
  if (net == nullptr) return errHtml(error);
  return okHtml(HtmlReport::nameList(
      net->alt_names(), QStringLiteral("Alternatives — ") + activeName_));
}

ResearcherEvalResult ResearcherSession::cmdMatrix(const QString& kind,
                                                  const QStringList& args) {
  const bool allowLimit = kind == QLatin1String("limit");
  CalcTarget target;
  LimitParse limit;
  const ResearcherEvalResult parseErr =
      parseCalcTarget(args, allowLimit, &target, &limit);
  if (!parseErr.html.isEmpty()) return parseErr;

  anpcpp::AnpNetwork* net = target.net;
  const auto nodeNames = net->node_names();
  const auto clusterNames = net->cluster_names();
  QString title;
  anpcpp::Matrix m;
  const std::vector<std::string>* rows = &nodeNames;
  const std::vector<std::string>* cols = &nodeNames;

  if (kind == QLatin1String("unscaled")) {
    title = QStringLiteral("Unscaled supermatrix");
    m = net->unscaled_supermatrix();
  } else if (kind == QLatin1String("scaled")) {
    title = QStringLiteral("Scaled supermatrix");
    m = net->scaled_supermatrix();
  } else if (kind == QLatin1String("cluster_matrix")) {
    title = QStringLiteral("Cluster weight matrix");
    m = net->cluster_weight_matrix();
    rows = &clusterNames;
    cols = &clusterNames;
  } else if (kind == QLatin1String("limit")) {
    title = QStringLiteral("Limit matrix (%1)").arg(limit.methodLabel);
    if (limit.options.with_limit) title += QStringLiteral(", with_limit");
    if (limit.options.method == anpcpp::LimitMatrixMethod::Sinks &&
        !limit.options.straight_normalizer) {
      title += QStringLiteral(", no_straight");
    }
    m = net->limit_matrix(limit.options);
  } else {
    return errHtml(QStringLiteral("Unknown matrix '%1'.").arg(kind));
  }

  return okHtml(QStringLiteral("<h3>") + HtmlReport::escape(title) +
                QStringLiteral(" — ") + HtmlReport::escape(target.label) +
                QStringLiteral("</h3>") +
                HtmlReport::matrixTable(m, *rows, *cols));
}

ResearcherEvalResult ResearcherSession::cmdGlobals(const QStringList& args) {
  CalcTarget target;
  LimitParse limit;
  const ResearcherEvalResult parseErr =
      parseCalcTarget(args, true, &target, &limit);
  if (!parseErr.html.isEmpty()) return parseErr;

  QString title = QStringLiteral("Global priorities (%1)").arg(limit.methodLabel);
  if (limit.options.with_limit) title += QStringLiteral(", with_limit");
  if (limit.options.method == anpcpp::LimitMatrixMethod::Sinks &&
      !limit.options.straight_normalizer) {
    title += QStringLiteral(", no_straight");
  }

  return okHtml(
      QStringLiteral("<h3>") + HtmlReport::escape(title) +
      QStringLiteral(" — ") + HtmlReport::escape(target.label) +
      QStringLiteral("</h3>") +
      HtmlReport::vectorTable(target.net->global_priority(limit.options),
                              target.net->node_names(), QStringLiteral("Node")));
}

ResearcherEvalResult ResearcherSession::cmdAltScores() {
  QString error;
  anpcpp::AnpNetwork* net = active(&error);
  if (net == nullptr) return errHtml(error);
  return okHtml(QStringLiteral("<h3>Alternative scores — ") +
                HtmlReport::escape(activeName_) + QStringLiteral("</h3>") +
                HtmlReport::vectorTable(net->priority(), net->alt_names(),
                                        QStringLiteral("Alternative")));
}
