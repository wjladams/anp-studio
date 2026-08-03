/**
 * @file judgment_template_io.cpp
 * @brief Per-participant judgment templates (Excel respondent + legacy CSV).
 */

#include "io/judgment_template_io.hpp"

#include "document.hpp"
#include "io/judgment_question_text.hpp"
#include "io/xlsx_sheet_reader.hpp"

#include "anpcpp/network.hpp"
#include "anpcpp/ratings.hpp"

#include <OpenXLSX.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStringConverter>
#include <QTextStream>
#include <QVector>

#include <cmath>
#include <string>
#include <utility>

using namespace OpenXLSX;

namespace {

struct JudgmentRow {
  QString kind;  // pairwise | cluster_pairwise | ratings
  QString wrt;
  QString destCluster;
  QString altA;
  QString altB;
  QString alt;
  QString anpTag;
  QString valueHint;
};

void collectRows(const anpcpp::AnpNetwork& net, QVector<JudgmentRow>& out) {
  for (const anpcpp::AnpNode* n : net.nodes()) {
    for (const anpcpp::AnpCluster* dest : net.clusters()) {
      const anpcpp::NodePrioritizerSlot* slot =
          n->node_prioritizer(dest->name());
      if (slot == nullptr || slot->empty()) continue;

      if (slot->kind == anpcpp::NodePrioritizerKind::Pairwise) {
        const auto& alts = slot->pairwise.alternatives();
        for (std::size_t i = 0; i < alts.size(); ++i) {
          for (std::size_t j = i + 1; j < alts.size(); ++j) {
            JudgmentRow r;
            r.kind = QStringLiteral("pairwise");
            r.wrt = QString::fromStdString(n->name());
            r.destCluster = QString::fromStdString(dest->name());
            r.altA = QString::fromStdString(alts[i]);
            r.altB = QString::fromStdString(alts[j]);
            r.anpTag = QStringLiteral("[anp:pw|%1|%2|%3|%4]")
                           .arg(r.wrt, r.destCluster, r.altA, r.altB);
            out.push_back(std::move(r));
          }
        }
      } else {
        const auto& rt = slot->ratings;
        for (const std::string& alt : rt.alternatives()) {
          JudgmentRow r;
          r.kind = QStringLiteral("ratings");
          r.wrt = QString::fromStdString(n->name());
          r.destCluster = QString::fromStdString(dest->name());
          r.alt = QString::fromStdString(alt);
          r.anpTag = QStringLiteral("[anp:rt|%1|%2|%3]")
                         .arg(r.wrt, r.destCluster, r.alt);
          if (rt.mode() == anpcpp::RatingsPrioritizer::Mode::Categorical &&
              !rt.categories().empty()) {
            QStringList cats;
            for (const auto& c : rt.categories()) {
              cats << QStringLiteral("%1 (%2)")
                          .arg(QString::fromStdString(c.label),
                               QString::fromStdString(c.id));
            }
            r.valueHint = QStringLiteral("category id, e.g. %1")
                              .arg(cats.join(QStringLiteral(" | ")));
          } else {
            r.valueHint = QStringLiteral("numeric score");
          }
          out.push_back(std::move(r));
        }
      }
    }
    if (n->has_subnetwork()) collectRows(*n->subnetwork(), out);
  }

  for (const anpcpp::AnpCluster* c : net.clusters()) {
    const auto& pw = c->cluster_pairwise();
    if (pw.size() < 2) continue;
    const auto& alts = pw.alternatives();
    for (std::size_t i = 0; i < alts.size(); ++i) {
      for (std::size_t j = i + 1; j < alts.size(); ++j) {
        JudgmentRow r;
        r.kind = QStringLiteral("cluster_pairwise");
        r.wrt = QString::fromStdString(c->name());
        r.destCluster = r.wrt;
        r.altA = QString::fromStdString(alts[i]);
        r.altB = QString::fromStdString(alts[j]);
        r.anpTag = QStringLiteral("[anp:cpw|%1|%2|%3]")
                       .arg(r.wrt, r.altA, r.altB);
        out.push_back(std::move(r));
      }
    }
  }
}

QString comparisonText(const JudgmentRow& r) {
  if (r.kind == QStringLiteral("pairwise")) {
    return pairwiseComparisonText(r.wrt, r.destCluster, r.altA, r.altB);
  }
  if (r.kind == QStringLiteral("cluster_pairwise")) {
    return clusterPairwiseComparisonText(r.wrt, r.altA, r.altB);
  }
  if (r.kind == QStringLiteral("ratings")) {
    return ratingsComparisonText(r.wrt, r.destCluster, r.alt, r.valueHint);
  }
  return r.anpTag;
}

QString sectionTitle(const JudgmentRow& r) {
  if (r.kind == QStringLiteral("cluster_pairwise")) {
    return clusterSectionTitle(r.wrt);
  }
  return nodeSectionTitle(r.wrt, r.destCluster);
}

anpcpp::AnpNetwork* findNetworkWithNode(anpcpp::AnpNetwork& root,
                                        const std::string& nodeName) {
  if (root.find_node(nodeName) != nullptr) return &root;
  for (anpcpp::AnpNode* n : root.nodes()) {
    if (!n->has_subnetwork()) continue;
    if (anpcpp::AnpNetwork* found =
            findNetworkWithNode(*n->subnetwork(), nodeName)) {
      return found;
    }
  }
  return nullptr;
}

anpcpp::AnpNetwork* findNetworkWithCluster(anpcpp::AnpNetwork& root,
                                           const std::string& clusterName) {
  if (root.find_cluster(clusterName) != nullptr) return &root;
  for (anpcpp::AnpNode* n : root.nodes()) {
    if (!n->has_subnetwork()) continue;
    if (anpcpp::AnpNetwork* found =
            findNetworkWithCluster(*n->subnetwork(), clusterName)) {
      return found;
    }
  }
  return nullptr;
}

QString csvEscape(const QString& s) {
  if (!s.contains(QLatin1Char(',')) && !s.contains(QLatin1Char('"')) &&
      !s.contains(QLatin1Char('\n')) && !s.contains(QLatin1Char('\r'))) {
    return s;
  }
  QString out = s;
  out.replace(QLatin1Char('"'), QStringLiteral("\"\""));
  return QStringLiteral("\"%1\"").arg(out);
}

QString formatValue(double v) {
  if (!std::isfinite(v)) return {};
  static const double recip[] = {2, 3, 4, 5, 6, 7, 8, 9};
  for (double d : recip) {
    if (std::abs(v - 1.0 / d) < 1e-9)
      return QStringLiteral("1/%1").arg(static_cast<int>(d));
  }
  if (std::abs(v - std::round(v)) < 1e-9)
    return QString::number(static_cast<qint64>(std::lround(v)));
  return QString::number(v, 'g', 12);
}

bool parseRatio(const QString& text, double* out) {
  if (out == nullptr) return false;
  const QString s = text.trimmed();
  if (s.isEmpty()) return false;
  if (s.contains(QLatin1Char('/'))) {
    const QStringList parts = s.split(QLatin1Char('/'));
    if (parts.size() != 2) return false;
    bool ok1 = false, ok2 = false;
    const double a = parts[0].toDouble(&ok1);
    const double b = parts[1].toDouble(&ok2);
    if (!ok1 || !ok2 || b == 0.0) return false;
    *out = a / b;
    return true;
  }
  bool ok = false;
  const double v = s.toDouble(&ok);
  if (!ok || !(v > 0.0)) return false;
  *out = v;
  return true;
}

QString existingValue(anpcpp::AnpNetwork& root,
                      const QString& userId,
                      const JudgmentRow& row) {
  const std::string uid = userId.toStdString();
  if (row.kind == QStringLiteral("pairwise")) {
    anpcpp::AnpNetwork* found =
        findNetworkWithNode(root, row.wrt.toStdString());
    if (found == nullptr) return {};
    const anpcpp::AnpNode* n = found->find_node(row.wrt.toStdString());
    if (n == nullptr) return {};
    const auto* slot = n->node_prioritizer(row.destCluster.toStdString());
    if (slot == nullptr) return {};
    const auto it = slot->user_pairwise.find(uid);
    if (it == slot->user_pairwise.end()) return {};
    try {
      return formatValue(it->second.comparison(row.altA.toStdString(),
                                               row.altB.toStdString()));
    } catch (...) {
      return {};
    }
  }
  if (row.kind == QStringLiteral("cluster_pairwise")) {
    anpcpp::AnpNetwork* found =
        findNetworkWithCluster(root, row.wrt.toStdString());
    if (found == nullptr) return {};
    const anpcpp::AnpCluster* c = found->find_cluster(row.wrt.toStdString());
    if (c == nullptr) return {};
    const auto& map = c->user_cluster_pairwise();
    const auto it = map.find(uid);
    if (it == map.end()) return {};
    try {
      return formatValue(it->second.comparison(row.altA.toStdString(),
                                               row.altB.toStdString()));
    } catch (...) {
      return {};
    }
  }
  if (row.kind == QStringLiteral("ratings")) {
    anpcpp::AnpNetwork* found =
        findNetworkWithNode(root, row.wrt.toStdString());
    if (found == nullptr) return {};
    const anpcpp::AnpNode* n = found->find_node(row.wrt.toStdString());
    if (n == nullptr) return {};
    const auto* slot = n->node_prioritizer(row.destCluster.toStdString());
    if (slot == nullptr) return {};
    const auto it = slot->user_ratings.find(uid);
    if (it == slot->user_ratings.end()) return {};
    const auto& rt = it->second;
    if (rt.mode() == anpcpp::RatingsPrioritizer::Mode::Categorical) {
      const auto cat = rt.rating(row.alt.toStdString());
      if (!cat) return {};
      return QString::fromStdString(*cat);
    }
    const auto raw = rt.value(row.alt.toStdString());
    if (!raw) return {};
    return formatValue(*raw);
  }
  return {};
}

QString cellAsString(XLWorksheet& wks, uint32_t row, uint16_t col) {
  try {
    XLCell cell = wks.cell(row, col);
    if (cell.empty()) return {};
    XLCellValue v = cell.value();
    switch (v.type()) {
      case XLValueType::Empty:
        return {};
      case XLValueType::Boolean:
        return static_cast<bool>(v) ? QStringLiteral("true")
                                    : QStringLiteral("false");
      case XLValueType::Integer:
        return QString::number(static_cast<qint64>(v));
      case XLValueType::Float: {
        const double d = static_cast<double>(v);
        if (std::abs(d - std::round(d)) < 1e-9)
          return QString::number(static_cast<qint64>(std::llround(d)));
        return QString::number(d, 'g', 12);
      }
      case XLValueType::String:
        return QString::fromStdString(v.getString()).trimmed();
      case XLValueType::Error:
        return {};
      default:
        return QString::fromStdString(v.getString()).trimmed();
    }
  } catch (...) {
    return {};
  }
}

void setCellString(XLWorksheet& wks, uint32_t row, uint16_t col,
                   const QString& text) {
  wks.cell(row, col).value() = text.toStdString();
}

QStringList parseCsvLine(const QString& line) {
  QStringList fields;
  QString cur;
  bool inQuotes = false;
  for (int i = 0; i < line.size(); ++i) {
    const QChar c = line[i];
    if (inQuotes) {
      if (c == QLatin1Char('"')) {
        if (i + 1 < line.size() && line[i + 1] == QLatin1Char('"')) {
          cur += QLatin1Char('"');
          ++i;
        } else {
          inQuotes = false;
        }
      } else {
        cur += c;
      }
    } else if (c == QLatin1Char('"')) {
      inQuotes = true;
    } else if (c == QLatin1Char(',')) {
      fields << cur;
      cur.clear();
    } else {
      cur += c;
    }
  }
  fields << cur;
  return fields;
}

struct ResolvedParticipant {
  QString id;
  bool created = false;
};

ResolvedParticipant resolveOrCreate(Document& doc,
                                    const QString& id,
                                    const QString& name,
                                    const QString& email) {
  ResolvedParticipant out;
  const QString idTrim = id.trimmed();
  const QString nameTrim = name.trimmed();
  const QString emailTrim = email.trimmed();

  if (!idTrim.isEmpty() &&
      doc.root().find_participant(idTrim.toStdString()) != nullptr) {
    out.id = idTrim;
    const anpcpp::JudgmentParticipant* p =
        doc.root().find_participant(idTrim.toStdString());
    const QString keepName =
        nameTrim.isEmpty() ? QString::fromStdString(p->name) : nameTrim;
    doc.addParticipant(out.id, keepName, emailTrim);
    return out;
  }

  const auto& parts = doc.root().participants();
  if (!emailTrim.isEmpty()) {
    for (const auto& p : parts) {
      if (QString::fromStdString(p.email).compare(emailTrim,
                                                  Qt::CaseInsensitive) == 0) {
        out.id = QString::fromStdString(p.id);
        return out;
      }
    }
  }
  if (!nameTrim.isEmpty()) {
    for (const auto& p : parts) {
      if (QString::fromStdString(p.name).compare(nameTrim,
                                                 Qt::CaseInsensitive) == 0) {
        out.id = QString::fromStdString(p.id);
        if (!emailTrim.isEmpty() && p.email.empty()) {
          doc.addParticipant(out.id, nameTrim, emailTrim);
        }
        return out;
      }
    }
  }

  QString base = idTrim;
  if (base.isEmpty()) {
    base = judgmentTemplateFileStem(!emailTrim.isEmpty() ? emailTrim : nameTrim)
               .toLower();
    base.replace(QLatin1Char('-'), QLatin1Char('_'));
  }
  if (base.isEmpty()) base = QStringLiteral("participant");
  QString newId = base;
  int n = 2;
  while (doc.root().find_participant(newId.toStdString()) != nullptr) {
    newId = base + QStringLiteral("_") + QString::number(n++);
  }
  const QString display = !nameTrim.isEmpty()
                              ? nameTrim
                              : (!emailTrim.isEmpty() ? emailTrim : newId);
  doc.addParticipant(newId, display, emailTrim);
  out.id = newId;
  out.created = true;
  return out;
}

struct PendingRow {
  QString kind;
  QString wrt;
  QString dest;
  QString altA;
  QString altB;
  QString alt;
  QString value;
  QString tag;
};

struct TemplateUnit {
  QString label;
  QString pid;
  QString pname;
  QString pemail;
  QVector<PendingRow> pending;
};

bool parseTemplateUnit(const QVector<QStringList>& rows, TemplateUnit* unit) {
  if (unit == nullptr) return false;
  bool inTable = false;
  bool sawKindHeader = false;
  QStringList headers;
  for (const QStringList& fieldsIn : rows) {
    QStringList fields = fieldsIn;
    while (!fields.isEmpty() && fields.last().trimmed().isEmpty())
      fields.removeLast();
    if (fields.isEmpty()) continue;
    if (fields[0].trimmed().startsWith(QLatin1Char('#'))) continue;

    if (!inTable) {
      if (fields.size() >= 2) {
        const QString key = fields[0].trimmed().toLower();
        const QString val = fields[1].trimmed();
        if (key == QStringLiteral("participant_id")) {
          if (val.compare(QStringLiteral("participant_name"),
                          Qt::CaseInsensitive) == 0)
            return false;
          unit->pid = val;
          continue;
        }
        if (key == QStringLiteral("participant_name")) {
          unit->pname = val;
          continue;
        }
        if (key == QStringLiteral("participant_email")) {
          unit->pemail = val;
          continue;
        }
      }
      if (fields[0].trimmed().toLower() == QStringLiteral("kind")) {
        headers.clear();
        for (const QString& h : fields) headers << h.trimmed().toLower();
        inTable = true;
        sawKindHeader = true;
      }
      continue;
    }
    auto col = [&](const char* name) -> QString {
      const int i = headers.indexOf(QLatin1String(name));
      if (i < 0 || i >= fields.size()) return {};
      return fields[i].trimmed();
    };
    PendingRow row;
    row.kind = col("kind");
    row.wrt = col("wrt");
    row.dest = col("dest_cluster");
    row.altA = col("alt_a");
    row.altB = col("alt_b");
    row.alt = col("alt");
    row.value = col("value");
    row.tag = col("anp_tag");
    if (row.value.isEmpty()) continue;
    unit->pending.push_back(std::move(row));
  }
  if (!sawKindHeader) return false;
  return !(unit->pid.isEmpty() && unit->pname.isEmpty() &&
           unit->pemail.isEmpty());
}

QVector<QStringList> loadCsvRows(const QString& path, QString* error) {
  QVector<QStringList> rows;
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    if (error) *error = QStringLiteral("Could not open %1").arg(path);
    return rows;
  }
  QTextStream in(&file);
  in.setEncoding(QStringConverter::Utf8);
  while (!in.atEnd()) {
    QString line = in.readLine();
    if (!line.isEmpty() && line[0] == QChar(0xfeff)) line.remove(0, 1);
    if (line.trimmed().isEmpty()) {
      rows.push_back({});
      continue;
    }
    rows.push_back(parseCsvLine(line));
  }
  return rows;
}

