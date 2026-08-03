/**
 * @file researcher_panel.cpp
 * @brief Researcher stage: browser-style multi-notebook UI.
 */

#include "panels/researcher_panel.hpp"

#include "html_report.hpp"

#include <QAbstractTextDocumentLayout>
#include <QDateTime>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QShowEvent>
#include <QSplitter>
#include <QTabBar>
#include <QTextBrowser>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtMath>

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

QFrame* makeCell(QWidget* parent, const QString& command, const QString& html) {
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
  out->setReportHtml(html);
  lay->addWidget(out);
  return cell;
}

}  // namespace

QString ResearcherPanel::middleTruncate(const QString& name) {
  constexpr int kMax = 14;
  if (name.size() <= kMax) return name;
  constexpr int kLeft = 9;
  constexpr int kRight = 4;
  return name.left(kLeft) + QChar(0x2026) + name.right(kRight);
}

ResearcherPanel::ResearcherPanel(Document* doc, QWidget* parent)
    : QWidget(parent),
      doc_(doc),
      session_(std::make_unique<ResearcherSession>(doc)) {
  auto* root = new QHBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);

  auto* split = new QSplitter(Qt::Horizontal, this);
  root->addWidget(split);

  // Center: browser tabs + notebook + input
  auto* center = new QWidget(split);
  auto* centerLay = new QVBoxLayout(center);
  centerLay->setContentsMargins(0, 0, 0, 0);
  centerLay->setSpacing(0);

  auto* tabRow = new QWidget(center);
  tabRow->setObjectName(QStringLiteral("researcherTabRow"));
  auto* tabLay = new QHBoxLayout(tabRow);
  tabLay->setContentsMargins(8, 6, 8, 0);
  tabLay->setSpacing(4);

  menuBtn_ = new QToolButton(tabRow);
  menuBtn_->setObjectName(QStringLiteral("researcherMenuBtn"));
  menuBtn_->setText(QStringLiteral("☰"));
  menuBtn_->setToolTip(QStringLiteral("Notebook actions"));
  menuBtn_->setPopupMode(QToolButton::InstantPopup);
  menuBtn_->setAutoRaise(true);
  auto* menu = new QMenu(menuBtn_);
  menu->addAction(QStringLiteral("New notebook"), this,
                  &ResearcherPanel::newNotebook);
  menu->addAction(QStringLiteral("Duplicate"), this,
                  &ResearcherPanel::duplicateNotebook);
  menu->addAction(QStringLiteral("Delete"), this,
                  &ResearcherPanel::deleteNotebook);
  menu->addAction(QStringLiteral("Clear notebook"), this,
                  &ResearcherPanel::clearNotebook);
  menu->addSeparator();
  menu->addAction(QStringLiteral("Export HTML…"), this,
                  &ResearcherPanel::exportHtml);
  menuBtn_->setMenu(menu);
  tabLay->addWidget(menuBtn_);

  tabBar_ = new QTabBar(tabRow);
  tabBar_->setObjectName(QStringLiteral("researcherTabBar"));
  tabBar_->setDocumentMode(true);
  tabBar_->setTabsClosable(true);
  tabBar_->setMovable(false);
  tabBar_->setExpanding(false);
  tabBar_->setUsesScrollButtons(true);
  tabBar_->setElideMode(Qt::ElideNone);
  tabBar_->setDrawBase(false);
  tabLay->addWidget(tabBar_, 0);

  addTabBtn_ = new QToolButton(tabRow);
  addTabBtn_->setObjectName(QStringLiteral("researcherAddTabBtn"));
  addTabBtn_->setText(QStringLiteral("+"));
  addTabBtn_->setToolTip(QStringLiteral("New notebook"));
  addTabBtn_->setAutoRaise(true);
  tabLay->addWidget(addTabBtn_, 0);
  tabLay->addStretch(1);
  centerLay->addWidget(tabRow);

  auto* body = new QWidget(center);
  auto* bodyLay = new QVBoxLayout(body);
  bodyLay->setContentsMargins(8, 8, 8, 8);
  bodyLay->setSpacing(8);

  auto* intro = new QLabel(
      QStringLiteral(
          "Researcher — run ANP inspection commands on thisModel / "
          "parentModel (and loaded JSON). Enter runs; Tab completes; "
          "Up/Down for history."),
      body);
  intro->setWordWrap(true);
  intro->setObjectName(QStringLiteral("researcherIntro"));
  bodyLay->addWidget(intro);

  notebookScroll_ = new QScrollArea(body);
  notebookScroll_->setWidgetResizable(true);
  notebookScroll_->setFrameShape(QFrame::NoFrame);
  notebookHost_ = new QWidget(notebookScroll_);
  notebookLay_ = new QVBoxLayout(notebookHost_);
  notebookLay_->setContentsMargins(0, 0, 0, 0);
  notebookLay_->setSpacing(10);
  notebookLay_->addStretch(1);
  notebookScroll_->setWidget(notebookHost_);
  bodyLay->addWidget(notebookScroll_, 1);

  auto* inputRow = new QWidget(body);
  auto* inputLay = new QHBoxLayout(inputRow);
  inputLay->setContentsMargins(0, 0, 0, 0);
  inputLay->setSpacing(6);
  promptLabel_ = new QLabel(QStringLiteral("thisModel ›"), inputRow);
  promptLabel_->setObjectName(QStringLiteral("researcherPrompt"));
  input_ = new QLineEdit(inputRow);
  input_->setPlaceholderText(QStringLiteral(
      "e.g. limit   or   load samples/01_hamburger_marketshare.anpstudio as h"));
  input_->installEventFilter(this);
  auto* runBtn = new QPushButton(QStringLiteral("Run"), inputRow);
  inputLay->addWidget(promptLabel_);
  inputLay->addWidget(input_, 1);
  inputLay->addWidget(runBtn);
  bodyLay->addWidget(inputRow);
  centerLay->addWidget(body, 1);
  split->addWidget(center);

  // Right: one scrollable rail — Bindings then Starters (mock Option 1)
  auto* right = new QWidget(split);
  right->setObjectName(QStringLiteral("researcherRightRail"));
  auto* rightLay = new QVBoxLayout(right);
  rightLay->setContentsMargins(8, 8, 8, 8);
  rightLay->setSpacing(6);

  auto* sideScroll = new QScrollArea(right);
  sideScroll->setObjectName(QStringLiteral("researcherSideScroll"));
  sideScroll->setWidgetResizable(true);
  sideScroll->setFrameShape(QFrame::NoFrame);
  sideScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  auto* sideHost = new QWidget(sideScroll);
  sideHost->setObjectName(QStringLiteral("researcherSideHost"));
  auto* sideLay = new QVBoxLayout(sideHost);
  sideLay->setContentsMargins(0, 0, 0, 0);
  sideLay->setSpacing(8);

  auto* bindCaption = new QLabel(QStringLiteral("Bindings"), sideHost);
  bindCaption->setObjectName(QStringLiteral("researcherCaption"));
  sideLay->addWidget(bindCaption);

  auto* bindingsView = new ExpandingHtmlView(sideHost);
  bindingsView->setObjectName(QStringLiteral("researcherBindings"));
  bindingsView->document()->setDocumentMargin(8);
  bindings_ = bindingsView;
  sideLay->addWidget(bindings_);

  auto* startersCaption = new QLabel(QStringLiteral("Starters"), sideHost);
  startersCaption->setObjectName(QStringLiteral("researcherCaption"));
  sideLay->addWidget(startersCaption);

  starters_ = new QListWidget(sideHost);
  starters_->setObjectName(QStringLiteral("researcherStarters"));
  starters_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  starters_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  starters_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  starters_->setFocusPolicy(Qt::NoFocus);
  starters_->setFrameShape(QFrame::NoFrame);
  starters_->setUniformItemSizes(true);
  for (const QString& cmd : ResearcherSession::starterCommands()) {
    starters_->addItem(cmd);
  }
  // Size to all rows so the outer side scroll owns scrolling.
  {
    int h = 2;  // outer border
    for (int i = 0; i < starters_->count(); ++i) {
      h += qMax(starters_->sizeHintForRow(i), 28);
    }
    starters_->setFixedHeight(qMax(h, 1));
  }
  sideLay->addWidget(starters_);
  sideLay->addStretch(1);

  sideScroll->setWidget(sideHost);
  rightLay->addWidget(sideScroll, 1);
  split->addWidget(right);

  split->setStretchFactor(0, 1);
  split->setStretchFactor(1, 0);
  split->setSizes({780, 240});

  connect(starters_, &QListWidget::itemClicked, this,
          &ResearcherPanel::onStarterActivated);
  connect(runBtn, &QPushButton::clicked, this, &ResearcherPanel::runCurrentInput);
  connect(input_, &QLineEdit::returnPressed, this,
          &ResearcherPanel::runCurrentInput);
  connect(addTabBtn_, &QToolButton::clicked, this,
          &ResearcherPanel::addNotebookTab);
  connect(tabBar_, &QTabBar::currentChanged, this,
          &ResearcherPanel::onTabChanged);
  connect(tabBar_, &QTabBar::tabCloseRequested, this,
          &ResearcherPanel::onTabCloseRequested);
  tabBar_->installEventFilter(this);

  if (doc_ != nullptr) {
    connect(doc_, &Document::modelChanged, this,
            &ResearcherPanel::refreshBindings);
    connect(doc_, &Document::viewNetworkChanged, this,
            &ResearcherPanel::refreshBindings);
    connect(doc_, &Document::pathChanged, this,
            &ResearcherPanel::refreshBindings);
    connect(doc_, &Document::researcherSessionChanged, this,
            &ResearcherPanel::reloadFromDocument);
  }

  refreshBindings();
  reloadFromDocument();
}

