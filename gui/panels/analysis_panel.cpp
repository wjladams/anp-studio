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
#include <QTabWidget>
#include <QTableWidget>
#include <QTextBrowser>
#include <QVBoxLayout>

#include <cmath>
#include <map>
#include <stdexcept>

namespace {

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

}  // namespace

AnalysisPanel::AnalysisPanel(Document* doc, QWidget* parent)
    : QWidget(parent), doc_(doc) {
  auto* root = new QHBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);

  tabs_ = new QTabWidget(this);
  tabs_->setTabPosition(QTabWidget::West);

  // --- Synthesis ---
  synthBrowser_ = new QTextBrowser(this);
  synthBrowser_->setOpenExternalLinks(false);
  tabs_->addTab(synthBrowser_, QStringLiteral("Synthesis"));

  // --- Sensitivity ---
  auto* sensPage = new QWidget(this);
  auto* sensLay = new QVBoxLayout(sensPage);
  auto* sensControls = new QHBoxLayout;
  sensMode_ = new QComboBox(sensPage);
  sensMode_->addItem(QStringLiteral("Interactive"));
  sensMode_->addItem(QStringLiteral("Global"));
  sensControls->addWidget(new QLabel(QStringLiteral("Mode:"), sensPage));
  sensControls->addWidget(sensMode_);
  sensControls->addWidget(new QLabel(QStringLiteral("Row Node:"), sensPage));
  sensWrt_ = new QComboBox(sensPage);
  sensControls->addWidget(sensWrt_, 1);
  sensLay->addLayout(sensControls);

  sensInteractiveHost_ = new QWidget(sensPage);
  auto* pRow = new QHBoxLayout(sensInteractiveHost_);
  pRow->setContentsMargins(0, 0, 0, 0);
  pRow->addWidget(new QLabel(QStringLiteral("p:"), sensInteractiveHost_));
  sensSlider_ = new QSlider(Qt::Horizontal, sensInteractiveHost_);
  sensSlider_->setRange(0, 100);
  sensSlider_->setValue(50);
  pRow->addWidget(sensSlider_, 1);
  sensPSpin_ = new QDoubleSpinBox(sensInteractiveHost_);
  sensPSpin_->setRange(0.0, 1.0);
  sensPSpin_->setSingleStep(0.01);
  sensPSpin_->setDecimals(2);
  sensPSpin_->setValue(0.5);
  pRow->addWidget(sensPSpin_);
  sensLay->addWidget(sensInteractiveHost_);

  sensChart_ = new SensitivityChartWidget(sensPage);
  sensLay->addWidget(sensChart_, 1);
  tabs_->addTab(sensPage, QStringLiteral("Sensitivity"));

  // --- Influence ---
  auto* inflPage = new QWidget(this);
  auto* inflLay = new QVBoxLayout(inflPage);
  auto* inflTop = new QHBoxLayout;
  inflMode_ = new QComboBox(inflPage);
  inflMode_->addItem(QStringLiteral("Raw"));
  inflMode_->addItem(QStringLiteral("Rank"));
  inflMode_->addItem(QStringLiteral("Marginal"));
  inflTop->addWidget(new QLabel(QStringLiteral("Mode:"), inflPage));
  inflTop->addWidget(inflMode_);
  inflTop->addWidget(new QLabel(QStringLiteral("Wrt:"), inflPage));
  inflWrt_ = new QComboBox(inflPage);
  inflTop->addWidget(inflWrt_, 1);
  inflLay->addLayout(inflTop);

  inflRawParams_ = new QWidget(inflPage);
  auto* rawForm = new QHBoxLayout(inflRawParams_);
  rawForm->setContentsMargins(0, 0, 0, 0);
  rawForm->addWidget(new QLabel(QStringLiteral("Δ up:"), inflRawParams_));
  inflDeltaUp_ = new QDoubleSpinBox(inflRawParams_);
  inflDeltaUp_->setRange(0.01, 0.5);
  inflDeltaUp_->setSingleStep(0.01);
  inflDeltaUp_->setValue(0.1);
  rawForm->addWidget(inflDeltaUp_);
  rawForm->addWidget(new QLabel(QStringLiteral("Δ down:"), inflRawParams_));
  inflDeltaDown_ = new QDoubleSpinBox(inflRawParams_);
  inflDeltaDown_->setRange(0.01, 0.5);
  inflDeltaDown_->setSingleStep(0.01);
  inflDeltaDown_->setValue(0.1);
  rawForm->addWidget(inflDeltaDown_);
  rawForm->addWidget(new QLabel(QStringLiteral("Decimals:"), inflRawParams_));
  inflDecimals_ = new QSpinBox(inflRawParams_);
  inflDecimals_->setRange(2, 8);
  inflDecimals_->setValue(4);
  rawForm->addWidget(inflDecimals_);
  rawForm->addStretch();
  inflLay->addWidget(inflRawParams_);

  inflTable_ = new QTableWidget(inflPage);
  inflTable_->setSortingEnabled(true);
  inflTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  inflTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  inflLay->addWidget(inflTable_, 1);
  tabs_->addTab(inflPage, QStringLiteral("Influence"));

  root->addWidget(tabs_);

  connect(doc_, &Document::modelChanged, this, &AnalysisPanel::refresh);
  connect(doc_, &Document::viewNetworkChanged, this, &AnalysisPanel::refresh);
  connect(sensMode_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &AnalysisPanel::onSensModeChanged);
  connect(sensWrt_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &AnalysisPanel::refreshSensitivity);
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
  connect(inflMode_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &AnalysisPanel::onInfluenceModeChanged);
  connect(inflWrt_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &AnalysisPanel::refreshInfluence);
  connect(inflDeltaUp_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &AnalysisPanel::onInfluenceParamsChanged);
  connect(inflDeltaDown_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &AnalysisPanel::onInfluenceParamsChanged);
  connect(inflDecimals_, QOverload<int>::of(&QSpinBox::valueChanged), this,
          &AnalysisPanel::onInfluenceParamsChanged);

  onSensModeChanged(0);
  onInfluenceModeChanged(0);
  refresh();
}

