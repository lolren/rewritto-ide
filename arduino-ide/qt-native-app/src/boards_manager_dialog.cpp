#include "boards_manager_dialog.h"

#include "arduino_cli.h"
#include "index_update_policy.h"
#include "output_widget.h"
#include "platform_filter_proxy_model.h"

#include <QAbstractItemView>
#include <QBoxLayout>
#include <QComboBox>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QLabel>
#include <QLineEdit>
#include <QItemSelectionModel>
#include <QMessageBox>
#include <QPointer>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTabWidget>
#include <QTableView>
#include <QTextBrowser>
#include <QTimer>
#include <QVersionNumber>
#include <QSet>

namespace {
enum InstalledColumns { ColId = 0, ColInstalled, ColLatest, ColName, ColCount };
enum SearchColumns { SColId = 0, SColInstalled, SColLatest, SColName, SColCount };

constexpr int kRoleAvailableVersions = Qt::UserRole + 1;
constexpr int kRolePlatformJson = Qt::UserRole + 2;

constexpr auto kBoardsManagerSettingsGroup = "BoardsManager";
constexpr auto kCoreIndexLastSuccessUtcKey = "coreIndexLastSuccessUtc";
constexpr auto kCoreIndexLastAttemptUtcKey = "coreIndexLastAttemptUtc";
constexpr auto kCoreIndexLastErrorUtcKey = "coreIndexLastErrorUtc";
constexpr auto kCoreIndexLastErrorMessageKey = "coreIndexLastErrorMessage";
constexpr auto kPinnedPlatformVersionsKey = "pinnedPlatformVersionsJson";

QString platformNameFromReleases(const QJsonObject& platform,
                                 const QString& preferredVersion) {
  const QJsonObject releases = platform.value("releases").toObject();
  const QJsonObject rel = releases.value(preferredVersion).toObject();
  const QString name = rel.value("name").toString();
  if (!name.isEmpty()) {
    return name;
  }
  const QString latest = platform.value("latest_version").toString();
  return releases.value(latest).toObject().value("name").toString();
}

QStringList sortVersions(QStringList versions) {
  versions.removeAll(QString{});
  versions.removeDuplicates();
  std::sort(versions.begin(), versions.end(), [](const QString& a, const QString& b) {
    const QVersionNumber av = QVersionNumber::fromString(a);
    const QVersionNumber bv = QVersionNumber::fromString(b);
    if (!av.isNull() && !bv.isNull()) {
      return QVersionNumber::compare(av, bv) > 0;
    }
    if (av.isNull() != bv.isNull()) {
      return !av.isNull();
    }
    return a > b;
  });
  return versions;
}

QString htmlEscape(QString text) {
  text.replace('&', "&amp;");
  text.replace('<', "&lt;");
  text.replace('>', "&gt;");
  text.replace('"', "&quot;");
  return text;
}

QString truncateMessage(QString message, int maxChars) {
  message = message.trimmed();
  if (message.size() <= maxChars) {
    return message;
  }
  message.truncate(maxChars);
  message = message.trimmed();
  message += QStringLiteral("\n\u2026");
  return message;
}

QMap<QString, QString> loadPinnedPlatformVersions() {
  QMap<QString, QString> out;
  QSettings settings;
  settings.beginGroup(kBoardsManagerSettingsGroup);
  const QByteArray raw = settings.value(kPinnedPlatformVersionsKey).toByteArray();
  settings.endGroup();

  if (raw.isEmpty()) {
    return out;
  }
  const QJsonDocument doc = QJsonDocument::fromJson(raw);
  if (!doc.isObject()) {
    return out;
  }
  const QJsonObject obj = doc.object();
  for (auto it = obj.begin(); it != obj.end(); ++it) {
    const QString id = it.key().trimmed();
    const QString version = it.value().toString().trimmed();
    if (!id.isEmpty() && !version.isEmpty()) {
      out.insert(id, version);
    }
  }
  return out;
}

void storePinnedPlatformVersions(const QMap<QString, QString>& pinned) {
  QJsonObject obj;
  for (auto it = pinned.begin(); it != pinned.end(); ++it) {
    const QString id = it.key().trimmed();
    const QString version = it.value().trimmed();
    if (!id.isEmpty() && !version.isEmpty()) {
      obj.insert(id, version);
    }
  }
  QSettings settings;
  settings.beginGroup(kBoardsManagerSettingsGroup);
  settings.setValue(kPinnedPlatformVersionsKey,
                    QJsonDocument(obj).toJson(QJsonDocument::Compact));
  settings.endGroup();
}

QString formatPlatformDetails(const QJsonObject& p, const QString& pinnedVersion) {
  const QString id = p.value("id").toString().trimmed();
  const QString installed = p.value("installed_version").toString().trimmed();
  const QString latest = p.value("latest_version").toString().trimmed();
  const QString maintainer = p.value("maintainer").toString().trimmed();
  const QString email = p.value("email").toString().trimmed();
  const QString website = p.value("website").toString().trimmed();
  const bool indexed = p.value("indexed").toBool();

  QString html;
  html += QStringLiteral("<b>%1</b><br/>")
              .arg(htmlEscape(platformNameFromReleases(p, installed.isEmpty() ? latest : installed)));
  if (!id.isEmpty()) {
    html += QStringLiteral("<b>ID:</b> %1<br/>").arg(htmlEscape(id));
  }
  if (!installed.isEmpty()) {
    html += QStringLiteral("<b>Installed:</b> %1<br/>").arg(htmlEscape(installed));
  }
  if (!pinnedVersion.trimmed().isEmpty()) {
    html += QStringLiteral("<b>Pinned:</b> %1<br/>").arg(htmlEscape(pinnedVersion.trimmed()));
  }
  if (!latest.isEmpty()) {
    html += QStringLiteral("<b>Latest:</b> %1<br/>").arg(htmlEscape(latest));
  }
  html += QStringLiteral("<b>Indexed:</b> %1<br/>").arg(indexed ? "true" : "false");

  if (!maintainer.isEmpty()) {
    html += QStringLiteral("<b>Maintainer:</b> %1<br/>").arg(htmlEscape(maintainer));
  }
  if (!email.isEmpty()) {
    html += QStringLiteral("<b>Email:</b> %1<br/>").arg(htmlEscape(email));
  }
  if (!website.isEmpty()) {
    html += QStringLiteral("<b>Website:</b> <a href=\"%1\">%2</a><br/>")
                .arg(htmlEscape(website), htmlEscape(website));
  }

  const QStringList versions = sortVersions(p.value("releases").toObject().keys());
  if (!versions.isEmpty()) {
    html += QStringLiteral("<br/><b>Available versions:</b> %1<br/>")
                .arg(QString::number(versions.size()));
    const int maxShow = 15;
    QStringList shown = versions.mid(0, maxShow);
    html += QStringLiteral("<code>%1</code>").arg(htmlEscape(shown.join(", ")));
    if (versions.size() > maxShow) {
      html += QStringLiteral("<br/>(%1 more\u2026)")
                  .arg(QString::number(versions.size() - maxShow));
    }
  }

  return html;
}

QString platformVendor(const QString& platformId) {
  const int colon = platformId.indexOf(':');
  return (colon >= 0 ? platformId.left(colon) : platformId).trimmed();
}

QString platformArch(const QString& platformId) {
  const int colon = platformId.indexOf(':');
  return (colon >= 0 ? platformId.mid(colon + 1) : QString{}).trimmed();
}

QStringList platformTypesForVersion(const QJsonObject& platform, QString version) {
  version = version.trimmed();
  if (version.isEmpty()) {
    version = platform.value(QStringLiteral("latest_version")).toString().trimmed();
  }
  if (version.isEmpty()) {
    return {};
  }

  const QJsonObject rel =
      platform.value(QStringLiteral("releases")).toObject().value(version).toObject();
  const QJsonArray types = rel.value(QStringLiteral("types")).toArray();

  QStringList out;
  out.reserve(types.size());
  for (const QJsonValue& v : types) {
    const QString t = v.toString().trimmed();
    if (!t.isEmpty()) {
      out << t;
    }
  }
  out.removeDuplicates();
  return out;
}

QStringList setToSortedList(QSet<QString> set) {
  set.remove(QString{});
  QStringList out = set.values();
  std::sort(out.begin(), out.end(), [](const QString& a, const QString& b) {
    return QString::localeAwareCompare(a, b) < 0;
  });
  return out;
}
}  // namespace

