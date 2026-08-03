/**
 * @file google_oauth.hpp
 * @brief Desktop Google OAuth (loopback) + token store for Forms Connect.
 */

#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QTcpServer;

/**
 * @brief App-level Google account connection (not model participants).
 *
 * Uses compile-time client id/secret from google_oauth_config.hpp (.env).
 * Tokens live in QSettings under ConnectedAccounts/Google.
 */
class GoogleOAuth : public QObject {
  Q_OBJECT
public:
  explicit GoogleOAuth(QObject* parent = nullptr);
  ~GoogleOAuth() override;

  /** @return True if the build has client id + secret. */
  [[nodiscard]] static bool isBuildConfigured();

  /** @return True if a refresh token is stored. */
  [[nodiscard]] bool isConnected() const;

  /** @return Cached account email, or empty. */
  [[nodiscard]] QString accountEmail() const;

  /** @brief Starts browser loopback OAuth; emits connected or errorOccurred. */
  void connectAccount();

  /** @brief Clears stored tokens. */
  void disconnectAccount();

  /**
   * @brief Ensures a valid access token (refreshing if needed).
   * @return Access token, or empty on failure (emits errorOccurred).
   */
  [[nodiscard]] QString accessToken();

signals:
  void connected(const QString& email);
  void disconnected();
  void errorOccurred(const QString& message);
  void statusChanged();

private:
  void loadFromSettings();
  void saveToSettings() const;
  void clearSettings() const;
  void startLoopbackServer();
  void stopLoopbackServer();
  void exchangeCode(const QString& code, const QString& redirectUri);
  void fetchUserInfo(const QString& accessToken);
  bool refreshAccessToken();

  QNetworkAccessManager* nam_ = nullptr;
  QTcpServer* server_ = nullptr;
  QString accessToken_;
  QString refreshToken_;
  QString email_;
  qint64 accessExpiresAtMs_ = 0;
  bool authInProgress_ = false;
};