void AnalysisPanel::refresh() {
  rebuildSensWrtNodes();
  rebuildInfluenceWrtNodes();
  refreshSynthesisHtml();
  refreshSensitivity();
  refreshInfluence();
}

void AnalysisPanel::rebuildSensWrtNodes() {
  const QString cur = sensWrt_->currentText();
  sensWrt_->blockSignals(true);
  sensWrt_->clear();
  for (const auto& n : doc_->network().node_names()) {
    sensWrt_->addItem(QString::fromStdString(n));
  }
  const int idx = sensWrt_->findText(cur);
  if (idx >= 0) sensWrt_->setCurrentIndex(idx);
  sensWrt_->blockSignals(false);
}

void AnalysisPanel::rebuildInfluenceWrtNodes() {
  const QString cur = inflWrt_->currentText();
  inflWrt_->blockSignals(true);
  inflWrt_->clear();
  for (const auto& n : doc_->network().node_names()) {
    inflWrt_->addItem(QString::fromStdString(n));
  }
  const int idx = inflWrt_->findText(cur);
  if (idx >= 0) inflWrt_->setCurrentIndex(idx);
  inflWrt_->blockSignals(false);
}

void AnalysisPanel::onSensModeChanged(int index) {
  sensInteractiveHost_->setVisible(index == 0);
  refreshSensitivity();
}

void AnalysisPanel::onSensPChanged() {
  refreshSensitivity();
}

