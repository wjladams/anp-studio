#include "panels/synthesis_summary_panel.hpp"

#include "document.hpp"

#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>

SynthesisSummaryPanel::SynthesisSummaryPanel(Document* doc, QWidget* parent)
    : QWidget(parent), doc_(doc) {
  auto* layout = new QVBoxLayout(this);
  layout->addWidget(new QLabel(QStringLiteral("Summary"), this));
  staleLabel_ = new QLabel(this);
  layout->addWidget(staleLabel_);
  list_ = new QListWidget(this);
  layout->addWidget(list_);
  connect(doc_, &Document::resultsFreshnessChanged, this,
          &SynthesisSummaryPanel::refreshStale);
  refreshStale();
}

void SynthesisSummaryPanel::setAlternatives(
    const std::vector<std::pair<QString, double>>& ranked) {
  list_->clear();
  for (const auto& [name, v] : ranked) {
    list_->addItem(QStringLiteral("%1 — %2").arg(name).arg(v, 0, 'f', 4));
  }
}

void SynthesisSummaryPanel::refreshStale() {
  if (!doc_->hasResults()) {
    staleLabel_->setText(QStringLiteral("No results yet — Calculate to populate."));
  } else if (doc_->resultsStale()) {
    staleLabel_->setText(QStringLiteral("Results outdated — recalculate."));
    staleLabel_->setStyleSheet(QStringLiteral("color: #a60;"));
  } else {
    staleLabel_->setText(QStringLiteral("Results current."));
    staleLabel_->setStyleSheet(QString());
  }
}
