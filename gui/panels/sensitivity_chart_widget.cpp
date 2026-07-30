#include "panels/sensitivity_chart_widget.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QSizePolicy>
#include <algorithm>
#include <cmath>

SensitivityChartWidget::SensitivityChartWidget(QWidget* parent)
    : QWidget(parent) {
  setMinimumHeight(220);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void SensitivityChartWidget::setBarEntries(
    QVector<std::pair<QString, double>> entries) {
  mode_ = Mode::Bars;
  bars_ = std::move(entries);
  updateGeometry();
  update();
}

void SensitivityChartWidget::setLineSeries(QVector<QString> names,
                                          QVector<double> xs,
                                          QVector<QVector<double>> series) {
  mode_ = Mode::Lines;
  lineNames_ = std::move(names);
  xs_ = std::move(xs);
  series_ = std::move(series);
  updateGeometry();
  update();
}

QSize SensitivityChartWidget::minimumSizeHint() const {
  if (mode_ == Mode::Bars) {
    const int rows = std::max(1, static_cast<int>(bars_.size()));
    return {240, 28 + rows * 28};
  }
  return {320, 240};
}

QSize SensitivityChartWidget::sizeHint() const {
  return minimumSizeHint();
}

void SensitivityChartWidget::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.fillRect(rect(), QColor(0xffffff));

  static const QColor kPalette[] = {
      QColor(0x1a73e8), QColor(0xd93025), QColor(0x188038), QColor(0xf9ab00),
      QColor(0x9334e6), QColor(0x12b5cb), QColor(0xe37400), QColor(0x5f6368)};

  if (mode_ == Mode::Bars) {
    if (bars_.isEmpty()) {
      p.setPen(QColor(0x5f6368));
      p.drawText(rect().adjusted(8, 8, -8, -8), Qt::AlignCenter,
                 QStringLiteral("No alternative scores."));
      return;
    }
    double maxVal = 0.0;
    for (const auto& e : bars_) maxVal = std::max(maxVal, e.second);
    if (maxVal <= 0.0) maxVal = 1.0;

    const int labelW = 100;
    const int valueW = 56;
    const int rowH = 26;
    const int gap = 6;
    const int top = 8;
    const int barLeft = labelW + 8;
    const int barMaxW = std::max(20, width() - valueW - barLeft - 8);

    for (int i = 0; i < bars_.size(); ++i) {
      const int y = top + i * (rowH + gap);
      p.setPen(QColor(0x202124));
      p.drawText(QRect(4, y, labelW - 4, rowH),
                 Qt::AlignVCenter | Qt::AlignRight, bars_[i].first);
      const QRect track(barLeft, y + 5, barMaxW, rowH - 10);
      p.setPen(Qt::NoPen);
      p.setBrush(QColor(0xe8f0fe));
      p.drawRoundedRect(track, 4, 4);
      const int fillW = static_cast<int>(
          std::lround((bars_[i].second / maxVal) * barMaxW));
      p.setBrush(kPalette[0]);
      p.drawRoundedRect(QRect(barLeft, y + 5, std::max(2, fillW), rowH - 10), 4,
                        4);
      p.setPen(QColor(0x5f6368));
      p.drawText(QRect(barLeft + barMaxW + 6, y, valueW, rowH),
                 Qt::AlignVCenter | Qt::AlignLeft,
                 QString::number(bars_[i].second, 'f', 4));
    }
    return;
  }

  // Line chart
  if (xs_.isEmpty() || series_.isEmpty()) {
    p.setPen(QColor(0x5f6368));
    p.drawText(rect().adjusted(8, 8, -8, -8), Qt::AlignCenter,
               QStringLiteral("No sensitivity series."));
    return;
  }

  const int left = 48;
  const int right = width() - 12;
  const int top = 16;
  const int bottom = height() - 36;
  const QRect plot(left, top, std::max(10, right - left),
                   std::max(10, bottom - top));

  double ymin = 0.0;
  double ymax = 1.0;
  for (const auto& s : series_) {
    for (double v : s) {
      ymin = std::min(ymin, v);
      ymax = std::max(ymax, v);
    }
  }
  if (ymax <= ymin) ymax = ymin + 1.0;

  p.setPen(QPen(QColor(0xdadce0), 1));
  p.setBrush(Qt::NoBrush);
  p.drawRect(plot);

  auto mapX = [&](double x) {
    const double t = (x - xs_.first()) /
                     std::max(1e-12, xs_.last() - xs_.first());
    return plot.left() + t * plot.width();
  };
  auto mapY = [&](double y) {
    const double t = (y - ymin) / (ymax - ymin);
    return plot.bottom() - t * plot.height();
  };

  for (int s = 0; s < series_.size(); ++s) {
    if (series_[s].size() != xs_.size()) continue;
    QPainterPath path;
    for (int i = 0; i < xs_.size(); ++i) {
      const QPointF pt(mapX(xs_[i]), mapY(series_[s][i]));
      if (i == 0) path.moveTo(pt);
      else path.lineTo(pt);
    }
    p.setPen(QPen(kPalette[s % 8], 2.0));
    p.drawPath(path);
  }

  p.setPen(QColor(0x5f6368));
  p.drawText(QRect(plot.left(), bottom + 4, plot.width(), 24), Qt::AlignCenter,
             QStringLiteral("p"));
  // Legend
  int lx = left;
  int ly = 2;
  for (int s = 0; s < lineNames_.size() && s < 8; ++s) {
    p.setPen(Qt::NoPen);
    p.setBrush(kPalette[s % 8]);
    p.drawRect(lx, ly + 4, 10, 10);
    p.setPen(QColor(0x202124));
    p.drawText(lx + 14, ly + 14, lineNames_[s]);
    lx += 14 + p.fontMetrics().horizontalAdvance(lineNames_[s]) + 16;
    if (lx > width() - 80) {
      lx = left;
      ly += 14;
    }
  }
}