BoardsManagerDialog::BoardsManagerDialog(ArduinoCli* arduinoCli,
                                         OutputWidget* output,
                                         QWidget* parent)
    : QWidget(parent), arduinoCli_(arduinoCli), output_(output) {
  setWindowTitle("Boards Manager");
  resize(900, 520);
  buildUi();
  pinnedPlatformVersions_ = loadPinnedPlatformVersions();
  wireSignals();
}

bool BoardsManagerDialog::isBusy() const {
  return busy_;
}

void BoardsManagerDialog::refresh() {
  updateIndexStatusLabel();
  if (shouldAutoUpdateIndexNow()) {
    runUpdateIndex(true);
    return;
  }
  refreshInstalled();
}

void BoardsManagerDialog::cancel() {
  if (!process_) {
    return;
  }

  process_->disconnect();
  if (process_->state() != QProcess::NotRunning) {
    process_->kill();
    process_->waitForFinished(250);
  }
  process_->deleteLater();
  process_ = nullptr;
  processOutput_.clear();
  if (tabs_) {
    tabs_->setEnabled(true);
  }
  if (output_) {
    output_->appendLine("[Boards Manager] Canceled.");
  }
  setBusy(false);
}

void BoardsManagerDialog::showSearchFor(const QString& query) {
  const QString q = query.trimmed();
  if (tabs_) {
    tabs_->setCurrentIndex(1);
  }
  if (searchEdit_) {
    searchEdit_->setText(q);
    searchEdit_->setFocus();
    searchEdit_->selectAll();
  }

  if (searchDebounceTimer_) {
    searchDebounceTimer_->stop();
  }

  if (isBusy()) {
    QPointer<BoardsManagerDialog> self(this);
    auto* conn = new QMetaObject::Connection;
    *conn = connect(this, &BoardsManagerDialog::busyChanged, this,
                    [self, conn](bool busy) {
                      if (busy || !self) {
                        return;
                      }
                      QObject::disconnect(*conn);
                      delete conn;
                      self->runSearch();
                    });
    return;
  }

  runSearch();
}

