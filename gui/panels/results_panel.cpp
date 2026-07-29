#include "panels/results_panel.hpp"

#include "commands/network_commands.hpp"
#include "document.hpp"

#include <QAbstractItemView>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>

ResultsPanel::ResultsPanel(Document* doc, QWidget* parent)
    : QWidget(parent), doc_(doc) {
  auto* layout = new QVBoxLayout(this);

  controlsHost_ = new QWidget(this);
  auto* synthRow = new QHBoxLayout(controlsHost_);
  synthRow->addWidget(new QLabel(QStringLiteral("Synthesis:"), controlsHost_));
  synthKind_ = new QComboBox(controlsHost_);
  synthKind_->addItem(QStringLiteral("Additive"),
                      static_cast<int>(anpcpp::SynthesisKind::Additive));
  synthKind_->addItem(QStringLiteral("Multiplicative"),
                      static_cast<int>(anpcpp::SynthesisKind::Multiplicative));
  synthKind_->addItem(QStringLiteral("Custom"),
                      static_cast<int>(anpcpp::SynthesisKind::Custom));
  synthRow->addWidget(synthKind_);
  customExpr_ = new QLineEdit(controlsHost_);
  customExpr_->setPlaceholderText(QStringLiteral("e.g. Benefits / Costs"));
  synthRow->addWidget(customExpr_);
  layout->addWidget(controlsHost_);

  staleLabel_ = new QLabel(this);
  layout->addWidget(staleLabel_);

  calcButton_ = new QPushButton(QStringLiteral("Calculate"), this);
  calcButton_->setObjectName(QStringLiteral("primaryButton"));
  calcButton_->setCursor(Qt::PointingHandCursor);
  layout->addWidget(calcButton_);

  tabs_ = new QTabWidget(this);
  auto makeTable = [this]() {
    auto* t = new QTableWidget(this);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    return t;
  };
  unscaled_ = makeTable();
  clusterW_ = makeTable();
  scaled_ = makeTable();
  limit_ = makeTable();
  global_ = makeTable();
  alts_ = makeTable();
  tabs_->addTab(unscaled_, QStringLiteral("Unscaled"));
  tabs_->addTab(clusterW_, QStringLiteral("Cluster weights"));
  tabs_->addTab(scaled_, QStringLiteral("Scaled"));
  tabs_->addTab(limit_, QStringLiteral("Limit"));
  tabs_->addTab(global_, QStringLiteral("Global priority"));
  tabs_->addTab(alts_, QStringLiteral("Alternatives"));
  layout->addWidget(tabs_);

  connect(calcButton_, &QPushButton::clicked, this, &ResultsPanel::calculate);
  connect(synthKind_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &ResultsPanel::onSynthesisKindChanged);
  connect(customExpr_, &QLineEdit::editingFinished, this,
          &ResultsPanel::onCustomExprEdited);
  connect(doc_, &Document::modelChanged, this,
          &ResultsPanel::refreshSynthesisControls);
  connect(doc_, &Document::viewNetworkChanged, this,
          &ResultsPanel::refreshSynthesisControls);
  connect(doc_, &Document::resultsFreshnessChanged, this,
          &ResultsPanel::refreshStaleBadge);

  refreshSynthesisControls();
  refreshStaleBadge();
}

void ResultsPanel::refreshSynthesisControls() {
  const auto& opt = doc_->network().synthesis_options();
  const int idx = synthKind_->findData(static_cast<int>(opt.kind));
  if (idx >= 0) synthKind_->setCurrentIndex(idx);
  customExpr_->setText(QString::fromStdString(opt.custom_expr));
  customExpr_->setEnabled(opt.kind == anpcpp::SynthesisKind::Custom);
}

void ResultsPanel::refreshStaleBadge() {
  if (!doc_->hasResults()) {
    staleLabel_->setText(QStringLiteral("Calculate to populate matrices."));
    staleLabel_->setStyleSheet(QString());
  } else if (doc_->resultsStale()) {
    staleLabel_->setText(QStringLiteral("Results outdated."));
    staleLabel_->setStyleSheet(QStringLiteral("color: #a60; font-weight: bold;"));
  } else {
    staleLabel_->setText(QStringLiteral("Results current."));
    staleLabel_->setStyleSheet(QString());
  }
}

