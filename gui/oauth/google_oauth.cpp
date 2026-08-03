#include "oauth/google_oauth.hpp"

#include "google_oauth_config.hpp"

#include <QDateTime>
#include <QDesktopServices>
#include <QEventLoop>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>
#include <QUrlQuery>

namespace {

constexpr const char* kSettingsGroup = "ConnectedAccounts/Google";
constexpr const char* kAuthEndpoint =
    "https://accounts.google.com/o/oauth2/v2/auth";
constexpr const char* kTokenEndpoint = "https://oauth2.googleapis.com/token";
constexpr const char* kUserInfoEndpoint =
    "https://www.googleapis.com/oauth2/v2/userinfo";

// Forms create + read responses; openid email for account label.
const char* kScopes =
    "openid email "
    "https://www.googleapis.com/auth/forms.body "
    "https://www.googleapis.com/auth/forms.responses.readonly "
    "https://www.googleapis.com/auth/drive.file";

QByteArray readReplyBody(QNetworkReply* reply) {
  const QByteArray body = reply->readAll();
  reply->deleteLater();
  return body;
}

}  // namespace

GoogleOAuth::GoogleOAuth(QObject* parent)
    : QObject(parent), nam_(new QNetworkAccessManager(this)) {
  loadFromSettings();
}

GoogleOAuth::~GoogleOAuth() { stopLoopbackServer(); }

bool GoogleOAuth::isBuildConfigured() {
  return anpstudio::oauth::kGoogleOAuthConfigured &&
         anpstudio::oauth::kGoogleClientId[0] != '\0' &&
         anpstudio::oauth::kGoogleClientSecret[0] != '\0';
}

bool GoogleOAuth::isConnected() const { return !refreshToken_.isEmpty(); }

QString GoogleOAuth::accountEmail() const { return email_; }

void GoogleOAuth::loadFromSettings() {
  QSettings s;
  s.beginGroup(QLatin1String(kSettingsGroup));
  refreshToken_ = s.value(QStringLiteral("refreshToken")).toString();
  accessToken_ = s.value(QStringLiteral("accessToken")).toString();
  email_ = s.value(QStringLiteral("email")).toString();
  accessExpiresAtMs_ = s.value(QStringLiteral("accessExpiresAtMs")).toLongLong();
  s.endGroup();
}

void GoogleOAuth::saveToSettings() const {
  QSettings s;
  s.beginGroup(QLatin1String(kSettingsGroup));
  s.setValue(QStringLiteral("refreshToken"), refreshToken_);
  s.setValue(QStringLiteral("accessToken"), accessToken_);
  s.setValue(QStringLiteral("email"), email_);
  s.setValue(QStringLiteral("accessExpiresAtMs"), accessExpiresAtMs_);
  s.endGroup();
}

void GoogleOAuth::clearSettings() const {
  QSettings s;
  s.remove(QLatin1String(kSettingsGroup));
}

void GoogleOAuth::disconnectAccount() {
  stopLoopbackServer();
  authInProgress_ = false;
  accessToken_.clear();
  refreshToken_.clear();
  email_.clear();
  accessExpiresAtMs_ = 0;
  clearSettings();
  emit disconnected();
  emit statusChanged();
}

void GoogleOAuth::stopLoopbackServer() {
  if (server_ == nullptr) return;
  server_->close();
  server_->deleteLater();
  server_ = nullptr;
}

void GoogleOAuth::connectAccount() {
  if (!isBuildConfigured()) {
    emit errorOccurred(
        QStringLiteral("Google OAuth is not configured for this build.\n"
                       "Add credentials to .env and re-run cmake.\n"
                       "See docs/google-oauth.md."));
    return;
  }
  if (authInProgress_) {
    emit errorOccurred(QStringLiteral("Sign-in already in progress."));
    return;
  }
  authInProgress_ = true;
  startLoopbackServer();
}

