#include "panels/consensus_analysis_widget.hpp"

#include "document.hpp"

#include "anpcpp/multiuser.hpp"
#include "anpcpp/network.hpp"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QButtonGroup>
#include <QComboBox>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int kModeHeat = 0;
constexpr int kModeCoverage = 1;
constexpr int kModeCohorts = 2;

// Option 2 traffic-light chip colors
constexpr const char* kAlignOkBg = "#b7d9b4";
constexpr const char* kAlignOkFg = "#143018";
constexpr const char* kAlignWatchBg = "#f0e08a";
constexpr const char* kAlignWatchFg = "#3a3010";
constexpr const char* kAlignHotBg = "#e08a7a";
constexpr const char* kAlignHotFg = "#2a1010";

enum class BlockKind { NodePairwise, ClusterPairwise, NodeRatings };

struct BlockKey {
  BlockKind kind = BlockKind::NodePairwise;
  QString wrt;
  QString dest;
};

QString blockKeyEncode(const BlockKey& k) {
  const char* prefix = "pw";
  if (k.kind == BlockKind::ClusterPairwise) prefix = "cpw";
  else if (k.kind == BlockKind::NodeRatings) prefix = "rt";
  return QStringLiteral("%1\n%2\n%3")
      .arg(QString::fromLatin1(prefix), k.wrt, k.dest);
}

std::optional<BlockKey> blockKeyDecode(const QString& s) {
  const QStringList parts = s.split(QLatin1Char('\n'));
  if (parts.size() != 3) return std::nullopt;
  BlockKey k;
  k.wrt = parts[1];
  k.dest = parts[2];
  if (parts[0] == QStringLiteral("pw")) k.kind = BlockKind::NodePairwise;
  else if (parts[0] == QStringLiteral("cpw"))
    k.kind = BlockKind::ClusterPairwise;
  else if (parts[0] == QStringLiteral("rt"))
    k.kind = BlockKind::NodeRatings;
  else
    return std::nullopt;
  return k;
}

QString blockLabel(const BlockKey& k) {
  if (k.kind == BlockKind::ClusterPairwise)
    return QStringLiteral("Cluster · %1").arg(k.wrt);
  if (k.kind == BlockKind::NodeRatings)
    return QStringLiteral("%1 → %2 (ratings)").arg(k.wrt, k.dest);
  return QStringLiteral("%1 → %2").arg(k.wrt, k.dest);
}

QString formatPairwiseValue(double v) {
  if (!(v > 0.0) || !std::isfinite(v)) return QStringLiteral("—");
  static const double recip[] = {2, 3, 4, 5, 6, 7, 8, 9};
  for (double d : recip) {
    if (std::abs(v - 1.0 / d) < 1e-6)
      return QStringLiteral("1/%1").arg(static_cast<int>(d));
  }
  if (std::abs(v - std::round(v)) < 1e-6)
    return QString::number(static_cast<qint64>(std::lround(v)));
  return QString::number(v, 'g', 4);
}

enum class AlignBand { Ok, Watch, Hot };

AlignBand alignBand(double pct) {
  if (pct >= 70.0) return AlignBand::Ok;
  if (pct >= 35.0) return AlignBand::Watch;
  return AlignBand::Hot;
}

QWidget* makeAlignChip(double pct, QWidget* parent) {
  auto* lab = new QLabel(parent);
  const int rounded = static_cast<int>(std::lround(std::clamp(pct, 0.0, 100.0)));
  lab->setText(QStringLiteral("%1%").arg(rounded));
  lab->setAlignment(Qt::AlignCenter);
  const AlignBand band = alignBand(pct);
  QString bg = kAlignHotBg;
  QString fg = kAlignHotFg;
  if (band == AlignBand::Ok) {
    bg = kAlignOkBg;
    fg = kAlignOkFg;
  } else if (band == AlignBand::Watch) {
    bg = kAlignWatchBg;
    fg = kAlignWatchFg;
  }
  lab->setStyleSheet(
      QStringLiteral("QLabel { background:%1; color:%2; font-weight:700; "
                     "padding:6px 10px; border-radius:4px; "
                     "font-variant-numeric:tabular-nums; }")
          .arg(bg, fg));
  return lab;
}

/** Vote range strip: green min–max, mean tick, hybrid stacked dots. */
class VoteRangeStrip : public QWidget {
public:
  enum class Scale { LogSaaty, Linear01 };

  explicit VoteRangeStrip(QWidget* parent = nullptr) : QWidget(parent) {
    setMinimumHeight(56);
    setMinimumWidth(220);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  }

  void setSpread(const anpcpp::VoteSpreadSummary& spread, Scale scale,
                 const QString& tip) {
    spread_ = spread;
    scale_ = scale;
    setToolTip(tip);
    update();
  }

protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF track(0, 10, width() - 1, 28);
    p.setPen(QColor(221, 214, 202));
    p.setBrush(QColor(235, 230, 220));
    p.drawRoundedRect(track, 4, 4);