void applyTemplateUnit(Document& doc,
                       const TemplateUnit& unit,
                       JudgmentTemplateImportResult* out) {
  const ResolvedParticipant who =
      resolveOrCreate(doc, unit.pid, unit.pname, unit.pemail);
  if (who.created) {
    ++out->participantsCreated;
    out->createdParticipantNames << (unit.pname.isEmpty()
                                         ? (unit.pemail.isEmpty() ? who.id
                                                                  : unit.pemail)
                                         : unit.pname);
  }

  anpcpp::AnpNetwork& root = doc.root();
  for (const PendingRow& row : unit.pending) {
    QString kind = row.kind.toLower();
    if (kind.isEmpty() && row.tag.startsWith(QStringLiteral("[anp:pw|")))
      kind = QStringLiteral("pairwise");
    if (kind.isEmpty() && row.tag.startsWith(QStringLiteral("[anp:cpw|")))
      kind = QStringLiteral("cluster_pairwise");
    if (kind.isEmpty() && row.tag.startsWith(QStringLiteral("[anp:rt|")))
      kind = QStringLiteral("ratings");

    try {
      if (kind == QStringLiteral("pairwise")) {
        QString wrt = row.wrt;
        QString a = row.altA;
        QString b = row.altB;
        if (wrt.isEmpty() && row.tag.startsWith(QStringLiteral("[anp:pw|"))) {
          const QString inner = row.tag.mid(8, row.tag.size() - 9);
          const QStringList p = inner.split(QLatin1Char('|'));
          if (p.size() == 4) {
            wrt = p[0];
            a = p[2];
            b = p[3];
          }
        }
        double value = 0.0;
        if (!parseRatio(row.value, &value)) {
          ++out->judgmentsSkipped;
          continue;
        }
        anpcpp::AnpNetwork* net = findNetworkWithNode(root, wrt.toStdString());
        if (net == nullptr) {
          ++out->judgmentsSkipped;
          continue;
        }
        net->set_node_comparison_for(who.id.toStdString(), wrt.toStdString(),
                                     a.toStdString(), b.toStdString(), value);
        ++out->judgmentsSet;
      } else if (kind == QStringLiteral("cluster_pairwise")) {
        QString wrt = row.wrt;
        QString a = row.altA;
        QString b = row.altB;
        if (wrt.isEmpty() && row.tag.startsWith(QStringLiteral("[anp:cpw|"))) {
          const QString inner = row.tag.mid(9, row.tag.size() - 10);
          const QStringList p = inner.split(QLatin1Char('|'));
          if (p.size() == 3) {
            wrt = p[0];
            a = p[1];
            b = p[2];
          }
        }
        double value = 0.0;
        if (!parseRatio(row.value, &value)) {
          ++out->judgmentsSkipped;
          continue;
        }
        anpcpp::AnpNetwork* net =
            findNetworkWithCluster(root, wrt.toStdString());
        if (net == nullptr) {
          ++out->judgmentsSkipped;
          continue;
        }
        net->set_cluster_comparison_for(who.id.toStdString(), wrt.toStdString(),
                                        a.toStdString(), b.toStdString(),
                                        value);
        ++out->judgmentsSet;
      } else if (kind == QStringLiteral("ratings")) {
        QString wrt = row.wrt;
        QString alt = row.alt;
        if (wrt.isEmpty() && row.tag.startsWith(QStringLiteral("[anp:rt|"))) {
          const QString inner = row.tag.mid(8, row.tag.size() - 9);
          const QStringList p = inner.split(QLatin1Char('|'));
          if (p.size() == 3) {
            wrt = p[0];
            alt = p[2];
          }
        }
        anpcpp::AnpNetwork* net = findNetworkWithNode(root, wrt.toStdString());
        if (net == nullptr) {
          ++out->judgmentsSkipped;
          continue;
        }
        bool asNumber = false;
        const double num = row.value.toDouble(&asNumber);
        if (asNumber && !row.value.contains(QLatin1Char('/'))) {
          net->set_node_rating_value_for(who.id.toStdString(),
                                         wrt.toStdString(), alt.toStdString(),
                                         num);
        } else {
          QString cat = row.value;
          static const QRegularExpression re(
              QStringLiteral(R"(\(([^)]+)\)\s*$)"));
          const auto m = re.match(cat);
          if (m.hasMatch()) cat = m.captured(1).trimmed();
          net->set_node_rating_for(who.id.toStdString(), wrt.toStdString(),
                                   alt.toStdString(), cat.toStdString());
        }
        ++out->judgmentsSet;
      } else {
        ++out->judgmentsSkipped;
      }
    } catch (...) {
      ++out->judgmentsSkipped;
    }
  }
}