void BoardsManagerDialog::buildUi() {
  busyRow_ = new QWidget(this);
  {
    auto* h = new QHBoxLayout(busyRow_);
    h->setContentsMargins(0, 0, 0, 0);
    busyLabel_ = new QLabel(tr("Working\u2026"), busyRow_);
    busyBar_ = new QProgressBar(busyRow_);
    busyBar_->setRange(0, 0);
    busyBar_->setTextVisible(false);
    busyBar_->setFixedHeight(14);
    busyCancelButton_ = new QPushButton(tr("Cancel"), busyRow_);
    h->addWidget(busyLabel_);
    h->addWidget(busyBar_, 1);
    h->addWidget(busyCancelButton_);
    connect(busyCancelButton_, &QPushButton::clicked, this,
            [this] { cancel(); });
    busyRow_->hide();
  }

  tabs_ = new QTabWidget(this);

  // Installed tab
  {
    auto* page = new QWidget(tabs_);
    auto* v = new QVBoxLayout(page);

    auto* header = new QHBoxLayout();
    installedRefreshButton_ = new QPushButton("Refresh", page);
    installedUpgradeButton_ = new QPushButton("Upgrade Selected", page);
    installedUpgradeAllButton_ = new QPushButton("Upgrade All", page);
    installedUninstallButton_ = new QPushButton("Uninstall Selected", page);
    installedPinButton_ = new QPushButton("Pin Version", page);
    installedPinButton_->setCheckable(true);
    installedPinButton_->setToolTip(tr("Pin the installed version to prevent upgrades"));
    header->addWidget(installedRefreshButton_);
    header->addWidget(installedUpgradeButton_);
    header->addWidget(installedUpgradeAllButton_);
    header->addWidget(installedUninstallButton_);
    header->addWidget(installedPinButton_);
    header->addSpacing(12);
    header->addWidget(new QLabel("Version:", page));
    installedVersionCombo_ = new QComboBox(page);
    installedVersionCombo_->setMinimumWidth(140);
    installedVersionCombo_->setEnabled(false);
    header->addWidget(installedVersionCombo_);
    installedInstallButton_ = new QPushButton("Install Version", page);
    installedInstallButton_->setEnabled(false);
    header->addWidget(installedInstallButton_);
    header->addStretch(1);
    v->addLayout(header);

    auto* filters = new QHBoxLayout();
    filters->addWidget(new QLabel(tr("Show:"), page));
    installedShowCombo_ = new QComboBox(page);
    installedShowCombo_->addItem(tr("All"),
                                 static_cast<int>(PlatformFilterProxyModel::ShowMode::All));
    installedShowCombo_->addItem(
        tr("Updatable"),
        static_cast<int>(PlatformFilterProxyModel::ShowMode::Updatable));
    filters->addWidget(installedShowCombo_);
    filters->addSpacing(12);
    filters->addWidget(new QLabel(tr("Vendor:"), page));
    installedVendorCombo_ = new QComboBox(page);
    installedVendorCombo_->setMinimumWidth(140);
    filters->addWidget(installedVendorCombo_);
    filters->addSpacing(12);
    filters->addWidget(new QLabel(tr("Architecture:"), page));
    installedArchCombo_ = new QComboBox(page);
    installedArchCombo_->setMinimumWidth(140);
    filters->addWidget(installedArchCombo_);
    filters->addSpacing(12);
    filters->addWidget(new QLabel(tr("Type:"), page));
    installedTypeCombo_ = new QComboBox(page);
    installedTypeCombo_->setMinimumWidth(140);
    filters->addWidget(installedTypeCombo_);
    filters->addStretch(1);
    v->addLayout(filters);

    installedModel_ = new QStandardItemModel(0, ColCount, page);
    installedModel_->setHorizontalHeaderLabels(
        {"ID", "Installed", "Latest", "Name"});

    installedProxy_ = new PlatformFilterProxyModel(page);
    installedProxy_->setIdColumn(ColId);
    installedProxy_->setInstalledColumn(ColInstalled);
    installedProxy_->setLatestColumn(ColLatest);
    installedProxy_->setSourceModel(installedModel_);

    installedView_ = new QTableView(page);
    installedView_->setModel(installedProxy_);
    installedView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    installedView_->setSelectionMode(QAbstractItemView::SingleSelection);
    installedView_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    installedView_->horizontalHeader()->setStretchLastSection(true);
    installedView_->verticalHeader()->setVisible(false);
    installedDetails_ = new QTextBrowser(page);
    installedDetails_->setOpenExternalLinks(true);
    installedDetails_->setMinimumHeight(120);

    auto* split = new QSplitter(Qt::Vertical, page);
    split->addWidget(installedView_);
    split->addWidget(installedDetails_);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 1);
    v->addWidget(split, 1);

    tabs_->addTab(page, "Installed");
  }

  // Search tab
  {
    auto* page = new QWidget(tabs_);
    auto* v = new QVBoxLayout(page);

    auto* header = new QHBoxLayout();
    header->addWidget(new QLabel("Search:", page));
    searchEdit_ = new QLineEdit(page);
    searchEdit_->setPlaceholderText("e.g. arduino, esp32, rp2040");
    searchButton_ = new QPushButton("Search", page);
    updateIndexButton_ = new QPushButton("Update Index", page);
    installButton_ = new QPushButton("Install", page);
    installButton_->setEnabled(false);
    header->addWidget(searchEdit_, 1);
    header->addWidget(searchButton_);
    header->addWidget(updateIndexButton_);
    indexStatusLabel_ = new QLabel(page);
    indexStatusLabel_->setText(tr("Index: (unknown)"));
    indexStatusLabel_->setMinimumWidth(160);
    header->addWidget(indexStatusLabel_);
    header->addWidget(new QLabel("Version:", page));
    searchVersionCombo_ = new QComboBox(page);
    searchVersionCombo_->setMinimumWidth(140);
    searchVersionCombo_->setEnabled(false);
    header->addWidget(searchVersionCombo_);
    header->addWidget(installButton_);
    v->addLayout(header);

    auto* filters = new QHBoxLayout();
    filters->addWidget(new QLabel(tr("Show:"), page));
    searchShowCombo_ = new QComboBox(page);
    searchShowCombo_->addItem(tr("All"),
                              static_cast<int>(PlatformFilterProxyModel::ShowMode::All));
    searchShowCombo_->addItem(
        tr("Installed"),
        static_cast<int>(PlatformFilterProxyModel::ShowMode::Installed));
    searchShowCombo_->addItem(
        tr("Updatable"),
        static_cast<int>(PlatformFilterProxyModel::ShowMode::Updatable));
    searchShowCombo_->addItem(
        tr("Not installed"),
        static_cast<int>(PlatformFilterProxyModel::ShowMode::NotInstalled));
    filters->addWidget(searchShowCombo_);
    filters->addSpacing(12);
    filters->addWidget(new QLabel(tr("Vendor:"), page));
    searchVendorCombo_ = new QComboBox(page);
    searchVendorCombo_->setMinimumWidth(140);
    filters->addWidget(searchVendorCombo_);
    filters->addSpacing(12);
    filters->addWidget(new QLabel(tr("Architecture:"), page));
    searchArchCombo_ = new QComboBox(page);
    searchArchCombo_->setMinimumWidth(140);
    filters->addWidget(searchArchCombo_);
    filters->addSpacing(12);
    filters->addWidget(new QLabel(tr("Type:"), page));
    searchTypeCombo_ = new QComboBox(page);
    searchTypeCombo_->setMinimumWidth(140);
    filters->addWidget(searchTypeCombo_);
    filters->addStretch(1);
    v->addLayout(filters);

    searchModel_ = new QStandardItemModel(0, SColCount, page);
    searchModel_->setHorizontalHeaderLabels({"ID", "Installed", "Latest", "Name"});

    searchProxy_ = new PlatformFilterProxyModel(page);
    searchProxy_->setIdColumn(SColId);
    searchProxy_->setInstalledColumn(SColInstalled);
    searchProxy_->setLatestColumn(SColLatest);
    searchProxy_->setSourceModel(searchModel_);

    searchView_ = new QTableView(page);
    searchView_->setModel(searchProxy_);
    searchView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    searchView_->setSelectionMode(QAbstractItemView::SingleSelection);
    searchView_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    searchView_->horizontalHeader()->setStretchLastSection(true);
    searchView_->verticalHeader()->setVisible(false);
    searchDetails_ = new QTextBrowser(page);
    searchDetails_->setOpenExternalLinks(true);
    searchDetails_->setMinimumHeight(120);

    auto* split = new QSplitter(Qt::Vertical, page);
    split->addWidget(searchView_);
    split->addWidget(searchDetails_);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 1);
    v->addWidget(split, 1);

    tabs_->addTab(page, "Search");
  }

  auto* layout = new QVBoxLayout(this);
  layout->addWidget(busyRow_);
  layout->addWidget(tabs_);

  searchDebounceTimer_ = new QTimer(this);
  searchDebounceTimer_->setSingleShot(true);
  searchDebounceTimer_->setInterval(250);
}