    if (spread_.contributor_count <= 0) return;

    const double scaleLo = (scale_ == Scale::LogSaaty) ? (1.0 / 9.0) : 0.0;
    const double scaleHi = (scale_ == Scale::LogSaaty) ? 9.0 : 1.0;

    auto posOf = [&](double v) -> double {
      if (scale_ == Scale::LogSaaty) {
        const double c = std::clamp(v, scaleLo, scaleHi);
        return (std::log(c) - std::log(scaleLo)) /
               (std::log(scaleHi) - std::log(scaleLo));
      }
      return (std::clamp(v, scaleLo, scaleHi) - scaleLo) / (scaleHi - scaleLo);
    };

    const double x0 = track.left() + 4;
    const double x1 = track.right() - 4;
    auto xAt = [&](double v) { return x0 + posOf(v) * (x1 - x0); };

    const double cy = track.center().y();
    const double barH = track.height();

    // Green bar: observed min → max
    if (spread_.contributor_count >= 1 && spread_.max >= spread_.min) {
      const double xmin = xAt(spread_.min);
      const double xmax = xAt(spread_.max);
      QRectF range(std::min(xmin, xmax), track.top() + 2,
                   std::max(2.0, std::abs(xmax - xmin)), barH - 4);
      p.setPen(Qt::NoPen);
      p.setBrush(QColor(47, 93, 80, 82));
      p.drawRoundedRect(range, 2, 2);
    }

    // Stack equal (or near-equal) votes (under mean tick)
    std::vector<double> sorted = spread_.values;
    std::sort(sorted.begin(), sorted.end());
    std::vector<std::pair<double, int>> stacks;
    for (double v : sorted) {
      if (!stacks.empty()) {
        const double prev = stacks.back().first;
        const double tol = 1e-9 * std::max(1.0, std::abs(prev));
        if (std::abs(v - prev) <= tol) {
          ++stacks.back().second;
          continue;
        }
      }
      stacks.emplace_back(v, 1);
    }

    // Diameter: bar/5 at n=1 → full bar at n=5; n≥5 stays max + digit
    auto diameterForN = [&](int n) -> double {
      const int t = std::clamp(n, 1, 5);
      const double frac = (t - 1) / 4.0;  // 0..1
      return barH * (0.2 + 0.8 * frac);
    };

    for (const auto& [v, n] : stacks) {
      const double x = xAt(v);
      const double r = diameterForN(n) * 0.5;
      p.setBrush(QColor(61, 58, 52));
      p.setPen(QPen(Qt::white, 1.5));
      p.drawEllipse(QPointF(x, cy), r, r);
      if (n >= 5) {
        p.setPen(Qt::white);
        QFont f = p.font();
        f.setBold(true);
        f.setPixelSize(n >= 10 ? 9 : 11);
        p.setFont(f);
        p.drawText(QRectF(x - r, cy - r, 2 * r, 2 * r), Qt::AlignCenter,
                   QString::number(n));
      }
    }

    // Mean tick on top with white halo so it stays visible on large stacks
    const double mx = xAt(spread_.mean);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 253, 248));
    p.drawRoundedRect(
        QRectF(mx - 2.5, track.top() + 1, 5, barH - 2), 1, 1);
    p.setBrush(QColor(47, 93, 80));
    p.drawRect(QRectF(mx - 1.5, track.top() + 2, 3, barH - 4));
  }

private:
  anpcpp::VoteSpreadSummary spread_;
  Scale scale_ = Scale::LogSaaty;
};

std::vector<BlockKey> collectBlocks(const anpcpp::AnpNetwork& net) {
  std::vector<BlockKey> out;
  for (const anpcpp::AnpNode* n : net.nodes()) {
    for (const anpcpp::AnpCluster* dest : net.clusters()) {
      const auto* slot = n->node_prioritizer(dest->name());
      if (slot == nullptr || slot->empty()) continue;
      BlockKey k;
      k.wrt = QString::fromStdString(n->name());
      k.dest = QString::fromStdString(dest->name());
      k.kind = (slot->kind == anpcpp::NodePrioritizerKind::Ratings)
                   ? BlockKind::NodeRatings
                   : BlockKind::NodePairwise;
      out.push_back(k);
    }
  }
  for (const anpcpp::AnpCluster* c : net.clusters()) {
    if (c->cluster_pairwise().size() < 2) continue;
    BlockKey k;
    k.kind = BlockKind::ClusterPairwise;
    k.wrt = QString::fromStdString(c->name());
    k.dest = k.wrt;
    out.push_back(k);
  }
  return out;
}