bool tryLoadRespondentWorkbook(const QString& path, TemplateUnit* unit,
                               QString* error) {
  if (unit == nullptr) return false;
  try {
    XLDocument doc;
    doc.open(path.toStdString());
    XLWorkbook wb = doc.workbook();
    if (!wb.sheetExists("Your judgments") || !wb.sheetExists("_meta")) {
      return false;
    }
    XLWorksheet human = wb.worksheet("Your judgments");
    XLWorksheet meta = wb.worksheet("_meta");

    // Identity from _meta A/B key-value rows.
    for (uint32_t r = 1; r <= 20; ++r) {
      const QString key = cellAsString(meta, r, 1).toLower();
      const QString val = cellAsString(meta, r, 2);
      if (key == QStringLiteral("participant_id")) unit->pid = val;
      else if (key == QStringLiteral("participant_name")) unit->pname = val;
      else if (key == QStringLiteral("participant_email")) unit->pemail = val;
      else if (key == QStringLiteral("template_version")) {
        if (!val.isEmpty() && val != QStringLiteral("1")) {
          if (error)
            *error = QStringLiteral("%1: unsupported template_version %2")
                         .arg(QFileInfo(path).fileName(), val);
          return false;
        }
      }
    }
    if (unit->pid.isEmpty() && unit->pname.isEmpty() &&
        unit->pemail.isEmpty()) {
      return false;
    }

    // Find header row with "row" / "anp_tag"
    uint32_t headerRow = 0;
    for (uint32_t r = 1; r <= 40; ++r) {
      if (cellAsString(meta, r, 1).toLower() == QStringLiteral("row") &&
          cellAsString(meta, r, 2).toLower() == QStringLiteral("anp_tag")) {
        headerRow = r;
        break;
      }
    }
    if (headerRow == 0) return false;

    for (uint32_t r = headerRow + 1; r <= headerRow + 5000; ++r) {
      const QString rowStr = cellAsString(meta, r, 1);
      const QString tag = cellAsString(meta, r, 2);
      const QString kind = cellAsString(meta, r, 3);
      if (rowStr.isEmpty() && tag.isEmpty()) break;
      bool ok = false;
      const int humanRow = rowStr.toInt(&ok);
      if (!ok || humanRow < 1 || tag.isEmpty()) continue;
      const QString value = cellAsString(human, static_cast<uint32_t>(humanRow),
                                         3);  // column C = Your rating
      if (value.isEmpty()) continue;
      PendingRow pr;
      pr.kind = kind;
      pr.tag = tag;
      pr.value = value;
      unit->pending.push_back(std::move(pr));
    }
    unit->label = QFileInfo(path).fileName();
    return true;
  } catch (const std::exception& ex) {
    if (error)
      *error = QStringLiteral("%1: %2")
                   .arg(QFileInfo(path).fileName(),
                        QString::fromUtf8(ex.what()));
    return false;
  } catch (...) {
    return false;
  }
}

