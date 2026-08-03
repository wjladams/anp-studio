/**
 * @file connected_accounts_page.cpp
 * @brief Settings page: Google Forms OAuth connect/disconnect.
 */

#include "panels/connected_accounts_page.hpp"

#include "oauth/google_oauth.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

ConnectedAccountsPage::ConnectedAccountsPage(GoogleOAuth* oauth, QWidget* parent)
    : QWidget(parent), oauth_(oauth) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  auto* heading = new QLabel(QStringLiteral("<h2 style='margin:0'>Connected accounts</h2>"),
                             this);
  layout->addWidget(heading);

  auto* intro = new QLabel(
      QStringLiteral(
          "Used only for live Google Forms. Model participants are managed "
          "under Participants → Manage participants…. Collect Excel/Forms "
          "via Participants → Collect judgments…"),
      this);
  intro->setWordWrap(true);
  layout->addWidget(intro);

  auto* row = new QHBoxLayout();
  auto* titleCol = new QVBoxLayout();
  auto* title = new QLabel(QStringLiteral("<b>Google</b>"), this);
  auto* sub = new QLabel(QStringLiteral("Forms create & import results"), this);
  sub->setStyleSheet(QStringLiteral("color: #5c574e;"));
  titleCol->addWidget(title);
  titleCol->addWidget(sub);
  row->addLayout(titleCol, 1);

  auto* actions = new QVBoxLayout();
  statusLabel_ = new QLabel(this);
  connectBtn_ = new QPushButton(QStringLiteral("Connect…"), this);
  disconnectBtn_ = new QPushButton(QStringLiteral("Disconnect"), this);
  actions->addWidget(statusLabel_, 0, Qt::AlignRight);
  actions->addWidget(connectBtn_, 0, Qt::AlignRight);
  actions->addWidget(disconnectBtn_, 0, Qt::AlignRight);
  row->addLayout(actions);
  layout->addLayout(row);

  auto* docsHint = new QLabel(
      QStringLiteral(
          "Setup guide: docs/google-oauth.md in the ANP Studio source tree."),
      this);
  docsHint->setWordWrap(true);
  docsHint->setStyleSheet(QStringLiteral("color: #5c574e;"));
  layout->addWidget(docsHint);
  layout->addStretch(1);

  connect(connectBtn_, &QPushButton::clicked, this,
          &ConnectedAccountsPage::onConnect);
  connect(disconnectBtn_, &QPushButton::clicked, this,
          &ConnectedAccountsPage::onDisconnect);

  if (oauth_ != nullptr) {
    connect(oauth_, &GoogleOAuth::statusChanged, this,
            &ConnectedAccountsPage::refreshUi);
    connect(oauth_, &GoogleOAuth::connected, this, [this](const QString& email) {
      QMessageBox::information(
          this, QStringLiteral("Google connected"),
          email.isEmpty()
              ? QStringLiteral("Google account connected.")
              : QStringLiteral("Connected as %1.").arg(email));
      refreshUi();
    });
    connect(oauth_, &GoogleOAuth::errorOccurred, this,
            [this](const QString& msg) {
              QMessageBox::warning(this, QStringLiteral("Google sign-in"), msg);
              refreshUi();
            });
  }
  refreshUi();
}

void ConnectedAccountsPage::refreshUi() {
  const bool configured =
      oauth_ != nullptr && GoogleOAuth::isBuildConfigured();
  const bool connected = oauth_ != nullptr && oauth_->isConnected();

  if (!configured) {
    statusLabel_->setText(
        QStringLiteral("<span style='color:#8a5a2b'>Not configured in this "
                       "build</span>"));
    connectBtn_->setEnabled(false);
    disconnectBtn_->setEnabled(false);
    return;
  }
  if (connected) {
    const QString email = oauth_->accountEmail();
    statusLabel_->setText(
        QStringLiteral("<span style='color:#2f5d50'>Connected%1</span>")
            .arg(email.isEmpty() ? QString()
                                 : QStringLiteral(" · %1").arg(email)));
    connectBtn_->setEnabled(false);
    disconnectBtn_->setEnabled(true);
  } else {
    statusLabel_->setText(
        QStringLiteral("<span style='color:#8a5a2b'>Not connected</span>"));
    connectBtn_->setEnabled(true);
    disconnectBtn_->setEnabled(false);
  }
}

void ConnectedAccountsPage::onConnect() {
  if (oauth_ == nullptr) return;
  oauth_->connectAccount();
}

void ConnectedAccountsPage::onDisconnect() {
  if (oauth_ == nullptr) return;
  oauth_->disconnectAccount();
  refreshUi();
}