anpcpp::JudgmentFillCounts userBlockFill(const anpcpp::AnpNetwork& net,
                                         const BlockKey& block,
                                         const std::string& uid) {
  if (block.kind == BlockKind::NodePairwise ||
      block.kind == BlockKind::NodeRatings) {
    const anpcpp::AnpNode* n = net.find_node(block.wrt.toStdString());
    if (n == nullptr) return {};
    const auto* slot = n->node_prioritizer(block.dest.toStdString());
    if (slot == nullptr) return {};
    if (block.kind == BlockKind::NodePairwise) {
      const auto it = slot->user_pairwise.find(uid);
      if (it == slot->user_pairwise.end()) {
        anpcpp::JudgmentFillCounts c;
        c.needed = anpcpp::pairwise_fill_counts(slot->pairwise).needed;
        return c;
      }
      return anpcpp::pairwise_fill_counts(it->second);
    }
    const auto it = slot->user_ratings.find(uid);
    if (it == slot->user_ratings.end()) {
      anpcpp::JudgmentFillCounts c;
      c.needed = anpcpp::ratings_fill_counts(slot->ratings).needed;
      return c;
    }
    return anpcpp::ratings_fill_counts(it->second);
  }
  const anpcpp::AnpCluster* c = net.find_cluster(block.wrt.toStdString());
  if (c == nullptr) return {};
  const auto& map = c->user_cluster_pairwise();
  const auto it = map.find(uid);
  if (it == map.end()) {
    anpcpp::JudgmentFillCounts counts;
    counts.needed = anpcpp::pairwise_fill_counts(c->cluster_pairwise()).needed;
    return counts;
  }
  return anpcpp::pairwise_fill_counts(it->second);
}

anpcpp::JudgmentFillCounts userAllFill(const anpcpp::AnpNetwork& net,
                                       const std::string& uid) {
  anpcpp::JudgmentFillCounts total;
  for (const BlockKey& b : collectBlocks(net)) {
    const auto c = userBlockFill(net, b, uid);
    total.filled += c.filled;
    total.needed += c.needed;
  }
  return total;
}

const anpcpp::PairwiseJudgments* userPairwise(const anpcpp::AnpNetwork& net,
                                              const BlockKey& block,
                                              const std::string& uid) {
  if (block.kind == BlockKind::NodePairwise) {
    const anpcpp::AnpNode* n = net.find_node(block.wrt.toStdString());
    if (n == nullptr) return nullptr;
    const auto* slot = n->node_prioritizer(block.dest.toStdString());
    if (slot == nullptr) return nullptr;
    const auto it = slot->user_pairwise.find(uid);
    return it == slot->user_pairwise.end() ? nullptr : &it->second;
  }
  if (block.kind == BlockKind::ClusterPairwise) {
    const anpcpp::AnpCluster* c = net.find_cluster(block.wrt.toStdString());
    if (c == nullptr) return nullptr;
    const auto& map = c->user_cluster_pairwise();
    const auto it = map.find(uid);
    return it == map.end() ? nullptr : &it->second;
  }
  return nullptr;
}

const anpcpp::RatingsPrioritizer* userRatings(const anpcpp::AnpNetwork& net,
                                              const BlockKey& block,
                                              const std::string& uid) {
  if (block.kind != BlockKind::NodeRatings) return nullptr;
  const anpcpp::AnpNode* n = net.find_node(block.wrt.toStdString());
  if (n == nullptr) return nullptr;
  const auto* slot = n->node_prioritizer(block.dest.toStdString());
  if (slot == nullptr) return nullptr;
  const auto it = slot->user_ratings.find(uid);
  return it == slot->user_ratings.end() ? nullptr : &it->second;
}

QWidget* makeDualBar(QWidget* parent, double left, double right,
                     const QString& leftTip, const QString& rightTip) {
  auto* host = new QWidget(parent);
  auto* lay = new QHBoxLayout(host);
  lay->setContentsMargins(0, 4, 0, 4);
  lay->setSpacing(0);
  const double sum = left + right;
  int lStretch = 1;
  int rStretch = 1;
  if (sum > 0.0) {
    lStretch = std::max(1, static_cast<int>(std::lround(1000.0 * left / sum)));
    rStretch = std::max(1, static_cast<int>(std::lround(1000.0 * right / sum)));
  }
  auto* l = new QFrame(host);
  l->setMinimumHeight(14);
  l->setToolTip(leftTip);
  l->setStyleSheet(QStringLiteral("background:#c4785a; border:none;"));
  auto* r = new QFrame(host);
  r->setMinimumHeight(14);
  r->setToolTip(rightTip);
  r->setStyleSheet(QStringLiteral("background:#5a7aa8; border:none;"));
  lay->addWidget(l, lStretch);
  lay->addWidget(r, rStretch);
  return host;
}