void BoardsManagerDialog::wireSignals() {
  auto updateInstalledSelectionUi = [this] {
    const QString id = selectedInstalledPlatformId();
    if (!installedInstallButton_ || !installedVersionCombo_ ||
        !installedUpgradeButton_ || !installedUninstallButton_ || !installedPinButton_) {
      return;
    }
    const bool hasSelection = !id.isEmpty();
    const QString pinnedVersion = pinnedPlatformVersions_.value(id).trimmed();
    const bool pinned = hasSelection && !pinnedVersion.isEmpty();

    installedUpgradeButton_->setEnabled(hasSelection && !pinned);
    installedUninstallButton_->setEnabled(hasSelection);
    installedInstallButton_->setEnabled(false);
    installedVersionCombo_->setEnabled(false);
    installedVersionCombo_->clear();
    installedPinButton_->setEnabled(hasSelection);
    installedPinButton_->blockSignals(true);
    installedPinButton_->setChecked(pinned);
    installedPinButton_->setText(pinned ? tr("Unpin") : tr("Pin Version"));
    installedPinButton_->blockSignals(false);
    if (installedDetails_) {
      installedDetails_->clear();
    }

    if (!hasSelection || !installedModel_ || !installedView_) {
      return;
    }

    QModelIndex idx = installedView_->currentIndex();
    if (!idx.isValid()) {
      return;
    }
    if (installedProxy_) {
      idx = installedProxy_->mapToSource(idx);
    }
    if (!idx.isValid()) {
      return;
    }
    const QModelIndex idIdx = installedModel_->index(idx.row(), ColId);
    const QJsonObject platform =
        installedModel_->data(idIdx, kRolePlatformJson).value<QJsonObject>();
    if (installedDetails_) {
      installedDetails_->setHtml(platform.isEmpty() ? QString{} : formatPlatformDetails(platform, pinnedVersion));
    }
    const QStringList versions =
        installedModel_->data(idIdx, kRoleAvailableVersions).toStringList();
    if (versions.isEmpty()) {
      return;
    }
    installedVersionCombo_->addItems(versions);
    installedVersionCombo_->setEnabled(!pinned);
    installedInstallButton_->setEnabled(!pinned);

    const QString installed = installedModel_
                                  ->data(installedModel_->index(idx.row(), ColInstalled),
                                         Qt::DisplayRole)
                                  .toString()
                                  .trimmed();
    const int vIdx = !installed.isEmpty() ? installedVersionCombo_->findText(installed)
                                          : -1;
    if (vIdx >= 0) {
      installedVersionCombo_->setCurrentIndex(vIdx);
    }
  };

  auto updateSearchSelectionUi = [this] {
    const QString id = selectedSearchPlatformId();
    if (!installButton_ || !searchVersionCombo_) {
      return;
    }
    installButton_->setEnabled(false);
    searchVersionCombo_->setEnabled(false);
    searchVersionCombo_->clear();
    if (searchDetails_) {
      searchDetails_->clear();
    }

    if (id.isEmpty() || !searchModel_ || !searchView_) {
      return;
    }

    QModelIndex idx = searchView_->currentIndex();
    if (!idx.isValid()) {
      return;
    }
    if (searchProxy_) {
      idx = searchProxy_->mapToSource(idx);
    }
    if (!idx.isValid()) {
      return;
    }
    const QModelIndex idIdx = searchModel_->index(idx.row(), SColId);
    const QJsonObject platform =
        searchModel_->data(idIdx, kRolePlatformJson).value<QJsonObject>();
    if (searchDetails_) {
      searchDetails_->setHtml(
          platform.isEmpty() ? QString{} : formatPlatformDetails(platform, QString{}));
    }
    const QStringList versions =
        searchModel_->data(idIdx, kRoleAvailableVersions).toStringList();
    if (versions.isEmpty()) {
      return;
    }
    searchVersionCombo_->addItems(versions);
    searchVersionCombo_->setEnabled(true);
    installButton_->setEnabled(true);

    const QString latest = searchModel_
                               ->data(searchModel_->index(idx.row(), SColLatest),
                                     Qt::DisplayRole)
                               .toString()
                               .trimmed();
    const int vIdx = !latest.isEmpty() ? searchVersionCombo_->findText(latest) : -1;
    if (vIdx >= 0) {
      searchVersionCombo_->setCurrentIndex(vIdx);
    }
  };

  connect(installedRefreshButton_, &QPushButton::clicked, this,
          [this] { refreshInstalled(); });
  connect(installedShowCombo_, &QComboBox::currentIndexChanged, this,
          [this](int) { applyInstalledFilters(); });
  connect(installedVendorCombo_, &QComboBox::currentIndexChanged, this,
          [this](int) { applyInstalledFilters(); });
  connect(installedArchCombo_, &QComboBox::currentIndexChanged, this,
          [this](int) { applyInstalledFilters(); });
  connect(installedTypeCombo_, &QComboBox::currentIndexChanged, this,
          [this](int) { applyInstalledFilters(); });
  connect(installedUpgradeAllButton_, &QPushButton::clicked, this, [this] {
    if (!installedModel_) {
      return;
    }

    QStringList platforms;
    platforms.reserve(installedModel_->rowCount());
    for (int row = 0; row < installedModel_->rowCount(); ++row) {
      const QString id =
          installedModel_->data(installedModel_->index(row, ColId)).toString().trimmed();
      if (id.isEmpty()) {
        continue;
      }
      if (pinnedPlatformVersions_.contains(id)) {
        continue;
      }
      platforms << id;
    }
    platforms.removeAll(QString{});
    platforms.removeDuplicates();

    if (platforms.isEmpty()) {
      if (output_) {
        output_->appendLine("[Boards Manager] No platforms to upgrade (all pinned).");
      }
      return;
    }

    QStringList args = {QStringLiteral("core"), QStringLiteral("upgrade")};
    args.append(platforms);
    runCommand(args, false, false,
               [this](const QByteArray&) {
                 emit platformsChanged();
                 refreshInstalled();
               });
  });
  connect(searchButton_, &QPushButton::clicked, this, [this] { runSearch(); });
  connect(searchShowCombo_, &QComboBox::currentIndexChanged, this,
          [this](int) { applySearchFilters(); });
  connect(searchVendorCombo_, &QComboBox::currentIndexChanged, this,
          [this](int) { applySearchFilters(); });
  connect(searchArchCombo_, &QComboBox::currentIndexChanged, this,
          [this](int) { applySearchFilters(); });
  connect(searchTypeCombo_, &QComboBox::currentIndexChanged, this,
          [this](int) { applySearchFilters(); });
  connect(searchEdit_, &QLineEdit::returnPressed, this,
          [this] { runSearch(); });
  if (searchDebounceTimer_) {
    connect(searchDebounceTimer_, &QTimer::timeout, this, [this] {
      if (!tabs_ || !searchEdit_ || tabs_->currentIndex() != 1) {
        return;
      }
      if (searchEdit_->text().trimmed().size() < 2) {
        return;
      }
      if (isBusy()) {
        pendingAutoSearch_ = true;
        return;
      }
      runSearch();
    });
  }
  if (searchEdit_) {
    connect(searchEdit_, &QLineEdit::textChanged, this, [this](const QString& text) {
      if (!tabs_ || tabs_->currentIndex() != 1) {
        return;
      }
      const QString q = text.trimmed();
      if (q.size() < 2) {
        pendingAutoSearch_ = false;
        if (searchDebounceTimer_) {
          searchDebounceTimer_->stop();
        }
        if (installButton_) {
          installButton_->setEnabled(false);
        }
        if (searchVersionCombo_) {
          searchVersionCombo_->setEnabled(false);
          searchVersionCombo_->clear();
        }
        if (searchDetails_) {
          searchDetails_->clear();
        }
        if (searchModel_) {
          searchModel_->removeRows(0, searchModel_->rowCount());
        }
        rebuildSearchFilters();
        applySearchFilters();
        return;
      }
      if (searchDebounceTimer_) {
        searchDebounceTimer_->start();
      }
    });
  }
  if (tabs_) {
    connect(tabs_, &QTabWidget::currentChanged, this, [this](int idx) {
      if (idx != 1 || !searchEdit_ || !searchDebounceTimer_) {
        return;
      }
      if (searchEdit_->text().trimmed().size() < 2) {
        return;
      }
      if (isBusy()) {
        pendingAutoSearch_ = true;
        return;
      }
      searchDebounceTimer_->start();
    });
  }

  connect(updateIndexButton_, &QPushButton::clicked, this, [this] {
    runUpdateIndex(false);
  });

  if (searchView_ && searchView_->selectionModel()) {
    connect(searchView_->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [updateSearchSelectionUi](const QModelIndex&, const QModelIndex&) {
              updateSearchSelectionUi();
            });
  }
  if (searchView_) {
    connect(searchView_, &QTableView::doubleClicked, this,
            [this](const QModelIndex&) {
              if (installButton_) {
                installButton_->click();
              }
            });
  }
  if (installedView_ && installedView_->selectionModel()) {
    connect(installedView_->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [updateInstalledSelectionUi](const QModelIndex&, const QModelIndex&) {
              updateInstalledSelectionUi();
            });
  }

  connect(installButton_, &QPushButton::clicked, this, [this] {
    const QString id = selectedSearchPlatformId();
    const QString version = searchVersionCombo_ ? searchVersionCombo_->currentText().trimmed()
                                                : selectedSearchLatestVersion();
    if (id.isEmpty() || version.isEmpty()) {
      return;
    }
    runCommand({QStringLiteral("core"), QStringLiteral("install"),
                QString("%1@%2").arg(id, version)},
               false, false, [this](const QByteArray&) {
                 emit platformsChanged();
                 refreshInstalled();
               });
  });

  connect(installedUninstallButton_, &QPushButton::clicked, this, [this] {
    const QString id = selectedInstalledPlatformId();
    if (id.isEmpty()) {
      return;
    }
    runCommand({QStringLiteral("core"), QStringLiteral("uninstall"), id}, false, false,
               [this, id](const QByteArray&) {
                 if (pinnedPlatformVersions_.remove(id) > 0) {
                   storePinnedPlatformVersions(pinnedPlatformVersions_);
                 }
                 emit platformsChanged();
                 refreshInstalled();
               });
  });

  connect(installedUpgradeButton_, &QPushButton::clicked, this, [this] {
    const QString id = selectedInstalledPlatformId();
    if (id.isEmpty()) {
      return;
    }
    if (pinnedPlatformVersions_.contains(id)) {
      if (output_) {
        output_->appendLine("[Boards Manager] Platform is pinned; unpin to upgrade.");
      }
      return;
    }
    runCommand({QStringLiteral("core"), QStringLiteral("upgrade"), id}, false, false,
               [this](const QByteArray&) {
                 emit platformsChanged();
                 refreshInstalled();
               });
  });

  connect(installedInstallButton_, &QPushButton::clicked, this, [this] {
    const QString id = selectedInstalledPlatformId();
    const QString version = installedVersionCombo_ ? installedVersionCombo_->currentText().trimmed()
                                                   : QString{};
    if (id.isEmpty() || version.isEmpty()) {
      return;
    }
    if (pinnedPlatformVersions_.contains(id)) {
      if (output_) {
        output_->appendLine("[Boards Manager] Platform is pinned; unpin to change version.");
      }
      return;
    }
    runCommand({QStringLiteral("core"), QStringLiteral("install"),
                QString("%1@%2").arg(id, version)},
               false, false, [this](const QByteArray&) {
                 emit platformsChanged();
                 refreshInstalled();
               });
  });

  connect(installedPinButton_, &QPushButton::toggled, this,
          [this, updateInstalledSelectionUi](bool checked) {
            if (!installedModel_ || !installedView_) {
              return;
            }
            QModelIndex idx = installedView_->currentIndex();
            if (!idx.isValid()) {
              return;
            }
            if (installedProxy_) {
              idx = installedProxy_->mapToSource(idx);
            }
            if (!idx.isValid()) {
              return;
            }

            const QString id = selectedInstalledPlatformId();
            if (id.isEmpty()) {
              return;
            }
            const QString installed =
                installedModel_->data(installedModel_->index(idx.row(), ColInstalled)).toString().trimmed();
            if (checked) {
              if (installed.isEmpty()) {
                return;
              }
              pinnedPlatformVersions_.insert(id, installed);
            } else {
              pinnedPlatformVersions_.remove(id);
            }
            storePinnedPlatformVersions(pinnedPlatformVersions_);
            updateInstalledSelectionUi();
          });

  updateInstalledSelectionUi();
  updateSearchSelectionUi();

  rebuildInstalledFilters();
  rebuildSearchFilters();
  applyInstalledFilters();
  applySearchFilters();
}

