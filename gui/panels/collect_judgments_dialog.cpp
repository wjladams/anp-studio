/**
 * @file collect_judgments_dialog.cpp
 * @brief Judgments-stage hub for Excel / Google Forms / CSV collection.
 */

#include "panels/collect_judgments_dialog.hpp"

#include "document.hpp"
#include "oauth/google_forms_client.hpp"
#include "oauth/google_oauth.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

QFrame* makeCard(QWidget* parent) {
  auto* card = new QFrame(parent);
  card->setFrameShape(QFrame::StyledPanel);
  card->setObjectName(QStringLiteral("collectHubCard"));
  card->setStyleSheet(QStringLiteral(
      "QFrame#collectHubCard {"
      "  background: #fff;"
      "  border: 1px solid #d4cfc4;"
      "  border-radius: 4px;"
      "}"
      "QLabel#collectChannelTag {"
      "  color: #5c574e;"
      "  font-size: 10px;"
      "  font-weight: 600;"
      "  letter-spacing: 0.04em;"
      "}"
      "QLabel#collectCardBody {"
      "  color: #5c574e;"
      "}"));
  return card;
}

QLabel* channelTag(const QString& text, QWidget* parent) {
  auto* tag = new QLabel(text.toUpper(), parent);
  tag->setObjectName(QStringLiteral("collectChannelTag"));
  return tag;
}

}  // namespace

CollectJudgmentsDialog::CollectJudgmentsDialog(Document* doc, GoogleOAuth* oauth,
                                               QWidget* parent)
    : QDialog(parent), doc_(doc), oauth_(oauth) {
  setWindowTitle(QStringLiteral("Collect judgments"));
  setMinimumWidth(640);
  resize(720, 480);

  auto* root = new QVBoxLayout(this);
  root->setSpacing(12);

  auto* intro = new QLabel(
      QStringLiteral(
          "Choose how to gather pairwise and ratings votes. Channels write "
          "into the same participant roster."),
      this);
  intro->setWordWrap(true);
  intro->setStyleSheet(QStringLiteral("color: #5c574e;"));
  root->addWidget(intro);

  auto* participantRow = new QHBoxLayout();
  participantRow->addWidget(new QLabel(QStringLiteral("Participant"), this));
  participantCombo_ = new QComboBox(this);
  participantCombo_->setMinimumWidth(220);
  participantRow->addWidget(participantCombo_, 1);
  root->addLayout(participantRow);

  auto* grid = new QGridLayout();
  grid->setHorizontalSpacing(10);
  grid->setVerticalSpacing(10);

  // --- Excel ---
  auto* excelCard = makeCard(this);
  auto* excelLay = new QVBoxLayout(excelCard);
  excelLay->setContentsMargins(12, 12, 12, 12);
  excelLay->setSpacing(6);
  excelLay->addWidget(channelTag(QStringLiteral("File"), excelCard));
  excelLay->addWidget(new QLabel(QStringLiteral("<b>Excel template</b>"), excelCard));
  auto* excelBody = new QLabel(
      QStringLiteral(
          "Export a per-person .xlsx (Your judgments + hidden _meta). "
          "Share offline; import when they return it. Always available."),
      excelCard);
  excelBody->setObjectName(QStringLiteral("collectCardBody"));
  excelBody->setWordWrap(true);
  excelLay->addWidget(excelBody, 1);
  auto* excelActions = new QHBoxLayout();
  auto* exportBtn = new QPushButton(QStringLiteral("Export .xlsx…"), excelCard);
  auto* importXlsxBtn =
      new QPushButton(QStringLiteral("Import .xlsx…"), excelCard);
  excelActions->addWidget(exportBtn);
  excelActions->addWidget(importXlsxBtn);
  excelActions->addStretch();
  excelLay->addLayout(excelActions);
  grid->addWidget(excelCard, 0, 0);

  // --- Google Forms ---
  auto* googleCard = makeCard(this);
  auto* googleLay = new QVBoxLayout(googleCard);
  googleLay->setContentsMargins(12, 12, 12, 12);
  googleLay->setSpacing(6);
  googleStatus_ = new QLabel(googleCard);
  googleLay->addWidget(googleStatus_);
  googleLay->addWidget(channelTag(QStringLiteral("Live survey"), googleCard));
  googleLay->addWidget(new QLabel(QStringLiteral("<b>Google Forms</b>"), googleCard));
  auto* googleBody = new QLabel(
      QStringLiteral(
          "Create a form from the current structure; share the URL; later "
          "pull responses into participants."),
      googleCard);
  googleBody->setObjectName(QStringLiteral("collectCardBody"));
  googleBody->setWordWrap(true);
  googleLay->addWidget(googleBody, 1);

  googleEnabledActions_ = new QWidget(googleCard);
  auto* enabledLay = new QHBoxLayout(googleEnabledActions_);
  enabledLay->setContentsMargins(0, 0, 0, 0);
  createFormBtn_ =
      new QPushButton(QStringLiteral("Create form…"), googleEnabledActions_);
  importFormBtn_ =
      new QPushButton(QStringLiteral("Import results…"), googleEnabledActions_);
  openFormBtn_ =
      new QPushButton(QStringLiteral("Open linked form…"), googleEnabledActions_);
  createFormBtn_->setDefault(true);
  enabledLay->addWidget(createFormBtn_);
  enabledLay->addWidget(importFormBtn_);
  enabledLay->addWidget(openFormBtn_);
  enabledLay->addStretch();
  googleLay->addWidget(googleEnabledActions_);

  googleConnectActions_ = new QWidget(googleCard);
  auto* connectLay = new QHBoxLayout(googleConnectActions_);
  connectLay->setContentsMargins(0, 0, 0, 0);
  auto* connectBtn =
      new QPushButton(QStringLiteral("Connect Google…"), googleConnectActions_);
  connectBtn->setDefault(true);
  connectLay->addWidget(connectBtn);
  connectLay->addStretch();
  googleLay->addWidget(googleConnectActions_);

  grid->addWidget(googleCard, 0, 1);

  // --- Forms CSV (fallback) ---
  auto* csvCard = makeCard(this);
  auto* csvOuter = new QHBoxLayout(csvCard);
  csvOuter->setContentsMargins(12, 12, 12, 12);
  auto* csvText = new QVBoxLayout();
  csvText->addWidget(channelTag(QStringLiteral("Fallback"), csvCard));
  csvText->addWidget(new QLabel(QStringLiteral("<b>Forms CSV</b>"), csvCard));
  auto* csvBody = new QLabel(
      QStringLiteral(
          "Manual download from any Forms tool → import. No OAuth. Kept for "
          "legacy / offline survey exports."),
      csvCard);
  csvBody->setObjectName(QStringLiteral("collectCardBody"));
  csvBody->setWordWrap(true);
  csvText->addWidget(csvBody);
  csvOuter->addLayout(csvText, 1);
  auto* importCsvBtn = new QPushButton(QStringLiteral("Import CSV…"), csvCard);
  csvOuter->addWidget(importCsvBtn, 0, Qt::AlignVCenter);
  grid->addWidget(csvCard, 1, 0, 1, 2);

  root->addLayout(grid, 1);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  if (auto* closeBtn = buttons->button(QDialogButtonBox::Close)) {
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
  }
  root->addWidget(buttons);

  connect(exportBtn, &QPushButton::clicked, this, [this]() {
    emit exportExcelRequested(selectedParticipantId());
  });
  connect(importXlsxBtn, &QPushButton::clicked, this,
          &CollectJudgmentsDialog::importExcelRequested);
  connect(importCsvBtn, &QPushButton::clicked, this,
          &CollectJudgmentsDialog::importCsvRequested);
  connect(createFormBtn_, &QPushButton::clicked, this,
          &CollectJudgmentsDialog::createGoogleFormRequested);
  connect(importFormBtn_, &QPushButton::clicked, this,
          &CollectJudgmentsDialog::importGoogleFormRequested);
  connect(openFormBtn_, &QPushButton::clicked, this,
          &CollectJudgmentsDialog::openLinkedFormRequested);
  connect(connectBtn, &QPushButton::clicked, this,
          &CollectJudgmentsDialog::connectGoogleRequested);

  if (doc_ != nullptr) {
    connect(doc_, &Document::modelChanged, this, &CollectJudgmentsDialog::refresh);
    connect(doc_, &Document::linkedFormsChanged, this,
            &CollectJudgmentsDialog::refresh);
  }
  if (oauth_ != nullptr) {
    connect(oauth_, &GoogleOAuth::statusChanged, this,
            &CollectJudgmentsDialog::refresh);
  }

  refresh();
}