QWidget* makeRangeCell(const anpcpp::VoteSpreadSummary& spread, bool pairwise,
                       const QString& detail, QWidget* parent) {
  auto* host = new QWidget(parent);
  auto* lay = new QVBoxLayout(host);
  lay->setContentsMargins(0, 2, 0, 2);
  lay->setSpacing(2);
  auto* strip = new VoteRangeStrip(host);
  strip->setSpread(spread,
                   pairwise ? VoteRangeStrip::Scale::LogSaaty
                            : VoteRangeStrip::Scale::Linear01,
                   detail);
  lay->addWidget(strip);
  auto* ticks = new QLabel(
      pairwise ? QStringLiteral("1/9  1/3  1  3  9")
               : QStringLiteral("0  0.25  0.5  0.75  1"),
      host);
  ticks->setStyleSheet(QStringLiteral("color:#5c574e; font-size:10px;"));
  ticks->setAlignment(Qt::AlignHCenter);
  lay->addWidget(ticks);
  QString stats;
  if (spread.contributor_count <= 0) {
    stats = QStringLiteral("No votes");
  } else if (pairwise) {
    stats = QStringLiteral("Mean %1 (geometric) · range %2 – %3")
                .arg(formatPairwiseValue(spread.mean),
                     formatPairwiseValue(spread.min),
                     formatPairwiseValue(spread.max));
  } else {
    stats = QStringLiteral("Mean %1 (arithmetic) · range %2 – %3")
                .arg(QString::number(spread.mean, 'g', 3),
                     QString::number(spread.min, 'g', 3),
                     QString::number(spread.max, 'g', 3));
  }
  auto* statLab = new QLabel(stats, host);
  statLab->setStyleSheet(QStringLiteral("color:#5c574e; font-size:11px;"));
  statLab->setWordWrap(true);
  lay->addWidget(statLab);
  return host;
}

std::vector<std::string> resolveAlts(const anpcpp::AnpNetwork& net,
                                     const BlockKey& block,
                                     const std::vector<const anpcpp::PairwiseJudgments*>& inputs) {
  for (const anpcpp::PairwiseJudgments* pw : inputs) {
    if (pw != nullptr && !pw->empty()) return pw->alternatives();
  }
  if (block.kind == BlockKind::NodePairwise) {
    const anpcpp::AnpNode* n = net.find_node(block.wrt.toStdString());
    if (n) {
      const auto* slot = n->node_prioritizer(block.dest.toStdString());
      if (slot) return slot->pairwise.alternatives();
    }
  }
  if (block.kind == BlockKind::ClusterPairwise) {
    const anpcpp::AnpCluster* c = net.find_cluster(block.wrt.toStdString());
    if (c) return c->cluster_pairwise().alternatives();
  }
  return {};
}

}  // namespace