void ResultsPanel::onSynthesisKindChanged(int) {
  anpcpp::SynthesisOptions neu = doc_->network().synthesis_options();
  neu.kind = static_cast<anpcpp::SynthesisKind>(
      synthKind_->currentData().toInt());
  neu.custom_expr = customExpr_->text().toStdString();
  const auto old = doc_->network().synthesis_options();
  if (neu.kind == old.kind && neu.custom_expr == old.custom_expr) return;
  doc_->undoStack()->push(new SetSynthesisOptionsCmd(doc_, neu, old));
  customExpr_->setEnabled(neu.kind == anpcpp::SynthesisKind::Custom);
}

void ResultsPanel::onCustomExprEdited() {
  anpcpp::SynthesisOptions neu = doc_->network().synthesis_options();
  neu.custom_expr = customExpr_->text().toStdString();
  const auto old = doc_->network().synthesis_options();
  if (neu.custom_expr == old.custom_expr) return;
  doc_->undoStack()->push(new SetSynthesisOptionsCmd(doc_, neu, old));
}

void ResultsPanel::fillMatrix(QTableWidget* table,
                              const anpcpp::Matrix& m,
                              const std::vector<std::string>& rowLabels,
                              const std::vector<std::string>& colLabels) {
  table->clear();
  table->setRowCount(static_cast<int>(m.rows()));
  table->setColumnCount(static_cast<int>(m.cols()));
  QStringList rh, ch;
  for (const auto& s : rowLabels) rh << QString::fromStdString(s);
  for (const auto& s : colLabels) ch << QString::fromStdString(s);
  table->setVerticalHeaderLabels(rh);
  table->setHorizontalHeaderLabels(ch);
  for (std::size_t i = 0; i < m.rows(); ++i) {
    for (std::size_t j = 0; j < m.cols(); ++j) {
      table->setItem(static_cast<int>(i), static_cast<int>(j),
                     new QTableWidgetItem(QString::number(m(i, j), 'f', 4)));
    }
  }
}

void ResultsPanel::fillVector(QTableWidget* table,
                              const anpcpp::Vector& v,
                              const std::vector<std::string>& labels) {
  table->clear();
  table->setRowCount(static_cast<int>(v.size()));
  table->setColumnCount(1);
  table->setHorizontalHeaderLabels({QStringLiteral("Priority")});
  QStringList rh;
  for (const auto& s : labels) rh << QString::fromStdString(s);
  table->setVerticalHeaderLabels(rh);
  for (std::size_t i = 0; i < v.size(); ++i) {
    table->setItem(static_cast<int>(i), 0,
                   new QTableWidgetItem(QString::number(v[i], 'f', 6)));
  }
}

void ResultsPanel::calculate() {
  try {
    auto& net = doc_->network();
    const auto nodes = net.node_names();
    const auto clusters = net.cluster_names();
    fillMatrix(unscaled_, net.unscaled_supermatrix(), nodes, nodes);
    fillMatrix(clusterW_, net.cluster_weight_matrix(), clusters, clusters);
    fillMatrix(scaled_, net.scaled_supermatrix(), nodes, nodes);
    fillMatrix(limit_, net.limit_matrix(), nodes, nodes);
    fillVector(global_, net.global_priority(), nodes);
    const auto altNames = net.alt_names();
    const auto altPri = net.priority();
    fillVector(alts_, altPri, altNames);
    tabs_->setCurrentWidget(alts_);

    std::vector<std::pair<QString, double>> ranked;
    ranked.reserve(altNames.size());
    for (std::size_t i = 0; i < altNames.size(); ++i) {
      ranked.emplace_back(QString::fromStdString(altNames[i]), altPri[i]);
    }
    std::sort(ranked.begin(), ranked.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    emit alternativesUpdated(ranked);
    doc_->markResultsCurrent();
  } catch (const std::exception& e) {
    QMessageBox::warning(this, QStringLiteral("Calculation error"),
                         QString::fromUtf8(e.what()));
  }
}
