/**
 * @file ratings_panel.hpp
 * @brief Editor for RatingsPrioritizer scale and votes.
 */

#pragma once

#include <QWidget>

#include "anpcpp/ratings.hpp"

class Document;
class QComboBox;
class QTableWidget;
class QLabel;
class QDoubleSpinBox;
class QStackedWidget;
class QPushButton;

class RatingsPanel : public QWidget {
  Q_OBJECT
public:
  explicit RatingsPanel(Document* doc, QWidget* parent = nullptr);

public slots:
  void refresh();
  void selectLink(const QString& parent, const QString& destCluster);

private slots:
  void onModeChanged();
  void onInterpreterChanged();
  void onConstantChanged();
  void onApplyCategories();
  void onAddCategory();
  void onVotesChanged(int row, int col);
  void onApplyKnots();

private:
  anpcpp::RatingsPrioritizer* activeRatings();
  void rebuildVotes();
  void rebuildScaleUi();
  void updateReadout();

  Document* doc_ = nullptr;
  QString parent_;
  QString destCluster_;
  QLabel* header_ = nullptr;
  QComboBox* modeBox_ = nullptr;
  QStackedWidget* scaleStack_ = nullptr;
  QTableWidget* catTable_ = nullptr;
  QComboBox* interpreterBox_ = nullptr;
  QDoubleSpinBox* constantSpin_ = nullptr;
  QTableWidget* knotTable_ = nullptr;
  QTableWidget* votesTable_ = nullptr;
  QLabel* readout_ = nullptr;
  QPushButton* applyCats_ = nullptr;
  QPushButton* applyKnots_ = nullptr;
  bool updating_ = false;
};