void ResearcherPanel::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);
  refreshBindings();
  if (input_ != nullptr) input_->setFocus(Qt::OtherFocusReason);
}

bool ResearcherPanel::eventFilter(QObject* watched, QEvent* event) {
  if (watched == tabBar_ && event->type() == QEvent::MouseButtonDblClick) {
    auto* me = static_cast<QMouseEvent*>(event);
    const int idx = tabBar_->tabAt(me->pos());
    if (idx >= 0) {
      onTabDoubleClicked(idx);
      return true;
    }
  }
  if (watched == input_ && event->type() == QEvent::KeyPress) {
    auto* ke = static_cast<QKeyEvent*>(event);
    if (ke->key() == Qt::Key_Tab) {
      if (session_ == nullptr) return true;
      const QString line = input_->text();
      const QStringList matches = session_->completions(line);
      if (matches.isEmpty()) return true;

      const bool trailingSpace = line.endsWith(QLatin1Char(' '));
      int tokenStart = line.size();
      if (!trailingSpace) {
        // Find start of the last token (respecting simple quotes).
        bool inQuote = false;
        QChar quote;
        tokenStart = 0;
        for (int i = 0; i < line.size(); ++i) {
          const QChar c = line.at(i);
          if (inQuote) {
            if (c == quote) inQuote = false;
            continue;
          }
          if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
            inQuote = true;
            quote = c;
            continue;
          }
          if (c.isSpace()) tokenStart = i + 1;
        }
      }

      QString chosen = matches.first();
      if (matches.size() > 1) {
        const QString partial =
            trailingSpace ? QString() : line.mid(tokenStart);
        QString barePartial = partial;
        if (barePartial.startsWith(QLatin1Char('"')) ||
            barePartial.startsWith(QLatin1Char('\''))) {
          barePartial = barePartial.mid(1);
        }
        if (barePartial.endsWith(QLatin1Char('"')) ||
            barePartial.endsWith(QLatin1Char('\''))) {
          barePartial.chop(1);
        }

        // If the typed token is only a substring (not a shared prefix of the
        // matches), jump straight to cycling full candidates — e.g. "bene"
        // → "Root / Benefits".
        bool partialIsPrefixOfAll = !barePartial.isEmpty();
        for (const QString& m : matches) {
          QString bare = m;
          if (bare.startsWith(QLatin1Char('"')) &&
              bare.endsWith(QLatin1Char('"'))) {
            bare = bare.mid(1, bare.size() - 2);
          }
          if (!bare.startsWith(barePartial, Qt::CaseInsensitive) &&
              !m.startsWith(partial, Qt::CaseInsensitive)) {
            partialIsPrefixOfAll = false;
            break;
          }
        }

        if (partialIsPrefixOfAll) {
          QString common = matches.first();
          for (int i = 1; i < matches.size(); ++i) {
            const QString& m = matches.at(i);
            int n = 0;
            while (n < common.size() && n < m.size() &&
                   common.at(n).toLower() == m.at(n).toLower()) {
              ++n;
            }
            common = common.left(n);
          }
          if (common.size() > partial.size()) {
            chosen = common;
          } else {
            int idx = matches.indexOf(partial);
            if (idx < 0) {
              for (int i = 0; i < matches.size(); ++i) {
                QString m = matches.at(i);
                if (m.startsWith(QLatin1Char('"')) &&
                    m.endsWith(QLatin1Char('"'))) {
                  m = m.mid(1, m.size() - 2);
                }
                if (QString::compare(m, barePartial, Qt::CaseInsensitive) ==
                    0) {
                  idx = i;
                  break;
                }
              }
            }
            chosen = matches.at((idx + 1) % matches.size());
          }
        } else {
          // Substring completion: pick the unique match, or cycle.
          int idx = -1;
          for (int i = 0; i < matches.size(); ++i) {
            if (matches.at(i) == partial) {
              idx = i;
              break;
            }
          }
          chosen = matches.at((idx + 1) % matches.size());
        }
      }

      // Completions already quote network paths; don't double-quote.
      input_->setText(line.left(tokenStart) + chosen);
      input_->setCursorPosition(input_->text().size());
      return true;
    }
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
  if (auto* expanding = static_cast<ExpandingHtmlView*>(bindings_)) {
    expanding->setReportHtml(HtmlReport::wrapDocument(body));
  } else {
    bindings_->setHtml(HtmlReport::wrapDocument(body));
  }
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
  ensureNotebooks();
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

void ResearcherPanel::ensureNotebooks() {
  if (!notebooks_.isEmpty()) return;
  ResearcherNotebook nb;
  nb.name = QStringLiteral("Notebook 1");
  notebooks_.push_back(nb);
  activeIndex_ = 0;
}

void ResearcherPanel::clearNotebookUi(bool keepEphemeralHelp) {
  cells_.clear();
  history_.clear();
  historyIndex_ = -1;
  historyDraft_.clear();
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

  if (keepEphemeralHelp && session_ != nullptr) {
    const ResearcherEvalResult help = session_->eval(QStringLiteral("help"));
    const int stretchIndex = notebookLay_->count() - 1;
    notebookLay_->insertWidget(
        qMax(0, stretchIndex),
        makeCell(notebookHost_, QStringLiteral("help"), help.html));
  }
}

void ResearcherPanel::persistActiveNotebookToModel(bool markDirty) {
  ensureNotebooks();
  if (activeIndex_ < 0 || activeIndex_ >= notebooks_.size()) return;
  notebooks_[activeIndex_].cells = cells_;
  syncSessionToDocument(markDirty);
}

void ResearcherPanel::syncSessionToDocument(bool markDirty) {
  if (doc_ == nullptr) return;
  doc_->setResearcherSession(notebooks_, activeIndex_);
  if (markDirty) doc_->setDirty(true);
}

void ResearcherPanel::updateTabLabels() {
  if (tabBar_ == nullptr) return;
  for (int i = 0; i < tabBar_->count() && i < notebooks_.size(); ++i) {
    const QString& full = notebooks_[i].name;
    tabBar_->setTabText(i, middleTruncate(full));
    tabBar_->setTabToolTip(i, full);
  }
}

void ResearcherPanel::rebuildTabBar() {
  if (tabBar_ == nullptr) return;
  updatingTabs_ = true;
  while (tabBar_->count() > 0) {
    tabBar_->removeTab(0);
  }
  for (const ResearcherNotebook& nb : notebooks_) {
    tabBar_->addTab(middleTruncate(nb.name));
  }
  updateTabLabels();
  if (!notebooks_.isEmpty()) {
    tabBar_->setCurrentIndex(
        qBound(0, activeIndex_, notebooks_.size() - 1));
  }
  updatingTabs_ = false;
}

void ResearcherPanel::loadActiveNotebookIntoUi() {
  history_.clear();
  historyIndex_ = -1;
  historyDraft_.clear();
  if (notebookLay_ != nullptr) {
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

  if (notebooks_.isEmpty()) {
    cells_.clear();
    if (session_ != nullptr && notebookLay_ != nullptr) {
      const ResearcherEvalResult help = session_->eval(QStringLiteral("help"));
      const int stretchIndex = notebookLay_->count() - 1;
      notebookLay_->insertWidget(
          qMax(0, stretchIndex),
          makeCell(notebookHost_, QStringLiteral("help"), help.html));
    }
    refreshBindings();
    return;
  }

  activeIndex_ = qBound(0, activeIndex_, notebooks_.size() - 1);
  cells_ = notebooks_[activeIndex_].cells;
  for (const ResearcherCell& cell : cells_) {
    history_.removeAll(cell.command);
    history_.append(cell.command);
    const int stretchIndex = notebookLay_->count() - 1;
    notebookLay_->insertWidget(
        qMax(0, stretchIndex),
        makeCell(notebookHost_, cell.command, cell.html));
  }
  if (cells_.isEmpty() && session_ != nullptr && notebookLay_ != nullptr) {
    const ResearcherEvalResult help = session_->eval(QStringLiteral("help"));
    const int stretchIndex = notebookLay_->count() - 1;
    notebookLay_->insertWidget(
        qMax(0, stretchIndex),
        makeCell(notebookHost_, QStringLiteral("help"), help.html));
  }
  refreshBindings();
}

void ResearcherPanel::reloadFromDocument() {
  notebooks_.clear();
  activeIndex_ = 0;
  if (doc_ != nullptr) {
    notebooks_ = doc_->researcherNotebooks();
    activeIndex_ = doc_->researcherActiveIndex();
  }
  if (notebooks_.isEmpty()) {
    ensureNotebooks();
  }
  rebuildTabBar();
  loadActiveNotebookIntoUi();
}

void ResearcherPanel::onTabChanged(int index) {
  if (updatingTabs_ || index < 0) return;
  if (index == activeIndex_) return;
  persistActiveNotebookToModel(false);
  activeIndex_ = index;
  loadActiveNotebookIntoUi();
  syncSessionToDocument(false);
}

void ResearcherPanel::onTabCloseRequested(int index) {
  if (index < 0 || index >= notebooks_.size()) return;
  if (notebooks_.size() == 1) {
    // Closing the last tab clears its cells instead of removing it.
    if (index == activeIndex_) {
      clearNotebookUi(true);
      persistActiveNotebookToModel(true);
    } else {
      notebooks_[index].cells.clear();
      syncSessionToDocument(true);
    }
    return;
  }

  persistActiveNotebookToModel(false);
  notebooks_.removeAt(index);
  if (activeIndex_ >= notebooks_.size()) {
    activeIndex_ = notebooks_.size() - 1;
  } else if (index < activeIndex_) {
    --activeIndex_;
  }
  rebuildTabBar();
  loadActiveNotebookIntoUi();
  syncSessionToDocument(true);
}

void ResearcherPanel::onTabDoubleClicked(int index) {
  if (index < 0 || index >= notebooks_.size()) return;
  bool ok = false;
  const QString name = QInputDialog::getText(
      this, QStringLiteral("Rename notebook"), QStringLiteral("Name:"),
      QLineEdit::Normal, notebooks_[index].name, &ok);
  if (!ok) return;
  const QString trimmed = name.trimmed();
  if (trimmed.isEmpty()) return;
  notebooks_[index].name = trimmed;
  updateTabLabels();
  syncSessionToDocument(true);
}

void ResearcherPanel::newNotebook() {
  persistActiveNotebookToModel(false);
  ResearcherNotebook nb;
  nb.name = QStringLiteral("Notebook %1").arg(notebooks_.size() + 1);
  notebooks_.push_back(nb);
  activeIndex_ = notebooks_.size() - 1;
  rebuildTabBar();
  loadActiveNotebookIntoUi();
  syncSessionToDocument(true);
}

void ResearcherPanel::addNotebookTab() { newNotebook(); }

void ResearcherPanel::duplicateNotebook() {
  ensureNotebooks();
  persistActiveNotebookToModel(false);
  ResearcherNotebook copy = notebooks_[activeIndex_];
  copy.name = copy.name + QStringLiteral(" copy");
  notebooks_.insert(activeIndex_ + 1, copy);
  activeIndex_ = activeIndex_ + 1;
  rebuildTabBar();
  loadActiveNotebookIntoUi();
  syncSessionToDocument(true);
}

void ResearcherPanel::deleteNotebook() {
  if (notebooks_.isEmpty()) return;
  onTabCloseRequested(activeIndex_);
}

void ResearcherPanel::clearNotebook() {
  clearNotebookUi(true);
  cells_.clear();
  persistActiveNotebookToModel(true);
}

void ResearcherPanel::appendCell(const QString& command,
                                 const ResearcherEvalResult& result) {
  if (notebookLay_ == nullptr) return;
  ensureNotebooks();

  // Drop ephemeral help-only UI when the first real cell is added.
  if (cells_.isEmpty()) {
    clearNotebookUi(false);
  }

  cells_.push_back(ResearcherCell{command, result.html, result.ok});
  persistActiveNotebookToModel(true);

  const int stretchIndex = notebookLay_->count() - 1;
  auto* cell = makeCell(notebookHost_, command, result.html);
  notebookLay_->insertWidget(qMax(0, stretchIndex), cell);

  QTimer::singleShot(0, this, [this]() {
    if (notebookScroll_ == nullptr) return;
    if (auto* bar = notebookScroll_->verticalScrollBar()) {
      bar->setValue(bar->maximum());
    }
  });
}

QString ResearcherPanel::defaultExportFileName() const {
  QString base = QStringLiteral("researcher-session");
  if (!notebooks_.isEmpty() && activeIndex_ >= 0 &&
      activeIndex_ < notebooks_.size()) {
    base = notebooks_[activeIndex_].name;
    base.replace(QLatin1Char('/'), QLatin1Char('-'));
  } else if (doc_ != nullptr && !doc_->path().isEmpty()) {
    base = QFileInfo(doc_->path()).completeBaseName() +
           QStringLiteral("-researcher");
  }
  return base + QStringLiteral(".html");
}

QString ResearcherPanel::buildExportHtml() const {
  QString body;
  body += QStringLiteral("<h1>ANP Studio — Researcher session</h1>");
  if (!notebooks_.isEmpty() && activeIndex_ >= 0 &&
      activeIndex_ < notebooks_.size()) {
    body += QStringLiteral("<h2>") +
            HtmlReport::escape(notebooks_[activeIndex_].name) +
            QStringLiteral("</h2>");
  }
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
      const ResearcherCell& cell = cells_.at(i);
      body += QStringLiteral("<h3>") + QString::number(i + 1) +
              QStringLiteral(". <code>") + HtmlReport::escape(cell.command) +
              QStringLiteral("</code></h3>");
      body += extractHtmlBody(cell.html);
    }
  }

  return HtmlReport::wrapDocument(body);
}

void ResearcherPanel::exportHtml() {
  persistActiveNotebookToModel(false);
  const QString path = QFileDialog::getSaveFileName(
      this, QStringLiteral("Export Researcher notebook"),
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
      QStringLiteral("Saved Researcher notebook to:\n%1").arg(path));
}
