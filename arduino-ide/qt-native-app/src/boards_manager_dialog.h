#pragma once

#include <functional>
#include <QMap>
#include <QWidget>

class ArduinoCli;
class OutputWidget;

class QLineEdit;
class QComboBox;
class QPushButton;
class QProcess;
class QProgressBar;
class QStandardItemModel;
class QTabWidget;
class QTableView;
class QLabel;
class QTextBrowser;
class QWidget;
class QTimer;
class PlatformFilterProxyModel;

class BoardsManagerDialog final : public QWidget {
  Q_OBJECT

 public:
  explicit BoardsManagerDialog(ArduinoCli* arduinoCli,
                               OutputWidget* output,
                               QWidget* parent = nullptr);

  bool isBusy() const;

 public slots:
  void refresh();
  void cancel();
  void showSearchFor(const QString& query);

 signals:
  void platformsChanged();
  void busyChanged(bool busy);

 private:
  ArduinoCli* arduinoCli_ = nullptr;
  OutputWidget* output_ = nullptr;

  QWidget* busyRow_ = nullptr;
  QLabel* busyLabel_ = nullptr;
  QProgressBar* busyBar_ = nullptr;
  QPushButton* busyCancelButton_ = nullptr;

  QTabWidget* tabs_ = nullptr;

  // Installed tab
  QPushButton* installedRefreshButton_ = nullptr;
  QPushButton* installedUpgradeButton_ = nullptr;
  QPushButton* installedUpgradeAllButton_ = nullptr;
  QPushButton* installedUninstallButton_ = nullptr;
  QPushButton* installedPinButton_ = nullptr;
  QComboBox* installedVersionCombo_ = nullptr;
  QPushButton* installedInstallButton_ = nullptr;
  QComboBox* installedShowCombo_ = nullptr;
  QComboBox* installedVendorCombo_ = nullptr;
  QComboBox* installedArchCombo_ = nullptr;
  QComboBox* installedTypeCombo_ = nullptr;
  QTableView* installedView_ = nullptr;
  QStandardItemModel* installedModel_ = nullptr;
  PlatformFilterProxyModel* installedProxy_ = nullptr;
  QTextBrowser* installedDetails_ = nullptr;

  // Search tab
  QLineEdit* searchEdit_ = nullptr;
  QPushButton* searchButton_ = nullptr;
  QPushButton* updateIndexButton_ = nullptr;
  QLabel* indexStatusLabel_ = nullptr;
  QComboBox* searchShowCombo_ = nullptr;
  QComboBox* searchVendorCombo_ = nullptr;
  QComboBox* searchArchCombo_ = nullptr;
  QComboBox* searchTypeCombo_ = nullptr;
  QComboBox* searchVersionCombo_ = nullptr;
  QPushButton* installButton_ = nullptr;
  QTableView* searchView_ = nullptr;
  QStandardItemModel* searchModel_ = nullptr;
  PlatformFilterProxyModel* searchProxy_ = nullptr;
  QTextBrowser* searchDetails_ = nullptr;

  QTimer* searchDebounceTimer_ = nullptr;
  bool pendingAutoSearch_ = false;

  QProcess* process_ = nullptr;
  QByteArray processOutput_;
  bool busy_ = false;

  QMap<QString, QString> pinnedPlatformVersions_;

  void buildUi();
  void wireSignals();

  void setBusy(bool busy);

  void runCommand(const QStringList& args,
                  bool expectJson,
                  bool streamOutput = false,
                  std::function<void(const QByteArray&)> onSuccess = {},
                  std::function<void(int exitCode, const QByteArray& out)> onFinished = {});

  void refreshInstalled();
  void runSearch();

  QString selectedInstalledPlatformId() const;
  QString selectedSearchPlatformId() const;
  QString selectedSearchLatestVersion() const;

  void rebuildInstalledFilters();
  void rebuildSearchFilters();
  void applyInstalledFilters();
  void applySearchFilters();

  bool shouldAutoUpdateIndexNow() const;
  void updateIndexStatusLabel();
  void runUpdateIndex(bool automatic);
};
