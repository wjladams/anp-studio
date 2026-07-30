#include "panels/analysis_panel.hpp"

#include "document.hpp"
#include "panels/sensitivity_chart_widget.hpp"

#include "anpcpp/limit_matrix.hpp"
#include "anpcpp/matrix.hpp"
#include "anpcpp/rowsens.hpp"
#include "anpcpp/synthesis.hpp"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTextBrowser>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUrl>
#include <QVBoxLayout>

#include <cmath>
#include <map>
#include <stdexcept>

namespace {

constexpr int kRolePage = Qt::UserRole;
constexpr int kRoleAnchor = Qt::UserRole + 1;

QString esc(const QString& s) {
  QString o = s;
  o.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
  o.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
  o.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
  return o;
}

QString fmt(double v, int decimals = 4) {
  return QString::number(v, 'f', decimals);
}

QString matrixHtml(const anpcpp::Matrix& m,
                   const std::vector<std::string>& rows,
                   const std::vector<std::string>& cols) {
  QString html = QStringLiteral("<table border='1' cellspacing='0' cellpadding='4'>");
  html += QStringLiteral("<tr><th></th>");
  for (const auto& c : cols) {
    html += QStringLiteral("<th>") + esc(QString::fromStdString(c)) +
            QStringLiteral("</th>");
  }
  html += QStringLiteral("</tr>");
  for (std::size_t i = 0; i < m.rows(); ++i) {
    html += QStringLiteral("<tr><th>") +
            esc(QString::fromStdString(i < rows.size() ? rows[i] : "?")) +
            QStringLiteral("</th>");
    for (std::size_t j = 0; j < m.cols(); ++j) {
      html += QStringLiteral("<td>") + fmt(m(i, j)) + QStringLiteral("</td>");
    }
    html += QStringLiteral("</tr>");
  }
  html += QStringLiteral("</table>");
  return html;
}

QString vectorHtml(const anpcpp::Vector& v,
                   const std::vector<std::string>& labels) {
  QString html = QStringLiteral("<table border='1' cellspacing='0' cellpadding='4'>");
  html += QStringLiteral("<tr><th>Node</th><th>Priority</th></tr>");
  for (std::size_t i = 0; i < v.size(); ++i) {
    html += QStringLiteral("<tr><td>") +
            esc(QString::fromStdString(i < labels.size() ? labels[i] : "?")) +
            QStringLiteral("</td><td>") + fmt(v[i]) + QStringLiteral("</td></tr>");
  }
  html += QStringLiteral("</table>");
  return html;
}

std::size_t nodeRowIndex(const anpcpp::AnpNetwork& net, const std::string& name) {
  const auto names = net.node_names();
  for (std::size_t i = 0; i < names.size(); ++i) {
    if (names[i] == name) return i;
  }
  throw std::out_of_range("unknown node");
}

QTreeWidgetItem* makeNavItem(QTreeWidgetItem* parent,
                             const QString& text,
                             AnalysisPanel::Page page,
                             const QString& anchor = QString()) {
  auto* item = parent ? new QTreeWidgetItem(parent)
                      : new QTreeWidgetItem();
  item->setText(0, text);
  item->setData(0, kRolePage, static_cast<int>(page));
  item->setData(0, kRoleAnchor, anchor);
  item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
  return item;
}

QTableWidget* makeInfluenceTable(QWidget* parent) {
  auto* table = new QTableWidget(parent);
  table->setSortingEnabled(true);
  table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  return table;
}

}  // namespace