void AnalysisPanel::onInfluenceModeChanged(int index) {
  inflRawParams_->setVisible(index == 0);
  refreshInfluence();
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

    html += QStringLiteral("<h1>Unscaled Supermatrix</h1>");
    html += matrixHtml(net.unscaled_supermatrix(), nodeNames, nodeNames);

    html += QStringLiteral("<h1>Cluster matrix</h1>");
    html += matrixHtml(net.cluster_weight_matrix(), clusterNames, clusterNames);

    html += QStringLiteral("<h1>Scaled Supermatrix</h1>");
    html += matrixHtml(net.scaled_supermatrix(), nodeNames, nodeNames);

    html += QStringLiteral("<h1>Global priorities</h1>");
    html += vectorHtml(net.global_priority(), nodeNames);

    if (net.has_subnet()) {
      html += QStringLiteral("<h1>Subnetwork synthesis results</h1>");
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

    html += QStringLiteral("<h1>Alternative Scores</h1>");
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
  if (sensWrt_->currentText().isEmpty()) {
    sensChart_->setBarEntries({});
    return;
  }
  const QString wrt = sensWrt_->currentText();
  if (sensMode_->currentIndex() == 0) {
    const double p = sensPSpin_->value();
    const auto scores = altScoresAtP(wrt, p);
    QVector<std::pair<QString, double>> bars;
    for (const auto& s : scores) bars.push_back(s);
    sensChart_->setBarEntries(bars);
  } else {
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
    sensChart_->setLineSeries(names, xs, series);
  }
}

void AnalysisPanel::refreshInfluence() {
  inflTable_->clear();
  inflTable_->setRowCount(0);
  inflTable_->setColumnCount(0);
  if (inflWrt_->currentText().isEmpty()) return;

  try {
    auto& net = doc_->network();
    const std::string wrt = inflWrt_->currentText().toStdString();
    const int mode = inflMode_->currentIndex();
    const int decimals = inflDecimals_->value();

    if (mode == 0) {
      const auto rows = net.influence_raw(
          wrt, inflDeltaUp_->value(), inflDeltaDown_->value(), 0.5);
      inflTable_->setColumnCount(3);
      inflTable_->setHorizontalHeaderLabels(
          {QStringLiteral("Original"), QStringLiteral("Up"),
           QStringLiteral("Down")});
      inflTable_->setRowCount(static_cast<int>(rows.size()));
      for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
        const auto& r = rows[static_cast<std::size_t>(i)];
        inflTable_->setVerticalHeaderItem(
            i, new QTableWidgetItem(QString::fromStdString(r.name)));
        auto* o = new QTableWidgetItem(fmt(r.original, decimals));
        o->setData(Qt::UserRole, r.original);
        inflTable_->setItem(i, 0, o);
        const QString upTxt =
            fmt(r.up_score, decimals) + QStringLiteral(" [") +
            fmt(r.up_diff, decimals) + QStringLiteral("]");
        auto* u = new QTableWidgetItem(upTxt);
        u->setData(Qt::UserRole, r.up_score);
        inflTable_->setItem(i, 1, u);
        const QString downTxt =
            fmt(r.down_score, decimals) + QStringLiteral(" [") +
            fmt(r.down_diff, decimals) + QStringLiteral("]");
        auto* d = new QTableWidgetItem(downTxt);
        d->setData(Qt::UserRole, r.down_score);
        inflTable_->setItem(i, 2, d);
      }
    } else if (mode == 1) {
      const auto rows = net.influence_rank(wrt);
      inflTable_->setColumnCount(2);
      inflTable_->setHorizontalHeaderLabels(
          {QStringLiteral("Original Score"), QStringLiteral("Rank Influence")});
      inflTable_->setRowCount(static_cast<int>(rows.size()));
      for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
        const auto& r = rows[static_cast<std::size_t>(i)];
        inflTable_->setVerticalHeaderItem(
            i, new QTableWidgetItem(QString::fromStdString(r.name)));
        auto* o = new QTableWidgetItem(fmt(r.original, decimals));
        o->setData(Qt::UserRole, r.original);
        inflTable_->setItem(i, 0, o);
        auto* s = new QTableWidgetItem(fmt(r.rank_influence, decimals));
        s->setData(Qt::UserRole, r.rank_influence);
        inflTable_->setItem(i, 1, s);
      }
    } else {
      const auto rows = net.influence_marginal_smart(wrt);
      inflTable_->setColumnCount(2);
      inflTable_->setHorizontalHeaderLabels(
          {QStringLiteral("Marginal"), QStringLiteral("Smart p₀")});
      inflTable_->setRowCount(static_cast<int>(rows.size()));
      for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
        const auto& r = rows[static_cast<std::size_t>(i)];
        inflTable_->setVerticalHeaderItem(
            i, new QTableWidgetItem(QString::fromStdString(r.name)));
        auto* m = new QTableWidgetItem(fmt(r.marginal, decimals));
        m->setData(Qt::UserRole, r.marginal);
        inflTable_->setItem(i, 0, m);
        auto* p0 = new QTableWidgetItem(fmt(r.smart_p0, decimals));
        p0->setData(Qt::UserRole, r.smart_p0);
        inflTable_->setItem(i, 1, p0);
      }
    }
  } catch (...) {
  }
}