ConsensusAnalysisWidget::ConsensusAnalysisWidget(Document* doc, QWidget* parent)
    : QWidget(parent), doc_(doc) {
  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(12, 12, 12, 12);

  emptyLabel_ = new QLabel(
      QStringLiteral(
          "Consensus / Variance needs at least two participants.\n"
          "Add them under Participants → Manage participants…"),
      this);
  emptyLabel_->setWordWrap(true);
  emptyLabel_->setAlignment(Qt::AlignCenter);
  root->addWidget(emptyLabel_);

  content_ = new QWidget(this);
  auto* contentLay = new QVBoxLayout(content_);
  contentLay->setContentsMargins(0, 0, 0, 0);

  auto* toolbar = new QHBoxLayout;
  toolbar->addWidget(new QLabel(QStringLiteral("Block:"), content_));
  blockCombo_ = new QComboBox(content_);
  toolbar->addWidget(blockCombo_, 1);
  toolbar->addWidget(new QLabel(QStringLiteral("Compare:"), content_));
  compareCombo_ = new QComboBox(content_);
  toolbar->addWidget(compareCombo_, 1);
  contentLay->addLayout(toolbar);

  auto* modeRow = new QHBoxLayout;
  modeGroup_ = new QButtonGroup(content_);
  modeGroup_->setExclusive(true);
  auto addMode = [&](const QString& text, int id) {
    auto* btn = new QPushButton(text, content_);
    btn->setCheckable(true);
    btn->setFlat(true);
    btn->setStyleSheet(
        QStringLiteral("QPushButton { padding: 4px 10px; border: 1px solid "
                       "#ccc; border-radius: 3px; background: #f5f5f5; }"
                       "QPushButton:checked { background: #e8eef5; "
                       "border-color: #5a7aa8; font-weight: 600; }"));
    modeGroup_->addButton(btn, id);
    modeRow->addWidget(btn);
  };
  addMode(QStringLiteral("Disagreement heat"), kModeHeat);
  addMode(QStringLiteral("Coverage"), kModeCoverage);
  addMode(QStringLiteral("Cohorts"), kModeCohorts);
  modeRow->addStretch();
  contentLay->addLayout(modeRow);
  if (auto* b = modeGroup_->button(kModeHeat)) b->setChecked(true);

  modeStack_ = new QStackedWidget(content_);

  // --- Disagreement (alignment + vote range) ---
  auto* heatPage = new QWidget(modeStack_);
  auto* heatLay = new QVBoxLayout(heatPage);
  disagreementTitle_ = new QLabel(heatPage);
  disagreementTitle_->setWordWrap(true);
  heatLay->addWidget(disagreementTitle_);
  disagreementLegend_ = new QLabel(
      QStringLiteral(
          "Alignment 0–100% (traffic light: green = aligned, yellow = watch, "
          "red = unaligned). Range strip: green bar = min→max; teal mean tick "
          "(drawn on top with a light halo). "
          "Dots grow with stack size (n=1…5); "
          "n≥5 also shows the count. Pairwise uses a log 1/9…9 track "
          "(geometric); ratings use 0…1 (arithmetic)."),
      heatPage);
  disagreementLegend_->setWordWrap(true);
  disagreementLegend_->setStyleSheet(QStringLiteral("color:#5c574e;"));
  heatLay->addWidget(disagreementLegend_);
  disagreementTable_ = new QTableWidget(heatPage);
  disagreementTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  disagreementTable_->setSelectionMode(QAbstractItemView::NoSelection);
  disagreementTable_->verticalHeader()->setVisible(false);
  disagreementTable_->horizontalHeader()->setStretchLastSection(true);
  // Per-column resize modes are applied in refreshDisagreement() after
  // setColumnCount — calling setSectionResizeMode(logicalIndex, …) with no
  // columns yet segfaults in Qt 6.
  disagreementTable_->verticalHeader()->setSectionResizeMode(
      QHeaderView::ResizeToContents);
  heatLay->addWidget(disagreementTable_, 1);
  distNote_ = new QLabel(heatPage);
  distNote_->setWordWrap(true);
  heatLay->addWidget(distNote_);
  modeStack_->addWidget(heatPage);

  // --- Coverage ---
  auto* covPage = new QWidget(modeStack_);
  auto* covLay = new QVBoxLayout(covPage);
  coverageTable_ = new QTableWidget(covPage);
  coverageTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  coverageTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  covLay->addWidget(coverageTable_, 1);
  modeStack_->addWidget(covPage);

  // --- Cohorts ---
  auto* cohortPage = new QWidget(modeStack_);
  auto* cohortLay = new QVBoxLayout(cohortPage);
  auto* cohortPick = new QHBoxLayout;
  cohortPick->addWidget(new QLabel(QStringLiteral("Left group:"), cohortPage));
  cohortLeft_ = new QComboBox(cohortPage);
  cohortPick->addWidget(cohortLeft_, 1);
  cohortPick->addWidget(new QLabel(QStringLiteral("Right group:"), cohortPage));
  cohortRight_ = new QComboBox(cohortPage);
  cohortPick->addWidget(cohortRight_, 1);
  cohortLay->addLayout(cohortPick);
  cohortHelp_ = new QLabel(
      QStringLiteral(
          "Define at least two named groups under Manage participants… "
          "to compare cohort alternative priorities."),
      cohortPage);
  cohortHelp_->setWordWrap(true);
  cohortLay->addWidget(cohortHelp_);
  cohortBarsHost_ = new QWidget(cohortPage);
  cohortBarsLay_ = new QVBoxLayout(cohortBarsHost_);
  cohortBarsLay_->setContentsMargins(0, 0, 0, 0);
  cohortLay->addWidget(cohortBarsHost_, 1);
  cohortDistNote_ = new QLabel(cohortPage);
  cohortDistNote_->setWordWrap(true);
  cohortLay->addWidget(cohortDistNote_);
  modeStack_->addWidget(cohortPage);

  contentLay->addWidget(modeStack_, 1);
  root->addWidget(content_, 1);

  connect(modeGroup_, &QButtonGroup::idClicked, this,
          &ConsensusAnalysisWidget::onModeChanged);
  connect(blockCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &ConsensusAnalysisWidget::onControlsChanged);
  connect(compareCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &ConsensusAnalysisWidget::onControlsChanged);
  connect(cohortLeft_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &ConsensusAnalysisWidget::onControlsChanged);
  connect(cohortRight_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &ConsensusAnalysisWidget::onControlsChanged);

  connect(doc_, &Document::modelChanged, this, &ConsensusAnalysisWidget::refresh);
  connect(doc_, &Document::sessionChanged, this,
          &ConsensusAnalysisWidget::refresh);
  connect(doc_, &Document::viewNetworkChanged, this,
          &ConsensusAnalysisWidget::refresh);

  refresh();
}

void ConsensusAnalysisWidget::onModeChanged(int id) {
  modeStack_->setCurrentIndex(id);
}

void ConsensusAnalysisWidget::onControlsChanged() {
  if (updating_) return;
  refreshDisagreement();
  refreshCoverage();
  refreshCohorts();
}

QStringList ConsensusAnalysisWidget::compareParticipantIds() const {
  QStringList ids;
  const QString groupId = compareCombo_->currentData().toString();
  const auto& parts = doc_->root().participants();
  if (groupId.isEmpty()) {
    for (const auto& p : parts) ids << QString::fromStdString(p.id);
    return ids;
  }
  const anpcpp::JudgmentGroup* g =
      doc_->root().find_judgment_group(groupId.toStdString());
  if (g == nullptr) return ids;
  for (const std::string& id : g->member_ids)
    ids << QString::fromStdString(id);
  return ids;
}