void GoogleOAuth::startLoopbackServer() {
  stopLoopbackServer();
  server_ = new QTcpServer(this);
  if (!server_->listen(QHostAddress::LocalHost, 0)) {
    authInProgress_ = false;
    emit errorOccurred(QStringLiteral("Could not start local OAuth listener: ") +
                       server_->errorString());
    stopLoopbackServer();
    return;
  }

  const quint16 port = server_->serverPort();
  const QString redirectUri =
      QStringLiteral("http://127.0.0.1:%1/").arg(port);

  connect(server_, &QTcpServer::newConnection, this, [this, redirectUri]() {
    QTcpSocket* sock = server_->nextPendingConnection();
    if (sock == nullptr) return;
    connect(sock, &QTcpSocket::readyRead, this, [this, sock, redirectUri]() {
      const QByteArray req = sock->readAll();
      // First line: GET /?code=...&scope=... HTTP/1.1
      const QList<QByteArray> lines = req.split('\n');
      if (lines.isEmpty()) {
        sock->close();
        sock->deleteLater();
        return;
      }
      const QByteArray requestLine = lines.first().trimmed();
      QString pathAndQuery;
      {
        const QList<QByteArray> parts = requestLine.split(' ');
        if (parts.size() >= 2) pathAndQuery = QString::fromUtf8(parts[1]);
      }
      QUrl url(QStringLiteral("http://127.0.0.1") + pathAndQuery);
      const QUrlQuery q(url);
      const QString code = q.queryItemValue(QStringLiteral("code"));
      const QString error = q.queryItemValue(QStringLiteral("error"));

      const QByteArray htmlOk =
          "<html><body><h2>ANP Studio</h2><p>Google account connected. "
          "You can close this tab and return to the app.</p></body></html>";
      const QByteArray htmlErr =
          "<html><body><h2>ANP Studio</h2><p>Sign-in failed. You can close "
          "this tab.</p></body></html>";
      const QByteArray body = error.isEmpty() && !code.isEmpty() ? htmlOk : htmlErr;
      const QByteArray response =
          "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
          "Content-Length: " +
          QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" +
          body;
      sock->write(response);
      sock->disconnectFromHost();
      sock->deleteLater();
      stopLoopbackServer();

      if (!error.isEmpty()) {
        authInProgress_ = false;
        emit errorOccurred(QStringLiteral("Google sign-in error: ") + error);
        return;
      }
      if (code.isEmpty()) {
        authInProgress_ = false;
        emit errorOccurred(QStringLiteral("No authorization code received."));
        return;
      }
      exchangeCode(code, redirectUri);
    });
  });

  QUrl authUrl{QLatin1String(kAuthEndpoint)};
  QUrlQuery query;
  query.addQueryItem(QStringLiteral("client_id"),
                     QLatin1String(anpstudio::oauth::kGoogleClientId));
  query.addQueryItem(QStringLiteral("redirect_uri"), redirectUri);
  query.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
  query.addQueryItem(QStringLiteral("scope"), QLatin1String(kScopes));
  query.addQueryItem(QStringLiteral("access_type"), QStringLiteral("offline"));
  query.addQueryItem(QStringLiteral("prompt"), QStringLiteral("consent"));
  authUrl.setQuery(query);

  if (!QDesktopServices::openUrl(authUrl)) {
    authInProgress_ = false;
    stopLoopbackServer();
    emit errorOccurred(QStringLiteral("Could not open the system browser."));
  }
}

