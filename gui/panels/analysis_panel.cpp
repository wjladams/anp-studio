#include "panels/analysis_panel.hpp"

/**
 * @file analysis_panel.cpp
 * @brief Analysis stage nav + Synthesis / Sensitivity / Influence /
 *        Perspective / Consensus.
 */

#include "document.hpp"
#include "html_report.hpp"
#include "panels/consensus_analysis_widget.hpp"
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
#include <QShowEvent>
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
#include <algorithm>
#include <map>
#include <stdexcept>
#include <vector>

namespace {

constexpr int kRolePage = Qt::UserRole;
constexpr int kRoleAnchor = Qt::UserRole + 1;

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

QString limitOptionsSummaryHtml(const anpcpp::LimitMatrixOptions& lim) {
  QString method = QStringLiteral("Calculus");
  if (lim.method == anpcpp::LimitMatrixMethod::NewHierarchy) {
    method = QStringLiteral("New Hierarchy");
  } else if (lim.method == anpcpp::LimitMatrixMethod::Sinks) {
    method = QStringLiteral("Sinks");
  }

  QString body = QStringLiteral(
      "<p class='muted'>Calculated from the scaled supermatrix using the "
      "network limit-matrix settings (Structure → Inspector):</p><ul>");
  body += QStringLiteral("<li>Method: <code>") + method +
          QStringLiteral("</code></li>");
  body += QStringLiteral("<li>Error tolerance: <code>") +
          QString::number(lim.error, 'g', 12) + QStringLiteral("</code></li>");
  body += QStringLiteral("<li>Max iterations: <code>") +
          QString::number(static_cast<qulonglong>(lim.max_iters)) +
          QStringLiteral("</code></li>");
  body += QStringLiteral("<li>Hierarchy formula shortcut: <code>") +
          QString(lim.use_hierarchy_formula ? QStringLiteral("yes")
                                            : QStringLiteral("no")) +
          QStringLiteral("</code></li>");
  body += QStringLiteral("<li>Start power: <code>") +
          (lim.start_pow == 0
               ? QStringLiteral("auto")
               : QString::number(static_cast<qulonglong>(lim.start_pow))) +
          QStringLiteral("</code></li>");
  if (lim.method == anpcpp::LimitMatrixMethod::NewHierarchy) {
    body += QStringLiteral("<li>With limit: <code>") +
            QString(lim.with_limit ? QStringLiteral("yes")
                                   : QStringLiteral("no")) +
            QStringLiteral("</code></li>");
    body += QStringLiteral("<li>With-limit max count: <code>") +
            QString::number(static_cast<qulonglong>(lim.max_count)) +
            QStringLiteral("</code></li>");
  }
  if (lim.method == anpcpp::LimitMatrixMethod::Sinks) {
    body += QStringLiteral("<li>Straight normalizer: <code>") +
            QString(lim.straight_normalizer ? QStringLiteral("yes")
                                            : QStringLiteral("no")) +
            QStringLiteral("</code></li>");
  }
  body += QStringLiteral("</ul>");
  return body;
}

/** Cell item for analysis tables (numeric key in Qt::UserRole). */
class AnalysisTableItem : public QTableWidgetItem {
public:
  using QTableWidgetItem::QTableWidgetItem;
};

constexpr int kRowLabelRole = Qt::UserRole + 1;
constexpr int kOriginalRowRole = Qt::UserRole + 2;

enum class AnalysisSortMode { Original = 0, Asc = 1, Desc = 2 };

void resetAnalysisTableSortState(QTableWidget* table) {
  table->setProperty("anpSortColumn", -1);
  table->setProperty("anpSortMode", static_cast<int>(AnalysisSortMode::Original));
  table->horizontalHeader()->setSortIndicatorShown(false);
}

void stampRowLabel(QTableWidget* table, int row, const QString& name) {
  table->setVerticalHeaderItem(row, new QTableWidgetItem(name));
  for (int c = 0; c < table->columnCount(); ++c) {
    if (QTableWidgetItem* item = table->item(row, c)) {
      item->setData(kRowLabelRole, name);
      item->setData(kOriginalRowRole, row);
    }
  }
}

/** Reorder whole rows (cells + vertical header) by column key or original index. */
void applyAnalysisTableSort(QTableWidget* table, int column,
                            AnalysisSortMode mode) {
  const int rows = table->rowCount();
  const int cols = table->columnCount();
  if (rows <= 0 || cols <= 0) return;

  struct RowPack {
    QString label;
    int original = 0;
    double key = 0.0;
    bool keyOk = false;
    std::vector<QTableWidgetItem*> cells;
  };

  std::vector<RowPack> packs;
  packs.reserve(static_cast<std::size_t>(rows));
  for (int r = 0; r < rows; ++r) {
    RowPack pack;
    if (const QTableWidgetItem* header = table->verticalHeaderItem(r)) {
      pack.label = header->text();
    }
    pack.cells.reserve(static_cast<std::size_t>(cols));
    for (int c = 0; c < cols; ++c) {
      QTableWidgetItem* item = table->takeItem(r, c);
      pack.cells.push_back(item);
      if (item != nullptr && c == 0) {
        if (pack.label.isEmpty()) {
          pack.label = item->data(kRowLabelRole).toString();
        }
        pack.original = item->data(kOriginalRowRole).toInt();
      }
    }
    if (mode != AnalysisSortMode::Original && column >= 0 && column < cols) {
      if (QTableWidgetItem* item = pack.cells[static_cast<std::size_t>(column)]) {
        const QVariant v = item->data(Qt::UserRole);
        if (v.isValid() && v.canConvert<double>()) {
          pack.key = v.toDouble();
          pack.keyOk = true;
        }
      }
    }
    packs.push_back(std::move(pack));
  }

  if (mode == AnalysisSortMode::Original) {
    std::stable_sort(packs.begin(), packs.end(),
                     [](const RowPack& a, const RowPack& b) {
                       return a.original < b.original;
                     });
  } else {
    const bool ascending = (mode == AnalysisSortMode::Asc);
    std::stable_sort(
        packs.begin(), packs.end(),
        [ascending](const RowPack& a, const RowPack& b) {
          if (a.keyOk && b.keyOk && a.key != b.key) {
            return ascending ? (a.key < b.key) : (a.key > b.key);
          }
          if (a.keyOk != b.keyOk) return a.keyOk;
          return a.original < b.original;
        });
  }

  for (int r = 0; r < rows; ++r) {
    RowPack& pack = packs[static_cast<std::size_t>(r)];
    for (int c = 0; c < cols; ++c) {
      table->setItem(r, c, pack.cells[static_cast<std::size_t>(c)]);
    }
    table->setVerticalHeaderItem(r, new QTableWidgetItem(pack.label));
  }

  QHeaderView* header = table->horizontalHeader();
  if (mode == AnalysisSortMode::Original) {
    header->setSortIndicatorShown(false);
  } else {
    header->setSortIndicatorShown(true);
    header->setSortIndicator(
        column, mode == AnalysisSortMode::Asc ? Qt::AscendingOrder
                                              : Qt::DescendingOrder);
  }
}

void cycleAnalysisTableSort(QTableWidget* table, int section) {
  if (section < 0 || section >= table->columnCount()) return;

  const int prevCol = table->property("anpSortColumn").toInt();
  const auto prevMode =
      static_cast<AnalysisSortMode>(table->property("anpSortMode").toInt());

  AnalysisSortMode mode;
  int col = section;
  if (prevMode == AnalysisSortMode::Original || prevCol != section) {
    mode = AnalysisSortMode::Asc;
  } else if (prevMode == AnalysisSortMode::Asc) {
    mode = AnalysisSortMode::Desc;
  } else {
    mode = AnalysisSortMode::Original;
    col = -1;
  }

  table->setProperty("anpSortColumn", col);
  table->setProperty("anpSortMode", static_cast<int>(mode));
  applyAnalysisTableSort(table, section, mode);
}

QTableWidget* makeInfluenceTable(QWidget* parent) {
  auto* table = new QTableWidget(parent);
  // Manual 3-state sort (asc → desc → original); built-in sorting leaves
  // vertical headers behind and only supports two directions.
  table->setSortingEnabled(false);
  table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  table->horizontalHeader()->setSectionsClickable(true);
  resetAnalysisTableSortState(table);
  QObject::connect(table->horizontalHeader(), &QHeaderView::sectionClicked, table,
                   [table](int section) { cycleAnalysisTableSort(table, section); });
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
  sensOverview_->setHtml(HtmlReport::wrapDocument(QStringLiteral(
      "<h1>Sensitivity</h1>"
      "<p>ANP Row Sensitivity varies a parameter <i>p</i> for a chosen row "
      "(Wrt) node while holding the rest of the model fixed. At "
      "<i>p</i>&nbsp;=&nbsp;0.5 the original priorities are recovered.</p>"
      "<h2><a href='anp://SensInteractive'>Interactive</a></h2>"
      "<p>Pick a Wrt node and adjust <i>p</i> with a slider. Alternative scores "
      "for the selected network are shown as a horizontal bar chart.</p>"
      "<h2><a href='anp://SensGlobal'>Global</a></h2>"
      "<p>Sweep <i>p</i> from 0.01 to 1.00 in steps of 0.01 and plot each "
      "alternative's synthesized score as a line series against <i>p</i>.</p>")));
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
  inflOverview_->setHtml(HtmlReport::wrapDocument(QStringLiteral(
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
      "max alt change, matching pyanp fixed influence.</p>")));
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

  // --- Perspective analysis ---
  auto* perspPage = new QWidget(stack_);
  auto* perspLay = new QVBoxLayout(perspPage);
  auto* perspIntro = new QLabel(
      QStringLiteral(
          "Perspective analysis is the limit of ANP row sensitivity as "
          "<i>p</i> → 1 for each node (evaluated near 1, never at exactly 1). "
          "Columns are perspectives (one per node); "
          "rows are alternative scores."),
      perspPage);
  perspIntro->setWordWrap(true);
  perspIntro->setTextFormat(Qt::RichText);
  perspLay->addWidget(perspIntro);
  auto* perspForm = new QHBoxLayout;
  perspForm->addWidget(new QLabel(QStringLiteral("Decimals:"), perspPage));
  perspDecimals_ = new QSpinBox(perspPage);
  perspDecimals_->setRange(2, 8);
  perspDecimals_->setValue(4);
  perspForm->addWidget(perspDecimals_);
  perspForm->addStretch();
  perspLay->addLayout(perspForm);
  perspTable_ = makeInfluenceTable(perspPage);
  perspLay->addWidget(perspTable_, 1);
  stack_->addWidget(perspPage);

  // --- Consensus / Variance ---
  consensus_ = new ConsensusAnalysisWidget(doc_, stack_);
  stack_->addWidget(consensus_);

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
  connect(perspDecimals_, QOverload<int>::of(&QSpinBox::valueChanged), this,
          &AnalysisPanel::onPerspectiveParamsChanged);

  navigateTo(Page::Synthesis);
  refresh();
}

// --- Left nav tree ----------------------------------------------------------

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
  makeNavItem(synthItem_, QStringLiteral("Limit Matrix"), Page::Synthesis,
              QStringLiteral("limit"));
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

