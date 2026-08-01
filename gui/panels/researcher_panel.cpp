/**
 * @file researcher_panel.cpp
 * @brief Researcher stage UI: starters, cells, command line, bindings.
 */

#include "panels/researcher_panel.hpp"

#include "document.hpp"
#include "html_report.hpp"

#include <QAbstractTextDocumentLayout>
#include <QDateTime>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QtMath>
#include <QScrollArea>
#include <QScrollBar>
#include <QShowEvent>
#include <QSplitter>
#include <QTextBrowser>
#include <QTimer>
#include <QVBoxLayout>

namespace {

QString extractHtmlBody(const QString& html) {
  const int open = html.indexOf(QLatin1String("<body"), 0, Qt::CaseInsensitive);
  if (open < 0) return html;
  const int gt = html.indexOf(QLatin1Char('>'), open);
  if (gt < 0) return html;
  const int close =
      html.lastIndexOf(QLatin1String("</body>"), -1, Qt::CaseInsensitive);
  if (close < 0 || close <= gt) return html;
  return html.mid(gt + 1, close - gt - 1).trimmed();
}

/**
 * @brief QTextBrowser that grows to fit its document (no internal scrollbars).
 *
 * Scrolling is left to the surrounding notebook QScrollArea.
 */
class ExpandingHtmlView : public QTextBrowser {
public:
  explicit ExpandingHtmlView(QWidget* parent = nullptr) : QTextBrowser(parent) {
    setOpenExternalLinks(false);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameShape(QFrame::NoFrame);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    document()->setDocumentMargin(0);
  }

  void setReportHtml(const QString& html) {
    setHtml(html);
    syncHeight();
  }

protected:
  void resizeEvent(QResizeEvent* event) override {
    QTextBrowser::resizeEvent(event);
    syncHeight();
  }

  void showEvent(QShowEvent* event) override {
    QTextBrowser::showEvent(event);
    syncHeight();
  }

private:
  void syncHeight() {
    const int width = qMax(1, viewport()->width());
    document()->setTextWidth(width);
    const QSizeF docSize = document()->documentLayout()->documentSize();
    const int h = qMax(1, qCeil(docSize.height()) + 2);
    if (height() != h) {
      setFixedHeight(h);
    }
  }
};

QFrame* makeCell(QWidget* parent,
                 const QString& command,
                 const ResearcherEvalResult& result) {
  auto* cell = new QFrame(parent);
  cell->setObjectName(QStringLiteral("researcherCell"));
  cell->setFrameShape(QFrame::StyledPanel);
  auto* lay = new QVBoxLayout(cell);
  lay->setContentsMargins(8, 8, 8, 8);
  lay->setSpacing(6);

  auto* cmd = new QLabel(QStringLiteral("› ") + command, cell);
  cmd->setObjectName(QStringLiteral("researcherCellCmd"));
  cmd->setTextInteractionFlags(Qt::TextSelectableByMouse);
  cmd->setWordWrap(true);
  lay->addWidget(cmd);

  auto* out = new ExpandingHtmlView(cell);
  out->setReportHtml(result.html);
  lay->addWidget(out);
  return cell;
}

}  // namespace

