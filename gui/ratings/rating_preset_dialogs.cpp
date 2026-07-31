#include "ratings/rating_preset_dialogs.hpp"

#include "ratings/rating_preset_store.hpp"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QUuid>
#include <QVBoxLayout>

bool promptSaveRatingPreset(QWidget* parent,
                            RatingPresetStore* store,
                            RatingPreset draft) {
  if (store == nullptr) return false;

  QDialog dlg(parent);
  dlg.setWindowTitle(QStringLiteral("Save as preset"));
  auto* lay = new QVBoxLayout(&dlg);
  auto* form = new QFormLayout;
  auto* nameEdit = new QLineEdit(draft.name, &dlg);
  auto* descEdit = new QLineEdit(draft.description, &dlg);
  form->addRow(QStringLiteral("Name"), nameEdit);
  form->addRow(QStringLiteral("Description"), descEdit);
  lay->addLayout(form);
  auto* buttons =
      new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dlg);
  lay->addWidget(buttons);
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  if (dlg.exec() != QDialog::Accepted) return false;

  draft.name = nameEdit->text().trimmed();
  draft.description = descEdit->text().trimmed();
  if (draft.name.isEmpty()) {
    QMessageBox::warning(parent, QStringLiteral("Save as preset"),
                         QStringLiteral("Name is required."));
    return false;
  }
  if (draft.id.isEmpty()) {
    draft.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  }
  draft.source = RatingPresetSource::User;
  QString err;
  if (!store->upsertUserPreset(draft, &err)) {
    QMessageBox::warning(parent, QStringLiteral("Save as preset"), err);
    return false;
  }
  return true;
}

void showManageRatingPresetsDialog(QWidget* parent, RatingPresetStore* store) {
  if (store == nullptr) return;

  QDialog dlg(parent);
  dlg.setWindowTitle(QStringLiteral("Manage scales"));
  dlg.resize(480, 360);
  auto* lay = new QVBoxLayout(&dlg);
  lay->addWidget(new QLabel(QStringLiteral("My scales"), &dlg));
  auto* list = new QListWidget(&dlg);
  const auto refill = [store, list]() {
    list->clear();
    for (const RatingPreset& p : store->userPresets()) {
      auto* item = new QListWidgetItem(p.name, list);
      item->setData(Qt::UserRole, p.id);
      item->setToolTip(p.description.isEmpty() ? p.id : p.description);
    }
  };
  refill();
  lay->addWidget(list, 1);

  auto* btnRow = new QHBoxLayout;
  auto* delBtn = new QPushButton(QStringLiteral("Delete"), &dlg);
  auto* closeBtn = new QPushButton(QStringLiteral("Close"), &dlg);
  btnRow->addWidget(delBtn);
  btnRow->addStretch();
  btnRow->addWidget(closeBtn);
  lay->addLayout(btnRow);

  QObject::connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
  QObject::connect(delBtn, &QPushButton::clicked, &dlg, [store, list, &dlg, refill]() {
    auto* item = list->currentItem();
    if (item == nullptr) return;
    const QString id = item->data(Qt::UserRole).toString();
    if (QMessageBox::question(
            &dlg, QStringLiteral("Delete preset"),
            QStringLiteral("Delete “%1”?").arg(item->text())) !=
        QMessageBox::Yes) {
      return;
    }
    QString err;
    if (!store->removeUserPreset(id, &err)) {
      QMessageBox::warning(&dlg, QStringLiteral("Delete failed"), err);
      return;
    }
    refill();
  });

  dlg.exec();
}