AnalysisPanel::AnalysisPanel(Document* doc, QWidget* parent)
    : QWidget(parent), doc_(doc) {
  auto* root = new QHBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);

  nav_ = new QTreeWidget(this);
  nav_->setObjectName(QStringLiteral("analysisNav"));
  nav_->setHeaderHidden(true);
  nav_->setRootIsDecorated(true);
  nav_->setAnimated(false);
  nav_->setIndentation(16);
  nav_->setMinimumWidth(220);
  nav_->setMaximumWidth(300);
  nav_->setFocusPolicy(Qt::NoFocus);
  buildNavTree();
  root->addWidget(nav_);

  stack_ = new QStackedWidget(this);

  // --- Synthesis ---
  synthBrowser_ = new QTextBrowser(stack_);
  synthBrowser_->setOpenExternalLinks(false);
  stack_->addWidget(synthBrowser_);

  // --- Sensitivity overview ---
  sensOverview_ = new QTextBrowser(stack_);
  sensOverview_->setOpenExternalLinks(false);
  sensOverview_->setOpenLinks(false);
  sensOverview_->setHtml(QStringLiteral(
      "<html><body style='font-family: sans-serif; max-width: 40em;'>"
      "<h1>Sensitivity</h1>"
      "<p>ANP Row Sensitivity varies a parameter <i>p</i> for a chosen row "
      "(Wrt) node while holding the rest of the model fixed. At "
      "<i>p</i>&nbsp;=&nbsp;0.5 the original priorities are recovered.</p>"
      "<h2><a href='anp://SensInteractive'>Interactive</a></h2>"
      "<p>Pick a Wrt node and adjust <i>p</i> with a slider. Alternative scores "
      "for the selected network are shown as a horizontal bar chart.</p>"
      "<h2><a href='anp://SensGlobal'>Global</a></h2>"
      "<p>Sweep <i>p</i> from 0.01 to 1.00 in steps of 0.01 and plot each "
      "alternative's synthesized score as a line series against <i>p</i>.</p>"
      "</body></html>"));
  stack_->addWidget(sensOverview_);

  // --- Sensitivity Interactive ---
  auto* sensInteractivePage = new QWidget(stack_);
  auto* sensIntLay = new QVBoxLayout(sensInteractivePage);
  auto* sensIntControls = new QHBoxLayout;
  sensIntControls->addWidget(new QLabel(QStringLiteral("Row Node:"), sensInteractivePage));
  sensWrtInteractive_ = new QComboBox(sensInteractivePage);
  sensIntControls->addWidget(sensWrtInteractive_, 1);
  sensIntLay->addLayout(sensIntControls);

  auto* pRow = new QHBoxLayout;
  pRow->addWidget(new QLabel(QStringLiteral("p:"), sensInteractivePage));
  sensSlider_ = new QSlider(Qt::Horizontal, sensInteractivePage);
  sensSlider_->setRange(0, 100);
  sensSlider_->setValue(50);
  pRow->addWidget(sensSlider_, 1);
  sensPSpin_ = new QDoubleSpinBox(sensInteractivePage);
  sensPSpin_->setRange(0.0, 1.0);
  sensPSpin_->setSingleStep(0.01);
  sensPSpin_->setDecimals(2);
  sensPSpin_->setValue(0.5);
  pRow->addWidget(sensPSpin_);
  sensIntLay->addLayout(pRow);

  sensChartInteractive_ = new SensitivityChartWidget(sensInteractivePage);
  sensIntLay->addWidget(sensChartInteractive_, 1);
  stack_->addWidget(sensInteractivePage);

  // --- Sensitivity Global ---
  auto* sensGlobalPage = new QWidget(stack_);
  auto* sensGlobLay = new QVBoxLayout(sensGlobalPage);
  auto* sensGlobControls = new QHBoxLayout;
  sensGlobControls->addWidget(new QLabel(QStringLiteral("Row Node:"), sensGlobalPage));
  sensWrtGlobal_ = new QComboBox(sensGlobalPage);
  sensGlobControls->addWidget(sensWrtGlobal_, 1);
  sensGlobLay->addLayout(sensGlobControls);
  sensChartGlobal_ = new SensitivityChartWidget(sensGlobalPage);
  sensGlobLay->addWidget(sensChartGlobal_, 1);
  stack_->addWidget(sensGlobalPage);

  // --- Influence overview ---
  inflOverview_ = new QTextBrowser(stack_);
  inflOverview_->setOpenExternalLinks(false);
  inflOverview_->setOpenLinks(false);
  inflOverview_->setHtml(QStringLiteral(
      "<html><body style='font-family: sans-serif; max-width: 40em;'>"
      "<h1>Influence analysis</h1>"
      "<p>Influence analysis measures how alternative scores respond when a "
      "node's row weight is perturbed around the resting parameter "
      "<i>p</i><sub>0</sub>&nbsp;=&nbsp;0.5 (the value that recovers the "
      "original supermatrix).</p>"
      "<h2><a href='anp://InflRaw'>Raw</a></h2>"
      "<p>Pick a Wrt node, apply fixed Δ up and Δ down to <i>p</i>, "
      "resynthesize, and report each alternative's original score plus the "
      "upward and downward scores with differences.</p>"
      "<h2><a href='anp://InflRank'>Rank</a></h2>"
      "<p>For each node (row), compute that row's rank influence score: how "
      "far <i>p</i> must move before alternative rankings change.</p>"
      "<h2><a href='anp://InflMarginal'>Marginal</a></h2>"
      "<p>For each node (row), compute that row's smart-<i>p</i><sub>0</sub> "
      "marginal influence (L1 of absolute alternative sensitivities).</p>"
      "<h2><a href='anp://InflTotal'>Total</a></h2>"
      "<p>For each node (row), apply a fixed Δ from <i>p</i><sub>0</sub> and "
      "report total influence (sum of absolute alternative-score changes) and "
      "max alt change, matching pyanp fixed influence.</p>"
      "</body></html>"));
  stack_->addWidget(inflOverview_);

  // --- Influence Raw ---
  auto* inflRawPage = new QWidget(stack_);
  auto* inflRawLay = new QVBoxLayout(inflRawPage);
  auto* inflRawTop = new QHBoxLayout;
  inflRawTop->addWidget(new QLabel(QStringLiteral("Wrt:"), inflRawPage));
  inflWrtRaw_ = new QComboBox(inflRawPage);
  inflRawTop->addWidget(inflWrtRaw_, 1);
  inflRawLay->addLayout(inflRawTop);

  auto* rawForm = new QHBoxLayout;
  rawForm->addWidget(new QLabel(QStringLiteral("Δ up:"), inflRawPage));
  inflDeltaUp_ = new QDoubleSpinBox(inflRawPage);
  inflDeltaUp_->setRange(0.01, 0.5);
  inflDeltaUp_->setSingleStep(0.01);
  inflDeltaUp_->setValue(0.1);
  rawForm->addWidget(inflDeltaUp_);
  rawForm->addWidget(new QLabel(QStringLiteral("Δ down:"), inflRawPage));
  inflDeltaDown_ = new QDoubleSpinBox(inflRawPage);
  inflDeltaDown_->setRange(0.01, 0.5);
  inflDeltaDown_->setSingleStep(0.01);
  inflDeltaDown_->setValue(0.1);
  rawForm->addWidget(inflDeltaDown_);
  rawForm->addWidget(new QLabel(QStringLiteral("Decimals:"), inflRawPage));
  inflDecimals_ = new QSpinBox(inflRawPage);
  inflDecimals_->setRange(2, 8);
  inflDecimals_->setValue(4);
  rawForm->addWidget(inflDecimals_);
  rawForm->addStretch();
  inflRawLay->addLayout(rawForm);

  inflTableRaw_ = makeInfluenceTable(inflRawPage);
  inflRawLay->addWidget(inflTableRaw_, 1);
  stack_->addWidget(inflRawPage);

  // --- Influence Rank ---
  auto* inflRankPage = new QWidget(stack_);
  auto* inflRankLay = new QVBoxLayout(inflRankPage);
  inflTableRank_ = makeInfluenceTable(inflRankPage);
  inflRankLay->addWidget(inflTableRank_, 1);
  stack_->addWidget(inflRankPage);

  // --- Influence Marginal ---
  auto* inflMargPage = new QWidget(stack_);
  auto* inflMargLay = new QVBoxLayout(inflMargPage);
  inflTableMarginal_ = makeInfluenceTable(inflMargPage);
  inflMargLay->addWidget(inflTableMarginal_, 1);
  stack_->addWidget(inflMargPage);

  // --- Influence Total ---
  auto* inflTotalPage = new QWidget(stack_);
  auto* inflTotalLay = new QVBoxLayout(inflTotalPage);
  auto* totalForm = new QHBoxLayout;
  totalForm->addWidget(new QLabel(QStringLiteral("Δ:"), inflTotalPage));
  inflDeltaTotal_ = new QDoubleSpinBox(inflTotalPage);
  inflDeltaTotal_->setRange(0.01, 0.5);
  inflDeltaTotal_->setSingleStep(0.01);
  inflDeltaTotal_->setValue(0.25);
  totalForm->addWidget(inflDeltaTotal_);
  totalForm->addStretch();
  inflTotalLay->addLayout(totalForm);
  inflTableTotal_ = makeInfluenceTable(inflTotalPage);
  inflTotalLay->addWidget(inflTableTotal_, 1);
  stack_->addWidget(inflTotalPage);

  root->addWidget(stack_, 1);

  connect(doc_, &Document::modelChanged, this, &AnalysisPanel::refresh);
  connect(doc_, &Document::viewNetworkChanged, this, &AnalysisPanel::refresh);
  connect(nav_, &QTreeWidget::itemActivated, this,
          &AnalysisPanel::onNavItemActivated);
  connect(nav_, &QTreeWidget::currentItemChanged, this,
          &AnalysisPanel::onNavCurrentChanged);
  connect(sensOverview_, &QTextBrowser::anchorClicked, this,
          &AnalysisPanel::onOverviewAnchorClicked);
  connect(inflOverview_, &QTextBrowser::anchorClicked, this,
          &AnalysisPanel::onOverviewAnchorClicked);

  connect(sensWrtInteractive_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &AnalysisPanel::refreshSensitivity);
  connect(sensWrtGlobal_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &AnalysisPanel::refreshSensitivity);
  connect(sensSlider_, &QSlider::valueChanged, this, [this](int v) {
    if (updating_) return;
    updating_ = true;
    sensPSpin_->setValue(v / 100.0);
    updating_ = false;
    refreshSensitivity();
  });
  connect(sensPSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, [this](double v) {
            if (updating_) return;
            updating_ = true;
            sensSlider_->setValue(static_cast<int>(std::lround(v * 100.0)));
            updating_ = false;
            refreshSensitivity();
          });

  connect(inflWrtRaw_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &AnalysisPanel::refreshInfluence);
  connect(inflDeltaUp_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &AnalysisPanel::onInfluenceParamsChanged);
  connect(inflDeltaDown_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &AnalysisPanel::onInfluenceParamsChanged);
  connect(inflDeltaTotal_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &AnalysisPanel::onInfluenceParamsChanged);
  connect(inflDecimals_, QOverload<int>::of(&QSpinBox::valueChanged), this,
          &AnalysisPanel::onInfluenceParamsChanged);

  navigateTo(Page::Synthesis);
  refresh();
}

