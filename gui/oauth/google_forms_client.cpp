#include "oauth/google_forms_client.hpp"

/**
 * @file google_forms_client.cpp
 * @brief Create Google Forms from the model and import response judgments.
 */

#include "document.hpp"
#include "io/judgment_question_text.hpp"
#include "oauth/google_oauth.hpp"

#include "anpcpp/network.hpp"
#include "anpcpp/ratings.hpp"

#include <QCryptographicHash>
#include <QEventLoop>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>
#include <QUrlQuery>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

QJsonObject httpJson(QNetworkAccessManager& nam,
                     const QString& accessToken,
                     const QByteArray& method,
                     const QUrl& url,
                     const QByteArray& body,
                     QString* error) {
  QNetworkRequest req(url);
  req.setRawHeader("Authorization",
                   QByteArray("Bearer ") + accessToken.toUtf8());
  if (!body.isEmpty()) {
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/json"));
  }

  QEventLoop loop;
  QNetworkReply* reply = nullptr;
  if (method == "POST") {
    reply = nam.post(req, body);
  } else if (method == "GET") {
    reply = nam.get(req);
  } else {
    if (error) *error = QStringLiteral("Unsupported HTTP method");
    return {};
  }
  QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  loop.exec();

  const QByteArray raw = reply->readAll();
  const auto netErr = reply->error();
  const QString netErrStr = reply->errorString();
  const int status =
      reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  reply->deleteLater();

  if (netErr != QNetworkReply::NoError || status >= 400) {
    if (error) {
      *error = QStringLiteral("HTTP %1: %2\n%3")
                   .arg(status)
                   .arg(netErrStr)
                   .arg(QString::fromUtf8(raw));
    }
    return {};
  }
  return QJsonDocument::fromJson(raw).object();
}

QStringList saatyLabels(const QString& a, const QString& b) {
  // Leading "(value)" must stay parseable by parseSaatyValue.
  return {
      QStringLiteral("(%1) %2 is extremely more important than %3")
          .arg(QStringLiteral("9"), a, b),
      QStringLiteral("(%1) %2 is very strongly to extremely more important than %3")
          .arg(QStringLiteral("8"), a, b),
      QStringLiteral("(%1) %2 is very strongly more important than %3")
          .arg(QStringLiteral("7"), a, b),
      QStringLiteral("(%1) %2 is strongly to very strongly more important than %3")
          .arg(QStringLiteral("6"), a, b),
      QStringLiteral("(%1) %2 is strongly more important than %3")
          .arg(QStringLiteral("5"), a, b),
      QStringLiteral("(%1) %2 is moderately to strongly more important than %3")
          .arg(QStringLiteral("4"), a, b),
      QStringLiteral("(%1) %2 is moderately more important than %3")
          .arg(QStringLiteral("3"), a, b),
      QStringLiteral("(%1) %2 is slightly more important than %3")
          .arg(QStringLiteral("2"), a, b),
      QStringLiteral("(1) They are essentially equal"),
      QStringLiteral("(%1) %2 is slightly more important than %3")
          .arg(QStringLiteral("1/2"), b, a),
      QStringLiteral("(%1) %2 is moderately more important than %3")
          .arg(QStringLiteral("1/3"), b, a),
      QStringLiteral("(%1) %2 is moderately to strongly more important than %3")
          .arg(QStringLiteral("1/4"), b, a),
      QStringLiteral("(%1) %2 is strongly more important than %3")
          .arg(QStringLiteral("1/5"), b, a),
      QStringLiteral("(%1) %2 is strongly to very strongly more important than %3")
          .arg(QStringLiteral("1/6"), b, a),
      QStringLiteral("(%1) %2 is very strongly more important than %3")
          .arg(QStringLiteral("1/7"), b, a),
      QStringLiteral("(%1) %2 is very strongly to extremely more important than %3")
          .arg(QStringLiteral("1/8"), b, a),
      QStringLiteral("(%1) %2 is extremely more important than %3")
          .arg(QStringLiteral("1/9"), b, a),
  };
}