void ConsensusAnalysisWidget::rebuildBlockCombo() {
  const QString prefer = blockCombo_->currentData().toString();
  blockCombo_->blockSignals(true);
  blockCombo_->clear();
  for (const BlockKey& b : collectBlocks(doc_->network())) {
    blockCombo_->addItem(blockLabel(b), blockKeyEncode(b));
  }
  const int idx = blockCombo_->findData(prefer);
  if (idx >= 0) blockCombo_->setCurrentIndex(idx);
  else if (blockCombo_->count() > 0) blockCombo_->setCurrentIndex(0);
  blockCombo_->blockSignals(false);
}

void ConsensusAnalysisWidget::rebuildCompareCombo() {
  const QString prefer = compareCombo_->currentData().toString();
  compareCombo_->blockSignals(true);
  compareCombo_->clear();
  compareCombo_->addItem(QStringLiteral("All participants"), QString());
  for (const auto& g : doc_->root().judgment_groups()) {
    compareCombo_->addItem(QString::fromStdString(g.name),
                           QString::fromStdString(g.id));
  }
  const int idx = compareCombo_->findData(prefer);
  if (idx >= 0) compareCombo_->setCurrentIndex(idx);
  compareCombo_->blockSignals(false);
}

void ConsensusAnalysisWidget::rebuildCohortGroupCombos() {
  const QString leftPrefer = cohortLeft_->currentData().toString();
  const QString rightPrefer = cohortRight_->currentData().toString();
  cohortLeft_->blockSignals(true);
  cohortRight_->blockSignals(true);
  cohortLeft_->clear();
  cohortRight_->clear();
  const auto& groups = doc_->root().judgment_groups();
  for (const auto& g : groups) {
    const QString id = QString::fromStdString(g.id);
    const QString name = QString::fromStdString(g.name);
    cohortLeft_->addItem(name, id);
    cohortRight_->addItem(name, id);
  }
  if (groups.size() >= 2) {
    int li = cohortLeft_->findData(leftPrefer);
    int ri = cohortRight_->findData(rightPrefer);
    if (li < 0) li = 0;
    if (ri < 0) ri = 1;
    if (li == ri) ri = (li + 1) % cohortRight_->count();
    cohortLeft_->setCurrentIndex(li);
    cohortRight_->setCurrentIndex(ri);
  }
  cohortLeft_->blockSignals(false);
  cohortRight_->blockSignals(false);
}

void ConsensusAnalysisWidget::refresh() {
  updating_ = true;
  const bool enough = doc_->root().participants().size() >= 2;
  emptyLabel_->setVisible(!enough);
  content_->setVisible(enough);
  if (!enough) {
    updating_ = false;
    return;
  }
  rebuildBlockCombo();
  rebuildCompareCombo();
  rebuildCohortGroupCombos();
  updating_ = false;
  refreshDisagreement();
  refreshCoverage();
  refreshCohorts();
}