void AnalysisPanel::buildNavTree() {
  nav_->clear();

  synthItem_ = makeNavItem(nullptr, QStringLiteral("Synthesis"), Page::Synthesis);
  nav_->addTopLevelItem(synthItem_);
  makeNavItem(synthItem_, QStringLiteral("Unscaled Supermatrix"), Page::Synthesis,
              QStringLiteral("unscaled"));
  makeNavItem(synthItem_, QStringLiteral("Cluster matrix"), Page::Synthesis,
              QStringLiteral("cluster"));
  makeNavItem(synthItem_, QStringLiteral("Scaled Supermatrix"), Page::Synthesis,
              QStringLiteral("scaled"));
  makeNavItem(synthItem_, QStringLiteral("Global priorities"), Page::Synthesis,
              QStringLiteral("global"));
  subnetNavItem_ = makeNavItem(synthItem_, QStringLiteral("Subnetwork synthesis results"),
                               Page::Synthesis, QStringLiteral("subnet"));
  makeNavItem(synthItem_, QStringLiteral("Alternative Scores"), Page::Synthesis,
              QStringLiteral("alts"));

  auto* sensItem =
      makeNavItem(nullptr, QStringLiteral("Sensitivity"), Page::SensOverview);
  nav_->addTopLevelItem(sensItem);
  makeNavItem(sensItem, QStringLiteral("Interactive"), Page::SensInteractive);
  makeNavItem(sensItem, QStringLiteral("Global"), Page::SensGlobal);

  auto* inflItem = makeNavItem(nullptr, QStringLiteral("Influence analysis"),
                               Page::InflOverview);
  nav_->addTopLevelItem(inflItem);
  makeNavItem(inflItem, QStringLiteral("Raw"), Page::InflRaw);
  makeNavItem(inflItem, QStringLiteral("Rank"), Page::InflRank);
  makeNavItem(inflItem, QStringLiteral("Marginal"), Page::InflMarginal);
  makeNavItem(inflItem, QStringLiteral("Total"), Page::InflTotal);

  nav_->expandAll();
}

