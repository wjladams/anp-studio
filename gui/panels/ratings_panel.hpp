/**
 * @file ratings_panel.hpp
 * @brief Option D ratings editor: presets, Advanced popout, votes.
 */

#pragma once

#include "anpcpp/ratings.hpp"
#include "ratings/rating_preset.hpp"

#include <QWidget>

#include <optional>

class Document;
class RatingPresetStore;
class QComboBox;
class QTableWidget;
class QLabel;
class QDoubleSpinBox;
class QStackedWidget;
class QPushButton;
class QFrame;
class QToolButton;

/**
 * @brief RatingsPrioritizer editor with preset-first Scale + Advanced popout.
 */
class RatingsPanel : public QWidget {
  Q_OBJECT
public:
  explicit RatingsPanel(Document* doc, QWidget* parent = nullptr);

public slots:
  void refresh();
  void selectLink(const QString& parent, const QString& destCluster);

private slots:
  void onScaleActivated(int index);
  void onAdvancedToggled(bool on);
  void onAdvModeChanged();
  void onAdvInterpreterChanged();
  void onAddCategory();
  void onAddKnot();
  void onVoteComboChanged(int row);
  void onVoteValueChanged(int row, int col);
  void onSavePreset();
  void onManagePresets();
  void onImportPresets();
  void rebuildScaleMenu();

private:
  anpcpp::RatingsPrioritizer* activeRatings();
  void setEnabledUi(bool on);
  void rebuildVotes();
  void loadAdvancedFromActive();
  void commitAdvancedIfNeeded();
  void applyPreset(const RatingPreset& preset);
  void setScaleDisplay(const QString& name, const QString& presetId);
  void showCustomScaleInCombo();
  [[nodiscard]] RatingPreset draftPresetFromAdvanced() const;
  [[nodiscard]] std::vector<anpcpp::RatingCategory> readCategoryTable() const;
  [[nodiscard]] anpcpp::ScoreInterpreter readInterpreter() const;
  void updateNumericAdvancedVisibility();

  Document* doc_ = nullptr;
  RatingPresetStore* store_ = nullptr;
  QString parent_;
  QString destCluster_;

  QLabel* header_ = nullptr;
  QFrame* scaleBlock_ = nullptr;
  QComboBox* scaleBox_ = nullptr;
  QLabel* modeHint_ = nullptr;
  QToolButton* advancedBtn_ = nullptr;
  QWidget* advancedPanel_ = nullptr;

  QComboBox* advModeBox_ = nullptr;
  QStackedWidget* advStack_ = nullptr;
  QTableWidget* catTable_ = nullptr;
  QComboBox* interpreterBox_ = nullptr;
  QDoubleSpinBox* constantSpin_ = nullptr;
  QLabel* constantLabel_ = nullptr;
  QTableWidget* knotTable_ = nullptr;
  QLabel* knotLabel_ = nullptr;
  QPushButton* addKnotBtn_ = nullptr;

  QTableWidget* votesTable_ = nullptr;
  QLabel* votesLabel_ = nullptr;

  QString currentPresetId_;
  int scaleBoxGuardIndex_ = 0;
  bool updating_ = false;
  bool applyingScale_ = false;
  bool scaleIsCustom_ = false;
  bool advancedOpen_ = false;
};