void BoardsManagerDialog::setBusy(bool busy) {
  if (busy_ == busy) {
    return;
  }
  busy_ = busy;
  if (busyRow_) {
    busyRow_->setVisible(busy_);
  }
  emit busyChanged(busy_);

  if (!busy_ && pendingAutoSearch_) {
    pendingAutoSearch_ = false;
    if (tabs_ && tabs_->currentIndex() == 1 && searchEdit_ &&
        searchEdit_->text().trimmed().size() >= 2) {
      if (searchDebounceTimer_) {
        searchDebounceTimer_->start();
      } else {
        runSearch();
      }
    }
  }
}

void BoardsManagerDialog::runCommand(
    const QStringList& args,
    bool expectJson,
    bool streamOutput,
    std::function<void(const QByteArray&)> onSuccess,
    std::function<void(int exitCode, const QByteArray& out)> onFinished) {
  if (process_) {
    return;
  }
  process_ = new QProcess(this);
  process_->setProcessChannelMode(QProcess::MergedChannels);
  processOutput_.clear();

  const QString program = arduinoCli_->arduinoCliPath();

  if (busyLabel_) {
    busyLabel_->setText(tr("Running: %1").arg(args.join(' ')));
  }

  auto setUiEnabled = [this](bool enabled) {
    tabs_->setEnabled(enabled);
  };
  setUiEnabled(false);
  setBusy(true);

  connect(process_, &QProcess::readyRead, this, [this, streamOutput] {
    const QByteArray chunk = process_->readAll();
    processOutput_.append(chunk);
    if (streamOutput && output_) {
      output_->appendText(QString::fromLocal8Bit(chunk));
    }
  });
  connect(process_, &QProcess::finished, this,
          [this, expectJson, onSuccess, onFinished, setUiEnabled](
              int exitCode, QProcess::ExitStatus) {
            const QByteArray out = processOutput_;
            process_->deleteLater();
            process_ = nullptr;
            setUiEnabled(true);
            setBusy(false);

            bool ok = exitCode == 0;
            if (ok && expectJson) {
              const QJsonDocument doc = QJsonDocument::fromJson(out);
              ok = !doc.isNull();
            }
            if (ok && onSuccess) {
              onSuccess(out);
            }
            if (onFinished) {
              onFinished(exitCode, out);
            }
          });

  const QStringList fullArgs =
      arduinoCli_ ? arduinoCli_->withGlobalFlags(args) : args;
  process_->start(program, fullArgs);
}

