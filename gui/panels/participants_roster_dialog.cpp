#include "panels/participants_roster_dialog.hpp"

#include "commands/network_commands.hpp"
#include "document.hpp"

#include <QAbstractItemView>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <algorithm>

namespace {

QString slugify(const QString& name) {
  QString s = name.trimmed().toLower();
  QString out;
  bool lastDash = false;
  for (const QChar& c : s) {
    if (c.isLetterOrNumber()) {
      out += c;
      lastDash = false;
    } else if (!lastDash && !out.isEmpty()) {
      out += QLatin1Char('-');
      lastDash = true;
    }
  }
  while (out.endsWith(QLatin1Char('-'))) out.chop(1);
  if (out.isEmpty()) out = QStringLiteral("participant");
  return out;
}

QString uniqueParticipantId(const QString& base, Document* doc) {
  QString candidate = base;
  int n = 2;
  auto exists = [&](const QString& id) {
    for (const auto& p : doc->participants()) {
      if (QString::fromStdString(p.id) == id) return true;
    }
    return false;
  };
  while (exists(candidate)) {
    candidate = base + QStringLiteral("-%1").arg(n++);
  }
  return candidate;
}

QString uniqueGroupId(const QString& base, Document* doc) {
  QString candidate = base;
  int n = 2;
  auto exists = [&](const QString& id) {
    for (const auto& g : doc->judgmentGroups()) {
      if (QString::fromStdString(g.id) == id) return true;
    }
    return false;
  };
  while (exists(candidate)) {
    candidate = base + QStringLiteral("-%1").arg(n++);
  }
  return candidate;
}

/** @brief Prompts for participant Name/Email (Id shown read-only when renaming). */
bool promptParticipantFields(QWidget* parent,
                             const QString& title,
                             QString* name,
                             QString* email,
                             const QString& idPreview) {
  QDialog dlg(parent);
  dlg.setWindowTitle(title);
  auto* lay = new QVBoxLayout(&dlg);
  auto* form = new QFormLayout;
  auto* nameEdit = new QLineEdit(*name, &dlg);
  auto* emailEdit = new QLineEdit(*email, &dlg);
  form->addRow(QStringLiteral("Name"), nameEdit);
  form->addRow(QStringLiteral("Email"), emailEdit);
  if (!idPreview.isEmpty()) {
    auto* idLabel = new QLabel(idPreview, &dlg);
    idLabel->setEnabled(false);
    form->addRow(QStringLiteral("Id"), idLabel);
  }
  lay->addLayout(form);
  auto* buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  lay->addWidget(buttons);
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  nameEdit->setFocus();
  if (dlg.exec() != QDialog::Accepted) return false;
  *name = nameEdit->text().trimmed();
  *email = emailEdit->text().trimmed();
  return !name->isEmpty();
}

void showManageGroupsDialog(QWidget* parent, Document* doc) {
  QDialog dlg(parent);
  dlg.setWindowTitle(QStringLiteral("Manage groups"));
  dlg.resize(560, 380);
  auto* lay = new QHBoxLayout(&dlg);

  auto* left = new QVBoxLayout;
  left->addWidget(new QLabel(QStringLiteral("Groups"), &dlg));
  auto* groupList = new QListWidget(&dlg);
  left->addWidget(groupList, 1);
  auto* groupBtnRow = new QHBoxLayout;
  auto* addGroupBtn = new QPushButton(QStringLiteral("Add"), &dlg);
  auto* renameGroupBtn = new QPushButton(QStringLiteral("Rename"), &dlg);
  auto* removeGroupBtn = new QPushButton(QStringLiteral("Remove"), &dlg);
  groupBtnRow->addWidget(addGroupBtn);
  groupBtnRow->addWidget(renameGroupBtn);
  groupBtnRow->addWidget(removeGroupBtn);
  left->addLayout(groupBtnRow);
  lay->addLayout(left, 1);

  auto* right = new QVBoxLayout;
  right->addWidget(new QLabel(QStringLiteral("Members"), &dlg));
  auto* memberList = new QListWidget(&dlg);
  right->addWidget(memberList, 1);
  lay->addLayout(right, 1);

  const auto refillGroups = [&, groupList](const QString& keepId) {
    groupList->clear();
    for (const auto& g : doc->judgmentGroups()) {
      auto* item =
          new QListWidgetItem(QString::fromStdString(g.name), groupList);
      item->setData(Qt::UserRole, QString::fromStdString(g.id));
      if (QString::fromStdString(g.id) == keepId) {
        groupList->setCurrentItem(item);
      }
    }
  };

  const auto refillMembers = [&, memberList](const QString& groupId) {
    const QSignalBlocker blocker(memberList);
    memberList->clear();
    if (groupId.isEmpty()) return;
    const anpcpp::JudgmentGroup* g = nullptr;
    for (const auto& candidate : doc->judgmentGroups()) {
      if (QString::fromStdString(candidate.id) == groupId) {
        g = &candidate;
        break;
      }
    }
    for (const auto& p : doc->participants()) {
      const QString pid = QString::fromStdString(p.id);
      auto* item = new QListWidgetItem(QString::fromStdString(p.name),
                                       memberList);
      item->setData(Qt::UserRole, pid);
      item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
      const bool member =
          g != nullptr &&
          std::find(g->member_ids.begin(), g->member_ids.end(),
                    p.id) != g->member_ids.end();
      item->setCheckState(member ? Qt::Checked : Qt::Unchecked);
    }
  };

  refillGroups({});
  if (groupList->count() > 0) {
    groupList->setCurrentRow(0);
    refillMembers(groupList->currentItem()->data(Qt::UserRole).toString());
  }

  QObject::connect(groupList, &QListWidget::currentItemChanged, &dlg,
                   [&, memberList](QListWidgetItem* cur, QListWidgetItem*) {
                     refillMembers(cur == nullptr
                                       ? QString()
                                       : cur->data(Qt::UserRole).toString());
                   });

  QObject::connect(
      memberList, &QListWidget::itemChanged, &dlg,
      [&, groupList, memberList](QListWidgetItem*) {
        auto* cur = groupList->currentItem();
        if (cur == nullptr) return;
        const QString groupId = cur->data(Qt::UserRole).toString();
        const anpcpp::JudgmentGroup* g = nullptr;
        for (const auto& candidate : doc->judgmentGroups()) {
          if (QString::fromStdString(candidate.id) == groupId) {
            g = &candidate;
            break;
          }
        }
        if (g == nullptr) return;
        QStringList oldMembers;
        for (const auto& m : g->member_ids) {
          oldMembers << QString::fromStdString(m);
        }
        const QString groupName = QString::fromStdString(g->name);
        QStringList members;
        for (int i = 0; i < memberList->count(); ++i) {
          auto* it = memberList->item(i);
          if (it->checkState() == Qt::Checked) {
            members << it->data(Qt::UserRole).toString();
          }
        }
        if (members == oldMembers) return;
        doc->undoStack()->push(new SetJudgmentGroupCmd(
            doc, groupId, groupName, members, true, groupName, oldMembers));
      });

  QObject::connect(addGroupBtn, &QPushButton::clicked, &dlg, [&]() {
    bool ok = false;
    const QString name = QInputDialog::getText(
        &dlg, QStringLiteral("Add group"), QStringLiteral("Group name:"),
        QLineEdit::Normal, {}, &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    const QString id = uniqueGroupId(slugify(name.trimmed()), doc);
    doc->undoStack()->push(new SetJudgmentGroupCmd(
        doc, id, name.trimmed(), {}, false, {}, {}));
    refillGroups(id);
    refillMembers(id);
  });

  QObject::connect(renameGroupBtn, &QPushButton::clicked, &dlg, [&]() {
    auto* cur = groupList->currentItem();
    if (cur == nullptr) return;
    const QString id = cur->data(Qt::UserRole).toString();
    const anpcpp::JudgmentGroup* g = nullptr;
    for (const auto& candidate : doc->judgmentGroups()) {
      if (QString::fromStdString(candidate.id) == id) {
        g = &candidate;
        break;
      }
    }
    if (g == nullptr) return;
    bool ok = false;
    const QString name = QInputDialog::getText(
        &dlg, QStringLiteral("Rename group"), QStringLiteral("Group name:"),
        QLineEdit::Normal, QString::fromStdString(g->name), &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    QStringList members;
    for (const auto& m : g->member_ids) members << QString::fromStdString(m);
    const QString oldName = QString::fromStdString(g->name);
    if (name.trimmed() == oldName) return;
    doc->undoStack()->push(new SetJudgmentGroupCmd(
        doc, id, name.trimmed(), members, true, oldName, members));
    refillGroups(id);
  });

  QObject::connect(removeGroupBtn, &QPushButton::clicked, &dlg, [&]() {
    auto* cur = groupList->currentItem();
    if (cur == nullptr) return;
    const QString id = cur->data(Qt::UserRole).toString();
    if (QMessageBox::question(
            &dlg, QStringLiteral("Remove group"),
            QStringLiteral("Remove group “%1”?").arg(cur->text())) !=
        QMessageBox::Yes) {
      return;
    }
    const anpcpp::JudgmentGroup* g = nullptr;
    for (const auto& candidate : doc->judgmentGroups()) {
      if (QString::fromStdString(candidate.id) == id) {
        g = &candidate;
        break;
      }
    }
    if (g == nullptr) return;
    QStringList members;
    for (const auto& m : g->member_ids) members << QString::fromStdString(m);
    const anpcpp::JudgmentSession oldSession = doc->judgmentSession();
    doc->undoStack()->push(new RemoveJudgmentGroupCmd(
        doc, id, QString::fromStdString(g->name), members, oldSession));
    refillGroups({});
    refillMembers(groupList->count() > 0
                      ? groupList->item(0)->data(Qt::UserRole).toString()
                      : QString());
  });

  dlg.exec();
}

}  // namespace

void showParticipantsRosterDialog(QWidget* parent, Document* doc) {
  if (doc == nullptr) return;

  QDialog dlg(parent);
  dlg.setWindowTitle(QStringLiteral("Participants"));
  dlg.resize(520, 380);
  auto* lay = new QVBoxLayout(&dlg);

  auto* btnRow = new QHBoxLayout;
  auto* addBtn = new QPushButton(QStringLiteral("Add"), &dlg);
  auto* renameBtn = new QPushButton(QStringLiteral("Rename"), &dlg);
  auto* removeBtn = new QPushButton(QStringLiteral("Remove"), &dlg);
  auto* groupsBtn = new QPushButton(QStringLiteral("Manage groups…"), &dlg);
  btnRow->addWidget(addBtn);
  btnRow->addWidget(renameBtn);
  btnRow->addWidget(removeBtn);
  btnRow->addWidget(groupsBtn);
  btnRow->addStretch();
  lay->addLayout(btnRow);

  auto* table = new QTableWidget(0, 3, &dlg);
  table->setHorizontalHeaderLabels(
      {QStringLiteral("Id"), QStringLiteral("Name"), QStringLiteral("Email")});
  table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  table->setSelectionBehavior(QAbstractItemView::SelectRows);
  table->setSelectionMode(QAbstractItemView::SingleSelection);
  table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  lay->addWidget(table, 1);

  auto* hint = new QLabel(
      QStringLiteral(
          "Participants live on this model file (not app accounts). "
          "Groups are named subsets used by Session and Analysis scope."),
      &dlg);
  hint->setObjectName(QStringLiteral("selectorMuted"));
  hint->setWordWrap(true);
  lay->addWidget(hint);

  auto* buttons =
      new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
  lay->addWidget(buttons);
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);

  const auto refill = [&, table](const QString& keepId) {
    table->setRowCount(0);
    for (const auto& p : doc->participants()) {
      const int row = table->rowCount();
      table->insertRow(row);
      table->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(p.id)));
      table->setItem(row, 1,
                     new QTableWidgetItem(QString::fromStdString(p.name)));
      table->setItem(row, 2,
                     new QTableWidgetItem(QString::fromStdString(p.email)));
      if (QString::fromStdString(p.id) == keepId) {
        table->selectRow(row);
      }
    }
  };
  refill({});

  QObject::connect(addBtn, &QPushButton::clicked, &dlg, [&]() {
    QString name;
    QString email;
    if (!promptParticipantFields(&dlg, QStringLiteral("Add participant"),
                                 &name, &email, {})) {
      return;
    }
    const QString id = uniqueParticipantId(slugify(name), doc);
    doc->undoStack()->push(new AddParticipantCmd(doc, id, name, email));
    refill(id);
  });

  QObject::connect(renameBtn, &QPushButton::clicked, &dlg, [&]() {
    const int row = table->currentRow();
    if (row < 0) return;
    const QString id = table->item(row, 0)->text();
    QString name = table->item(row, 1)->text();
    QString email = table->item(row, 2)->text();
    const QString oldName = name;
    const QString oldEmail = email;
    if (!promptParticipantFields(&dlg, QStringLiteral("Rename participant"),
                                 &name, &email, id)) {
      return;
    }
    if (name == oldName && email == oldEmail) return;
    doc->undoStack()->push(
        new UpdateParticipantCmd(doc, id, name, email, oldName, oldEmail));
    refill(id);
  });

  QObject::connect(removeBtn, &QPushButton::clicked, &dlg, [&]() {
    const int row = table->currentRow();
    if (row < 0) return;
    const QString id = table->item(row, 0)->text();
    const QString name = table->item(row, 1)->text();
    if (QMessageBox::question(
            &dlg, QStringLiteral("Remove participant"),
            QStringLiteral("Remove “%1” and their judgments?").arg(name)) !=
        QMessageBox::Yes) {
      return;
    }
    const QByteArray before = doc->snapshotNetworkJson();
    doc->removeParticipant(id);
    const QByteArray after = doc->snapshotNetworkJson();
    doc->undoStack()->push(new ApplyNetworkSnapshotCmd(
        doc, before, after,
        QStringLiteral("Remove participant %1").arg(name)));
    refill({});
  });

  QObject::connect(groupsBtn, &QPushButton::clicked, &dlg, [&]() {
    showManageGroupsDialog(&dlg, doc);
    refill({});
  });

  dlg.exec();
}