void AnalysisPanel::updateSubnetNavVisibility() {
  if (!subnetNavItem_) return;
  const bool show = doc_->network().has_subnet();
  subnetNavItem_->setHidden(!show);
  if (!show && synthAnchor_ == QStringLiteral("subnet")) {
    synthAnchor_.clear();
  }
}

void AnalysisPanel::selectNavForPage(Page page, const QString& anchor) {
  navigating_ = true;
  QTreeWidgetItemIterator it(nav_);
  while (*it) {
    QTreeWidgetItem* item = *it;
    const auto itemPage =
        static_cast<Page>(item->data(0, kRolePage).toInt());
    const QString itemAnchor = item->data(0, kRoleAnchor).toString();
    if (itemPage == page && itemAnchor == anchor && !item->isHidden()) {
      nav_->setCurrentItem(item);
      break;
    }
    ++it;
  }
  navigating_ = false;
}

void AnalysisPanel::navigateTo(Page page, const QString& anchor) {
  stack_->setCurrentIndex(static_cast<int>(page));
  if (page == Page::Synthesis) {
    synthAnchor_ = anchor;
    if (!anchor.isEmpty()) {
      // Defer scroll until after layout/HTML paint.
      QTimer::singleShot(0, this, [this, anchor]() {
        synthBrowser_->scrollToAnchor(anchor);
      });
    }
  } else {
    synthAnchor_.clear();
  }
  selectNavForPage(page, anchor);
}