  nav_->addTopLevelItem(
      makeNavItem(nullptr, QStringLiteral("Perspective analysis"),
                  Page::Perspective));

  nav_->addTopLevelItem(
      makeNavItem(nullptr, QStringLiteral("Consensus / Variance"),
                  Page::Consensus));

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

// --- Pane refresh (Synthesis / Sensitivity / Influence / Perspective /
// Consensus) ---

void AnalysisPanel::refresh() {
  updateSubnetNavVisibility();
  rebuildSensWrtNodes();
  rebuildInfluenceWrtNodes();
  // Supermatrix / sensitivity / influence / perspective are expensive; skip
  // while this stage is hidden (e.g. Structure connection edits) and rebuild
  // on show.
  if (!isVisible()) {
    heavyStale_ = true;
    return;
  }
  heavyStale_ = false;
  refreshSynthesisHtml();
  refreshSensitivity();
  refreshInfluence();
  refreshPerspective();
  if (consensus_) consensus_->refresh();
  if (stack_->currentIndex() == static_cast<int>(Page::Synthesis) &&
      !synthAnchor_.isEmpty()) {
    QTimer::singleShot(0, this, [this]() {
      synthBrowser_->scrollToAnchor(synthAnchor_);
    });
  }
}

void AnalysisPanel::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);
  if (heavyStale_) {
    refresh();
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

void AnalysisPanel::onPerspectiveParamsChanged() {
  refreshPerspective();
}

std::vector<std::pair<QString, double>> AnalysisPanel::altScoresAtP(
    const QString& wrt,
    double p) const {
  std::vector<std::pair<QString, double>> out;
  try {
    const auto& lim = doc_->network().limit_matrix_options();
    const auto scores = doc_->network().priority_map_at_p(
        wrt.toStdString(), p, anpcpp::P0Mode::Direct(0.5), lim);
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
  QString body;
  try {
    auto& net = doc_->network();
    const auto& lim = net.limit_matrix_options();
    const auto nodeNames = net.node_names();
    const auto clusterNames = net.cluster_names();

    body += QStringLiteral("<h1 id='unscaled'>Unscaled Supermatrix</h1>");
    body += HtmlReport::matrixTable(net.unscaled_supermatrix(), nodeNames,
                                    nodeNames);

    body += QStringLiteral("<h1 id='cluster'>Cluster matrix</h1>");
    body += HtmlReport::matrixTable(net.cluster_weight_matrix(), clusterNames,
                                    clusterNames);

    body += QStringLiteral("<h1 id='scaled'>Scaled Supermatrix</h1>");
    body +=
        HtmlReport::matrixTable(net.scaled_supermatrix(), nodeNames, nodeNames);

    body += QStringLiteral("<h1 id='limit'>Limit Matrix</h1>");
    body += limitOptionsSummaryHtml(lim);
    body += HtmlReport::matrixTable(net.limit_matrix(lim), nodeNames, nodeNames);

    body += QStringLiteral("<h1 id='global'>Global priorities</h1>");
    body += QStringLiteral(
        "<p class='muted'>Row-sum priorities of the Limit Matrix above.</p>");
    body += HtmlReport::vectorTable(net.global_priority(lim), nodeNames,
                                    QStringLiteral("Node"),
                                    QStringLiteral("Priority"));

    if (net.has_subnet()) {
      body += QStringLiteral("<h1 id='subnet'>Subnetwork synthesis results</h1>");
      std::vector<anpcpp::AnpNode*> hosts;
      for (anpcpp::AnpNode* n : net.nodes()) {
        if (n->has_subnetwork()) hosts.push_back(n);
      }
      const anpcpp::Vector g = net.global_priority(lim);
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
      body += HtmlReport::tableBegin();
      body += QStringLiteral("<tr>");
      body += HtmlReport::headerCell(QString(), true);
      for (anpcpp::AnpNode* h : hosts) {
        body += HtmlReport::headerCell(QString::fromStdString(h->name()));
      }
      body += QStringLiteral("</tr>");

      body += HtmlReport::rowBegin(0);
      body += HtmlReport::stubCell(QStringLiteral("Normalized host weights"), 0);
      for (double w : hostW) {
        body += HtmlReport::dataCell(w, 0);
      }
      body += HtmlReport::rowEnd();

      for (std::size_t ai = 0; ai < alts.size(); ++ai) {
        const auto& alt = alts[ai];
        const std::size_t rowIndex = ai + 1;
        body += HtmlReport::rowBegin(rowIndex);
        body += HtmlReport::stubCell(QString::fromStdString(alt), rowIndex);
        for (anpcpp::AnpNode* h : hosts) {
          double score = 0.0;
          try {
            anpcpp::AnpNetwork* sub = h->subnetwork();
            const auto pm = sub->priority_map(sub->limit_matrix_options());
            const auto it = pm.find(alt);
            if (it != pm.end()) score = it->second;
          } catch (...) {
          }
          body += HtmlReport::dataCell(score, rowIndex);
        }
        body += HtmlReport::rowEnd();
      }
      body += HtmlReport::tableEnd();
    }

    body += QStringLiteral("<h1 id='alts'>Alternative Scores</h1>");
    body += HtmlReport::vectorTable(net.priority(lim), net.alt_names(),
                                    QStringLiteral("Alternative"),
                                    QStringLiteral("Score"));
  } catch (const std::exception& e) {
    body += HtmlReport::errorParagraph(QString::fromUtf8(e.what()));
  }
  synthBrowser_->setHtml(HtmlReport::wrapDocument(body));
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
    resetAnalysisTableSortState(table);
  };
  auto cellText = [decimals](double v) {
    if (!std::isfinite(v)) return QStringLiteral("n/a");
    return HtmlReport::formatNumber(v, decimals);
  };

  clearTable(inflTableRaw_);
  clearTable(inflTableRank_);
  clearTable(inflTableMarginal_);
  clearTable(inflTableTotal_);

  auto& net = doc_->network();
  const auto& lim = net.limit_matrix_options();

  try {
    if (!inflWrtRaw_->currentText().isEmpty()) {
      const auto rows = net.influence_raw(
          inflWrtRaw_->currentText().toStdString(), inflDeltaUp_->value(),
          inflDeltaDown_->value(), 0.5, lim);
      inflTableRaw_->setColumnCount(3);
      inflTableRaw_->setHorizontalHeaderLabels(
          {QStringLiteral("Original"), QStringLiteral("Up"),
           QStringLiteral("Down")});
      inflTableRaw_->setRowCount(static_cast<int>(rows.size()));
      for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
        const auto& r = rows[static_cast<std::size_t>(i)];
        const QString name = QString::fromStdString(r.name);
        auto* o = new AnalysisTableItem(cellText(r.original));
        o->setData(Qt::UserRole, r.original);
        inflTableRaw_->setItem(i, 0, o);
        const QString upTxt = cellText(r.up_score) + QStringLiteral(" [") +
                              cellText(r.up_diff) + QStringLiteral("]");
        auto* u = new AnalysisTableItem(upTxt);
        u->setData(Qt::UserRole, r.up_score);
        inflTableRaw_->setItem(i, 1, u);
        const QString downTxt = cellText(r.down_score) + QStringLiteral(" [") +
                                cellText(r.down_diff) + QStringLiteral("]");
        auto* d = new AnalysisTableItem(downTxt);
        d->setData(Qt::UserRole, r.down_score);
        inflTableRaw_->setItem(i, 2, d);
        stampRowLabel(inflTableRaw_, i, name);
      }
    }
  } catch (...) {
  }

  try {
    const auto rows = net.influence_rank(1e-5, 5, lim);
    inflTableRank_->setColumnCount(2);
    inflTableRank_->setHorizontalHeaderLabels(
        {QStringLiteral("Original Score"), QStringLiteral("Rank Influence")});
    inflTableRank_->setRowCount(static_cast<int>(rows.size()));
    for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
      const auto& r = rows[static_cast<std::size_t>(i)];
      const QString name = QString::fromStdString(r.name);
      auto* o = new AnalysisTableItem(cellText(r.original));
      o->setData(Qt::UserRole, r.original);
      inflTableRank_->setItem(i, 0, o);
      auto* s = new AnalysisTableItem(cellText(r.rank_influence));
      s->setData(Qt::UserRole, r.rank_influence);
      inflTableRank_->setItem(i, 1, s);
      stampRowLabel(inflTableRank_, i, name);
    }
  } catch (...) {
  }

  try {
    const auto rows = net.influence_marginal_smart(1e-6, lim);
    inflTableMarginal_->setColumnCount(2);
    inflTableMarginal_->setHorizontalHeaderLabels(
        {QStringLiteral("Marginal"), QStringLiteral("Smart p₀")});
    inflTableMarginal_->setRowCount(static_cast<int>(rows.size()));
    for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
      const auto& r = rows[static_cast<std::size_t>(i)];
      const QString name = QString::fromStdString(r.name);
      auto* m = new AnalysisTableItem(cellText(r.marginal));
      m->setData(Qt::UserRole, r.marginal);
      inflTableMarginal_->setItem(i, 0, m);
      auto* p0 = new AnalysisTableItem(cellText(r.smart_p0));
      p0->setData(Qt::UserRole, r.smart_p0);
      inflTableMarginal_->setItem(i, 1, p0);
      stampRowLabel(inflTableMarginal_, i, name);
    }
  } catch (...) {
  }

  try {
    const auto rows = net.influence_total(inflDeltaTotal_->value(), lim);
    inflTableTotal_->setColumnCount(2);
    inflTableTotal_->setHorizontalHeaderLabels(
        {QStringLiteral("Total Influence"), QStringLiteral("Max Alt Change")});
    inflTableTotal_->setRowCount(static_cast<int>(rows.size()));
    for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
      const auto& r = rows[static_cast<std::size_t>(i)];
      const QString name = QString::fromStdString(r.name);
      auto* t = new AnalysisTableItem(cellText(r.total_influence));
      t->setData(Qt::UserRole, r.total_influence);
      inflTableTotal_->setItem(i, 0, t);
      auto* m = new AnalysisTableItem(cellText(r.max_alt_change));
      m->setData(Qt::UserRole, r.max_alt_change);
      inflTableTotal_->setItem(i, 1, m);
      stampRowLabel(inflTableTotal_, i, name);
    }
  } catch (...) {
  }
}

