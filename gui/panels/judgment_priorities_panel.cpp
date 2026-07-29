#include "panels/judgment_priorities_panel.hpp"

#include "document.hpp"

#include <QLabel>
#include <QPainter>
#include <QPaintEvent>
#include <QSizePolicy>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

PriorityBarsWidget::PriorityBarsWidget(QWidget* parent) : QWidget(parent) {
  setMinimumWidth(180);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void PriorityBarsWidget::setEntries(
    QVector<std::pair<QString, double>> entries) {
  entries_ = std::move(entries);
  updateGeometry();
  update();
}

QSize PriorityBarsWidget::minimumSizeHint() const {
  const int rows = std::max(1, static_cast<int>(entries_.size()));
  return {180, 28 + rows * 28};
}

QSize PriorityBarsWidget::sizeHint() const {
  return minimumSizeHint();
}

void PriorityBarsWidget::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.fillRect(rect(), QColor(0xffffff));

  if (entries_.isEmpty()) {
    p.setPen(QColor(0x5f6368));
    p.drawText(rect().adjusted(8, 8, -8, -8),
               Qt::AlignCenter | Qt::TextWordWrap,
               QStringLiteral("No priorities to display."));
    return;
  }

  double maxVal = 0.0;
  for (const auto& e : entries_) {
    maxVal = std::max(maxVal, e.second);
  }
  if (maxVal <= 0.0) maxVal = 1.0;

  const int labelW = 90;
  const int valueW = 48;
  const int rowH = 26;
  const int gap = 6;
  const int top = 4;
  const int barLeft = labelW + 8;
  const int barRight = width() - valueW - 8;
  const int barMaxW = std::max(20, barRight - barLeft);
  const QColor barColor(0x1a73e8);
  const QColor barBg(0xe8f0fe);
  const QColor textColor(0x202124);
  const QColor muted(0x5f6368);

  for (int i = 0; i < entries_.size(); ++i) {
    const int y = top + i * (rowH + gap);
    const QRect labelRect(4, y, labelW - 4, rowH);
    p.setPen(textColor);
    p.drawText(labelRect, Qt::AlignVCenter | Qt::AlignRight | Qt::TextWordWrap,
               entries_[i].first);

    const QRect track(barLeft, y + 5, barMaxW, rowH - 10);
    p.setPen(Qt::NoPen);
    p.setBrush(barBg);
    p.drawRoundedRect(track, 4, 4);

    const int fillW = static_cast<int>(
        std::lround((entries_[i].second / maxVal) * barMaxW));
    if (fillW > 0) {
      p.setBrush(barColor);
      p.drawRoundedRect(QRect(barLeft, y + 5, fillW, rowH - 10), 4, 4);
    }

    p.setPen(muted);
    p.drawText(QRect(barRight + 4, y, valueW - 4, rowH),
               Qt::AlignVCenter | Qt::AlignLeft,
               QString::number(entries_[i].second, 'f', 3));
  }
}

JudgmentPrioritiesPanel::JudgmentPrioritiesPanel(Document* doc, QWidget* parent)
    : QWidget(parent), doc_(doc) {
  setObjectName(QStringLiteral("judgmentPriorities"));
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(6);

  title_ = new QLabel(QStringLiteral("Priorities"), this);
  title_->setObjectName(QStringLiteral("selectorCaption"));
  layout->addWidget(title_);

  subtitle_ = new QLabel(this);
  subtitle_->setObjectName(QStringLiteral("selectorMuted"));
  subtitle_->setWordWrap(true);
  layout->addWidget(subtitle_);

  bars_ = new PriorityBarsWidget(this);
  layout->addWidget(bars_, 1);

  connect(doc_, &Document::modelChanged, this, &JudgmentPrioritiesPanel::refresh);
  connect(doc_, &Document::viewNetworkChanged, this,
          &JudgmentPrioritiesPanel::refresh);
  clear();
}