void AnalysisPanel::onNavItemActivated(QTreeWidgetItem* item, int /*column*/) {
  if (!item || navigating_) return;
  const auto page = static_cast<Page>(item->data(0, kRolePage).toInt());
  const QString anchor = item->data(0, kRoleAnchor).toString();
  navigateTo(page, anchor);
}

void AnalysisPanel::onNavCurrentChanged(QTreeWidgetItem* current,
                                        QTreeWidgetItem* /*previous*/) {
  if (!current || navigating_) return;
  const auto page = static_cast<Page>(current->data(0, kRolePage).toInt());
  const QString anchor = current->data(0, kRoleAnchor).toString();
  navigateTo(page, anchor);
}

void AnalysisPanel::onOverviewAnchorClicked(const QUrl& url) {
  if (url.scheme() != QStringLiteral("anp")) return;
  const QString host = url.host();
  if (host == QStringLiteral("SensInteractive")) {
    navigateTo(Page::SensInteractive);
  } else if (host == QStringLiteral("SensGlobal")) {
    navigateTo(Page::SensGlobal);
  } else if (host == QStringLiteral("InflRaw")) {
    navigateTo(Page::InflRaw);
  } else if (host == QStringLiteral("InflRank")) {
    navigateTo(Page::InflRank);
  } else if (host == QStringLiteral("InflMarginal")) {
    navigateTo(Page::InflMarginal);
  } else if (host == QStringLiteral("InflTotal")) {
    navigateTo(Page::InflTotal);
  }
}

void AnalysisPanel::refresh() {
  updateSubnetNavVisibility();
  rebuildSensWrtNodes();
  rebuildInfluenceWrtNodes();
  refreshSynthesisHtml();
  refreshSensitivity();
  refreshInfluence();
  if (stack_->currentIndex() == static_cast<int>(Page::Synthesis) &&
      !synthAnchor_.isEmpty()) {
    QTimer::singleShot(0, this, [this]() {
      synthBrowser_->scrollToAnchor(synthAnchor_);
    });
  }
}