void AnalysisPanel::refreshPerspective() {
  perspTable_->clear();
  perspTable_->setRowCount(0);
  perspTable_->setColumnCount(0);
  resetAnalysisTableSortState(perspTable_);

  try {
    auto& net = doc_->network();
    const auto& lim = net.limit_matrix_options();
    const auto alts = net.alt_names();
    const auto nodes = net.node_names();
    if (alts.empty() || nodes.empty()) return;

    const anpcpp::Matrix P =
        net.perspective_matrix(anpcpp::P0Mode::Direct(0.5), lim);
    const int decimals = perspDecimals_->value();

    perspTable_->setRowCount(static_cast<int>(alts.size()));
    perspTable_->setColumnCount(static_cast<int>(nodes.size()));

    QStringList colHeaders;
    for (const auto& n : nodes) {
      colHeaders << QString::fromStdString(n);
    }
    perspTable_->setHorizontalHeaderLabels(colHeaders);

    for (int i = 0; i < static_cast<int>(alts.size()); ++i) {
      const QString name =
          QString::fromStdString(alts[static_cast<std::size_t>(i)]);
      for (int j = 0; j < static_cast<int>(nodes.size()); ++j) {
        const double v = P(static_cast<std::size_t>(i),
                           static_cast<std::size_t>(j));
        auto* item =
            new AnalysisTableItem(HtmlReport::formatNumber(v, decimals));
        item->setData(Qt::UserRole, v);
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        perspTable_->setItem(i, j, item);
      }
      stampRowLabel(perspTable_, i, name);
    }
  } catch (...) {
  }
}