QString formInfoDescription() {
  return QStringLiteral(
      "Fill only the judgment questions below. "
      "For pairwise comparisons, choose how much more important one item is "
      "than the other (1 = equal; 9 = extreme preference for the first-named "
      "item; 1/9 = extreme preference for the second). "
      "Save is automatic when you submit.");
}

QJsonObject choiceQuestion(const QString& title, const QStringList& options,
                           bool required,
                           const QString& description = {}) {
  QJsonArray opts;
  for (const QString& o : options) {
    opts.append(QJsonObject{{QStringLiteral("value"), o}});
  }
  QJsonObject choice;
  choice.insert(QStringLiteral("type"), QStringLiteral("RADIO"));
  choice.insert(QStringLiteral("options"), opts);

  QJsonObject question;
  question.insert(QStringLiteral("required"), required);
  question.insert(QStringLiteral("choiceQuestion"), choice);

  QJsonObject questionItem;
  questionItem.insert(QStringLiteral("question"), question);

  QJsonObject item;
  item.insert(QStringLiteral("title"), title);
  if (!description.isEmpty())
    item.insert(QStringLiteral("description"), description);
  item.insert(QStringLiteral("questionItem"), questionItem);
  return item;
}

QJsonObject textQuestion(const QString& title, bool required,
                         const QString& description = {}) {
  QJsonObject question;
  question.insert(QStringLiteral("required"), required);
  question.insert(QStringLiteral("textQuestion"), QJsonObject());

  QJsonObject questionItem;
  questionItem.insert(QStringLiteral("question"), question);

  QJsonObject item;
  item.insert(QStringLiteral("title"), title);
  if (!description.isEmpty())
    item.insert(QStringLiteral("description"), description);
  item.insert(QStringLiteral("questionItem"), questionItem);
  return item;
}

QJsonObject sectionBreakItem(const QString& title) {
  QJsonObject item;
  item.insert(QStringLiteral("title"), title);
  item.insert(QStringLiteral("pageBreakItem"), QJsonObject());
  return item;
}

void appendCreateItem(QJsonArray& requests, int index, const QJsonObject& item) {
  QJsonObject location;
  location.insert(QStringLiteral("index"), index);
  QJsonObject createItem;
  createItem.insert(QStringLiteral("item"), item);
  createItem.insert(QStringLiteral("location"), location);
  QJsonObject req;
  req.insert(QStringLiteral("createItem"), createItem);
  requests.append(req);
}

struct FormQuestionDraft {
  QString tag;
  QString title;
  QString section;  // empty = no section change
  bool choice = true;
  bool saatyScale = false;
  QStringList options;
  QString description;
};

