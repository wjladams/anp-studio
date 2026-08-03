/**
 * @file sensitivity_chart_widget.hpp
 * @brief Custom bar/line charts for Sensitivity analysis (QPainter).
 */

#pragma once

#include <QWidget>
#include <QString>
#include <QVector>
#include <utility>

/**
 * @brief Paints Interactive horizontal bars or Global multi-series line charts.
 */
class SensitivityChartWidget : public QWidget {
  Q_OBJECT
public:
  enum class Mode { Bars, Lines };

  explicit SensitivityChartWidget(QWidget* parent = nullptr);

  void setBarEntries(QVector<std::pair<QString, double>> entries);
  void setLineSeries(QVector<QString> names,
                     QVector<double> xs,
                     QVector<QVector<double>> series);

  [[nodiscard]] QSize minimumSizeHint() const override;
  [[nodiscard]] QSize sizeHint() const override;

protected:
  void paintEvent(QPaintEvent* event) override;

private:
  Mode mode_ = Mode::Bars;
  QVector<std::pair<QString, double>> bars_;
  QVector<QString> lineNames_;
  QVector<double> xs_;
  QVector<QVector<double>> series_;
};