QVector<TemplateUnit> loadTemplateUnits(const QString& path, QString* error) {
  QVector<TemplateUnit> units;
  const QString lower = path.toLower();
  if (lower.endsWith(QStringLiteral(".csv"))) {
    const QVector<QStringList> rows = loadCsvRows(path, error);
    if (rows.isEmpty() && error && !error->isEmpty()) return units;
    TemplateUnit u;
    u.label = QFileInfo(path).fileName();
    if (parseTemplateUnit(rows, &u)) {
      units.push_back(std::move(u));
    } else if (error) {
      *error = QStringLiteral(
                   "%1: no participant_id / participant_name / "
                   "participant_email (filename is ignored).")
                   .arg(u.label);
    }
    return units;
  }

  if (lower.endsWith(QStringLiteral(".xlsx"))) {
    TemplateUnit respondent;
    QString respErr;
    if (tryLoadRespondentWorkbook(path, &respondent, &respErr)) {
      units.push_back(std::move(respondent));
      return units;
    }
    if (!respErr.isEmpty() &&
        respErr.contains(QStringLiteral("unsupported template_version"))) {
      if (error) *error = respErr;
      return units;
    }

    // Legacy flat sheets.
    QString xerr;
    const QVector<XlsxSheet> sheets = readXlsxSheets(path, &xerr);
    if (sheets.isEmpty()) {
      if (error)
        *error = xerr.isEmpty()
                     ? QStringLiteral("Could not open Excel file %1").arg(path)
                     : xerr;
      return units;
    }
    for (const XlsxSheet& sheet : sheets) {
      TemplateUnit u;
      u.label = QFileInfo(path).fileName() + QStringLiteral(" [") + sheet.name +
                QLatin1Char(']');
      if (!parseTemplateUnit(sheet.rows, &u)) continue;
      units.push_back(std::move(u));
    }
    if (units.isEmpty() && error) {
      *error = QStringLiteral(
                   "%1: no recognizable judgment template sheets.")
                   .arg(QFileInfo(path).fileName());
    }
    return units;
  }

  if (error)
    *error = QStringLiteral("Unsupported file type (use .csv or .xlsx): %1")
                 .arg(QFileInfo(path).fileName());
  return units;
}

}  // namespace