void ConsensusAnalysisWidget::refreshDisagreement() {
  disagreementTable_->clear();
  disagreementTable_->setRowCount(0);
  disagreementTable_->setColumnCount(3);
  disagreementTable_->setHorizontalHeaderLabels(
      {QStringLiteral("Comparison"), QStringLiteral("Alignment"),
       QStringLiteral("Vote range")});
  disagreementTable_->horizontalHeader()->setSectionResizeMode(
      0, QHeaderView::ResizeToContents);
  disagreementTable_->horizontalHeader()->setSectionResizeMode(
      1, QHeaderView::ResizeToContents);
  disagreementTable_->horizontalHeader()->setSectionResizeMode(
      2, QHeaderView::Stretch);
  distNote_->clear();

  const auto blockOpt = blockKeyDecode(blockCombo_->currentData().toString());
  if (!blockOpt) {
    disagreementTitle_->setText(
        QStringLiteral("No judgment block in this network."));
    return;
  }
  const BlockKey block = *blockOpt;
  disagreementTitle_->setText(
      QStringLiteral("%1 · disagreement").arg(blockLabel(block)));

  const QStringList pids = compareParticipantIds();
  auto& net = doc_->network();

  auto participantLabel = [&](const QString& id) -> QString {
    if (const auto* p = doc_->root().find_participant(id.toStdString())) {
      if (!p->name.empty()) return QString::fromStdString(p->name);
    }
    return id;
  };

  double lowestAlign = 100.0;
  QString lowestLabel;
  int row = 0;

  if (block.kind == BlockKind::NodeRatings) {
    std::vector<std::string> alts;
    for (const QString& id : pids) {
      const auto* rt = userRatings(net, block, id.toStdString());
      if (rt && !rt->empty()) {
        alts = rt->alternatives();
        break;
      }
    }
    if (alts.empty()) {
      const anpcpp::AnpNode* n = net.find_node(block.wrt.toStdString());
      if (n) {
        const auto* slot = n->node_prioritizer(block.dest.toStdString());
        if (slot) alts = slot->ratings.alternatives();
      }
    }

    disagreementTable_->setRowCount(static_cast<int>(alts.size()));
    for (const std::string& alt : alts) {
      std::vector<double> vals;
      QStringList who;
      for (const QString& id : pids) {
        const auto* rt = userRatings(net, block, id.toStdString());
        if (rt == nullptr || !rt->has_alternative(alt)) continue;
        const bool present =
            (rt->mode() == anpcpp::RatingsPrioritizer::Mode::Categorical)
                ? rt->rating(alt).has_value()
                : rt->value(alt).has_value();
        if (!present) continue;
        const auto scores = rt->scores();
        const std::size_t idx = rt->index_of(alt);
        if (idx >= scores.size()) continue;
        vals.push_back(scores[idx]);
        who << QStringLiteral("%1=%2")
                   .arg(participantLabel(id),
                        QString::number(scores[idx], 'g', 3));
      }
      const auto spread = anpcpp::summarize_ratings_votes(vals, 1.0);

      auto* pairLab = new QLabel(
          QStringLiteral("<b>%1</b><br/><span style='color:#5c574e;font-size:11px'>"
                         "%2 vote(s)%3</span>")
              .arg(QString::fromStdString(alt))
              .arg(spread.contributor_count)
              .arg(who.isEmpty()
                       ? QString()
                       : QStringLiteral(" · %1").arg(who.join(QStringLiteral(", ")))),
          disagreementTable_);
      pairLab->setWordWrap(true);
      disagreementTable_->setCellWidget(row, 0, pairLab);
      disagreementTable_->setCellWidget(
          row, 1, makeAlignChip(spread.alignment_pct, disagreementTable_));
      disagreementTable_->setCellWidget(
          row, 2,
          makeRangeCell(spread, false,
                        QStringLiteral("Ratings for %1").arg(QString::fromStdString(alt)),
                        disagreementTable_));

      if (spread.contributor_count >= 2 &&
          spread.alignment_pct < lowestAlign) {
        lowestAlign = spread.alignment_pct;
        lowestLabel = QString::fromStdString(alt);
      }
      ++row;
    }
  } else {
    std::vector<const anpcpp::PairwiseJudgments*> inputs;
    inputs.reserve(static_cast<std::size_t>(pids.size()));
    for (const QString& id : pids)
      inputs.push_back(userPairwise(net, block, id.toStdString()));
    const auto alts = resolveAlts(net, block, inputs);
    const int n = static_cast<int>(alts.size());
    const int pairCount = n < 2 ? 0 : n * (n - 1) / 2;
    disagreementTable_->setRowCount(pairCount);

    for (int i = 0; i < n; ++i) {
      for (int j = i + 1; j < n; ++j) {
        std::vector<double> vals;
        QStringList who;
        for (int pi = 0; pi < pids.size(); ++pi) {
          const anpcpp::PairwiseJudgments* pw =
              inputs[static_cast<std::size_t>(pi)];
          if (pw == nullptr || pw->size() != static_cast<std::size_t>(n))
            continue;
          const double v = pw->comparison(static_cast<std::size_t>(i),
                                          static_cast<std::size_t>(j));
          if (!(v > 0.0) || !std::isfinite(v)) continue;
          vals.push_back(v);
          who << QStringLiteral("%1=%2")
                     .arg(participantLabel(pids[pi]), formatPairwiseValue(v));
        }
        const auto spread = anpcpp::summarize_pairwise_votes(vals);
        const QString a = QString::fromStdString(alts[static_cast<std::size_t>(i)]);
        const QString b = QString::fromStdString(alts[static_cast<std::size_t>(j)]);

        auto* pairLab = new QLabel(
            QStringLiteral(
                "<b>%1 vs %2</b><br/><span style='color:#5c574e;font-size:11px'>"
                "%3 vote(s)%4</span>")
                .arg(a, b)
                .arg(spread.contributor_count)
                .arg(who.isEmpty()
                         ? QString()
                         : QStringLiteral(" · %1").arg(
                               who.join(QStringLiteral(", ")))),
            disagreementTable_);
        pairLab->setWordWrap(true);
        disagreementTable_->setCellWidget(row, 0, pairLab);
        disagreementTable_->setCellWidget(
            row, 1, makeAlignChip(spread.alignment_pct, disagreementTable_));
        disagreementTable_->setCellWidget(
            row, 2,
            makeRangeCell(spread, true,
                          QStringLiteral("%1 vs %2").arg(a, b),
                          disagreementTable_));

        if (spread.contributor_count >= 2 &&
            spread.alignment_pct < lowestAlign) {
          lowestAlign = spread.alignment_pct;
          lowestLabel = QStringLiteral("%1 vs %2").arg(a, b);
        }
        ++row;
      }
    }
  }

  disagreementTable_->setRowCount(row);
  if (!lowestLabel.isEmpty() && lowestAlign < 70.0) {
    distNote_->setText(
        QStringLiteral(
            "Lowest alignment in this block: %1 (%2%).")
            .arg(lowestLabel)
            .arg(QString::number(lowestAlign, 'f', 0)));
  } else if (!lowestLabel.isEmpty()) {
    distNote_->setText(
        QStringLiteral("Comparisons in this block are relatively aligned."));
  }
}