ResearcherPanel::ResearcherPanel(Document* doc, QWidget* parent)
    : QWidget(parent),
      doc_(doc),
      session_(std::make_unique<ResearcherSession>(doc)) {
  auto* root = new QHBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);

  auto* split = new QSplitter(Qt::Horizontal, this);
  root->addWidget(split);

  // Left: starters
  auto* left = new QWidget(split);
  auto* leftLay = new QVBoxLayout(left);
  leftLay->setContentsMargins(8, 8, 8, 8);
  leftLay->setSpacing(6);
  auto* startersCaption = new QLabel(QStringLiteral("Starters"), left);
  startersCaption->setObjectName(QStringLiteral("researcherCaption"));
  leftLay->addWidget(startersCaption);
  starters_ = new QListWidget(left);
  starters_->setObjectName(QStringLiteral("researcherStarters"));
  for (const QString& cmd : ResearcherSession::starterCommands()) {
    starters_->addItem(cmd);
  }
  leftLay->addWidget(starters_, 1);
  auto* exportBtn = new QPushButton(QStringLiteral("Export HTML…"), left);
  leftLay->addWidget(exportBtn);
  auto* clearBtn = new QPushButton(QStringLiteral("Clear notebook"), left);
  leftLay->addWidget(clearBtn);
  split->addWidget(left);

  // Center: notebook + input
  auto* center = new QWidget(split);
  auto* centerLay = new QVBoxLayout(center);
  centerLay->setContentsMargins(8, 8, 8, 8);
  centerLay->setSpacing(8);

  auto* intro = new QLabel(
      QStringLiteral(
          "Researcher — run ANP inspection commands on thisModel / "
          "parentModel (and loaded JSON). Enter runs; Up/Down for history."),
      center);
  intro->setWordWrap(true);
  intro->setObjectName(QStringLiteral("researcherIntro"));
  centerLay->addWidget(intro);

  notebookScroll_ = new QScrollArea(center);
  notebookScroll_->setWidgetResizable(true);
  notebookScroll_->setFrameShape(QFrame::NoFrame);
  notebookHost_ = new QWidget(notebookScroll_);
  notebookLay_ = new QVBoxLayout(notebookHost_);
  notebookLay_->setContentsMargins(0, 0, 0, 0);
  notebookLay_->setSpacing(10);
  notebookLay_->addStretch(1);
  notebookScroll_->setWidget(notebookHost_);
  centerLay->addWidget(notebookScroll_, 1);

  auto* inputRow = new QWidget(center);
  auto* inputLay = new QHBoxLayout(inputRow);
  inputLay->setContentsMargins(0, 0, 0, 0);
  inputLay->setSpacing(6);
  promptLabel_ = new QLabel(QStringLiteral("thisModel ›"), inputRow);
  promptLabel_->setObjectName(QStringLiteral("researcherPrompt"));
  input_ = new QLineEdit(inputRow);
  input_->setPlaceholderText(QStringLiteral(
      "e.g. limit   or   load samples/01_hamburger_marketshare.json as h"));
  input_->installEventFilter(this);
  auto* runBtn = new QPushButton(QStringLiteral("Run"), inputRow);
  inputLay->addWidget(promptLabel_);
  inputLay->addWidget(input_, 1);
  inputLay->addWidget(runBtn);
  centerLay->addWidget(inputRow);
  split->addWidget(center);

  // Right: bindings
  auto* right = new QWidget(split);
  auto* rightLay = new QVBoxLayout(right);
  rightLay->setContentsMargins(8, 8, 8, 8);
  rightLay->setSpacing(6);
  auto* bindCaption = new QLabel(QStringLiteral("Bindings"), right);
  bindCaption->setObjectName(QStringLiteral("researcherCaption"));
  rightLay->addWidget(bindCaption);
  bindings_ = new QTextBrowser(right);
  bindings_->setObjectName(QStringLiteral("researcherBindings"));
  rightLay->addWidget(bindings_, 1);
  split->addWidget(right);

  split->setStretchFactor(0, 0);
  split->setStretchFactor(1, 1);
  split->setStretchFactor(2, 0);
  split->setSizes({180, 720, 220});

  connect(starters_, &QListWidget::itemClicked, this,
          &ResearcherPanel::onStarterActivated);
  connect(runBtn, &QPushButton::clicked, this, &ResearcherPanel::runCurrentInput);
  connect(input_, &QLineEdit::returnPressed, this,
          &ResearcherPanel::runCurrentInput);
  connect(clearBtn, &QPushButton::clicked, this, &ResearcherPanel::clearNotebook);
  connect(exportBtn, &QPushButton::clicked, this, &ResearcherPanel::exportHtml);

  if (doc_ != nullptr) {
    connect(doc_, &Document::modelChanged, this,
            &ResearcherPanel::refreshBindings);
    connect(doc_, &Document::viewNetworkChanged, this,
            &ResearcherPanel::refreshBindings);
    connect(doc_, &Document::pathChanged, this,
            &ResearcherPanel::refreshBindings);
  }

  refreshBindings();
  // Seed with help so the stage is not empty on first open.
  appendCell(QStringLiteral("help"), session_->eval(QStringLiteral("help")));
}

void ResearcherPanel::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);
  refreshBindings();
  if (input_ != nullptr) input_->setFocus(Qt::OtherFocusReason);
}

bool ResearcherPanel::eventFilter(QObject* watched, QEvent* event) {
  if (watched == input_ && event->type() == QEvent::KeyPress) {
    auto* ke = static_cast<QKeyEvent*>(event);
    if (ke->key() == Qt::Key_Up) {
      if (history_.isEmpty()) return true;
      if (historyIndex_ < 0) {
        historyDraft_ = input_->text();
        historyIndex_ = history_.size() - 1;
      } else if (historyIndex_ > 0) {
        --historyIndex_;
      }
      setHistoryFromIndex();
      return true;
    }
    if (ke->key() == Qt::Key_Down) {
      if (historyIndex_ < 0) return true;
      if (historyIndex_ + 1 >= history_.size()) {
        historyIndex_ = -1;
        input_->setText(historyDraft_);
      } else {
        ++historyIndex_;
        setHistoryFromIndex();
      }
      return true;
    }
  }
  return QWidget::eventFilter(watched, event);
}

void ResearcherPanel::setHistoryFromIndex() {
  if (historyIndex_ >= 0 && historyIndex_ < history_.size()) {
    input_->setText(history_.at(historyIndex_));
    input_->setCursorPosition(input_->text().size());
  }
}

void ResearcherPanel::refreshBindings() {
  if (session_ == nullptr || bindings_ == nullptr) return;
  QString body = QStringLiteral("<ul>");
  for (const QString& line : session_->bindingLines()) {
    body += QStringLiteral("<li><code>") + HtmlReport::escape(line) +
            QStringLiteral("</code></li>");
  }
  body += QStringLiteral(
      "</ul><p class='muted'>Reserved: <code>thisModel</code>, "
      "<code>parentModel</code></p>");
  bindings_->setHtml(HtmlReport::wrapDocument(body));
  if (promptLabel_ != nullptr) {
    promptLabel_->setText(session_->activeName() + QStringLiteral(" ›"));
  }
}

