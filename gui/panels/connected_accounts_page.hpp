/**
 * @file connected_accounts_page.hpp
 * @brief Settings page: Google Forms OAuth connect/disconnect.
 */

#pragma once

#include <QWidget>

class GoogleOAuth;
class QLabel;
class QPushButton;

class ConnectedAccountsPage : public QWidget {
  Q_OBJECT
public:
  explicit ConnectedAccountsPage(GoogleOAuth* oauth, QWidget* parent = nullptr);

private slots:
  void refreshUi();
  void onConnect();
  void onDisconnect();

private:
  GoogleOAuth* oauth_ = nullptr;
  QLabel* statusLabel_ = nullptr;
  QPushButton* connectBtn_ = nullptr;
  QPushButton* disconnectBtn_ = nullptr;
};