void BoardsManagerDialog::refreshInstalled() {
  runCommand({QStringLiteral("core"), QStringLiteral("list"), QStringLiteral("--json")},
             true, false, [this](const QByteArray& out) {
               const QJsonDocument doc = QJsonDocument::fromJson(out);
               if (!doc.isObject()) {
                 return;
               }
               const QJsonArray platforms =
                   doc.object().value("platforms").toArray();

               installedModel_->removeRows(0, installedModel_->rowCount());
               QSet<QString> installedIds;

               for (const QJsonValue& v : platforms) {
                 const QJsonObject p = v.toObject();
                 const QString id = p.value("id").toString();
                 const QString installed = p.value("installed_version").toString();
                 const QString latest = p.value("latest_version").toString();
                 const QString name = platformNameFromReleases(p, installed);
                 QStringList versions = sortVersions(p.value("releases").toObject().keys());

                 QList<QStandardItem*> row;
                 auto* idItem = new QStandardItem(id);
                 idItem->setData(versions, kRoleAvailableVersions);
                 idItem->setData(p, kRolePlatformJson);
                 row << idItem << new QStandardItem(installed)
                     << new QStandardItem(latest) << new QStandardItem(name);
                 installedModel_->appendRow(row);
                 installedIds.insert(id);
               }

               bool changed = false;
               for (auto it = pinnedPlatformVersions_.begin(); it != pinnedPlatformVersions_.end();) {
                 if (!installedIds.contains(it.key())) {
                   it = pinnedPlatformVersions_.erase(it);
                   changed = true;
                 } else {
                   ++it;
                 }
               }
               if (changed) {
                 storePinnedPlatformVersions(pinnedPlatformVersions_);
               }

               rebuildInstalledFilters();
               applyInstalledFilters();

               if (installedView_) {
                 installedView_->resizeColumnsToContents();
                 if (installedView_->model() && installedView_->model()->rowCount() > 0) {
                   installedView_->setCurrentIndex(installedView_->model()->index(0, ColId));
                 }
               }
             });
}

void BoardsManagerDialog::runSearch() {
  if (isBusy()) {
    pendingAutoSearch_ = true;
    return;
  }

  const QString q = searchEdit_->text().trimmed();
  QStringList args = {QStringLiteral("core"), QStringLiteral("search")};
  if (!q.isEmpty()) {
    args << q;
  }
  args << QStringLiteral("--json");

  runCommand(args, true, false, [this](const QByteArray& out) {
    const QJsonDocument doc = QJsonDocument::fromJson(out);
    if (!doc.isObject()) {
      return;
    }
    const QJsonArray platforms = doc.object().value("platforms").toArray();

    searchModel_->removeRows(0, searchModel_->rowCount());
    for (const QJsonValue& v : platforms) {
      const QJsonObject p = v.toObject();
      const QString id = p.value("id").toString();
      const QString installed = p.value("installed_version").toString();
      const QString latest = p.value("latest_version").toString();
      const QString name =
          platformNameFromReleases(p, latest.isEmpty() ? installed : latest);
      QStringList versions = sortVersions(p.value("releases").toObject().keys());

      QList<QStandardItem*> row;
      auto* idItem = new QStandardItem(id);
      idItem->setData(versions, kRoleAvailableVersions);
      idItem->setData(p, kRolePlatformJson);
      row << idItem << new QStandardItem(installed)
          << new QStandardItem(latest) << new QStandardItem(name);
      searchModel_->appendRow(row);
    }

    rebuildSearchFilters();
    applySearchFilters();

    if (searchView_) {
      searchView_->resizeColumnsToContents();
      if (searchView_->model() && searchView_->model()->rowCount() > 0) {
        searchView_->setCurrentIndex(searchView_->model()->index(0, SColId));
      }
    }
  });
}

QString BoardsManagerDialog::selectedInstalledPlatformId() const {
  if (!installedView_ || !installedModel_) {
    return {};
  }
  QModelIndex idx = installedView_->currentIndex();
  if (!idx.isValid()) {
    return {};
  }
  if (installedProxy_) {
    idx = installedProxy_->mapToSource(idx);
  }
  if (!idx.isValid()) {
    return {};
  }
  const QModelIndex idIdx = installedModel_->index(idx.row(), ColId);
  return installedModel_->data(idIdx).toString();
}

QString BoardsManagerDialog::selectedSearchPlatformId() const {
  if (!searchView_ || !searchModel_) {
    return {};
  }
  QModelIndex idx = searchView_->currentIndex();
  if (!idx.isValid()) {
    return {};
  }
  if (searchProxy_) {
    idx = searchProxy_->mapToSource(idx);
  }
  if (!idx.isValid()) {
    return {};
  }
  const QModelIndex idIdx = searchModel_->index(idx.row(), SColId);
  return searchModel_->data(idIdx).toString();
}