QString judgmentTemplateFileStem(const QString& displayName) {
  QString s = displayName.trimmed();
  if (s.isEmpty()) s = QStringLiteral("participant");
  QString out;
  for (const QChar& c : s) {
    if (c.isLetterOrNumber()) {
      out += c;
    } else if (c.isSpace() || c == QLatin1Char('-') || c == QLatin1Char('_')) {
      if (!out.isEmpty() && !out.endsWith(QLatin1Char('_')))
        out += QLatin1Char('_');
    }
  }
  while (out.endsWith(QLatin1Char('_'))) out.chop(1);
  if (out.isEmpty()) out = QStringLiteral("participant");
  if (out.size() > 60) out = out.left(60);
  return out;
}

QString judgmentTemplateFileName(
    const anpcpp::JudgmentParticipant& participant) {
  const QString name = QString::fromStdString(participant.name);
  const QString id = QString::fromStdString(participant.id);
  const QString stem = judgmentTemplateFileStem(!name.isEmpty() ? name : id);
  return QStringLiteral("ANP_judgments_%1.xlsx").arg(stem);
}

bool writeJudgmentTemplateXlsx(const anpcpp::AnpNetwork& root,
                               const anpcpp::JudgmentParticipant& participant,
                               const QString& filePath,
                               const JudgmentTemplateExportOptions& options,
                               QString* error) {
  QVector<JudgmentRow> rows;
  collectRows(root, rows);
  const QString pid = QString::fromStdString(participant.id);
  const QString pname = QString::fromStdString(participant.name);
  const QString pemail = QString::fromStdString(participant.email);
  const QString display = !pname.isEmpty() ? pname : pid;

  try {
    XLDocument doc;
    doc.create(filePath.toStdString(), XLForceOverwrite);
    XLWorkbook wb = doc.workbook();
    XLWorksheet human = wb.worksheet(1);
    human.setName("Your judgments");
    wb.addWorksheet("_meta");
    XLWorksheet meta = wb.worksheet("_meta");
    meta.setVisibility(XLSheetState::Hidden);

    // Styles: bold header + yellow fill for rating column.
    XLFonts& fonts = doc.styles().fonts();
    XLFills& fills = doc.styles().fills();
    XLCellFormats& cellFormats = doc.styles().cellFormats();

    const XLStyleIndex boldFont = fonts.create();
    fonts[boldFont].setBold();
    fonts[boldFont].setFontSize(14);
    const XLStyleIndex boldFmt = cellFormats.create();
    cellFormats[boldFmt].setFontIndex(boldFont);
    cellFormats[boldFmt].setApplyFont(true);

    const XLStyleIndex headerFont = fonts.create();
    fonts[headerFont].setBold();
    const XLStyleIndex headerFmt = cellFormats.create();
    cellFormats[headerFmt].setFontIndex(headerFont);
    cellFormats[headerFmt].setApplyFont(true);

    const XLStyleIndex sectionFont = fonts.create();
    fonts[sectionFont].setBold();
    fonts[sectionFont].setItalic();
    const XLStyleIndex sectionFmt = cellFormats.create();
    cellFormats[sectionFmt].setFontIndex(sectionFont);
    cellFormats[sectionFmt].setApplyFont(true);

    const XLStyleIndex yellowFill = fills.create();
    fills[yellowFill].setPatternType(XLPatternSolid);
    fills[yellowFill].setColor(XLColor("FFF2CC"));  // light yellow
    const XLStyleIndex yellowFmt = cellFormats.create();
    cellFormats[yellowFmt].setFillIndex(yellowFill);
    cellFormats[yellowFmt].setApplyFill(true);

    // --- Human sheet ---
    setCellString(human, 1, 1,
                  QStringLiteral("Judgments for %1").arg(display));
    human.cell(1, 1).setCellFormat(boldFmt);

    setCellString(
        human, 2, 1,
        QStringLiteral(
            "Fill only the yellow cells in the \"Your rating\" column. "
            "Save this file and return it. Do not rename the sheets."));
    setCellString(
        human, 3, 1,
        QStringLiteral(
            "Scale (pairwise): 9 = extreme preference for the first item; "
            "1 = equal; 1/9 = extreme preference for the second. "
            "You may also enter 2–8 or fractions like 1/3."));

    setCellString(human, 5, 1, QStringLiteral("#"));
    setCellString(human, 5, 2, QStringLiteral("Comparison"));
    setCellString(human, 5, 3, QStringLiteral("Your rating"));
    setCellString(human, 5, 4, QStringLiteral("Notes (optional)"));
    human.cell(5, 1).setCellFormat(headerFmt);
    human.cell(5, 2).setCellFormat(headerFmt);
    human.cell(5, 3).setCellFormat(headerFmt);
    human.cell(5, 4).setCellFormat(headerFmt);

    // --- Meta identity ---
    setCellString(meta, 1, 1, QStringLiteral("participant_id"));
    setCellString(meta, 1, 2, pid);
    setCellString(meta, 2, 1, QStringLiteral("participant_name"));
    setCellString(meta, 2, 2, pname);
    setCellString(meta, 3, 1, QStringLiteral("participant_email"));
    setCellString(meta, 3, 2, pemail);
    setCellString(meta, 4, 1, QStringLiteral("template_version"));
    setCellString(meta, 4, 2, QStringLiteral("1"));
    setCellString(meta, 6, 1, QStringLiteral("row"));
    setCellString(meta, 6, 2, QStringLiteral("anp_tag"));
    setCellString(meta, 6, 3, QStringLiteral("kind"));

    anpcpp::AnpNetwork& mutableRoot = const_cast<anpcpp::AnpNetwork&>(root);
    uint32_t humanRow = 6;
    uint32_t metaRow = 7;
    int questionNum = 0;
    QString lastSection;

    for (const JudgmentRow& r : rows) {
      const QString section = sectionTitle(r);
      if (section != lastSection) {
        lastSection = section;
        setCellString(human, humanRow, 1, section);
        human.cell(humanRow, 1).setCellFormat(sectionFmt);
        ++humanRow;
      }
      ++questionNum;
      setCellString(human, humanRow, 1, QString::number(questionNum));
      setCellString(human, humanRow, 2, comparisonText(r));
      QString value;
      if (options.includeExistingVotes)
        value = existingValue(mutableRoot, pid, r);
      if (!value.isEmpty()) setCellString(human, humanRow, 3, value);
      human.cell(humanRow, 3).setCellFormat(yellowFmt);

      meta.cell(metaRow, 1).value() = static_cast<int64_t>(humanRow);
      setCellString(meta, metaRow, 2, r.anpTag);
      setCellString(meta, metaRow, 3, r.kind);

      ++humanRow;
      ++metaRow;
    }

    human.column(1).setWidth(6);
    human.column(2).setWidth(72);
    human.column(3).setWidth(14);
    human.column(4).setWidth(24);

    doc.save();
    doc.close();
    return true;
  } catch (const std::exception& ex) {
    if (error)
      *error = QStringLiteral("Excel export failed: %1")
                   .arg(QString::fromUtf8(ex.what()));
    return false;
  } catch (...) {
    if (error) *error = QStringLiteral("Excel export failed.");
    return false;
  }
}