void JudgmentPrioritiesPanel::showNodePairwise(const QString& parent,
                                               const QString& destCluster) {
  source_ = Source::NodePairwise;
  parent_ = parent;
  destCluster_ = destCluster;
  refresh();
}

void JudgmentPrioritiesPanel::showNodeRatings(const QString& parent,
                                              const QString& destCluster) {
  source_ = Source::NodeRatings;
  parent_ = parent;
  destCluster_ = destCluster;
  refresh();
}

void JudgmentPrioritiesPanel::showClusterPairwise(const QString& parent) {
  source_ = Source::ClusterPairwise;
  parent_ = parent;
  destCluster_.clear();
  refresh();
}

void JudgmentPrioritiesPanel::clear() {
  source_ = Source::None;
  parent_.clear();
  destCluster_.clear();
  setEntries({}, QStringLiteral("Select a judgment above."));
}

void JudgmentPrioritiesPanel::setEntries(
    QVector<std::pair<QString, double>> entries, const QString& subtitle) {
  subtitle_->setText(subtitle);
  bars_->setEntries(std::move(entries));
}

void JudgmentPrioritiesPanel::refresh() {
  if (source_ == Source::None || parent_.isEmpty()) {
    setEntries({}, QStringLiteral("Select a judgment above."));
    return;
  }

  auto& net = doc_->network();
  QVector<std::pair<QString, double>> entries;

  try {
    if (source_ == Source::NodePairwise) {
      if (destCluster_.isEmpty() || net.find_node(parent_.toStdString()) == nullptr) {
        setEntries({}, QStringLiteral("No pairwise priorities."));
        return;
      }
      const auto* pw =
          net.node(parent_.toStdString())
              .node_pairwise(destCluster_.toStdString());
      if (pw == nullptr || pw->size() == 0) {
        setEntries({}, QStringLiteral("No pairwise priorities."));
        return;
      }
      const auto& names = pw->alternatives();
      const auto pri = pw->priorities();
      for (std::size_t i = 0; i < names.size() && i < pri.size(); ++i) {
        entries.push_back(
            {QString::fromStdString(names[i]), pri[i]});
      }
      setEntries(std::move(entries),
                 QStringLiteral("Wrt %1 → %2 (pairwise)")
                     .arg(parent_, destCluster_));
      return;
    }

    if (source_ == Source::NodeRatings) {
      if (destCluster_.isEmpty() || net.find_node(parent_.toStdString()) == nullptr) {
        setEntries({}, QStringLiteral("No ratings priorities."));
        return;
      }
      const auto* rt =
          net.node(parent_.toStdString())
              .node_ratings(destCluster_.toStdString());
      if (rt == nullptr || rt->empty()) {
        setEntries({}, QStringLiteral("No ratings priorities."));
        return;
      }
      const auto& names = rt->alternatives();
      const auto pri = rt->priorities();
      for (std::size_t i = 0; i < names.size() && i < pri.size(); ++i) {
        entries.push_back(
            {QString::fromStdString(names[i]), pri[i]});
      }
      setEntries(std::move(entries),
                 QStringLiteral("Wrt %1 → %2 (ratings)")
                     .arg(parent_, destCluster_));
      return;
    }

    if (source_ == Source::ClusterPairwise) {
      if (net.find_cluster(parent_.toStdString()) == nullptr) {
        setEntries({}, QStringLiteral("No cluster priorities."));
        return;
      }
      const auto& pw =
          net.cluster(parent_.toStdString()).cluster_pairwise();
      if (pw.size() == 0) {
        setEntries({}, QStringLiteral("No cluster priorities."));
        return;
      }
      const auto& names = pw.alternatives();
      const auto pri = pw.priorities();
      for (std::size_t i = 0; i < names.size() && i < pri.size(); ++i) {
        entries.push_back(
            {QString::fromStdString(names[i]), pri[i]});
      }
      setEntries(std::move(entries),
                 QStringLiteral("Wrt cluster %1 (pairwise)").arg(parent_));
      return;
    }
  } catch (...) {
    setEntries({}, QStringLiteral("Unable to compute priorities."));
  }
}