void AnalysisPanel::fillWrtCombo(QComboBox* combo, const QString& prefer) {
  combo->blockSignals(true);
  combo->clear();
  for (const auto& n : doc_->network().node_names()) {
    combo->addItem(QString::fromStdString(n));
  }
  const int idx = combo->findText(prefer);
  if (idx >= 0) combo->setCurrentIndex(idx);
  combo->blockSignals(false);
}

void AnalysisPanel::rebuildSensWrtNodes() {
  fillWrtCombo(sensWrtInteractive_, sensWrtInteractive_->currentText());
  fillWrtCombo(sensWrtGlobal_, sensWrtGlobal_->currentText());
}

void AnalysisPanel::rebuildInfluenceWrtNodes() {
  fillWrtCombo(inflWrtRaw_, inflWrtRaw_->currentText());
}

void AnalysisPanel::onInfluenceParamsChanged() {
  refreshInfluence();
}

std::vector<std::pair<QString, double>> AnalysisPanel::altScoresAtP(
    const QString& wrt,
    double p) const {
  std::vector<std::pair<QString, double>> out;
  try {
    const auto scores = doc_->network().priority_map_at_p(
        wrt.toStdString(), p, anpcpp::P0Mode::Direct(0.5));
    for (const auto& alt : doc_->network().alt_names()) {
      const auto it = scores.find(alt);
      out.emplace_back(QString::fromStdString(alt),
                       it == scores.end() ? 0.0 : it->second);
    }
  } catch (...) {
  }
  return out;
}

void AnalysisPanel::refreshSynthesisHtml() {
  QString html = QStringLiteral("<html><body style='font-family: sans-serif;'>");
  try {
    auto& net = doc_->network();
    const auto nodeNames = net.node_names();
    const auto clusterNames = net.cluster_names();

    html += QStringLiteral("<h1 id='unscaled'>Unscaled Supermatrix</h1>");
    html += matrixHtml(net.unscaled_supermatrix(), nodeNames, nodeNames);

    html += QStringLiteral("<h1 id='cluster'>Cluster matrix</h1>");
    html += matrixHtml(net.cluster_weight_matrix(), clusterNames, clusterNames);

    html += QStringLiteral("<h1 id='scaled'>Scaled Supermatrix</h1>");
    html += matrixHtml(net.scaled_supermatrix(), nodeNames, nodeNames);

    html += QStringLiteral("<h1 id='global'>Global priorities</h1>");
    html += vectorHtml(net.global_priority(), nodeNames);

    if (net.has_subnet()) {
      html += QStringLiteral("<h1 id='subnet'>Subnetwork synthesis results</h1>");
      std::vector<anpcpp::AnpNode*> hosts;
      for (anpcpp::AnpNode* n : net.nodes()) {
        if (n->has_subnetwork()) hosts.push_back(n);
      }
      const anpcpp::Vector g = net.global_priority();
      std::vector<double> hostW;
      double hostSum = 0.0;
      for (anpcpp::AnpNode* h : hosts) {
        const double w = g[nodeRowIndex(net, h->name())];
        hostW.push_back(w);
        hostSum += w;
      }
      if (hostSum > 0.0) {
        for (double& w : hostW) w /= hostSum;
      }

      const auto alts = net.alt_names();
      html += QStringLiteral("<table border='1' cellspacing='0' cellpadding='4'><tr><th></th>");
      for (anpcpp::AnpNode* h : hosts) {
        html += QStringLiteral("<th>") + esc(QString::fromStdString(h->name())) +
                QStringLiteral("</th>");
      }
      html += QStringLiteral("</tr><tr><th>Normalized host weights</th>");
      for (double w : hostW) {
        html += QStringLiteral("<td>") + fmt(w) + QStringLiteral("</td>");
      }
      html += QStringLiteral("</tr>");

      for (const auto& alt : alts) {
        html += QStringLiteral("<tr><th>") + esc(QString::fromStdString(alt)) +
                QStringLiteral("</th>");
        for (anpcpp::AnpNode* h : hosts) {
          double score = 0.0;
          try {
            const auto pm = h->subnetwork()->priority_map();
            const auto it = pm.find(alt);
            if (it != pm.end()) score = it->second;
          } catch (...) {
          }
          html += QStringLiteral("<td>") + fmt(score) + QStringLiteral("</td>");
        }
        html += QStringLiteral("</tr>");
      }
      html += QStringLiteral("</table>");
    }

    html += QStringLiteral("<h1 id='alts'>Alternative Scores</h1>");
    const auto alts = net.alt_names();
    const anpcpp::Vector scores = net.priority();
    html += QStringLiteral("<table border='1' cellspacing='0' cellpadding='4'>");
    html += QStringLiteral("<tr><th>Alternative</th><th>Score</th></tr>");
    for (std::size_t i = 0; i < alts.size() && i < scores.size(); ++i) {
      html += QStringLiteral("<tr><td>") + esc(QString::fromStdString(alts[i])) +
              QStringLiteral("</td><td>") + fmt(scores[i]) +
              QStringLiteral("</td></tr>");
    }
    html += QStringLiteral("</table>");
  } catch (const std::exception& e) {
    html += QStringLiteral("<p><b>Error:</b> ") + esc(QString::fromUtf8(e.what())) +
            QStringLiteral("</p>");
  }
  html += QStringLiteral("</body></html>");
  synthBrowser_->setHtml(html);
}

