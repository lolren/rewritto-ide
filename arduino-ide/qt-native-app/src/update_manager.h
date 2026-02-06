#pragma once

#include <QObject>
#include <QString>
#include <QVersionNumber>

class QNetworkAccessManager;
class QNetworkReply;

class UpdateManager : public QObject {
  Q_OBJECT

 public:
  enum class ReleaseChannel {
    Stable,
    Beta,
    Nightly,
  };

  enum class UpdateCheckStatus {
    Success,
    NetworkError,
    ParseError,
    NoUpdateAvailable,
    CheckDisabled,
  };

  struct ReleaseInfo {
    QString version;
    QString url;
    QString changelog;
    QDateTime releaseDate;
    ReleaseChannel channel;
  };

  explicit UpdateManager(QObject* parent = nullptr);
  ~UpdateManager() override;

  // Check for updates
  void checkForUpdates(ReleaseChannel channel = ReleaseChannel::Stable);

  // Settings
  void setAutoCheckEnabled(bool enabled);
  bool autoCheckEnabled() const;
  void setCheckIntervalDays(int days);
  int checkIntervalDays() const;
  void setReleaseChannel(ReleaseChannel channel);
  ReleaseChannel releaseChannel() const;
  void setLastCheckTime(const QDateTime& time);
  QDateTime lastCheckTime() const;

  // Version info
  QString currentVersion() const;
  bool isUpdateAvailable() const;
  ReleaseInfo latestRelease() const;

 signals:
  void updateCheckCompleted(UpdateCheckStatus status, const ReleaseInfo& release);
  void updateAvailable(const ReleaseInfo& release);
  void noUpdateAvailable();
  void networkError(const QString& error);

 private:
  void parseReleaseInfo(const QByteArray& data, ReleaseInfo& out) const;
  void saveSettings();
  void loadSettings();

  QNetworkAccessManager* networkManager_ = nullptr;
  ReleaseInfo latestRelease_;
  bool updateAvailable_ = false;
  static constexpr char kVersion[] = "1.0.0";  // Current application version
};

inline Q_DECLARE_METATYPE(UpdateManager::ReleaseChannel)
inline Q_DECLARE_METATYPE(UpdateManager::UpdateCheckStatus)
