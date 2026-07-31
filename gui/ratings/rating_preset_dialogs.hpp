/**
 * @file rating_preset_dialogs.hpp
 * @brief Save / manage dialogs for rating scale presets.
 */

#pragma once

#include "ratings/rating_preset.hpp"

#include <QDialog>

class RatingPresetStore;
class Document;

/**
 * @brief Prompts for name/description and saves a preset into My scales.
 * @return True if saved.
 */
bool promptSaveRatingPreset(QWidget* parent,
                            RatingPresetStore* store,
                            RatingPreset draft);

/**
 * @brief Shows Manage scales dialog (user presets edit/delete).
 */
void showManageRatingPresetsDialog(QWidget* parent, RatingPresetStore* store);
