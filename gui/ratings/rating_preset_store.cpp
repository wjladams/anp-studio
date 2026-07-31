#include "ratings/rating_preset_store.hpp"

#include <QDir>
#include <QFile>
#include <QStandardPaths>

#include <algorithm>

RatingPresetStore::RatingPresetStore(QObject* parent) : QObject(parent) {
  reload();
}

void RatingPresetStore::reload() {
  builtIn_.clear();
  user_.clear();

  QFile builtin(QStringLiteral(":/presets/rating_presets_builtin.json"));
  if (builtin.open(QIODevice::ReadOnly)) {
    QString err;
    builtIn_ = ratingPresetsFromJson(builtin.readAll(),
                                     RatingPresetSource::BuiltIn, &err);
    for (RatingPreset& p : builtIn_) {
      p.source = RatingPresetSource::BuiltIn;
    }
  }

  const QString path = userLibraryPath();
  QFile userFile(path);
  if (userFile.exists() && userFile.open(QIODevice::ReadOnly)) {
    QString err;
    user_ = ratingPresetsFromJson(userFile.readAll(), RatingPresetSource::User,
                                  &err);
    for (RatingPreset& p : user_) {
      p.source = RatingPresetSource::User;
    }
  }
  emit changed();
}

QVector<RatingPreset> RatingPresetStore::all() const {
  QVector<RatingPreset> out = builtIn_;
  out += user_;
  return out;
}

std::optional<RatingPreset> RatingPresetStore::findById(
    const QString& id) const {
  for (const RatingPreset& p : builtIn_) {
    if (p.id == id) return p;
  }
  for (const RatingPreset& p : user_) {
    if (p.id == id) return p;
  }
  return std::nullopt;
}

QString RatingPresetStore::userLibraryPath() const {
  const QString dir =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir().mkpath(dir);
  return dir + QStringLiteral("/rating_presets.json");
}

bool RatingPresetStore::saveUser(QString* error) {
  QFile f(userLibraryPath());
  if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    if (error) *error = f.errorString();
    return false;
  }
  f.write(ratingPresetsToJson(user_));
  return true;
}

bool RatingPresetStore::upsertUserPreset(RatingPreset preset, QString* error) {
  preset.source = RatingPresetSource::User;
  if (preset.id.isEmpty()) {
    if (error) *error = QStringLiteral("Preset id required");
    return false;
  }
  // Do not allow overwriting built-in ids in the user list with same id
  // as a different concept — user may shadow by id in menu; store separately.
  bool found = false;
  for (RatingPreset& p : user_) {
    if (p.id == preset.id) {
      p = preset;
      found = true;
      break;
    }
  }
  if (!found) user_.push_back(std::move(preset));
  if (!saveUser(error)) return false;
  emit changed();
  return true;
}

bool RatingPresetStore::removeUserPreset(const QString& id, QString* error) {
  const auto before = user_.size();
  user_.erase(std::remove_if(user_.begin(), user_.end(),
                             [&](const RatingPreset& p) { return p.id == id; }),
              user_.end());
  if (user_.size() == before) {
    if (error) *error = QStringLiteral("Preset not found");
    return false;
  }
  if (!saveUser(error)) return false;
  emit changed();
  return true;
}

int RatingPresetStore::importFromFile(const QString& path, QString* error) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) {
    if (error) *error = f.errorString();
    return 0;
  }
  QString err;
  QVector<RatingPreset> imported =
      ratingPresetsFromJson(f.readAll(), RatingPresetSource::User, &err);
  if (!err.isEmpty() && imported.isEmpty()) {
    if (error) *error = err;
    return 0;
  }
  int count = 0;
  for (RatingPreset p : imported) {
    p.source = RatingPresetSource::User;
    // Avoid colliding with built-in ids: suffix if needed.
    if (findById(p.id) && findById(p.id)->source == RatingPresetSource::BuiltIn) {
      p.id += QStringLiteral("-imported");
    }
    int n = 2;
    QString base = p.id;
    while (true) {
      bool clash = false;
      for (const RatingPreset& u : user_) {
        if (u.id == p.id) {
          clash = true;
          break;
        }
      }
      if (!clash) break;
      p.id = base + QStringLiteral("-%1").arg(n++);
    }
    user_.push_back(std::move(p));
    ++count;
  }
  if (count == 0) return 0;
  if (!saveUser(error)) return 0;
  emit changed();
  return count;
}