QString CollectJudgmentsDialog::selectedParticipantId() const {
  if (participantCombo_ == nullptr) return {};
  return participantCombo_->currentData().toString();
}

void CollectJudgmentsDialog::refresh() {
  const QString prevId =
      participantCombo_ != nullptr ? participantCombo_->currentData().toString()
                                   : QString();
  participantCombo_->clear();
  participantCombo_->addItem(QStringLiteral("All participants"), QString());
  if (doc_ != nullptr) {
    int restore = 0;
    for (const auto& p : doc_->participants()) {
      const QString id = QString::fromStdString(p.id);
      const int i = participantCombo_->count();
      participantCombo_->addItem(QString::fromStdString(p.name), id);
      if (!prevId.isEmpty() && id == prevId) restore = i;
    }
    participantCombo_->setCurrentIndex(restore);
  }

  const bool configured =
      oauth_ != nullptr && GoogleOAuth::isBuildConfigured();
  const bool connected = oauth_ != nullptr && oauth_->isConnected();
  const LinkedGoogleForm* linked =
      doc_ != nullptr ? doc_->latestLinkedGoogleForm() : nullptr;
  const bool hasForm = linked != nullptr && !linked->formId.isEmpty();
  const bool formCurrent =
      hasForm &&
      googleFormFingerprintMatches(linked->structureFingerprint, doc_->root());

  if (!configured) {
    googleStatus_->setText(
        QStringLiteral("<span style='color:#8a5a2b'>○ Google OAuth not "
                       "configured in this build</span>"));
    googleEnabledActions_->setVisible(false);
    googleConnectActions_->setVisible(false);
  } else if (connected) {
    const QString email = oauth_->accountEmail();
    googleStatus_->setText(
        QStringLiteral("<span style='color:#2f5d50'>● Google connected%1</span>")
            .arg(email.isEmpty() ? QString()
                                 : QStringLiteral(" · %1").arg(email)));
    googleEnabledActions_->setVisible(true);
    googleConnectActions_->setVisible(false);
    importFormBtn_->setEnabled(hasForm);
    openFormBtn_->setEnabled(hasForm);
    openFormBtn_->setText(
        formCurrent ? QStringLiteral("Open linked form…")
                    : (hasForm ? QStringLiteral("Open linked form… (out of date)")
                               : QStringLiteral("Open linked form…")));
    importFormBtn_->setText(
        formCurrent
            ? QStringLiteral("Import results…")
            : (hasForm ? QStringLiteral("Import results… (out of date)")
                       : QStringLiteral("Import results…")));
  } else {
    googleStatus_->setText(
        QStringLiteral("<span style='color:#8a5a2b'>○ Google not connected</span>"));
    googleEnabledActions_->setVisible(false);
    googleConnectActions_->setVisible(true);
  }
}