void GoogleOAuth::exchangeCode(const QString& code, const QString& redirectUri) {
  QNetworkRequest req{QUrl(QLatin1String(kTokenEndpoint))};
  req.setHeader(QNetworkRequest::ContentTypeHeader,
                QStringLiteral("application/x-www-form-urlencoded"));

  QUrlQuery body;
  body.addQueryItem(QStringLiteral("code"), code);
  body.addQueryItem(QStringLiteral("client_id"),
                    QLatin1String(anpstudio::oauth::kGoogleClientId));
  body.addQueryItem(QStringLiteral("client_secret"),
                    QLatin1String(anpstudio::oauth::kGoogleClientSecret));
  body.addQueryItem(QStringLiteral("redirect_uri"), redirectUri);
  body.addQueryItem(QStringLiteral("grant_type"),
                    QStringLiteral("authorization_code"));

  QNetworkReply* reply =
      nam_->post(req, body.query(QUrl::FullyEncoded).toUtf8());
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    const QByteArray raw = readReplyBody(reply);
    authInProgress_ = false;
    if (reply->error() != QNetworkReply::NoError) {
      emit errorOccurred(QStringLiteral("Token exchange failed: ") +
                         reply->errorString() + QStringLiteral("\n") +
                         QString::fromUtf8(raw));
      return;
    }
    const QJsonObject obj = QJsonDocument::fromJson(raw).object();
    accessToken_ = obj.value(QStringLiteral("access_token")).toString();
    const QString newRefresh =
        obj.value(QStringLiteral("refresh_token")).toString();
    if (!newRefresh.isEmpty()) refreshToken_ = newRefresh;
    const int expiresIn = obj.value(QStringLiteral("expires_in")).toInt(3600);
    accessExpiresAtMs_ =
        QDateTime::currentMSecsSinceEpoch() +
        static_cast<qint64>(expiresIn - 60) * 1000;
    if (accessToken_.isEmpty() || refreshToken_.isEmpty()) {
      emit errorOccurred(
          QStringLiteral("Token response missing access or refresh token.\n") +
          QString::fromUtf8(raw));
      return;
    }
    saveToSettings();
    fetchUserInfo(accessToken_);
  });
}

void GoogleOAuth::fetchUserInfo(const QString& accessToken) {
  QNetworkRequest req{QUrl(QLatin1String(kUserInfoEndpoint))};
  req.setRawHeader("Authorization",
                   QByteArray("Bearer ") + accessToken.toUtf8());
  QNetworkReply* reply = nam_->get(req);
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    const QByteArray raw = readReplyBody(reply);
    if (reply->error() == QNetworkReply::NoError) {
      const QJsonObject obj = QJsonDocument::fromJson(raw).object();
      email_ = obj.value(QStringLiteral("email")).toString();
      saveToSettings();
    }
    emit connected(email_);
    emit statusChanged();
  });
}

bool GoogleOAuth::refreshAccessToken() {
  if (refreshToken_.isEmpty() || !isBuildConfigured()) return false;

  QNetworkRequest req{QUrl(QLatin1String(kTokenEndpoint))};
  req.setHeader(QNetworkRequest::ContentTypeHeader,
                QStringLiteral("application/x-www-form-urlencoded"));
  QUrlQuery body;
  body.addQueryItem(QStringLiteral("client_id"),
                    QLatin1String(anpstudio::oauth::kGoogleClientId));
  body.addQueryItem(QStringLiteral("client_secret"),
                    QLatin1String(anpstudio::oauth::kGoogleClientSecret));
  body.addQueryItem(QStringLiteral("refresh_token"), refreshToken_);
  body.addQueryItem(QStringLiteral("grant_type"),
                    QStringLiteral("refresh_token"));

  QEventLoop loop;
  QNetworkReply* reply =
      nam_->post(req, body.query(QUrl::FullyEncoded).toUtf8());
  connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  loop.exec();

  const QByteArray raw = readReplyBody(reply);
  if (reply->error() != QNetworkReply::NoError) {
    emit errorOccurred(QStringLiteral("Token refresh failed: ") +
                       reply->errorString());
    return false;
  }
  const QJsonObject obj = QJsonDocument::fromJson(raw).object();
  accessToken_ = obj.value(QStringLiteral("access_token")).toString();
  const int expiresIn = obj.value(QStringLiteral("expires_in")).toInt(3600);
  accessExpiresAtMs_ = QDateTime::currentMSecsSinceEpoch() +
                       static_cast<qint64>(expiresIn - 60) * 1000;
  if (accessToken_.isEmpty()) return false;
  saveToSettings();
  return true;
}

QString GoogleOAuth::accessToken() {
  if (refreshToken_.isEmpty()) return {};
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  if (accessToken_.isEmpty() || now >= accessExpiresAtMs_) {
    if (!refreshAccessToken()) return {};
  }
  return accessToken_;
}
