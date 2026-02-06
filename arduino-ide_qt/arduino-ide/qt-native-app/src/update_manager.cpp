#include "update_manager.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

namespace {
constexpr auto kSettingsGroup = "UpdateManager";
constexpr auto kAutoCheckKey = "autoCheckEnabled";
constexpr auto kCheckIntervalDaysKey = "checkIntervalDays";
constexpr auto kLastCheckTimeKey = "lastCheckTime";
constexpr auto kReleaseChannelKey = "releaseChannel";
constexpr auto kSkippedVersionKey = "skippedVersion";

constexpr int kDefaultCheckIntervalDays = 7;  // Check weekly by default
constexpr char kStableReleaseUrl[] =
    "https://api.github.com/repos/vibeblink/vibeblink-ide/releases/latest";
constexpr char kBetaReleaseUrl[] =
    "https://api.github.com/repos/vibeblink/vibeblink-ide/releases";  // All releases
}  // namespace

UpdateManager::UpdateManager(QObject* parent)
    : QObject(parent), networkManager_(new QNetworkAccessManager(this)) {
  loadSettings();
}

UpdateManager::~UpdateManager() = default;

void UpdateManager::checkForUpdates(ReleaseChannel channel) {
  if (!autoCheckEnabled()) {
    emit updateCheckCompleted(UpdateCheckStatus::CheckDisabled, ReleaseInfo{});
    return;
  }

  // Check if enough time has passed since last check
  const QDateTime lastCheck = lastCheckTime();
  const QDateTime now = QDateTime::currentDateTime();
  if (lastCheck.isValid() &&
      lastCheck.daysTo(now) < checkIntervalDays()) {
    // Not time to check yet
    return;
  }

  QString url;
  switch (channel) {
    case ReleaseChannel::Stable:
      url = QString::fromUtf8(kStableReleaseUrl);
      break;
    case ReleaseChannel::Beta:
    case ReleaseChannel::Nightly:
      url = QString::fromUtf8(kBetaReleaseUrl);
      break;
  }

  QNetworkRequest request(QUrl(url));
  request.setRawHeader("Accept", "application/vnd.github.v3+json");

  QNetworkReply* reply = networkManager_->get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply, channel]() {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
      emit networkError(reply->errorString());
      emit updateCheckCompleted(UpdateCheckStatus::NetworkError, ReleaseInfo{});
      return;
    }

    const QByteArray data = reply->readAll();
    ReleaseInfo release;
    parseReleaseInfo(data, release);
    release.channel = channel;

    // Update last check time
    setLastCheckTime(QDateTime::currentDateTime());

    // Compare versions
    const QVersionNumber currentVer(currentVersion());
    const QVersionNumber latestVer(release.version);

    if (latestVer > currentVer) {
      updateAvailable_ = true;
      latestRelease_ = release;
      emit updateAvailable(release);
    } else {
      updateAvailable_ = false;
      emit noUpdateAvailable();
    }

    emit updateCheckCompleted(UpdateCheckStatus::Success, release);
  });
}

void UpdateManager::parseReleaseInfo(const QByteArray& data, ReleaseInfo& out) const {
  QJsonDocument doc = QJsonDocument::fromJson(data);
  if (!doc.isArray()) {
    // Single release (latest endpoint)
    if (!doc.isObject()) {
      return;
    }

    const QJsonObject obj = doc.object();
    out.version = obj.value("tag_name").toString().remove(0, 1);  // Remove 'v' prefix
    out.url = obj.value("html_url").toString();
    out.changelog = obj.value("body").toString();
    out.releaseDate = QDateTime::fromString(obj.value("published_at").toString(), Qt::ISODate);
    return;
  }

  // Array of releases
  const QJsonArray releases = doc.array();
  if (releases.isEmpty()) {
    return;
  }

  // Find the most recent release matching the channel criteria
  for (const QJsonValue& v : releases) {
    const QJsonObject obj = v.toObject();
    const QString prerelease = obj.value("prerelease").toString().toLower();
    const bool isPrerelease = (prerelease == "true");

    if (obj.contains("draft") && obj.value("draft").toBool()) {
      continue;  // Skip drafts
    }

    // For stable channel, skip pre-releases
    if (isPrerelease) {
      continue;
    }

    out.version = obj.value("tag_name").toString().remove(0, 1);
    out.url = obj.value("html_url").toString();
    out.changelog = obj.value("body").toString();
    out.releaseDate = QDateTime::fromString(obj.value("published_at").toString(), Qt::ISODate);
    break;  // First matching release is the most recent
  }
}

void UpdateManager::setAutoCheckEnabled(bool enabled) {
  QSettings settings;
  settings.beginGroup(kSettingsGroup);
  settings.setValue(kAutoCheckKey, enabled);
  settings.endGroup();
}

bool UpdateManager::autoCheckEnabled() const {
  QSettings settings;
  settings.beginGroup(kSettingsGroup);
  const bool enabled = settings.value(kAutoCheckKey, true).toBool();
  settings.endGroup();
  return enabled;
}

void UpdateManager::setCheckIntervalDays(int days) {
  QSettings settings;
  settings.beginGroup(kSettingsGroup);
  settings.setValue(kCheckIntervalDaysKey, qBound(1, days, 365));
  settings.endGroup();
}

int UpdateManager::checkIntervalDays() const {
  QSettings settings;
  settings.beginGroup(kSettingsGroup);
  const int days = settings.value(kCheckIntervalDaysKey, kDefaultCheckIntervalDays).toInt();
  settings.endGroup();
  return qBound(1, days, 365);
}

void UpdateManager::setReleaseChannel(ReleaseChannel channel) {
  QSettings settings;
  settings.beginGroup(kSettingsGroup);
  settings.setValue(kReleaseChannelKey, static_cast<int>(channel));
  settings.endGroup();
}

UpdateManager::ReleaseChannel UpdateManager::releaseChannel() const {
  QSettings settings;
  settings.beginGroup(kSettingsGroup);
  const int channel = settings.value(kReleaseChannelKey, static_cast<int>(ReleaseChannel::Stable)).toInt();
  settings.endGroup();
  return static_cast<ReleaseChannel>(channel);
}

void UpdateManager::setLastCheckTime(const QDateTime& time) {
  QSettings settings;
  settings.beginGroup(kSettingsGroup);
  settings.setValue(kLastCheckTimeKey, time);
  settings.endGroup();
}

QDateTime UpdateManager::lastCheckTime() const {
  QSettings settings;
  settings.beginGroup(kSettingsGroup);
  const QVariant var = settings.value(kLastCheckTimeKey);
  settings.endGroup();
  return var.toDateTime();
}

QString UpdateManager::currentVersion() const {
  return QString::fromUtf8(kVersion);
}

bool UpdateManager::isUpdateAvailable() const {
  return updateAvailable_;
}

UpdateManager::ReleaseInfo UpdateManager::latestRelease() const {
  return latestRelease_;
}

void UpdateManager::saveSettings() {
  // Settings are saved individually in the setters
}

void UpdateManager::loadSettings() {
  // Settings are loaded individually in the getters
}