bool writeJudgmentTemplateCsv(const anpcpp::AnpNetwork& root,
                              const anpcpp::JudgmentParticipant& participant,
                              const QString& filePath,
                              const JudgmentTemplateExportOptions& options,
                              QString* error) {
  QVector<JudgmentRow> rows;
  collectRows(root, rows);

  QSaveFile file(filePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    if (error) *error = QStringLiteral("Could not write %1").arg(filePath);
    return false;
  }
  QTextStream out(&file);
  out.setEncoding(QStringConverter::Utf8);
  out << QChar(0xfeff);

  const QString pid = QString::fromStdString(participant.id);
  const QString pname = QString::fromStdString(participant.name);
  const QString pemail = QString::fromStdString(participant.email);

  out << "# ANP Studio judgment template (legacy CSV)\n";
  out << "participant_id," << csvEscape(pid) << "\n";
  out << "participant_name," << csvEscape(pname) << "\n";
  out << "participant_email," << csvEscape(pemail) << "\n";
  out << "\n";
  out << "kind,wrt,dest_cluster,alt_a,alt_b,alt,value,anp_tag,value_hint\n";

  anpcpp::AnpNetwork& mutableRoot = const_cast<anpcpp::AnpNetwork&>(root);
  for (const JudgmentRow& r : rows) {
    QString value;
    if (options.includeExistingVotes) value = existingValue(mutableRoot, pid, r);
    out << csvEscape(r.kind) << ',' << csvEscape(r.wrt) << ','
        << csvEscape(r.destCluster) << ',' << csvEscape(r.altA) << ','
        << csvEscape(r.altB) << ',' << csvEscape(r.alt) << ','
        << csvEscape(value) << ',' << csvEscape(r.anpTag) << ','
        << csvEscape(r.valueHint) << '\n';
  }

  if (!file.commit()) {
    if (error) *error = QStringLiteral("Failed to save %1").arg(filePath);
    return false;
  }
  return true;
}

