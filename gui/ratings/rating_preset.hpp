/**
 * @file rating_preset.hpp
 * @brief Named ratings scale presets (built-in and user library).
 */

#pragma once

#include "anpcpp/ratings.hpp"

#include <QString>
#include <QVector>
#include <optional>

/** @brief Origin of a rating scale preset. */
enum class RatingPresetSource { BuiltIn, User };

/**
 * @brief A reusable ratings scale definition (categorical or numeric).
 */
struct RatingPreset {
  QString id;
  QString name;
  QString description;
  anpcpp::RatingsPrioritizer::Mode mode =
      anpcpp::RatingsPrioritizer::Mode::Categorical;
  std::vector<anpcpp::RatingCategory> categories;
  anpcpp::ScoreInterpreter interpreter = anpcpp::IdentityInterpreter{};
  RatingPresetSource source = RatingPresetSource::User;
};

/**
 * @brief Loads presets from a JSON object/array document.
 * @param jsonBytes UTF-8 JSON.
 * @param defaultSource Source flag applied to loaded presets.
 * @param error Optional error output.
 * @return Presets on success.
 */
[[nodiscard]] QVector<RatingPreset> ratingPresetsFromJson(
    const QByteArray& jsonBytes,
    RatingPresetSource defaultSource,
    QString* error = nullptr);

/**
 * @brief Serializes user presets to JSON (versioned library file).
 */
[[nodiscard]] QByteArray ratingPresetsToJson(const QVector<RatingPreset>& presets);

/**
 * @brief Builds a preset snapshot from a live RatingsPrioritizer.
 */
[[nodiscard]] RatingPreset ratingPresetFromPrioritizer(
    const anpcpp::RatingsPrioritizer& rt,
    const QString& id,
    const QString& name,
    const QString& description);
