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

#include <stdexcept>
#include <utility>

namespace {

/**
 * @brief Tokenize a command line, respecting single/double quotes.
 */
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
    // Unclosed quote: keep partial token so the caller can error clearly.
    cur.prepend(quote);
  }
  if (!cur.isEmpty()) out << cur;
  return out;
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
      "<td style='background-color:#f8f9fa;'>Matrices</td></tr>"
      "<tr><td style='background-color:#ffffff;'><code>globals</code></td>"
      "<td style='background-color:#ffffff;'>Global priorities</td></tr>"
      "<tr><td style='background-color:#f8f9fa;'><code>altscores</code></td>"
      "<td style='background-color:#f8f9fa;'>Alternative scores</td></tr>");
  body += HtmlReport::tableEnd();
  body += QStringLiteral(
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
    if (error)
      *error = QStringLiteral("Unknown handle '%1'.").arg(name);
    return nullptr;
  }
  return it->second.get();
}

anpcpp::AnpNetwork* ResearcherSession::active(QString* error) const {
  return resolve(activeName_, error);
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
    if (cmd == QLatin1String("unscaled") ||
        cmd == QLatin1String("scaled") ||
        cmd == QLatin1String("cluster_matrix") ||
        cmd == QLatin1String("limit")) {
      return cmdMatrix(cmd);
    }
    if (cmd == QLatin1String("globals") ||
        cmd == QLatin1String("global")) {
      return cmdGlobals();
    }
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

  // Fall through to the original path so load_network_file reports a clear error.
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
  return okHtml(QStringLiteral("<p>Active handle: <code>") + HtmlReport::escape(activeName_) +
                QStringLiteral("</code></p>"));
}

ResearcherEvalResult ResearcherSession::cmdUse(const QStringList& args) {
  if (args.size() != 1) {
    return errHtml(QStringLiteral("Usage: use <name>"));
  }
  QString error;
  if (resolve(args.at(0), &error) == nullptr) {
    return errHtml(error);
  }
  activeName_ = args.at(0);
  return okHtml(QStringLiteral("<p>Active handle is now <code>") +
                HtmlReport::escape(activeName_) + QStringLiteral("</code>.</p>"));
}

ResearcherEvalResult ResearcherSession::cmdLoad(const QStringList& tokens) {
  // tokens: load <path> [as <name>]
  if (tokens.size() < 2) {
    return errHtml(
        QStringLiteral("Usage: load <path> [as <name>]"));
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
    return errHtml(
        QStringLiteral("Usage: load <path> [as <name>]"));
  }

  if (isReservedName(name)) {
    return errHtml(
        QStringLiteral("Cannot overwrite reserved handle '%1'.").arg(name));
  }
  if (!name.contains(QRegularExpression(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$")))) {
    return errHtml(QStringLiteral(
        "Handle name must be an identifier (letters, digits, underscore)."));
  }

  try {
    auto net = anpcpp::load_network_file(resolveLoadPath(path).toStdString());
    loaded_[name] = std::move(net);
    activeName_ = name;
    return okHtml(QStringLiteral("<p>Loaded <code>") + HtmlReport::escape(path) +
                  QStringLiteral("</code> as <code>") + HtmlReport::escape(name) +
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
  if (activeName_ == name) {
    activeName_ = QStringLiteral("thisModel");
  }
  return okHtml(QStringLiteral("<p>Dropped <code>") + HtmlReport::escape(name) +
                QStringLiteral("</code>.</p>"));
}

ResearcherEvalResult ResearcherSession::cmdInfo() {
  QString error;
  anpcpp::AnpNetwork* net = active(&error);
  if (net == nullptr) return errHtml(error);

  QString body = QStringLiteral("<h3>info — ") + HtmlReport::escape(activeName_) +
                 QStringLiteral("</h3>");
  body += QStringLiteral("<ul>");
  body += QStringLiteral("<li>Clusters: ") +
          QString::number(net->nclusters()) + QStringLiteral("</li>");
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
            HtmlReport::escape(doc_->currentNetworkPath()) + QStringLiteral("</li>");
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
  return okHtml(QStringLiteral("<p><code>") + HtmlReport::escape(doc_->currentNetworkPath()) +
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
  return okHtml(HtmlReport::nameList(net->alt_names(),
                             QStringLiteral("Alternatives — ") + activeName_));
}

ResearcherEvalResult ResearcherSession::cmdMatrix(const QString& kind) {
  QString error;
  anpcpp::AnpNetwork* net = active(&error);
  if (net == nullptr) return errHtml(error);

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
    title = QStringLiteral("Limit matrix");
    m = net->limit_matrix();
  } else {
    return errHtml(QStringLiteral("Unknown matrix '%1'.").arg(kind));
  }

  return okHtml(QStringLiteral("<h3>") + HtmlReport::escape(title) + QStringLiteral(" — ") +
                HtmlReport::escape(activeName_) + QStringLiteral("</h3>") +
                HtmlReport::matrixTable(m, *rows, *cols));
}

ResearcherEvalResult ResearcherSession::cmdGlobals() {
  QString error;
  anpcpp::AnpNetwork* net = active(&error);
  if (net == nullptr) return errHtml(error);
  return okHtml(QStringLiteral("<h3>Global priorities — ") + HtmlReport::escape(activeName_) +
                QStringLiteral("</h3>") +
                HtmlReport::vectorTable(net->global_priority(), net->node_names(),
                           QStringLiteral("Node")));
}

ResearcherEvalResult ResearcherSession::cmdAltScores() {
  QString error;
  anpcpp::AnpNetwork* net = active(&error);
  if (net == nullptr) return errHtml(error);
  return okHtml(QStringLiteral("<h3>Alternative scores — ") + HtmlReport::escape(activeName_) +
                QStringLiteral("</h3>") +
                HtmlReport::vectorTable(net->priority(), net->alt_names(),
                           QStringLiteral("Alternative")));
}
