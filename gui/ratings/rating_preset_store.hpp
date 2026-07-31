/**
 * @file rating_preset_store.hpp
 * @brief Loads built-in and user rating scale presets.
 */

#pragma once

#include "ratings/rating_preset.hpp"

#include <QObject>
#include <QVector>

/**
 * @brief Application library of built-in + My scales presets.
 */
class RatingPresetStore : public QObject {
  Q_OBJECT
public:
  explicit RatingPresetStore(QObject* parent = nullptr);

  /** @brief Reloads built-in resources and user AppData file. */
  void reload();

  [[nodiscard]] QVector<RatingPreset> builtIn() const { return builtIn_; }
  [[nodiscard]] QVector<RatingPreset> userPresets() const { return user_; }
  [[nodiscard]] QVector<RatingPreset> all() const;

  [[nodiscard]] std::optional<RatingPreset> findById(const QString& id) const;
  [[nodiscard]] QString userLibraryPath() const;

  /** @brief Adds or replaces a user preset and persists. */
  bool upsertUserPreset(RatingPreset preset, QString* error = nullptr);
  /** @brief Removes a user preset by id. */
  bool removeUserPreset(const QString& id, QString* error = nullptr);
  /**
   * @brief Merges presets from a JSON file into My scales.
   * @return Number of presets imported.
   */
  int importFromFile(const QString& path, QString* error = nullptr);

signals:
  void changed();

private:
  bool saveUser(QString* error = nullptr);

  QVector<RatingPreset> builtIn_;
  QVector<RatingPreset> user_;
};