QString BoardsManagerDialog::selectedSearchLatestVersion() const {
  if (!searchView_ || !searchModel_) {
    return {};
  }
  QModelIndex idx = searchView_->currentIndex();
  if (!idx.isValid()) {
    return {};
  }
  if (searchProxy_) {
    idx = searchProxy_->mapToSource(idx);
  }
  if (!idx.isValid()) {
    return {};
  }
  const QModelIndex vIdx = searchModel_->index(idx.row(), SColLatest);
  return searchModel_->data(vIdx).toString();
}

void BoardsManagerDialog::rebuildInstalledFilters() {
  if (!installedModel_ || !installedVendorCombo_ || !installedArchCombo_ ||
      !installedTypeCombo_) {
    return;
  }

  const QString selectedVendor = installedVendorCombo_->currentData().toString();
  const QString selectedArch = installedArchCombo_->currentData().toString();
  const QString selectedType = installedTypeCombo_->currentData().toString();

  QSet<QString> vendors;
  QSet<QString> archs;
  QSet<QString> types;
  for (int row = 0; row < installedModel_->rowCount(); ++row) {
    const QString id = installedModel_->data(installedModel_->index(row, ColId)).toString();
    vendors.insert(platformVendor(id));
    archs.insert(platformArch(id));

    const QString installed =
        installedModel_->data(installedModel_->index(row, ColInstalled)).toString();
    const QString latest =
        installedModel_->data(installedModel_->index(row, ColLatest)).toString();
    const QString version = !installed.trimmed().isEmpty() ? installed : latest;

    const QJsonObject p =
        installedModel_->data(installedModel_->index(row, ColId), kRolePlatformJson)
            .value<QJsonObject>();
    const QStringList rowTypes = platformTypesForVersion(p, version);
    for (const QString& t : rowTypes) {
      types.insert(t);
    }
  }

  const QStringList vendorList = setToSortedList(std::move(vendors));
  const QStringList archList = setToSortedList(std::move(archs));
  const QStringList typeList = setToSortedList(std::move(types));

  installedVendorCombo_->blockSignals(true);
  installedVendorCombo_->clear();
  installedVendorCombo_->addItem(tr("All"), QString{});
  for (const QString& vendor : vendorList) {
    installedVendorCombo_->addItem(vendor, vendor);
  }
  const int vIdx = !selectedVendor.isEmpty()
                       ? installedVendorCombo_->findData(selectedVendor)
                       : 0;
  installedVendorCombo_->setCurrentIndex(vIdx >= 0 ? vIdx : 0);
  installedVendorCombo_->blockSignals(false);

  installedArchCombo_->blockSignals(true);
  installedArchCombo_->clear();
  installedArchCombo_->addItem(tr("All"), QString{});
  for (const QString& arch : archList) {
    installedArchCombo_->addItem(arch, arch);
  }
  const int aIdx =
      !selectedArch.isEmpty() ? installedArchCombo_->findData(selectedArch) : 0;
  installedArchCombo_->setCurrentIndex(aIdx >= 0 ? aIdx : 0);
  installedArchCombo_->blockSignals(false);

  installedTypeCombo_->blockSignals(true);
  installedTypeCombo_->clear();
  installedTypeCombo_->addItem(tr("All"), QString{});
  for (const QString& type : typeList) {
    installedTypeCombo_->addItem(type, type);
  }
  const int tIdx =
      !selectedType.isEmpty() ? installedTypeCombo_->findData(selectedType) : 0;
  installedTypeCombo_->setCurrentIndex(tIdx >= 0 ? tIdx : 0);
  installedTypeCombo_->blockSignals(false);
}

void BoardsManagerDialog::rebuildSearchFilters() {
  if (!searchModel_ || !searchVendorCombo_ || !searchArchCombo_ || !searchTypeCombo_) {
    return;
  }

  const QString selectedVendor = searchVendorCombo_->currentData().toString();
  const QString selectedArch = searchArchCombo_->currentData().toString();
  const QString selectedType = searchTypeCombo_->currentData().toString();

  QSet<QString> vendors;
  QSet<QString> archs;
  QSet<QString> types;
  for (int row = 0; row < searchModel_->rowCount(); ++row) {
    const QString id = searchModel_->data(searchModel_->index(row, SColId)).toString();
    vendors.insert(platformVendor(id));
    archs.insert(platformArch(id));

    const QString installed =
        searchModel_->data(searchModel_->index(row, SColInstalled)).toString();
    const QString latest =
        searchModel_->data(searchModel_->index(row, SColLatest)).toString();
    const QString version = !latest.trimmed().isEmpty() ? latest : installed;

    const QJsonObject p =
        searchModel_->data(searchModel_->index(row, SColId), kRolePlatformJson)
            .value<QJsonObject>();
    const QStringList rowTypes = platformTypesForVersion(p, version);
    for (const QString& t : rowTypes) {
      types.insert(t);
    }
  }

  const QStringList vendorList = setToSortedList(std::move(vendors));
  const QStringList archList = setToSortedList(std::move(archs));
  const QStringList typeList = setToSortedList(std::move(types));

  searchVendorCombo_->blockSignals(true);
  searchVendorCombo_->clear();
  searchVendorCombo_->addItem(tr("All"), QString{});
  for (const QString& vendor : vendorList) {
    searchVendorCombo_->addItem(vendor, vendor);
  }
  const int vIdx =
      !selectedVendor.isEmpty() ? searchVendorCombo_->findData(selectedVendor) : 0;
  searchVendorCombo_->setCurrentIndex(vIdx >= 0 ? vIdx : 0);
  searchVendorCombo_->blockSignals(false);

  searchArchCombo_->blockSignals(true);
  searchArchCombo_->clear();
  searchArchCombo_->addItem(tr("All"), QString{});
  for (const QString& arch : archList) {
    searchArchCombo_->addItem(arch, arch);
  }
  const int aIdx =
      !selectedArch.isEmpty() ? searchArchCombo_->findData(selectedArch) : 0;
  searchArchCombo_->setCurrentIndex(aIdx >= 0 ? aIdx : 0);
  searchArchCombo_->blockSignals(false);

  searchTypeCombo_->blockSignals(true);
  searchTypeCombo_->clear();
  searchTypeCombo_->addItem(tr("All"), QString{});
  for (const QString& type : typeList) {
    searchTypeCombo_->addItem(type, type);
  }
  const int tIdx = !selectedType.isEmpty() ? searchTypeCombo_->findData(selectedType) : 0;
  searchTypeCombo_->setCurrentIndex(tIdx >= 0 ? tIdx : 0);
  searchTypeCombo_->blockSignals(false);
}