void AnalysisPanel::refreshSensitivity() {
  // Interactive bars
  if (sensWrtInteractive_->currentText().isEmpty()) {
    sensChartInteractive_->setBarEntries({});
  } else {
    const QString wrt = sensWrtInteractive_->currentText();
    const double p = sensPSpin_->value();
    const auto scores = altScoresAtP(wrt, p);
    QVector<std::pair<QString, double>> bars;
    for (const auto& s : scores) bars.push_back(s);
    sensChartInteractive_->setBarEntries(bars);
  }

  // Global lines
  if (sensWrtGlobal_->currentText().isEmpty()) {
    sensChartGlobal_->setLineSeries({}, {}, {});
    return;
  }
  const QString wrt = sensWrtGlobal_->currentText();
  QVector<double> xs;
  for (int i = 1; i <= 100; ++i) xs.push_back(i / 100.0);
  std::vector<std::pair<QString, double>> first = altScoresAtP(wrt, xs.first());
  QVector<QString> names;
  for (const auto& s : first) names.push_back(s.first);
  QVector<QVector<double>> series(names.size());
  for (int s = 0; s < series.size(); ++s) series[s].resize(xs.size());

  for (int xi = 0; xi < xs.size(); ++xi) {
    const auto scores = altScoresAtP(wrt, xs[xi]);
    for (int s = 0; s < names.size(); ++s) {
      double v = 0.0;
      for (const auto& sc : scores) {
        if (sc.first == names[s]) {
          v = sc.second;
          break;
        }
      }
      series[s][xi] = v;
    }
  }
  sensChartGlobal_->setLineSeries(names, xs, series);
}