void ConsensusAnalysisWidget::refreshCoverage() {
  coverageTable_->clear();
  coverageTable_->setColumnCount(3);
  coverageTable_->setHorizontalHeaderLabels(
      {QStringLiteral("Participant"), QStringLiteral("This block"),
       QStringLiteral("All judgments")});

  const auto blockOpt = blockKeyDecode(blockCombo_->currentData().toString());
  const QStringList pids = compareParticipantIds();
  coverageTable_->setRowCount(pids.size());
  auto& net = doc_->network();

  for (int row = 0; row < pids.size(); ++row) {
    const QString id = pids[row];
    QString name = id;
    if (const auto* p = doc_->root().find_participant(id.toStdString())) {
      if (!p->name.empty()) name = QString::fromStdString(p->name);
    }
    coverageTable_->setItem(row, 0, new QTableWidgetItem(name));

    QString blockText = QStringLiteral("—");
    if (blockOpt) {
      const auto c = userBlockFill(net, *blockOpt, id.toStdString());
      if (c.needed == 0) {
        blockText = QStringLiteral("—");
      } else if (c.filled == c.needed) {
        blockText = QStringLiteral("✓  %1/%2").arg(c.filled).arg(c.needed);
      } else if (c.filled == 0) {
        blockText = QStringLiteral("—  0/%1").arg(c.needed);
      } else {
        blockText = QStringLiteral("%1/%2").arg(c.filled).arg(c.needed);
      }
    }
    coverageTable_->setItem(row, 1, new QTableWidgetItem(blockText));

    const auto all = userAllFill(net, id.toStdString());
    coverageTable_->setItem(
        row, 2,
        new QTableWidgetItem(
            QStringLiteral("%1/%2").arg(all.filled).arg(all.needed)));
  }
}

void ConsensusAnalysisWidget::refreshCohorts() {
  while (QLayoutItem* child = cohortBarsLay_->takeAt(0)) {
    if (child->widget()) child->widget()->deleteLater();
    delete child;
  }
  cohortDistNote_->clear();

  const auto& groups = doc_->root().judgment_groups();
  const bool ok = groups.size() >= 2;
  cohortHelp_->setVisible(!ok);
  cohortLeft_->setEnabled(ok);
  cohortRight_->setEnabled(ok);
  cohortBarsHost_->setVisible(ok);
  if (!ok) return;

  const QString leftId = cohortLeft_->currentData().toString();
  const QString rightId = cohortRight_->currentData().toString();
  if (leftId.isEmpty() || rightId.isEmpty() || leftId == rightId) {
    cohortDistNote_->setText(
        QStringLiteral("Choose two different groups to compare."));
    return;
  }

  auto& rootNet = doc_->root();
  const anpcpp::JudgmentSession saved = rootNet.judgment_session();
  const auto& lim = doc_->network().limit_matrix_options();
  const auto alts = doc_->network().alt_names();

  auto prioritiesForGroup = [&](const QString& gid)
      -> std::map<std::string, double> {
    rootNet.set_judgment_session(
        {anpcpp::JudgmentScopeKind::Group, gid.toStdString()});
    rootNet.rebuild_effective_judgments();
    try {
      return doc_->network().priority_map(lim);
    } catch (...) {
      return {};
    }
  };

  const auto leftMap = prioritiesForGroup(leftId);
  const auto rightMap = prioritiesForGroup(rightId);

  rootNet.set_judgment_session(saved);
  rootNet.rebuild_effective_judgments();

  const QString leftName = cohortLeft_->currentText();
  const QString rightName = cohortRight_->currentText();

  for (const std::string& alt : alts) {
    double l = 0.0;
    double r = 0.0;
    if (const auto it = leftMap.find(alt); it != leftMap.end()) l = it->second;
    if (const auto it = rightMap.find(alt); it != rightMap.end()) r = it->second;

    auto* rowW = new QWidget(cohortBarsHost_);
    auto* lay = new QHBoxLayout(rowW);
    lay->setContentsMargins(0, 2, 0, 2);
    auto* nameLab = new QLabel(QString::fromStdString(alt), rowW);
    nameLab->setMinimumWidth(48);
    lay->addWidget(nameLab);
    lay->addWidget(
        makeDualBar(rowW, l, r,
                    QStringLiteral("%1: %2").arg(leftName).arg(l, 0, 'g', 3),
                    QStringLiteral("%1: %2").arg(rightName).arg(r, 0, 'g', 3)),
        1);
    cohortBarsLay_->addWidget(rowW);
  }
  cohortBarsLay_->addStretch();

  cohortDistNote_->setText(
      QStringLiteral(
          "Left = %1, right = %2 (normalized pair per alternative).")
          .arg(leftName, rightName));
}