JudgmentTemplateExportResult exportJudgmentTemplates(
    const Document& doc,
    const QString& directory,
    const JudgmentTemplateExportOptions& options) {
  JudgmentTemplateExportResult out;
  if (directory.isEmpty() || !QDir(directory).exists()) {
    out.error = QStringLiteral("Choose an existing folder for the templates.");
    return out;
  }
  const auto& parts = doc.root().participants();
  if (parts.empty()) {
    out.error = QStringLiteral(
        "No participants in this model. Add them under Participants → "
        "Manage participants… first.");
    return out;
  }

  for (const auto& p : parts) {
    if (!options.participantIds.isEmpty() &&
        !options.participantIds.contains(QString::fromStdString(p.id))) {
      continue;
    }
    const QString path =
        QDir(directory).filePath(judgmentTemplateFileName(p));
    QString err;
    if (!writeJudgmentTemplateXlsx(doc.root(), p, path, options, &err)) {
      out.error = err;
      return out;
    }
    out.writtenPaths << path;
    ++out.filesWritten;
  }
  if (out.filesWritten == 0) {
    out.error = QStringLiteral(
        "No matching participants to export for the current selection.");
    return out;
  }
  out.ok = true;
  return out;
}

JudgmentTemplateImportResult importJudgmentTemplates(
    Document& doc,
    const QStringList& filePaths) {
  JudgmentTemplateImportResult out;
  if (filePaths.isEmpty()) {
    out.error = QStringLiteral("No files selected.");
    return out;
  }

  for (const QString& path : filePaths) {
    QString err;
    const QVector<TemplateUnit> units = loadTemplateUnits(path, &err);
    if (units.isEmpty()) {
      if (!err.isEmpty()) out.notes << err;
      continue;
    }
    for (const TemplateUnit& unit : units) {
      applyTemplateUnit(doc, unit, &out);
      ++out.filesProcessed;
    }
  }

  if (out.filesProcessed == 0) {
    out.error = out.notes.isEmpty()
                    ? QStringLiteral("No judgment templates could be imported.")
                    : out.notes.join(QLatin1Char('\n'));
    return out;
  }
  doc.rebuildEffectiveJudgments();
  out.ok = true;
  return out;
}