void AnalysisPanel::refreshInfluence() {
  const int decimals = inflDecimals_->value();

  auto clearTable = [](QTableWidget* table) {
    table->clear();
    table->setRowCount(0);
    table->setColumnCount(0);
  };

  clearTable(inflTableRaw_);
  clearTable(inflTableRank_);
  clearTable(inflTableMarginal_);
  clearTable(inflTableTotal_);

  try {
    auto& net = doc_->network();

    if (!inflWrtRaw_->currentText().isEmpty()) {
      const auto rows = net.influence_raw(
          inflWrtRaw_->currentText().toStdString(), inflDeltaUp_->value(),
          inflDeltaDown_->value(), 0.5);
      inflTableRaw_->setColumnCount(3);
      inflTableRaw_->setHorizontalHeaderLabels(
          {QStringLiteral("Original"), QStringLiteral("Up"),
           QStringLiteral("Down")});
      inflTableRaw_->setRowCount(static_cast<int>(rows.size()));
      for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
        const auto& r = rows[static_cast<std::size_t>(i)];
        inflTableRaw_->setVerticalHeaderItem(
            i, new QTableWidgetItem(QString::fromStdString(r.name)));
        auto* o = new QTableWidgetItem(fmt(r.original, decimals));
        o->setData(Qt::UserRole, r.original);
        inflTableRaw_->setItem(i, 0, o);
        const QString upTxt =
            fmt(r.up_score, decimals) + QStringLiteral(" [") +
            fmt(r.up_diff, decimals) + QStringLiteral("]");
        auto* u = new QTableWidgetItem(upTxt);
        u->setData(Qt::UserRole, r.up_score);
        inflTableRaw_->setItem(i, 1, u);
        const QString downTxt =
            fmt(r.down_score, decimals) + QStringLiteral(" [") +
            fmt(r.down_diff, decimals) + QStringLiteral("]");
        auto* d = new QTableWidgetItem(downTxt);
        d->setData(Qt::UserRole, r.down_score);
        inflTableRaw_->setItem(i, 2, d);
      }
    }

    {
      const auto rows = net.influence_rank();
      inflTableRank_->setSortingEnabled(false);
      inflTableRank_->setColumnCount(2);
      inflTableRank_->setHorizontalHeaderLabels(
          {QStringLiteral("Original Score"), QStringLiteral("Rank Influence")});
      inflTableRank_->setRowCount(static_cast<int>(rows.size()));
      for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
        const auto& r = rows[static_cast<std::size_t>(i)];
        inflTableRank_->setVerticalHeaderItem(
            i, new QTableWidgetItem(QString::fromStdString(r.name)));
        auto* o = new QTableWidgetItem(fmt(r.original, decimals));
        o->setData(Qt::UserRole, r.original);
        inflTableRank_->setItem(i, 0, o);
        auto* s = new QTableWidgetItem(fmt(r.rank_influence, decimals));
        s->setData(Qt::UserRole, r.rank_influence);
        inflTableRank_->setItem(i, 1, s);
      }
      inflTableRank_->setSortingEnabled(true);
    }

    {
      const auto rows = net.influence_marginal_smart();
      inflTableMarginal_->setSortingEnabled(false);
      inflTableMarginal_->setColumnCount(2);
      inflTableMarginal_->setHorizontalHeaderLabels(
          {QStringLiteral("Marginal"), QStringLiteral("Smart p₀")});
      inflTableMarginal_->setRowCount(static_cast<int>(rows.size()));
      for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
        const auto& r = rows[static_cast<std::size_t>(i)];
        inflTableMarginal_->setVerticalHeaderItem(
            i, new QTableWidgetItem(QString::fromStdString(r.name)));
        auto* m = new QTableWidgetItem(fmt(r.marginal, decimals));
        m->setData(Qt::UserRole, r.marginal);
        inflTableMarginal_->setItem(i, 0, m);
        auto* p0 = new QTableWidgetItem(fmt(r.smart_p0, decimals));
        p0->setData(Qt::UserRole, r.smart_p0);
        inflTableMarginal_->setItem(i, 1, p0);
      }
      inflTableMarginal_->setSortingEnabled(true);
    }

    {
      const auto rows = net.influence_total(inflDeltaTotal_->value());
      inflTableTotal_->setSortingEnabled(false);
      inflTableTotal_->setColumnCount(2);
      inflTableTotal_->setHorizontalHeaderLabels(
          {QStringLiteral("Total Influence"), QStringLiteral("Max Alt Change")});
      inflTableTotal_->setRowCount(static_cast<int>(rows.size()));
      for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
        const auto& r = rows[static_cast<std::size_t>(i)];
        inflTableTotal_->setVerticalHeaderItem(
            i, new QTableWidgetItem(QString::fromStdString(r.name)));
        auto* t = new QTableWidgetItem(fmt(r.total_influence, decimals));
        t->setData(Qt::UserRole, r.total_influence);
        inflTableTotal_->setItem(i, 0, t);
        auto* m = new QTableWidgetItem(fmt(r.max_alt_change, decimals));
        m->setData(Qt::UserRole, r.max_alt_change);
        inflTableTotal_->setItem(i, 1, m);
      }
      inflTableTotal_->setSortingEnabled(true);
    }
  } catch (...) {
  }
}
