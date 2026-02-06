#pragma once

#include <functional>
#include <QMap>
#include <QWidget>

class ArduinoCli;
class OutputWidget;

class QLineEdit;
class QComboBox;
class QCheckBox;
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

class LibraryManagerDialog final : public QWidget {
  Q_OBJECT

 public:
  explicit LibraryManagerDialog(ArduinoCli* arduinoCli,
                                OutputWidget* output,
                                QWidget* parent = nullptr);

  bool isBusy() const;

 public slots:
  void refresh();
  void cancel();
  void showSearchFor(const QString& query);

 signals:
  void librariesChanged();
  void busyChanged(bool busy);
  void includeLibraryRequested(QString libraryName, QStringList includes);
  void openLibraryExamplesRequested(QString libraryName);

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
  QPushButton* installedIncludeButton_ = nullptr;
  QPushButton* installedExamplesButton_ = nullptr;
  QPushButton* installedDepsButton_ = nullptr;
  QTableView* installedView_ = nullptr;
  QStandardItemModel* installedModel_ = nullptr;
  QTextBrowser* installedDetails_ = nullptr;

  // Search tab
  QLineEdit* searchEdit_ = nullptr;
  QPushButton* searchButton_ = nullptr;
  QPushButton* updateIndexButton_ = nullptr;
  QLabel* indexStatusLabel_ = nullptr;
  QComboBox* searchVersionCombo_ = nullptr;
  QCheckBox* installDepsCheck_ = nullptr;
  QPushButton* installButton_ = nullptr;
  QPushButton* searchDepsButton_ = nullptr;
  QTableView* searchView_ = nullptr;
  QStandardItemModel* searchModel_ = nullptr;
  QTextBrowser* searchDetails_ = nullptr;

  QTimer* searchDebounceTimer_ = nullptr;
  bool pendingAutoSearch_ = false;

  QProcess* process_ = nullptr;
  QByteArray processOutput_;
  bool busy_ = false;

  QMap<QString, QString> pinnedLibraryVersions_;

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

  QString selectedInstalledLibraryName() const;
  QString selectedSearchLibraryName() const;

  bool shouldAutoUpdateIndexNow() const;
  void updateIndexStatusLabel();
  void runUpdateIndex(bool automatic);
};