void ResearcherPanel::onStarterActivated(QListWidgetItem* item) {
  if (item == nullptr || input_ == nullptr) return;
  input_->setText(item->text());
  runCurrentInput();
}

void ResearcherPanel::runCurrentInput() {
  if (session_ == nullptr || input_ == nullptr) return;
  const QString line = input_->text().trimmed();
  if (line.isEmpty()) return;

  const ResearcherEvalResult result = session_->eval(line);
  appendCell(line, result);
  history_.removeAll(line);
  history_.append(line);
  historyIndex_ = -1;
  historyDraft_.clear();
  input_->clear();
  refreshBindings();
}

void ResearcherPanel::clearNotebook() {
  cells_.clear();
  if (notebookLay_ == nullptr) return;
  while (notebookLay_->count() > 0) {
    QLayoutItem* item = notebookLay_->takeAt(0);
    if (item == nullptr) continue;
    if (QWidget* w = item->widget()) {
      w->deleteLater();
    }
    delete item;
  }
  notebookLay_->addStretch(1);
}

QString ResearcherPanel::defaultExportFileName() const {
  QString base = QStringLiteral("researcher-session");
  if (doc_ != nullptr && !doc_->path().isEmpty()) {
    base = QFileInfo(doc_->path()).completeBaseName() +
           QStringLiteral("-researcher");
  }
  return base + QStringLiteral(".html");
}

QString ResearcherPanel::buildExportHtml() const {
  QString body;
  body += QStringLiteral("<h1>ANP Studio — Researcher session</h1>");
  body += QStringLiteral("<p class='muted'>Exported ") +
          HtmlReport::escape(
              QDateTime::currentDateTime().toString(Qt::ISODate)) +
          QStringLiteral("</p>");

  if (doc_ != nullptr) {
    body += QStringLiteral("<ul>");
    body += QStringLiteral("<li>Network path: <code>") +
            HtmlReport::escape(doc_->currentNetworkPath()) +
            QStringLiteral("</code></li>");
    if (!doc_->path().isEmpty()) {
      body += QStringLiteral("<li>Model file: <code>") +
              HtmlReport::escape(doc_->path()) + QStringLiteral("</code></li>");
    }
    body += QStringLiteral("</ul>");
  }

  if (session_ != nullptr) {
    body += QStringLiteral("<h2>Bindings at export</h2><ul>");
    for (const QString& line : session_->bindingLines()) {
      body += QStringLiteral("<li><code>") + HtmlReport::escape(line) +
              QStringLiteral("</code></li>");
    }
    body += QStringLiteral("</ul>");
  }

  body += QStringLiteral("<h2>Notebook</h2>");
  if (cells_.isEmpty()) {
    body += QStringLiteral("<p class='muted'><i>(empty session)</i></p>");
  } else {
    for (int i = 0; i < cells_.size(); ++i) {
      const CellRecord& cell = cells_.at(i);
      body += QStringLiteral("<h3>") + QString::number(i + 1) +
              QStringLiteral(". <code>") + HtmlReport::escape(cell.command) +
              QStringLiteral("</code></h3>");
      body += extractHtmlBody(cell.html);
    }
  }

  return HtmlReport::wrapDocument(body);
}

void ResearcherPanel::exportHtml() {
  const QString path = QFileDialog::getSaveFileName(
      this, QStringLiteral("Export Researcher session"),
      defaultExportFileName(),
      QStringLiteral("HTML files (*.html *.htm);;All files (*)"));
  if (path.isEmpty()) return;

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
    QMessageBox::warning(
        this, QStringLiteral("Export failed"),
        QStringLiteral("Could not write:\n%1\n\n%2")
            .arg(path, file.errorString()));
    return;
  }

  const QByteArray bytes = buildExportHtml().toUtf8();
  if (file.write(bytes) != bytes.size()) {
    QMessageBox::warning(
        this, QStringLiteral("Export failed"),
        QStringLiteral("Incomplete write to:\n%1").arg(path));
    return;
  }
  file.close();
  QMessageBox::information(
      this, QStringLiteral("Export complete"),
      QStringLiteral("Saved Researcher session to:\n%1").arg(path));
}

void ResearcherPanel::appendCell(const QString& command,
                                 const ResearcherEvalResult& result) {
  if (notebookLay_ == nullptr) return;
  cells_.push_back(CellRecord{command, result.html, result.ok});

  // Insert above the trailing stretch.
  const int stretchIndex = notebookLay_->count() - 1;
  auto* cell = makeCell(notebookHost_, command, result);
  notebookLay_->insertWidget(qMax(0, stretchIndex), cell);

  // Scroll to bottom after layout.
  QTimer::singleShot(0, this, [this]() {
    if (notebookScroll_ == nullptr) return;
    if (auto* bar = notebookScroll_->verticalScrollBar()) {
      bar->setValue(bar->maximum());
    }
  });
}