void collectQuestionDrafts(const anpcpp::AnpNetwork& net,
                           QVector<FormQuestionDraft>& out) {
  for (const anpcpp::AnpNode* n : net.nodes()) {
    for (const anpcpp::AnpCluster* dest : net.clusters()) {
      const anpcpp::NodePrioritizerSlot* slot =
          n->node_prioritizer(dest->name());
      if (slot == nullptr || slot->empty()) continue;

      const QString wrt = QString::fromStdString(n->name());
      const QString destName = QString::fromStdString(dest->name());
      const QString section = nodeSectionTitle(wrt, destName);

      if (slot->kind == anpcpp::NodePrioritizerKind::Pairwise) {
        const auto& alts = slot->pairwise.alternatives();
        for (std::size_t i = 0; i < alts.size(); ++i) {
          for (std::size_t j = i + 1; j < alts.size(); ++j) {
            FormQuestionDraft d;
            const QString a = QString::fromStdString(alts[i]);
            const QString b = QString::fromStdString(alts[j]);
            d.tag = QStringLiteral("[anp:pw|%1|%2|%3|%4]")
                        .arg(wrt, destName, a, b);
            d.title = pairwiseComparisonText(wrt, destName, a, b);
            d.section = section;
            d.choice = true;
            d.saatyScale = true;
            d.options = saatyLabels(a, b);
            out.push_back(std::move(d));
          }
        }
      } else {
        const auto& rt = slot->ratings;
        for (const std::string& alt : rt.alternatives()) {
          FormQuestionDraft d;
          const QString altName = QString::fromStdString(alt);
          d.tag = QStringLiteral("[anp:rt|%1|%2|%3]")
                      .arg(wrt, destName, altName);
          d.section = section;
          if (rt.mode() == anpcpp::RatingsPrioritizer::Mode::Categorical &&
              !rt.categories().empty()) {
            for (const auto& c : rt.categories()) {
              d.options << QStringLiteral("%1 (%2)")
                               .arg(QString::fromStdString(c.label),
                                    QString::fromStdString(c.id));
            }
            d.title = ratingsComparisonText(wrt, destName, altName);
            d.choice = true;
          } else {
            d.title = ratingsComparisonText(wrt, destName, altName,
                                            QStringLiteral("numeric score"));
            d.choice = false;
          }
          out.push_back(std::move(d));
        }
      }
    }
    if (n->has_subnetwork()) {
      collectQuestionDrafts(*n->subnetwork(), out);
    }
  }

  for (const anpcpp::AnpCluster* c : net.clusters()) {
    const auto& pw = c->cluster_pairwise();
    if (pw.size() < 2) continue;
    const QString cluster = QString::fromStdString(c->name());
    const QString section = clusterSectionTitle(cluster);
    const auto& alts = pw.alternatives();
    for (std::size_t i = 0; i < alts.size(); ++i) {
      for (std::size_t j = i + 1; j < alts.size(); ++j) {
        FormQuestionDraft d;
        const QString a = QString::fromStdString(alts[i]);
        const QString b = QString::fromStdString(alts[j]);
        d.tag = QStringLiteral("[anp:cpw|%1|%2|%3]").arg(cluster, a, b);
        d.title = clusterPairwiseComparisonText(cluster, a, b);
        d.section = section;
        d.choice = true;
        d.saatyScale = true;
        d.options = saatyLabels(a, b);
        out.push_back(std::move(d));
      }
    }
  }
}

QString fingerprintFromTags(QStringList tags) {
  tags.sort();
  const QByteArray payload = tags.join(QLatin1Char('\n')).toUtf8();
  return QString::fromLatin1(
      QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
}

QStringList fingerprintKeysFromDrafts(const QVector<FormQuestionDraft>& drafts) {
  QStringList keys;
  keys.reserve(drafts.size());
  for (const FormQuestionDraft& d : drafts) {
    QString k = d.tag;
    if (!d.choice) {
      k += QStringLiteral("|mode:numeric");
    } else if (d.saatyScale) {
      k += QStringLiteral("|mode:saaty");
    } else {
      k += QStringLiteral("|cats:") + d.options.join(QLatin1Char(';'));
    }
    keys << k;
  }
  return keys;
}

}  // namespace

QStringList collectGoogleFormJudgmentTags(const anpcpp::AnpNetwork& net) {
  QVector<FormQuestionDraft> drafts;
  collectQuestionDrafts(net, drafts);
  QStringList tags;
  tags.reserve(drafts.size());
  for (const FormQuestionDraft& d : drafts) tags << d.tag;
  return tags;
}

QString googleFormStructureFingerprint(const anpcpp::AnpNetwork& net) {
  QVector<FormQuestionDraft> drafts;
  collectQuestionDrafts(net, drafts);
  return fingerprintFromTags(fingerprintKeysFromDrafts(drafts));
}

bool googleFormFingerprintMatches(const QString& fingerprint,
                                  const anpcpp::AnpNetwork& net) {
  if (fingerprint.isEmpty()) return false;
  return fingerprint == googleFormStructureFingerprint(net);
}

// --- Create form from current network structure -----------------------------

