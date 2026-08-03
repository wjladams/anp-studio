/**
 * @file settings_dialog.cpp
 * @brief App settings dialog with a left category list and detail pane.
 */

#include "panels/settings_dialog.hpp"

#include "panels/connected_accounts_page.hpp"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(GoogleOAuth* oauth, QWidget* parent)
    : QDialog(parent) {
  setWindowTitle(QStringLiteral("Settings"));
  setMinimumSize(640, 420);
  resize(720, 480);

  auto* root = new QVBoxLayout(this);
  auto* body = new QHBoxLayout();
  body->setSpacing(0);

  nav_ = new QListWidget(this);
  nav_->setObjectName(QStringLiteral("settingsNav"));
  nav_->setFixedWidth(180);
  nav_->setSpacing(2);
  nav_->addItem(QStringLiteral("Connected accounts"));
  nav_->setCurrentRow(0);
  nav_->setStyleSheet(QStringLiteral(
      "QListWidget#settingsNav {"
      "  background: #f3f1ec;"
      "  border: none;"
      "  border-right: 1px solid #d8d2c8;"
      "  padding: 8px 0;"
      "  outline: none;"
      "}"
      "QListWidget#settingsNav::item {"
      "  padding: 10px 14px;"
      "  margin: 0 6px;"
      "  border-radius: 6px;"
      "}"
      "QListWidget#settingsNav::item:selected {"
      "  background: #e4efe9;"
      "  color: #1f3d34;"
      "  font-weight: 600;"
      "}"));

  pages_ = new QStackedWidget(this);
  auto* accountsPage = new ConnectedAccountsPage(oauth, pages_);
  auto* accountsWrap = new QWidget(pages_);
  auto* accountsLay = new QVBoxLayout(accountsWrap);
  accountsLay->setContentsMargins(20, 16, 20, 16);
  accountsLay->addWidget(accountsPage);
  pages_->addWidget(accountsWrap);

  body->addWidget(nav_);
  body->addWidget(pages_, 1);
  root->addLayout(body, 1);

  auto* buttons =
      new QDialogButtonBox(QDialogButtonBox::Close, this);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  // Close is RejectRole on some styles; wire clicked Close explicitly.
  if (auto* closeBtn = buttons->button(QDialogButtonBox::Close)) {
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
  }
  root->addWidget(buttons);

  connect(nav_, &QListWidget::currentRowChanged, pages_,
          &QStackedWidget::setCurrentIndex);
}

void SettingsDialog::setCurrentPage(Page page) {
  const int row = static_cast<int>(page);
  if (nav_ != nullptr && row >= 0 && row < nav_->count()) {
    nav_->setCurrentRow(row);
  }
}
