#include "panels/session_panel.hpp"

#include "commands/network_commands.hpp"
#include "document.hpp"
#include "panels/participants_roster_dialog.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QSignalBlocker>
#include <QToolBox>
#include <QVBoxLayout>

namespace {
constexpr int kRoleKind = Qt::UserRole + 1;
const char* kKindParticipant = "participant";
const char* kKindAverage = "average";
const char* kKindGroup = "group";
}  // namespace

SessionPanel::SessionPanel(Document* doc, QWidget* parent)
    : QWidget(parent), doc_(doc) {
  setObjectName(QStringLiteral("sessionPanel"));
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(6);

  auto* headerRow = new QHBoxLayout;
  auto* title = new QLabel(QStringLiteral("Session"), this);
  title->setObjectName(QStringLiteral("selectorCaption"));
  headerRow->addWidget(title);
  headerRow->addStretch();
  editBtn_ = new QPushButton(QStringLiteral("Edit…"), this);
  editBtn_->setToolTip(QStringLiteral("Manage participants…"));
  headerRow->addWidget(editBtn_);
  layout->addLayout(headerRow);

  activePill_ = new QLabel(this);
  activePill_->setObjectName(QStringLiteral("sessionActivePill"));
  activePill_->setWordWrap(true);
  layout->addWidget(activePill_);

  emptyHint_ = new QLabel(
      QStringLiteral(
          "No participants yet. Single-judge editing is active. Click "
          "Edit… to add participants for multi-user judgments."),
      this);
  emptyHint_->setObjectName(QStringLiteral("selectorMuted"));
  emptyHint_->setWordWrap(true);
  layout->addWidget(emptyHint_);

  toolBox_ = new QToolBox(this);
  individualsList_ = new QListWidget(toolBox_);
  aggregatesList_ = new QListWidget(toolBox_);
  toolBox_->addItem(individualsList_, QStringLiteral("Individuals"));
  toolBox_->addItem(aggregatesList_, QStringLiteral("Aggregates"));
  layout->addWidget(toolBox_, 1);

  connect(editBtn_, &QPushButton::clicked, this, &SessionPanel::onEditClicked);
  connect(individualsList_, &QListWidget::itemClicked, this,
          &SessionPanel::onIndividualClicked);
  connect(aggregatesList_, &QListWidget::itemClicked, this,
          &SessionPanel::onAggregateClicked);
  connect(doc_, &Document::modelChanged, this, &SessionPanel::refresh);
  connect(doc_, &Document::viewNetworkChanged, this, &SessionPanel::refresh);
  connect(doc_, &Document::sessionChanged, this, &SessionPanel::refresh);

  refresh();
}

void SessionPanel::onEditClicked() {
  showParticipantsRosterDialog(this, doc_);
  refresh();
}

void SessionPanel::onIndividualClicked(QListWidgetItem* item) {
  if (item == nullptr) return;
  const QString id = item->data(Qt::UserRole).toString();
  anpcpp::JudgmentSession s;
  s.kind = anpcpp::JudgmentScopeKind::Participant;
  s.id = id.toStdString();
  const anpcpp::JudgmentSession old = doc_->judgmentSession();
  if (old.kind == s.kind && old.id == s.id) return;
  doc_->undoStack()->push(new SetJudgmentSessionCmd(doc_, s, old));
}

void SessionPanel::onAggregateClicked(QListWidgetItem* item) {
  if (item == nullptr) return;
  const QString kind = item->data(kRoleKind).toString();
  anpcpp::JudgmentSession s;
  if (kind == QLatin1String(kKindGroup)) {
    s.kind = anpcpp::JudgmentScopeKind::Group;
    s.id = item->data(Qt::UserRole).toString().toStdString();
  } else {
    s.kind = anpcpp::JudgmentScopeKind::Average;
  }
  const anpcpp::JudgmentSession old = doc_->judgmentSession();
  if (old.kind == s.kind && old.id == s.id) return;
  doc_->undoStack()->push(new SetJudgmentSessionCmd(doc_, s, old));
}

QString SessionPanel::activeLabel() const {
  const anpcpp::JudgmentSession session = doc_->judgmentSession();
  switch (session.kind) {
    case anpcpp::JudgmentScopeKind::Participant: {
      const QString id = QString::fromStdString(session.id);
      for (const auto& p : doc_->participants()) {
        if (QString::fromStdString(p.id) == id) {
          return QStringLiteral("%1 · individual (editable)")
              .arg(QString::fromStdString(p.name));
        }
      }
      return QStringLiteral("(unknown participant) · individual");
    }
    case anpcpp::JudgmentScopeKind::Group: {
      const QString id = QString::fromStdString(session.id);
      for (const auto& g : doc_->judgmentGroups()) {
        if (QString::fromStdString(g.id) == id) {
          return QStringLiteral("%1 · group average (read-only)")
              .arg(QString::fromStdString(g.name));
        }
      }
      return QStringLiteral("(unknown group) · aggregate");
    }
    case anpcpp::JudgmentScopeKind::Average:
    default:
      return QStringLiteral("Group average · all participants (read-only)");
  }
}

void SessionPanel::refresh() {
  const bool hasParticipants = doc_->hasParticipants();
  emptyHint_->setVisible(!hasParticipants);
  toolBox_->setVisible(hasParticipants);
  activePill_->setVisible(hasParticipants);
  if (!hasParticipants) return;

  activePill_->setText(activeLabel());
  const anpcpp::JudgmentSession session = doc_->judgmentSession();

  {
    const QSignalBlocker blocker(individualsList_);
    individualsList_->clear();
    for (const auto& p : doc_->participants()) {
      const QString id = QString::fromStdString(p.id);
      auto* item = new QListWidgetItem(QString::fromStdString(p.name),
                                       individualsList_);
      item->setData(Qt::UserRole, id);
      item->setData(kRoleKind, QString::fromLatin1(kKindParticipant));
      if (session.kind == anpcpp::JudgmentScopeKind::Participant &&
          QString::fromStdString(session.id) == id) {
        individualsList_->setCurrentItem(item);
      }
    }
    toolBox_->setItemText(
        0, QStringLiteral("Individuals (%1)").arg(doc_->participants().size()));
  }

  {
    const QSignalBlocker blocker(aggregatesList_);
    aggregatesList_->clear();
    auto* avgItem = new QListWidgetItem(QStringLiteral("Group average"),
                                        aggregatesList_);
    avgItem->setData(kRoleKind, QString::fromLatin1(kKindAverage));
    if (session.kind == anpcpp::JudgmentScopeKind::Average) {
      aggregatesList_->setCurrentItem(avgItem);
    }
    for (const auto& g : doc_->judgmentGroups()) {
      const QString id = QString::fromStdString(g.id);
      auto* item = new QListWidgetItem(QString::fromStdString(g.name),
                                       aggregatesList_);
      item->setData(Qt::UserRole, id);
      item->setData(kRoleKind, QString::fromLatin1(kKindGroup));
      if (session.kind == anpcpp::JudgmentScopeKind::Group &&
          QString::fromStdString(session.id) == id) {
        aggregatesList_->setCurrentItem(item);
      }
    }
    toolBox_->setItemText(
        1, QStringLiteral("Aggregates (%1)")
               .arg(1 + doc_->judgmentGroups().size()));
  }
}