GoogleFormCreateResult createGoogleFormForNetwork(
    GoogleOAuth& oauth,
    const anpcpp::AnpNetwork& net,
    const QString& formTitle) {
  GoogleFormCreateResult out;
  if (!oauth.isConnected()) {
    out.error = QStringLiteral("Connect a Google account first "
                               "(File → Settings… → Connected accounts).");
    return out;
  }
  const QString token = oauth.accessToken();
  if (token.isEmpty()) {
    out.error = QStringLiteral("Could not obtain a Google access token.");
    return out;
  }

  QNetworkAccessManager nam;
  QString err;

  // forms.create accepts only info.title; description comes via batchUpdate.
  const QJsonObject createBody{
      {QStringLiteral("info"),
       QJsonObject{{QStringLiteral("title"), formTitle}}}};
  const QJsonObject created = httpJson(
      nam, token, "POST", QUrl(QStringLiteral("https://forms.googleapis.com/v1/forms")),
      QJsonDocument(createBody).toJson(QJsonDocument::Compact), &err);
  if (created.isEmpty()) {
    out.error = err.isEmpty() ? QStringLiteral("forms.create failed") : err;
    return out;
  }

  out.formId = created.value(QStringLiteral("formId")).toString();
  out.responderUrl =
      created.value(QStringLiteral("responderUri")).toString();
  if (out.formId.isEmpty()) {
    out.error = QStringLiteral("forms.create returned no formId:\n") +
                QString::fromUtf8(QJsonDocument(created).toJson());
    return out;
  }
  out.editUrl =
      QStringLiteral("https://docs.google.com/forms/d/%1/edit").arg(out.formId);

  QJsonArray requests;
  int index = 0;

  {
    QJsonObject updateInfo;
    updateInfo.insert(
        QStringLiteral("info"),
        QJsonObject{{QStringLiteral("description"), formInfoDescription()}});
    updateInfo.insert(QStringLiteral("updateMask"),
                      QStringLiteral("description"));
    requests.append(QJsonObject{{QStringLiteral("updateFormInfo"), updateInfo}});
  }

  // Expected mapped tags for question items only (sections are skipped on GET).
  QStringList expectedMappedTags;
  expectedMappedTags << QStringLiteral("[anp:respondent]")
                     << QStringLiteral("[anp:respondent_email]");

  appendCreateItem(
      requests, index++,
      textQuestion(QStringLiteral("Your name"), true,
                   QStringLiteral("Use the name from the study roster if you "
                                  "have one.")));
  appendCreateItem(
      requests, index++,
      textQuestion(QStringLiteral("Your email (optional)"), false,
                   QStringLiteral("Used to match you to a participant when "
                                  "results are imported.")));

  QVector<FormQuestionDraft> drafts;
  collectQuestionDrafts(net, drafts);
  out.questionTags.reserve(drafts.size());
  QString lastSection;
  bool firstPairwiseInSection = true;
  for (const FormQuestionDraft& d : drafts) {
    out.questionTags << d.tag;
    if (!d.section.isEmpty() && d.section != lastSection) {
      lastSection = d.section;
      firstPairwiseInSection = true;
      appendCreateItem(requests, index++, sectionBreakItem(d.section));
    }
    QString desc = d.description;
    if (d.saatyScale && firstPairwiseInSection) {
      desc = QStringLiteral(
          "1 = equal importance. Higher numbers favor the first-named item; "
          "fractions favor the second.");
      firstPairwiseInSection = false;
    } else if (d.saatyScale) {
      firstPairwiseInSection = false;
    }
    expectedMappedTags << d.tag;
    if (d.choice) {
      appendCreateItem(requests, index++,
                       choiceQuestion(d.title, d.options, true, desc));
    } else {
      appendCreateItem(requests, index++, textQuestion(d.title, true, desc));
    }
  }
  out.structureFingerprint =
      fingerprintFromTags(fingerprintKeysFromDrafts(drafts));
  out.questionCount = static_cast<int>(expectedMappedTags.size());

  if (drafts.isEmpty()) {
    out.error =
        QStringLiteral("No pairwise or ratings judgments found to put on the "
                       "form. Add connections and judgments first.");
    out.ok = true;
    return out;
  }

  // createItem locations already hold absolute form indices; do not remap by
  // request-array position (updateFormInfo is not an item).
  constexpr int kChunk = 20;
  for (int start = 0; start < requests.size(); start += kChunk) {
    QJsonArray chunk;
    const int end = qMin(start + kChunk, static_cast<int>(requests.size()));
    for (int i = start; i < end; ++i) chunk.append(requests.at(i));
    err.clear();
    const QJsonObject updated = httpJson(
        nam, token, "POST",
        QUrl(QStringLiteral("https://forms.googleapis.com/v1/forms/%1:batchUpdate")
                 .arg(out.formId)),
        QJsonDocument(QJsonObject{{QStringLiteral("requests"), chunk}})
            .toJson(QJsonDocument::Compact),
        &err);
    if (updated.isEmpty()) {
      out.error = err.isEmpty()
                      ? QStringLiteral("batchUpdate failed")
                      : QStringLiteral("batchUpdate failed at items %1–%2:\n%3")
                            .arg(start)
                            .arg(end - 1)
                            .arg(err);
      out.ok = !out.formId.isEmpty();
      return out;
    }
  }

  // Map questionIds → tags by walking GET form items (skip page breaks).
  err.clear();
  const QJsonObject form = httpJson(
      nam, token, "GET",
      QUrl(QStringLiteral("https://forms.googleapis.com/v1/forms/%1")
               .arg(out.formId)),
      {}, &err);
  if (!form.isEmpty()) {
    int tagIndex = 0;
    for (const QJsonValue& iv : form.value(QStringLiteral("items")).toArray()) {
      const QJsonObject item = iv.toObject();
      if (item.contains(QStringLiteral("pageBreakItem"))) continue;
      const QString qid =
          item.value(QStringLiteral("questionItem"))
              .toObject()
              .value(QStringLiteral("question"))
              .toObject()
              .value(QStringLiteral("questionId"))
              .toString();
      if (qid.isEmpty()) continue;
      if (tagIndex >= expectedMappedTags.size()) break;
      out.questionIds << qid;
      out.mappedTags << expectedMappedTags.at(tagIndex);
      ++tagIndex;
    }
    if (out.questionIds.size() != expectedMappedTags.size()) {
      out.error +=
          QStringLiteral("\nWarning: questionId map incomplete (%1 of %2). "
                         "Import may fall back to title tags.")
              .arg(out.questionIds.size())
              .arg(expectedMappedTags.size());
    }
  } else if (!err.isEmpty()) {
    out.error += QStringLiteral("\nWarning: could not fetch questionIds: ") + err;
  }

  out.ok = true;
  return out;
}