void BoardsManagerDialog::applyInstalledFilters() {
  if (!installedProxy_ || !installedShowCombo_ || !installedVendorCombo_ ||
      !installedArchCombo_ || !installedTypeCombo_) {
    return;
  }

  const auto mode = static_cast<PlatformFilterProxyModel::ShowMode>(
      installedShowCombo_->currentData().toInt());
  installedProxy_->setShowMode(mode);
  installedProxy_->setVendorFilter(installedVendorCombo_->currentData().toString());
  installedProxy_->setArchitectureFilter(installedArchCombo_->currentData().toString());
  installedProxy_->setTypeFilter(installedTypeCombo_->currentData().toString());

  if (installedView_ && installedView_->model() && installedView_->model()->rowCount() > 0 &&
      !installedView_->currentIndex().isValid()) {
    installedView_->setCurrentIndex(installedView_->model()->index(0, 0));
  }
}

void BoardsManagerDialog::applySearchFilters() {
  if (!searchProxy_ || !searchShowCombo_ || !searchVendorCombo_ || !searchArchCombo_ ||
      !searchTypeCombo_) {
    return;
  }

  const auto mode = static_cast<PlatformFilterProxyModel::ShowMode>(
      searchShowCombo_->currentData().toInt());
  searchProxy_->setShowMode(mode);
  searchProxy_->setVendorFilter(searchVendorCombo_->currentData().toString());
  searchProxy_->setArchitectureFilter(searchArchCombo_->currentData().toString());
  searchProxy_->setTypeFilter(searchTypeCombo_->currentData().toString());

  if (searchView_ && searchView_->model() && searchView_->model()->rowCount() > 0 &&
      !searchView_->currentIndex().isValid()) {
    searchView_->setCurrentIndex(searchView_->model()->index(0, 0));
  }
}

bool BoardsManagerDialog::shouldAutoUpdateIndexNow() const {
  const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
  QSettings settings;
  settings.beginGroup(kBoardsManagerSettingsGroup);
  const QDateTime lastSuccess = settings.value(kCoreIndexLastSuccessUtcKey).toDateTime();
  const QDateTime lastAttempt = settings.value(kCoreIndexLastAttemptUtcKey).toDateTime();
  settings.endGroup();
  return shouldAutoUpdateIndex(lastSuccess, lastAttempt, nowUtc);
}

void BoardsManagerDialog::updateIndexStatusLabel() {
  if (!indexStatusLabel_) {
    return;
  }

  QSettings settings;
  settings.beginGroup(kBoardsManagerSettingsGroup);
  const QDateTime lastSuccess = settings.value(kCoreIndexLastSuccessUtcKey).toDateTime();
  const QDateTime lastError = settings.value(kCoreIndexLastErrorUtcKey).toDateTime();
  const QString lastErrorMsg = settings.value(kCoreIndexLastErrorMessageKey).toString();
  settings.endGroup();

  if (!lastSuccess.isValid()) {
    if (!lastError.isValid()) {
      indexStatusLabel_->setText(tr("Index: never updated"));
      indexStatusLabel_->setToolTip(tr("The Boards Manager index has not been updated yet."));
      return;
    }

    indexStatusLabel_->setText(tr("Index: update failed"));
    const QDateTime errLocal = lastError.toLocalTime();
    QString tip = tr("The Boards Manager index could not be updated.\n"
                     "Last attempt: %1")
                      .arg(errLocal.toString());
    if (!lastErrorMsg.trimmed().isEmpty()) {
      tip += QStringLiteral("\n\n") + lastErrorMsg.trimmed();
    }
    indexStatusLabel_->setToolTip(tip);
    return;
  }

  const QDateTime local = lastSuccess.toLocalTime();
  const QString stamp = local.toString("yyyy-MM-dd HH:mm");
  QString label = tr("Index: %1").arg(stamp);
  QString tip = tr("Last successful index update: %1").arg(local.toString());

  if (lastError.isValid() && lastError > lastSuccess) {
    label = tr("Index: %1 (update failed)").arg(stamp);
    const QDateTime errLocal = lastError.toLocalTime();
    tip += tr("\n\nLast update attempt failed: %1").arg(errLocal.toString());
    if (!lastErrorMsg.trimmed().isEmpty()) {
      tip += QStringLiteral("\n\n") + lastErrorMsg.trimmed();
    }
  }

  indexStatusLabel_->setText(label);
  indexStatusLabel_->setToolTip(tip);
}

void BoardsManagerDialog::runUpdateIndex(bool automatic) {
  if (isBusy()) {
    return;
  }
  if (indexStatusLabel_) {
    indexStatusLabel_->setText(tr("Index: updating\u2026"));
  }

  const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
  {
    QSettings settings;
    settings.beginGroup(kBoardsManagerSettingsGroup);
    settings.setValue(kCoreIndexLastAttemptUtcKey, nowUtc);
    settings.endGroup();
  }

  runCommand({QStringLiteral("core"), QStringLiteral("update-index")}, false, false, {},
             [this, nowUtc, automatic](int exitCode, const QByteArray& out) {
               QString errorText;
               if (exitCode == 0) {
                 QSettings settings;
                 settings.beginGroup(kBoardsManagerSettingsGroup);
                 settings.setValue(kCoreIndexLastSuccessUtcKey, nowUtc);
                 settings.remove(kCoreIndexLastErrorUtcKey);
                 settings.remove(kCoreIndexLastErrorMessageKey);
                 settings.endGroup();
                 emit platformsChanged();
               } else {
                 errorText = truncateMessage(QString::fromLocal8Bit(out), 2048);
                 if (errorText.trimmed().isEmpty()) {
                   errorText = tr("Unknown error.");
                 }
                 QSettings settings;
                 settings.beginGroup(kBoardsManagerSettingsGroup);
                 settings.setValue(kCoreIndexLastErrorUtcKey, nowUtc);
                 settings.setValue(kCoreIndexLastErrorMessageKey, errorText);
                 settings.endGroup();

                 if (output_) {
                   output_->appendLine("[Boards Manager] Index update failed.");
                   if (!errorText.trimmed().isEmpty()) {
                     output_->appendLine(errorText);
                   }
                 }

                 if (!automatic) {
                   QMessageBox::warning(
                       this, tr("Index Update Failed"),
                       tr("Could not update the Boards Manager index.\n\n%1").arg(errorText));
                 }
               }

               updateIndexStatusLabel();
               refreshInstalled();
             });
}
