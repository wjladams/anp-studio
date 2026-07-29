/**
 * @file judgment_priorities_panel.hpp
 * @brief Horizontal bar chart of local priorities for the active judgment.
 */

#pragma once

#include <QString>
#include <QVector>
#include <QWidget>

#include <utility>

class Document;
class QLabel;
class PriorityBarsWidget;

/**
 * @brief Right-side Judgments chart of pairwise or ratings priorities.
 */
class JudgmentPrioritiesPanel : public QWidget {
  Q_OBJECT
public:
  explicit JudgmentPrioritiesPanel(Document* doc, QWidget* parent = nullptr);

public slots:
  /** @brief Show priorities for a node→cluster pairwise link. */
  void showNodePairwise(const QString& parent, const QString& destCluster);
  /** @brief Show priorities for a node→cluster ratings link. */
  void showNodeRatings(const QString& parent, const QString& destCluster);
  /** @brief Show priorities for cluster-level pairwise. */
  void showClusterPairwise(const QString& parent);
  /** @brief Clears the chart. */
  void clear();
  /** @brief Recomputes priorities for the current selection. */
  void refresh();

private:
  enum class Source { None, NodePairwise, NodeRatings, ClusterPairwise };

  void setEntries(QVector<std::pair<QString, double>> entries,
                  const QString& subtitle);

  Document* doc_ = nullptr;
  QLabel* title_ = nullptr;
  QLabel* subtitle_ = nullptr;
  PriorityBarsWidget* bars_ = nullptr;

  Source source_ = Source::None;
  QString parent_;
  QString destCluster_;
};

/**
 * @brief Paints horizontal priority bars.
 */
class PriorityBarsWidget : public QWidget {
  Q_OBJECT
public:
  explicit PriorityBarsWidget(QWidget* parent = nullptr);
  void setEntries(QVector<std::pair<QString, double>> entries);

protected:
  void paintEvent(QPaintEvent* event) override;
  [[nodiscard]] QSize minimumSizeHint() const override;
  [[nodiscard]] QSize sizeHint() const override;

private:
  QVector<std::pair<QString, double>> entries_;
};