namespace {

QString extractAnpTag(const QString& title) {
  static const QRegularExpression re(
      QStringLiteral(R"(\[anp:[^\]]+\])"));
  const auto m = re.match(title);
  return m.hasMatch() ? m.captured(0) : QString();
}

QHash<QString, QString> questionIdTagMap(const Document& doc,
                                         const QString& formId) {
  QHash<QString, QString> map;
  for (const LinkedGoogleForm& f : doc.linkedGoogleForms()) {
    if (f.formId != formId) continue;
    const int n = qMin(f.questionIds.size(), f.mappedTags.size());
    for (int i = 0; i < n; ++i) {
      if (!f.questionIds.at(i).isEmpty() && !f.mappedTags.at(i).isEmpty())
        map.insert(f.questionIds.at(i), f.mappedTags.at(i));
    }
    break;
  }
  return map;
}

QString resolveAnpTag(const QHash<QString, QString>& idMap,
                      const QString& questionId,
                      const QString& title) {
  const QString mapped = idMap.value(questionId);
  if (!mapped.isEmpty()) return mapped;
  return extractAnpTag(title);
}

QString answerText(const QJsonObject& answerObj) {
  // textAnswers.answers[0].value  or  choice answers similarly
  if (answerObj.contains(QStringLiteral("textAnswers"))) {
    const QJsonArray answers =
        answerObj.value(QStringLiteral("textAnswers"))
            .toObject()
            .value(QStringLiteral("answers"))
            .toArray();
    if (!answers.isEmpty()) {
      return answers.at(0).toObject().value(QStringLiteral("value")).toString().trimmed();
    }
  }
  return {};
}

bool parseSaatyValue(const QString& label, double* out) {
  if (out == nullptr) return false;
  const QString s = label.trimmed();
  // "(9) …" / "(1/3) …" / "9 — …" / "9 (extreme)" / plain "9" / "1/3"
  QString head;
  if (s.startsWith(QLatin1Char('('))) {
    const int close = s.indexOf(QLatin1Char(')'));
    if (close <= 1) return false;
    head = s.mid(1, close - 1).trimmed();
  } else {
    head = s.section(QLatin1Char(' '), 0, 0).trimmed();
  }
  if (head.contains(QLatin1Char('/'))) {
    const QStringList parts = head.split(QLatin1Char('/'));
    if (parts.size() != 2) return false;
    bool ok1 = false, ok2 = false;
    const double a = parts[0].toDouble(&ok1);
    const double b = parts[1].toDouble(&ok2);
    if (!ok1 || !ok2 || b == 0.0) return false;
    *out = a / b;
    return true;
  }
  bool ok = false;
  const double v = head.toDouble(&ok);
  if (!ok || !(v > 0.0)) return false;
  *out = v;
  return true;
}

QString parseCategoryId(const QString& label) {
  // "High (H)" → H
  static const QRegularExpression re(QStringLiteral(R"(\(([^)]+)\)\s*$)"));
  const auto m = re.match(label.trimmed());
  if (m.hasMatch()) return m.captured(1).trimmed();
  return label.trimmed();
}

QString slugifyId(const QString& nameOrEmail) {
  QString s = nameOrEmail.trimmed().toLower();
  s.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")),
            QStringLiteral("_"));
  s = s.trimmed();
  while (s.startsWith(QLatin1Char('_'))) s.remove(0, 1);
  while (s.endsWith(QLatin1Char('_'))) s.chop(1);
  if (s.isEmpty()) s = QStringLiteral("participant");
  if (s.size() > 40) s = s.left(40);
  return s;
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

struct ResolvedParticipant {
  QString id;
  bool created = false;
};

ResolvedParticipant resolveOrCreateParticipant(Document& doc,
                                               const QString& name,
                                               const QString& email) {
  ResolvedParticipant out;
  const QString nameTrim = name.trimmed();
  const QString emailTrim = email.trimmed();

  auto& parts = doc.root().participants();
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
        // Fill email if we have one and they don't.
        if (!emailTrim.isEmpty() && p.email.empty()) {
          doc.addParticipant(out.id, nameTrim, emailTrim);
        }
        return out;
      }
    }
  }

  QString base = !emailTrim.isEmpty() ? slugifyId(emailTrim)
                                      : slugifyId(nameTrim);
  if (base.isEmpty()) base = QStringLiteral("respondent");
  QString id = base;
  int n = 2;
  while (doc.root().find_participant(id.toStdString()) != nullptr) {
    id = base + QStringLiteral("_") + QString::number(n++);
  }
  const QString display =
      !nameTrim.isEmpty()
          ? nameTrim
          : (!emailTrim.isEmpty() ? emailTrim : id);
  doc.addParticipant(id, display, emailTrim);
  out.id = id;
  out.created = true;
  return out;
}

}  // namespace

