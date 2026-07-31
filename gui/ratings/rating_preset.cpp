#include "ratings/rating_preset.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

#include <type_traits>
#include <variant>

namespace {

QJsonObject interpreterToJson(const anpcpp::ScoreInterpreter& interpreter) {
  QJsonObject o;
  std::visit(
      [&](const auto& interp) {
        using T = std::decay_t<decltype(interp)>;
        if constexpr (std::is_same_v<T, anpcpp::IdentityInterpreter>) {
          o.insert(QStringLiteral("type"), QStringLiteral("identity"));
        } else if constexpr (std::is_same_v<T, anpcpp::DivideByMaxInterpreter>) {
          o.insert(QStringLiteral("type"), QStringLiteral("divide_by_max"));
        } else if constexpr (std::is_same_v<T,
                                            anpcpp::DivideByConstantInterpreter>) {
          o.insert(QStringLiteral("type"), QStringLiteral("divide_by_constant"));
          o.insert(QStringLiteral("constant"), interp.constant);
        } else if constexpr (std::is_same_v<T,
                                            anpcpp::MinMaxNormalizeInterpreter>) {
          o.insert(QStringLiteral("type"), QStringLiteral("minmax"));
        } else if constexpr (std::is_same_v<T,
                                            anpcpp::PiecewiseLinearInterpreter>) {
          o.insert(QStringLiteral("type"), QStringLiteral("piecewise"));
          QJsonArray knots;
          for (const auto& [x, y] : interp.knots) {
            QJsonObject k;
            k.insert(QStringLiteral("x"), x);
            k.insert(QStringLiteral("y"), y);
            knots.append(k);
          }
          o.insert(QStringLiteral("knots"), knots);
        }
      },
      interpreter);
  return o;
}

anpcpp::ScoreInterpreter interpreterFromJson(const QJsonObject& o) {
  const QString type = o.value(QStringLiteral("type")).toString();
  if (type == QLatin1String("divide_by_max")) {
    return anpcpp::DivideByMaxInterpreter{};
  }
  if (type == QLatin1String("divide_by_constant")) {
    return anpcpp::DivideByConstantInterpreter{
        o.value(QStringLiteral("constant")).toDouble(1.0)};
  }
  if (type == QLatin1String("minmax")) {
    return anpcpp::MinMaxNormalizeInterpreter{};
  }
  if (type == QLatin1String("piecewise")) {
    anpcpp::PiecewiseLinearInterpreter pl;
    const QJsonArray knots = o.value(QStringLiteral("knots")).toArray();
    for (const QJsonValue& v : knots) {
      const QJsonObject k = v.toObject();
      pl.knots.emplace_back(k.value(QStringLiteral("x")).toDouble(),
                            k.value(QStringLiteral("y")).toDouble());
    }
    return pl;
  }
  return anpcpp::IdentityInterpreter{};
}

RatingPreset presetFromObject(const QJsonObject& o,
                              RatingPresetSource defaultSource) {
  RatingPreset p;
  p.id = o.value(QStringLiteral("id")).toString();
  p.name = o.value(QStringLiteral("name")).toString();
  p.description = o.value(QStringLiteral("description")).toString();
  const QString mode = o.value(QStringLiteral("mode")).toString();
  p.mode = (mode == QLatin1String("numeric"))
               ? anpcpp::RatingsPrioritizer::Mode::Numeric
               : anpcpp::RatingsPrioritizer::Mode::Categorical;
  if (o.contains(QStringLiteral("categories"))) {
    for (const QJsonValue& cv : o.value(QStringLiteral("categories")).toArray()) {
      const QJsonObject c = cv.toObject();
      anpcpp::RatingCategory cat;
      cat.id = c.value(QStringLiteral("id")).toString().toStdString();
      cat.label = c.value(QStringLiteral("label")).toString().toStdString();
      cat.value = c.value(QStringLiteral("value")).toDouble();
      p.categories.push_back(std::move(cat));
    }
  }
  if (o.contains(QStringLiteral("interpreter"))) {
    p.interpreter =
        interpreterFromJson(o.value(QStringLiteral("interpreter")).toObject());
  }
  const QString src = o.value(QStringLiteral("source")).toString();
  if (src == QLatin1String("builtin")) {
    p.source = RatingPresetSource::BuiltIn;
  } else if (src == QLatin1String("user")) {
    p.source = RatingPresetSource::User;
  } else {
    p.source = defaultSource;
  }
  return p;
}

QJsonObject presetToObject(const RatingPreset& p) {
  QJsonObject o;
  o.insert(QStringLiteral("id"), p.id);
  o.insert(QStringLiteral("name"), p.name);
  o.insert(QStringLiteral("description"), p.description);
  o.insert(QStringLiteral("mode"),
           p.mode == anpcpp::RatingsPrioritizer::Mode::Numeric
               ? QStringLiteral("numeric")
               : QStringLiteral("categorical"));
  o.insert(QStringLiteral("source"),
           p.source == RatingPresetSource::BuiltIn ? QStringLiteral("builtin")
                                                   : QStringLiteral("user"));
  if (p.mode == anpcpp::RatingsPrioritizer::Mode::Categorical) {
    QJsonArray cats;
    for (const auto& c : p.categories) {
      QJsonObject cj;
      cj.insert(QStringLiteral("id"), QString::fromStdString(c.id));
      cj.insert(QStringLiteral("label"), QString::fromStdString(c.label));
      cj.insert(QStringLiteral("value"), c.value);
      cats.append(cj);
    }
    o.insert(QStringLiteral("categories"), cats);
  } else {
    o.insert(QStringLiteral("interpreter"), interpreterToJson(p.interpreter));
  }
  return o;
}

}  // namespace

QVector<RatingPreset> ratingPresetsFromJson(const QByteArray& jsonBytes,
                                            RatingPresetSource defaultSource,
                                            QString* error) {
  QJsonParseError pe;
  const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes, &pe);
  if (pe.error != QJsonParseError::NoError) {
    if (error) *error = pe.errorString();
    return {};
  }
  QJsonArray arr;
  if (doc.isArray()) {
    arr = doc.array();
  } else if (doc.isObject()) {
    arr = doc.object().value(QStringLiteral("presets")).toArray();
  } else {
    if (error) *error = QStringLiteral("Expected JSON object or array");
    return {};
  }
  QVector<RatingPreset> out;
  out.reserve(arr.size());
  for (const QJsonValue& v : arr) {
    if (!v.isObject()) continue;
    RatingPreset p = presetFromObject(v.toObject(), defaultSource);
    if (p.id.isEmpty()) {
      p.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    if (p.name.isEmpty()) p.name = p.id;
    out.push_back(std::move(p));
  }
  return out;
}

QByteArray ratingPresetsToJson(const QVector<RatingPreset>& presets) {
  QJsonArray arr;
  for (const RatingPreset& p : presets) {
    if (p.source != RatingPresetSource::User) continue;
    arr.append(presetToObject(p));
  }
  QJsonObject root;
  root.insert(QStringLiteral("version"), 1);
  root.insert(QStringLiteral("presets"), arr);
  return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

RatingPreset ratingPresetFromPrioritizer(const anpcpp::RatingsPrioritizer& rt,
                                         const QString& id,
                                         const QString& name,
                                         const QString& description) {
  RatingPreset p;
  p.id = id;
  p.name = name;
  p.description = description;
  p.mode = rt.mode();
  p.source = RatingPresetSource::User;
  if (rt.mode() == anpcpp::RatingsPrioritizer::Mode::Categorical) {
    p.categories = rt.categories();
  } else {
    p.interpreter = rt.interpreter();
  }
  return p;
}