// --- Import responses into Document participants / judgments ----------------

GoogleFormImportResult importGoogleFormResponses(GoogleOAuth& oauth,
                                                 Document& doc,
                                                 const QString& formId,
                                                 bool matchingOnly) {
  GoogleFormImportResult out;
  if (!oauth.isConnected()) {
    out.error = QStringLiteral("Connect a Google account first.");
    return out;
  }
  if (formId.isEmpty()) {
    out.error = QStringLiteral("No form id.");
    return out;
  }
  const QString token = oauth.accessToken();
  if (token.isEmpty()) {
    out.error = QStringLiteral("Could not obtain a Google access token.");
    return out;
  }

  QNetworkAccessManager nam;
  QString err;

  const QJsonObject form = httpJson(
      nam, token, "GET",
      QUrl(QStringLiteral("https://forms.googleapis.com/v1/forms/%1")
               .arg(formId)),
      {}, &err);
  if (form.isEmpty()) {
    out.error = err.isEmpty() ? QStringLiteral("forms.get failed") : err;
    return out;
  }

  const QHash<QString, QString> idToTag = questionIdTagMap(doc, formId);

  QHash<QString, QString> questionTitle;
  for (const QJsonValue& iv : form.value(QStringLiteral("items")).toArray()) {
    const QJsonObject item = iv.toObject();
    if (item.contains(QStringLiteral("pageBreakItem"))) continue;
    const QString qid =
        item.value(QStringLiteral("questionItem"))
            .toObject()
            .value(QStringLiteral("question"))
            .toObject()
            .value(QStringLiteral("questionId"))
            .toString();
    const QString title = item.value(QStringLiteral("title")).toString();
    if (!qid.isEmpty()) questionTitle.insert(qid, title);
  }

  const QStringList currentTags = collectGoogleFormJudgmentTags(doc.root());
  const QSet<QString> allowedTags(currentTags.begin(), currentTags.end());
  int structureSkipNotes = 0;

  // List all responses (paginate).
  QJsonArray allResponses;
  QString pageToken;
  do {
    QUrl listUrl(
        QStringLiteral("https://forms.googleapis.com/v1/forms/%1/responses")
            .arg(formId));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("pageSize"), QStringLiteral("100"));
    if (!pageToken.isEmpty()) {
      q.addQueryItem(QStringLiteral("pageToken"), pageToken);
    }
    listUrl.setQuery(q);
    err.clear();
    const QJsonObject page =
        httpJson(nam, token, "GET", listUrl, {}, &err);
    if (page.isEmpty()) {
      out.error = err.isEmpty() ? QStringLiteral("responses.list failed") : err;
      return out;
    }
    for (const QJsonValue& v : page.value(QStringLiteral("responses")).toArray()) {
      allResponses.append(v);
    }
    pageToken = page.value(QStringLiteral("nextPageToken")).toString();
  } while (!pageToken.isEmpty());

  if (allResponses.isEmpty()) {
    out.error = QStringLiteral("No responses found for this form yet.");
    return out;
  }

  for (const QJsonValue& rv : allResponses) {
    const QJsonObject resp = rv.toObject();
    const QJsonObject answers = resp.value(QStringLiteral("answers")).toObject();

    QString name;
    QString email = resp.value(QStringLiteral("respondentEmail")).toString().trimmed();

    for (auto it = answers.begin(); it != answers.end(); ++it) {
      const QString title = questionTitle.value(it.key());
      const QString tag = resolveAnpTag(idToTag, it.key(), title);
      const QString text = answerText(it.value().toObject());
      if (tag == QStringLiteral("[anp:respondent]")) {
        name = text;
        if (email.isEmpty() && text.contains(QLatin1Char('@'))) email = text;
      } else if (tag == QStringLiteral("[anp:respondent_email]")) {
        if (!text.isEmpty()) email = text;
      }
    }

    if (name.isEmpty() && email.isEmpty()) {
      out.skippedNotes << QStringLiteral("Skipped a response with no name/email.");
      continue;
    }

    const ResolvedParticipant who =
        resolveOrCreateParticipant(doc, name, email);
    if (who.created) {
      ++out.participantsCreated;
      out.createdParticipantNames << (name.isEmpty() ? email : name);
    }

    anpcpp::AnpNetwork& root = doc.root();
    for (auto it = answers.begin(); it != answers.end(); ++it) {
      const QString title = questionTitle.value(it.key());
      const QString tag = resolveAnpTag(idToTag, it.key(), title);
      if (tag.isEmpty() || tag.startsWith(QStringLiteral("[anp:respondent"))) {
        continue;
      }
      const QString text = answerText(it.value().toObject());
      if (text.isEmpty()) continue;

      if (matchingOnly && !allowedTags.contains(tag)) {
        ++out.judgmentsSkipped;
        if (structureSkipNotes < 8) {
          out.skippedNotes << QStringLiteral("Skipped outdated question: %1")
                                  .arg(tag);
          ++structureSkipNotes;
        }
        continue;
      }

      // [anp:pw|wrt|dest|A|B]
      if (tag.startsWith(QStringLiteral("[anp:pw|"))) {
        const QString inner = tag.mid(8, tag.size() - 9);  // strip [anp:pw| … ]
        const QStringList parts = inner.split(QLatin1Char('|'));
        if (parts.size() != 4) {
          ++out.judgmentsSkipped;
          continue;
        }
        double value = 0.0;
        if (!parseSaatyValue(text, &value)) {
          ++out.judgmentsSkipped;
          continue;
        }
        anpcpp::AnpNetwork* net =
            findNetworkWithNode(root, parts[0].toStdString());
        if (net == nullptr) {
          ++out.judgmentsSkipped;
          continue;
        }
        try {
          net->set_node_comparison_for(who.id.toStdString(),
                                       parts[0].toStdString(),
                                       parts[2].toStdString(),
                                       parts[3].toStdString(), value);
          ++out.judgmentsSet;
        } catch (...) {
          ++out.judgmentsSkipped;
        }
        continue;
      }

      // [anp:cpw|cluster|A|B]
      if (tag.startsWith(QStringLiteral("[anp:cpw|"))) {
        const QString inner = tag.mid(9, tag.size() - 10);
        const QStringList parts = inner.split(QLatin1Char('|'));
        if (parts.size() != 3) {
          ++out.judgmentsSkipped;
          continue;
        }
        double value = 0.0;
        if (!parseSaatyValue(text, &value)) {
          ++out.judgmentsSkipped;
          continue;
        }
        anpcpp::AnpNetwork* net =
            findNetworkWithCluster(root, parts[0].toStdString());
        if (net == nullptr) {
          ++out.judgmentsSkipped;
          continue;
        }
        try {
          net->set_cluster_comparison_for(who.id.toStdString(),
                                          parts[0].toStdString(),
                                          parts[1].toStdString(),
                                          parts[2].toStdString(), value);
          ++out.judgmentsSet;
        } catch (...) {
          ++out.judgmentsSkipped;
        }
        continue;
      }

      // [anp:rt|wrt|dest|alt]
      if (tag.startsWith(QStringLiteral("[anp:rt|"))) {
        const QString inner = tag.mid(8, tag.size() - 9);
        const QStringList parts = inner.split(QLatin1Char('|'));
        if (parts.size() != 3) {
          ++out.judgmentsSkipped;
          continue;
        }
        anpcpp::AnpNetwork* net =
            findNetworkWithNode(root, parts[0].toStdString());
        if (net == nullptr) {
          ++out.judgmentsSkipped;
          continue;
        }
        try {
          // Prefer categorical if label has (id); else numeric.
          if (text.contains(QLatin1Char('('))) {
            const QString catId = parseCategoryId(text);
            net->set_node_rating_for(who.id.toStdString(),
                                     parts[0].toStdString(),
                                     parts[2].toStdString(),
                                     catId.toStdString());
          } else {
            bool ok = false;
            const double v = text.toDouble(&ok);
            if (!ok) {
              ++out.judgmentsSkipped;
              continue;
            }
            net->set_node_rating_value_for(who.id.toStdString(),
                                           parts[0].toStdString(),
                                           parts[2].toStdString(), v);
          }
          ++out.judgmentsSet;
        } catch (...) {
          ++out.judgmentsSkipped;
        }
      }
    }
    ++out.responsesProcessed;
  }

  if (structureSkipNotes >= 8 && out.judgmentsSkipped > structureSkipNotes) {
    out.skippedNotes << QStringLiteral(
        "…additional outdated questions were skipped.");
  }

  doc.rebuildEffectiveJudgments();
  out.ok = true;
  return out;
}
