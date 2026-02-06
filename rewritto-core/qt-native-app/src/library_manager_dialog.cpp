#include "library_manager_dialog.h"

#include "arduino_cli.h"
#include "index_update_policy.h"
#include "output_widget.h"

#include <QAbstractItemView>
#include <QBoxLayout>
#include <QBrush>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QColor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QItemSelectionModel>
#include <QMessageBox>
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
#include <QTreeWidget>
#include <QVersionNumber>

namespace {
enum InstalledColumns {
  ColName = 0,
  ColVersion,
  ColPinned,
  ColLocation,
  ColSummary,
  ColCount
};
enum SearchColumns { SColName = 0, SColLatest, SColCount };

constexpr int kRoleAvailableVersions = Qt::UserRole + 1;
constexpr int kRoleLibraryJson = Qt::UserRole + 2;

constexpr auto kLibraryManagerSettingsGroup = "LibraryManager";
constexpr auto kLibIndexLastSuccessUtcKey = "libIndexLastSuccessUtc";
constexpr auto kLibIndexLastAttemptUtcKey = "libIndexLastAttemptUtc";
constexpr auto kLibIndexLastErrorUtcKey = "libIndexLastErrorUtc";
constexpr auto kLibIndexLastErrorMessageKey = "libIndexLastErrorMessage";
constexpr auto kPinnedLibraryVersionsKey = "pinnedLibraryVersionsJson";

QString latestVersionString(const QJsonObject& lib) {
  const QJsonValue latest = lib.value("latest");
  if (latest.isString()) {
    return latest.toString().trimmed();
  }
  if (latest.isObject()) {
    return latest.toObject().value("version").toString().trimmed();
  }
  return {};
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

QStringList availableVersions(const QJsonObject& lib) {
  QStringList versions;
  const QJsonArray arr = lib.value("available_versions").toArray();
  for (const QJsonValue& v : arr) {
    const QString s = v.toString().trimmed();
    if (!s.isEmpty()) {
      versions << s;
    }
  }
  if (versions.isEmpty()) {
    versions = lib.value("releases").toObject().keys();
  }
  return sortVersions(std::move(versions));
}

struct DepStatusEntry {
  QString name;
  QString versionRequired;
  QString versionInstalled;
};

QVector<DepStatusEntry> parseDepsStatusJson(const QByteArray& out) {
  const QJsonDocument doc = QJsonDocument::fromJson(out);
  if (!doc.isObject()) {
    return {};
  }
  const QJsonArray deps =
      doc.object().value(QStringLiteral("dependencies")).toArray();

  QVector<DepStatusEntry> entries;
  entries.reserve(deps.size());
  for (const QJsonValue& v : deps) {
    if (!v.isObject()) {
      continue;
    }
    const QJsonObject o = v.toObject();
    DepStatusEntry e;
    e.name = o.value(QStringLiteral("name")).toString().trimmed();
    e.versionRequired =
        o.value(QStringLiteral("version_required")).toString().trimmed();
    e.versionInstalled =
        o.value(QStringLiteral("version_installed")).toString().trimmed();
    if (!e.name.isEmpty()) {
      entries.push_back(std::move(e));
    }
  }

  std::sort(entries.begin(), entries.end(),
            [](const DepStatusEntry& a, const DepStatusEntry& b) {
              return a.name.toLower() < b.name.toLower();
            });
  return entries;
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

QString joinStringArray(const QJsonArray& arr) {
  QStringList out;
  out.reserve(arr.size());
  for (const QJsonValue& v : arr) {
    const QString s = v.toString().trimmed();
    if (!s.isEmpty()) {
      out << s;
    }
  }
  out.removeDuplicates();
  return out.join(QStringLiteral(", "));
}

QMap<QString, QString> loadPinnedLibraryVersions() {
  QMap<QString, QString> out;
  QSettings settings;
  settings.beginGroup(kLibraryManagerSettingsGroup);
  const QByteArray raw = settings.value(kPinnedLibraryVersionsKey).toByteArray();
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
    const QString name = it.key().trimmed();
    const QString version = it.value().toString().trimmed();
    if (!name.isEmpty() && !version.isEmpty()) {
      out.insert(name, version);
    }
  }
  return out;
}

void storePinnedLibraryVersions(const QMap<QString, QString>& pinned) {
  QJsonObject obj;
  for (auto it = pinned.begin(); it != pinned.end(); ++it) {
    const QString name = it.key().trimmed();
    const QString version = it.value().trimmed();
    if (!name.isEmpty() && !version.isEmpty()) {
      obj.insert(name, version);
    }
  }
  QSettings settings;
  settings.beginGroup(kLibraryManagerSettingsGroup);
  settings.setValue(kPinnedLibraryVersionsKey,
                    QJsonDocument(obj).toJson(QJsonDocument::Compact));
  settings.endGroup();
}

QString formatInstalledLibraryDetails(const QJsonObject& lib,
                                     const QString& pinnedVersion) {
  const QString name = lib.value("name").toString().trimmed();
  const QString version = lib.value("version").toString().trimmed();
  const QString author = lib.value("author").toString().trimmed();
  const QString maintainer = lib.value("maintainer").toString().trimmed();
  const QString sentence = lib.value("sentence").toString().trimmed();
  const QString paragraph = lib.value("paragraph").toString().trimmed();
  const QString website = lib.value("website").toString().trimmed();
  const QString category = lib.value("category").toString().trimmed();
  const QString license = lib.value("license").toString().trimmed();
  const QString location = lib.value("location").toString().trimmed();

  QString html;
  html += QStringLiteral("<b>%1</b><br/>").arg(htmlEscape(name));
  if (!version.isEmpty()) {
    html += QStringLiteral("<b>Version:</b> %1<br/>").arg(htmlEscape(version));
  }
  if (!pinnedVersion.trimmed().isEmpty()) {
    html += QStringLiteral("<b>Pinned:</b> %1<br/>").arg(htmlEscape(pinnedVersion.trimmed()));
  }
  if (!location.isEmpty()) {
    html += QStringLiteral("<b>Location:</b> %1<br/>").arg(htmlEscape(location));
  }
  if (!category.isEmpty()) {
    html += QStringLiteral("<b>Category:</b> %1<br/>").arg(htmlEscape(category));
  }
  if (!license.isEmpty()) {
    html += QStringLiteral("<b>License:</b> %1<br/>").arg(htmlEscape(license));
  }
  const QString arch = joinStringArray(lib.value("architectures").toArray());
  if (!arch.isEmpty()) {
    html += QStringLiteral("<b>Architectures:</b> %1<br/>").arg(htmlEscape(arch));
  }
  if (!author.isEmpty()) {
    html += QStringLiteral("<b>Author:</b> %1<br/>").arg(htmlEscape(author));
  }
  if (!maintainer.isEmpty()) {
    html += QStringLiteral("<b>Maintainer:</b> %1<br/>").arg(htmlEscape(maintainer));
  }
  if (!website.isEmpty()) {
    html += QStringLiteral("<b>Website:</b> <a href=\"%1\">%2</a><br/>")
                .arg(htmlEscape(website), htmlEscape(website));
  }
  if (!sentence.isEmpty()) {
    html += QStringLiteral("<br/><b>Summary</b><br/>%1<br/>")
                .arg(htmlEscape(sentence));
  }
  if (!paragraph.isEmpty() && paragraph != sentence) {
    html += QStringLiteral("<br/><b>Description</b><br/>%1<br/>")
                .arg(htmlEscape(paragraph));
  }

  return html;
}

QJsonObject releaseForVersion(const QJsonObject& lib, const QString& version) {
  const QString v = version.trimmed();
  const QJsonObject releases = lib.value("releases").toObject();
  if (!v.isEmpty()) {
    const QJsonValue rel = releases.value(v);
    if (rel.isObject()) {
      return rel.toObject();
    }
  }

  const QJsonValue latestVal = lib.value("latest");
  if (latestVal.isObject()) {
    return latestVal.toObject();
  }
  if (latestVal.isString()) {
    const QString lv = latestVal.toString().trimmed();
    const QJsonValue rel = releases.value(lv);
    if (rel.isObject()) {
      return rel.toObject();
    }
  }
  return {};
}

QString formatDependenciesHtml(const QJsonArray& deps) {
  if (deps.isEmpty()) {
    return {};
  }
  QString html;
  html += QStringLiteral("<br/><b>Dependencies</b><br/><ul>");
  int count = 0;
  for (const QJsonValue& v : deps) {
    if (!v.isObject()) {
      continue;
    }
    const QJsonObject dep = v.toObject();
    const QString name = dep.value("name").toString().trimmed();
    if (name.isEmpty()) {
      continue;
    }
    const QString constraint = dep.value("version_constraint").toString().trimmed();

    QString item = htmlEscape(name);
    if (!constraint.isEmpty()) {
      item += QStringLiteral(" <code>%1</code>").arg(htmlEscape(constraint));
    }
    html += QStringLiteral("<li>%1</li>").arg(item);
    ++count;
  }
  if (count == 0) {
    return {};
  }
  html += QStringLiteral("</ul>");
  return html;
}

QString formatSearchLibraryDetails(const QJsonObject& lib, const QString& selectedVersion) {
  const QString name = lib.value("name").toString().trimmed();
  const QString latest = latestVersionString(lib);
  const QStringList versions = availableVersions(lib);

  const QString chosen = selectedVersion.trimmed().isEmpty() ? latest : selectedVersion.trimmed();
  const QJsonObject chosenObj = releaseForVersion(lib, chosen);

  const QString author = chosenObj.value("author").toString().trimmed();
  const QString maintainer = chosenObj.value("maintainer").toString().trimmed();
  const QString sentence = chosenObj.value("sentence").toString().trimmed();
  const QString paragraph = chosenObj.value("paragraph").toString().trimmed();
  const QString website = chosenObj.value("website").toString().trimmed();
  const QString category = chosenObj.value("category").toString().trimmed();
  const QString license = chosenObj.value("license").toString().trimmed();

  QString html;
  html += QStringLiteral("<b>%1</b><br/>").arg(htmlEscape(name));
  if (!latest.isEmpty()) {
    html += QStringLiteral("<b>Latest:</b> %1<br/>").arg(htmlEscape(latest));
  }
  if (!chosen.isEmpty() && !latest.isEmpty() && chosen != latest) {
    html += QStringLiteral("<b>Selected:</b> %1<br/>").arg(htmlEscape(chosen));
  }
  if (!category.isEmpty()) {
    html += QStringLiteral("<b>Category:</b> %1<br/>").arg(htmlEscape(category));
  }
  if (!license.isEmpty()) {
    html += QStringLiteral("<b>License:</b> %1<br/>").arg(htmlEscape(license));
  }
  const QString arch = joinStringArray(chosenObj.value("architectures").toArray());
  if (!arch.isEmpty()) {
    html += QStringLiteral("<b>Architectures:</b> %1<br/>").arg(htmlEscape(arch));
  }
  if (!author.isEmpty()) {
    html += QStringLiteral("<b>Author:</b> %1<br/>").arg(htmlEscape(author));
  }
  if (!maintainer.isEmpty()) {
    html += QStringLiteral("<b>Maintainer:</b> %1<br/>").arg(htmlEscape(maintainer));
  }
  if (!website.isEmpty()) {
    html += QStringLiteral("<b>Website:</b> <a href=\"%1\">%2</a><br/>")
                .arg(htmlEscape(website), htmlEscape(website));
  }
  if (!sentence.isEmpty()) {
    html += QStringLiteral("<br/><b>Summary</b><br/>%1<br/>")
                .arg(htmlEscape(sentence));
  }
  if (!paragraph.isEmpty() && paragraph != sentence) {
    html += QStringLiteral("<br/><b>Description</b><br/>%1<br/>")
                .arg(htmlEscape(paragraph));
  }
  html += formatDependenciesHtml(chosenObj.value("dependencies").toArray());
  if (!versions.isEmpty()) {
    html += QStringLiteral("<br/><b>Available versions:</b> %1<br/>")
                .arg(QString::number(versions.size()));
    const int maxShow = 15;
    const QStringList shown = versions.mid(0, maxShow);
    html += QStringLiteral("<code>%1</code>").arg(htmlEscape(shown.join(", ")));
    if (versions.size() > maxShow) {
      html += QStringLiteral("<br/>(%1 more\u2026)")
                  .arg(QString::number(versions.size() - maxShow));
    }
  }

  return html;
}
}  // namespace

LibraryManagerDialog::LibraryManagerDialog(ArduinoCli* arduinoCli,
                                           OutputWidget* output,
                                           QWidget* parent)
    : QWidget(parent), arduinoCli_(arduinoCli), output_(output) {
  setWindowTitle("Library Manager");
  resize(900, 520);
  buildUi();
  pinnedLibraryVersions_ = loadPinnedLibraryVersions();
  wireSignals();
}

bool LibraryManagerDialog::isBusy() const {
  return busy_;
}

void LibraryManagerDialog::refresh() {
  updateIndexStatusLabel();
  if (shouldAutoUpdateIndexNow()) {
    runUpdateIndex(true);
    return;
  }
  refreshInstalled();
}

void LibraryManagerDialog::cancel() {
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
    output_->appendLine("[Library Manager] Canceled.");
  }
  setBusy(false);
}

void LibraryManagerDialog::showSearchFor(const QString& query) {
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

  if (q.isEmpty()) {
    return;
  }

  runSearch();
}

void LibraryManagerDialog::buildUi() {
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
    installedPinButton_->setToolTip(
        tr("Pin the installed version to prevent upgrades"));
    installedIncludeButton_ = new QPushButton(tr("Include"), page);
    installedIncludeButton_->setObjectName("libraryManagerInstalledIncludeButton");
    installedIncludeButton_->setEnabled(false);
    installedExamplesButton_ = new QPushButton(tr("Examples"), page);
    installedExamplesButton_->setObjectName("libraryManagerInstalledExamplesButton");
    installedExamplesButton_->setEnabled(false);
    installedDepsButton_ = new QPushButton(tr("Dependencies"), page);
    installedDepsButton_->setObjectName("libraryManagerInstalledDepsButton");
    installedDepsButton_->setEnabled(false);
    header->addWidget(installedRefreshButton_);
    header->addWidget(installedUpgradeButton_);
    header->addWidget(installedUpgradeAllButton_);
    header->addWidget(installedUninstallButton_);
    header->addWidget(installedPinButton_);
    header->addWidget(installedIncludeButton_);
    header->addWidget(installedExamplesButton_);
    header->addWidget(installedDepsButton_);
    header->addStretch(1);
    v->addLayout(header);

    installedModel_ = new QStandardItemModel(0, ColCount, page);
    installedModel_->setHorizontalHeaderLabels(
        {"Name", "Version", "Pinned", "Location", "Summary"});

    installedView_ = new QTableView(page);
    installedView_->setModel(installedModel_);
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
    searchEdit_->setObjectName("libraryManagerSearchEdit");
    searchEdit_->setPlaceholderText("e.g. wifi, json, display");
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
    installDepsCheck_ = new QCheckBox(tr("Install deps"), page);
    installDepsCheck_->setToolTip(tr("Install library dependencies"));
    {
      QSettings settings;
      settings.beginGroup(kLibraryManagerSettingsGroup);
      const bool installDeps = settings.value("installDeps", true).toBool();
      settings.endGroup();
      installDepsCheck_->setChecked(installDeps);
    }
    connect(installDepsCheck_, &QCheckBox::toggled, this, [this](bool checked) {
      QSettings settings;
      settings.beginGroup(kLibraryManagerSettingsGroup);
      settings.setValue("installDeps", checked);
      settings.endGroup();
    });
    header->addWidget(installDepsCheck_);
    searchDepsButton_ = new QPushButton(tr("Dependencies"), page);
    searchDepsButton_->setObjectName("libraryManagerSearchDepsButton");
    searchDepsButton_->setEnabled(false);
    header->addWidget(searchDepsButton_);
    header->addWidget(installButton_);
    v->addLayout(header);

    searchModel_ = new QStandardItemModel(0, SColCount, page);
    searchModel_->setHorizontalHeaderLabels({"Name", "Latest"});

    searchView_ = new QTableView(page);
    searchView_->setObjectName("libraryManagerSearchView");
    searchView_->setModel(searchModel_);
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

void LibraryManagerDialog::wireSignals() {
  auto updateInstalledSelectionUi = [this] {
    const QString name = selectedInstalledLibraryName();
    if (!installedUpgradeButton_ || !installedUninstallButton_ || !installedPinButton_ ||
        !installedIncludeButton_ || !installedExamplesButton_) {
      return;
    }
    const bool hasSelection = !name.isEmpty();
    const QString pinnedVersion = pinnedLibraryVersions_.value(name).trimmed();
    const bool pinned = hasSelection && !pinnedVersion.isEmpty();

    installedUpgradeButton_->setEnabled(hasSelection && !pinned);
    installedUninstallButton_->setEnabled(hasSelection);
    installedPinButton_->setEnabled(hasSelection);
    installedIncludeButton_->setEnabled(false);
    installedExamplesButton_->setEnabled(hasSelection);
    if (installedDepsButton_) {
      installedDepsButton_->setEnabled(hasSelection);
    }
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
    const QModelIndex idx = installedView_->currentIndex();
    if (!idx.isValid()) {
      return;
    }
    const QModelIndex nameIdx = installedModel_->index(idx.row(), ColName);
    const QJsonObject lib =
        installedModel_->data(nameIdx, kRoleLibraryJson).value<QJsonObject>();
    {
      QStringList includes;
      for (const QJsonValue& iv : lib.value("provides_includes").toArray()) {
        const QString inc = iv.toString().trimmed();
        if (!inc.isEmpty()) {
          includes << inc;
        }
      }
      includes.removeDuplicates();

      const bool canInclude = hasSelection && !includes.isEmpty();
      installedIncludeButton_->setEnabled(canInclude);
      installedIncludeButton_->setToolTip(
          canInclude ? tr("Insert #include lines for this library")
                     : tr("This library does not advertise include files."));
    }
    if (installedDetails_) {
      installedDetails_->setHtml(
          lib.isEmpty() ? QString{} : formatInstalledLibraryDetails(lib, pinnedVersion));
    }
  };

  auto renderSearchDetails = [this] {
    if (!searchDetails_ || !searchModel_ || !searchView_) {
      return;
    }
    const QModelIndex idx = searchView_->currentIndex();
    if (!idx.isValid()) {
      searchDetails_->clear();
      return;
    }
    const QModelIndex nameIdx = searchModel_->index(idx.row(), SColName);
    const QJsonObject lib =
        searchModel_->data(nameIdx, kRoleLibraryJson).value<QJsonObject>();
    const QString selectedVersion =
        searchVersionCombo_ ? searchVersionCombo_->currentText().trimmed() : QString{};
    searchDetails_->setHtml(
        lib.isEmpty() ? QString{} : formatSearchLibraryDetails(lib, selectedVersion));
  };

  auto updateSearchSelectionUi = [this] {
    const QString name = selectedSearchLibraryName();
    if (!installButton_ || !searchVersionCombo_ || !searchModel_ || !searchView_) {
      return;
    }
    if (searchDepsButton_) {
      searchDepsButton_->setEnabled(!name.isEmpty());
    }
    installButton_->setEnabled(false);
    searchVersionCombo_->setEnabled(false);
    searchVersionCombo_->clear();
    if (searchDetails_) {
      searchDetails_->clear();
    }

    if (name.isEmpty()) {
      return;
    }
    const QModelIndex idx = searchView_->currentIndex();
    if (!idx.isValid()) {
      return;
    }
    const QModelIndex nameIdx = searchModel_->index(idx.row(), SColName);
    const QJsonObject lib =
        searchModel_->data(nameIdx, kRoleLibraryJson).value<QJsonObject>();
    const QStringList versions =
        searchModel_->data(nameIdx, kRoleAvailableVersions).toStringList();
    if (versions.isEmpty()) {
      if (searchDetails_) {
        searchDetails_->setHtml(lib.isEmpty() ? QString{} : formatSearchLibraryDetails(lib, {}));
      }
      return;
    }
    searchVersionCombo_->addItems(versions);
    searchVersionCombo_->setEnabled(true);
    installButton_->setEnabled(true);

    const QString latest =
        searchModel_->data(searchModel_->index(idx.row(), SColLatest), Qt::DisplayRole)
            .toString()
            .trimmed();
    const int vIdx =
        !latest.isEmpty() ? searchVersionCombo_->findText(latest) : -1;
    if (vIdx >= 0) {
      searchVersionCombo_->setCurrentIndex(vIdx);
    }
    if (searchDetails_) {
      const QString selectedVersion =
          searchVersionCombo_ ? searchVersionCombo_->currentText().trimmed() : QString{};
      searchDetails_->setHtml(
          lib.isEmpty() ? QString{} : formatSearchLibraryDetails(lib, selectedVersion));
    }
  };

  auto showDepsStatusDialog = [this](QString rootName,
                                    QString rootVersion,
                                    const QByteArray& out) {
    rootName = rootName.trimmed();
    rootVersion = rootVersion.trimmed();

    const QVector<DepStatusEntry> entries = parseDepsStatusJson(out);
    if (entries.isEmpty()) {
      QMessageBox::information(
          this, tr("Dependencies"),
          tr("No dependency information was returned for \"%1\".").arg(rootName));
      return;
    }

    QDialog dlg(this);
    QString title = tr("Dependencies — %1").arg(rootName);
    if (!rootVersion.isEmpty()) {
      title += tr(" (%1)").arg(rootVersion);
    }
    dlg.setWindowTitle(title);

    auto* tree = new QTreeWidget(&dlg);
    tree->setObjectName("libraryDependenciesTree");
    tree->setColumnCount(4);
    tree->setHeaderLabels({tr("Library"), tr("Required"), tr("Installed"), tr("Status")});
    tree->setRootIsDecorated(false);
    tree->setItemsExpandable(false);
    tree->setUniformRowHeights(true);

    int missingCount = 0;
    int mismatchCount = 0;
    int okCount = 0;

    for (const DepStatusEntry& e : entries) {
      auto* item = new QTreeWidgetItem(tree);
      item->setText(0, e.name);
      item->setText(1, e.versionRequired);
      item->setText(2, e.versionInstalled);

      QString status;
      QBrush brush;
      if (e.versionInstalled.isEmpty()) {
        status = tr("Not installed");
        brush = QBrush(QColor(180, 0, 0));
        ++missingCount;
      } else if (!e.versionRequired.isEmpty() && e.versionInstalled != e.versionRequired) {
        status = tr("Version mismatch");
        brush = QBrush(QColor(180, 120, 0));
        ++mismatchCount;
      } else {
        status = tr("OK");
        brush = QBrush(QColor(0, 120, 0));
        ++okCount;
      }
      item->setText(3, status);
      item->setForeground(3, brush);
    }

    tree->resizeColumnToContents(0);
    tree->resizeColumnToContents(1);
    tree->resizeColumnToContents(2);
    tree->resizeColumnToContents(3);

    auto* buttons =
        new QDialogButtonBox(QDialogButtonBox::Close, Qt::Horizontal, &dlg);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    auto* layout = new QVBoxLayout(&dlg);
    layout->addWidget(tree, 1);

    auto* summary = new QLabel(&dlg);
    summary->setText(tr("OK: %1   Missing: %2   Mismatch: %3")
                         .arg(okCount)
                         .arg(missingCount)
                         .arg(mismatchCount));
    layout->addWidget(summary);
    layout->addWidget(buttons);

    dlg.resize(720, 420);
    (void)dlg.exec();
  };

  auto runDepsStatus = [this, showDepsStatusDialog](QString name, QString version) {
    name = name.trimmed();
    version = version.trimmed();
    if (name.isEmpty()) {
      return;
    }
    if (isBusy()) {
      return;
    }
    QString spec = name;
    if (!version.isEmpty()) {
      spec += QStringLiteral("@") + version;
    }
    runCommand({QStringLiteral("lib"), QStringLiteral("deps"), spec, QStringLiteral("--json")},
               true, false,
               [this, showDepsStatusDialog, name, version](const QByteArray& out) {
                 showDepsStatusDialog(name, version, out);
               });
  };

  connect(installedRefreshButton_, &QPushButton::clicked, this,
          [this] { refreshInstalled(); });
  connect(installedIncludeButton_, &QPushButton::clicked, this, [this] {
    if (!installedModel_ || !installedView_) {
      return;
    }
    const QModelIndex idx = installedView_->currentIndex();
    if (!idx.isValid()) {
      return;
    }
    const QModelIndex nameIdx = installedModel_->index(idx.row(), ColName);
    const QJsonObject lib = installedModel_->data(nameIdx, kRoleLibraryJson).value<QJsonObject>();
    const QString name = lib.value("name").toString().trimmed();
    if (name.isEmpty()) {
      return;
    }

    QStringList includes;
    for (const QJsonValue& iv : lib.value("provides_includes").toArray()) {
      const QString inc = iv.toString().trimmed();
      if (!inc.isEmpty()) {
        includes << inc;
      }
    }
    includes.removeDuplicates();
    emit includeLibraryRequested(name, includes);
  });
  connect(installedExamplesButton_, &QPushButton::clicked, this, [this] {
    const QString name = selectedInstalledLibraryName();
    if (name.isEmpty()) {
      return;
    }
    emit openLibraryExamplesRequested(name);
  });
  connect(installedDepsButton_, &QPushButton::clicked, this, [this, runDepsStatus] {
    if (!installedModel_ || !installedView_) {
      return;
    }
    const QModelIndex idx = installedView_->currentIndex();
    if (!idx.isValid()) {
      return;
    }
    const QString name =
        installedModel_->data(installedModel_->index(idx.row(), ColName)).toString().trimmed();
    const QString version =
        installedModel_->data(installedModel_->index(idx.row(), ColVersion)).toString().trimmed();
    runDepsStatus(name, version);
  });
  connect(installedUpgradeAllButton_, &QPushButton::clicked, this, [this] {
    if (!installedModel_) {
      return;
    }

    QStringList libs;
    libs.reserve(installedModel_->rowCount());
    for (int row = 0; row < installedModel_->rowCount(); ++row) {
      const QString name =
          installedModel_->data(installedModel_->index(row, ColName)).toString().trimmed();
      if (name.isEmpty()) {
        continue;
      }
      if (pinnedLibraryVersions_.contains(name)) {
        continue;
      }
      libs << name;
    }
    libs.removeAll(QString{});
    libs.removeDuplicates();

    if (libs.isEmpty()) {
      if (output_) {
        output_->appendLine("[Library Manager] No libraries to upgrade (all pinned).");
      }
      return;
    }

    QStringList args = {QStringLiteral("lib"), QStringLiteral("upgrade")};
    args.append(libs);
    runCommand(args, false, false,
               [this](const QByteArray&) {
                 emit librariesChanged();
                 refreshInstalled();
               });
  });
  connect(searchButton_, &QPushButton::clicked, this, [this] { runSearch(); });
  connect(searchDepsButton_, &QPushButton::clicked, this, [this, runDepsStatus] {
    const QString name = selectedSearchLibraryName();
    if (name.isEmpty()) {
      return;
    }
    const QString version =
        searchVersionCombo_ ? searchVersionCombo_->currentText().trimmed() : QString{};
    runDepsStatus(name, version);
  });
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

  if (searchVersionCombo_) {
    connect(searchVersionCombo_, &QComboBox::currentTextChanged, this,
            [renderSearchDetails](const QString&) { renderSearchDetails(); });
  }

  if (installedView_ && installedView_->selectionModel()) {
    connect(installedView_->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [updateInstalledSelectionUi](const QModelIndex&, const QModelIndex&) {
              updateInstalledSelectionUi();
            });
  }
  if (searchView_ && searchView_->selectionModel()) {
    connect(searchView_->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [updateSearchSelectionUi](const QModelIndex&, const QModelIndex&) {
              updateSearchSelectionUi();
            });
    connect(searchView_, &QTableView::doubleClicked, this,
            [this](const QModelIndex&) {
              if (installButton_) {
                installButton_->click();
              }
            });
  }

  connect(installButton_, &QPushButton::clicked, this, [this] {
    const QString name = selectedSearchLibraryName();
    if (name.isEmpty()) {
      return;
    }
    const QString version = searchVersionCombo_ ? searchVersionCombo_->currentText().trimmed()
                                                : QString{};
    QString spec = name;
    if (!version.isEmpty()) {
      spec = QString("%1@%2").arg(name, version);
    }

    QStringList args = {QStringLiteral("lib"), QStringLiteral("install")};
    if (installDepsCheck_ && !installDepsCheck_->isChecked()) {
      args << QStringLiteral("--no-deps");
    }
    args << spec;

    runCommand(args, false, false, [this](const QByteArray&) {
      emit librariesChanged();
      refreshInstalled();
    });
  });

  connect(installedUninstallButton_, &QPushButton::clicked, this, [this] {
    const QString name = selectedInstalledLibraryName();
    if (name.isEmpty()) {
      return;
    }
    runCommand({QStringLiteral("lib"), QStringLiteral("uninstall"), name}, false, false,
               [this, name](const QByteArray&) {
                 if (pinnedLibraryVersions_.remove(name) > 0) {
                   storePinnedLibraryVersions(pinnedLibraryVersions_);
                 }
                 emit librariesChanged();
                 refreshInstalled();
               });
  });

  connect(installedUpgradeButton_, &QPushButton::clicked, this, [this] {
    const QString name = selectedInstalledLibraryName();
    if (name.isEmpty()) {
      return;
    }
    if (pinnedLibraryVersions_.contains(name)) {
      if (output_) {
        output_->appendLine("[Library Manager] Library is pinned; unpin to upgrade.");
      }
      return;
    }
    runCommand({QStringLiteral("lib"), QStringLiteral("upgrade"), name}, false, false,
               [this](const QByteArray&) {
                 emit librariesChanged();
                 refreshInstalled();
               });
  });

  connect(installedPinButton_, &QPushButton::toggled, this,
          [this, updateInstalledSelectionUi](bool checked) {
            if (!installedModel_ || !installedView_) {
              return;
            }
            const QModelIndex idx = installedView_->currentIndex();
            if (!idx.isValid()) {
              return;
            }
            const QString name = selectedInstalledLibraryName();
            if (name.isEmpty()) {
              return;
            }

            const QModelIndex nameIdx = installedModel_->index(idx.row(), ColName);
            const QJsonObject lib = installedModel_->data(nameIdx, kRoleLibraryJson).value<QJsonObject>();
            const QString installedVersion = lib.value("version").toString().trimmed();

            if (checked) {
              if (installedVersion.isEmpty()) {
                return;
              }
              pinnedLibraryVersions_.insert(name, installedVersion);
              installedModel_->setData(installedModel_->index(idx.row(), ColPinned),
                                       installedVersion);
            } else {
              pinnedLibraryVersions_.remove(name);
              installedModel_->setData(installedModel_->index(idx.row(), ColPinned), QString{});
            }
            storePinnedLibraryVersions(pinnedLibraryVersions_);
            updateInstalledSelectionUi();
          });

  updateInstalledSelectionUi();
  updateSearchSelectionUi();
}

void LibraryManagerDialog::setBusy(bool busy) {
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

void LibraryManagerDialog::runCommand(
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

  tabs_->setEnabled(false);
  setBusy(true);
  connect(process_, &QProcess::readyRead, this, [this, streamOutput] {
    const QByteArray chunk = process_->readAll();
    processOutput_.append(chunk);
    if (streamOutput && output_) {
      output_->appendText(QString::fromLocal8Bit(chunk));
    }
  });
  connect(process_, &QProcess::finished, this,
          [this, expectJson, onSuccess, onFinished](int exitCode, QProcess::ExitStatus) {
            const QByteArray out = processOutput_;
            process_->deleteLater();
            process_ = nullptr;
            tabs_->setEnabled(true);
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

void LibraryManagerDialog::refreshInstalled() {
  runCommand(
      {QStringLiteral("lib"), QStringLiteral("list"), QStringLiteral("--json")},
      true, false, [this](const QByteArray& out) {
        const QJsonDocument doc = QJsonDocument::fromJson(out);
        if (!doc.isObject()) {
          return;
        }
        const QJsonArray libs =
            doc.object().value("installed_libraries").toArray();

        installedModel_->removeRows(0, installedModel_->rowCount());
        QSet<QString> installedNames;

        for (const QJsonValue& v : libs) {
          const QJsonObject lib = v.toObject().value("library").toObject();
          const QString name = lib.value("name").toString();
          const QString version = lib.value("version").toString();
          const QString location = lib.value("location").toString();
          const QString summary = lib.value("sentence").toString();
          installedNames.insert(name);
          const QString pinned = pinnedLibraryVersions_.value(name).trimmed();

          QList<QStandardItem*> row;
          auto* nameItem = new QStandardItem(name);
          nameItem->setData(lib, kRoleLibraryJson);
          row << nameItem << new QStandardItem(version) << new QStandardItem(pinned)
              << new QStandardItem(location) << new QStandardItem(summary);
          installedModel_->appendRow(row);
        }

        bool changed = false;
        for (auto it = pinnedLibraryVersions_.begin(); it != pinnedLibraryVersions_.end();) {
          if (!installedNames.contains(it.key())) {
            it = pinnedLibraryVersions_.erase(it);
            changed = true;
          } else {
            ++it;
          }
        }
        if (changed) {
          storePinnedLibraryVersions(pinnedLibraryVersions_);
        }

        installedView_->resizeColumnsToContents();
        if (installedView_ && installedModel_->rowCount() > 0) {
          installedView_->setCurrentIndex(installedModel_->index(0, ColName));
        }
      });
}

void LibraryManagerDialog::runSearch() {
  if (isBusy()) {
    pendingAutoSearch_ = true;
    return;
  }

  const QString q = searchEdit_->text().trimmed();
  QStringList args = {QStringLiteral("lib"), QStringLiteral("search")};
  if (!q.isEmpty()) {
    args << q;
  }
  args << QStringLiteral("--json");

  runCommand(args, true, false, [this](const QByteArray& out) {
    const QJsonDocument doc = QJsonDocument::fromJson(out);
    if (!doc.isObject()) {
      return;
    }
    const QJsonArray libs = doc.object().value("libraries").toArray();

    searchModel_->removeRows(0, searchModel_->rowCount());
    for (const QJsonValue& v : libs) {
      const QJsonObject lib = v.toObject();
      const QString name = lib.value("name").toString();
      const QString latest = latestVersionString(lib);
      const QStringList versions = availableVersions(lib);
      QList<QStandardItem*> row;
      auto* nameItem = new QStandardItem(name);
      nameItem->setData(versions, kRoleAvailableVersions);
      nameItem->setData(lib, kRoleLibraryJson);
      row << nameItem << new QStandardItem(latest);
      searchModel_->appendRow(row);
    }
    searchView_->resizeColumnsToContents();
    if (searchView_ && searchModel_->rowCount() > 0) {
      searchView_->setCurrentIndex(searchModel_->index(0, SColName));
    }
  });
}

QString LibraryManagerDialog::selectedInstalledLibraryName() const {
  const QModelIndex idx = installedView_->currentIndex();
  if (!idx.isValid()) {
    return {};
  }
  const QModelIndex nameIdx = installedModel_->index(idx.row(), ColName);
  return installedModel_->data(nameIdx).toString();
}

QString LibraryManagerDialog::selectedSearchLibraryName() const {
  const QModelIndex idx = searchView_->currentIndex();
  if (!idx.isValid()) {
    return {};
  }
  const QModelIndex nameIdx = searchModel_->index(idx.row(), SColName);
  return searchModel_->data(nameIdx).toString();
}

bool LibraryManagerDialog::shouldAutoUpdateIndexNow() const {
  const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
  QSettings settings;
  settings.beginGroup(kLibraryManagerSettingsGroup);
  const QDateTime lastSuccess = settings.value(kLibIndexLastSuccessUtcKey).toDateTime();
  const QDateTime lastAttempt = settings.value(kLibIndexLastAttemptUtcKey).toDateTime();
  settings.endGroup();
  return shouldAutoUpdateIndex(lastSuccess, lastAttempt, nowUtc);
}

void LibraryManagerDialog::updateIndexStatusLabel() {
  if (!indexStatusLabel_) {
    return;
  }

  QSettings settings;
  settings.beginGroup(kLibraryManagerSettingsGroup);
  const QDateTime lastSuccess = settings.value(kLibIndexLastSuccessUtcKey).toDateTime();
  const QDateTime lastError = settings.value(kLibIndexLastErrorUtcKey).toDateTime();
  const QString lastErrorMsg = settings.value(kLibIndexLastErrorMessageKey).toString();
  settings.endGroup();

  if (!lastSuccess.isValid()) {
    if (!lastError.isValid()) {
      indexStatusLabel_->setText(tr("Index: never updated"));
      indexStatusLabel_->setToolTip(tr("The Library Manager index has not been updated yet."));
      return;
    }

    indexStatusLabel_->setText(tr("Index: update failed"));
    const QDateTime errLocal = lastError.toLocalTime();
    QString tip = tr("The Library Manager index could not be updated.\n"
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

void LibraryManagerDialog::runUpdateIndex(bool automatic) {
  if (isBusy()) {
    return;
  }
  if (indexStatusLabel_) {
    indexStatusLabel_->setText(tr("Index: updating\u2026"));
  }

  const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
  {
    QSettings settings;
    settings.beginGroup(kLibraryManagerSettingsGroup);
    settings.setValue(kLibIndexLastAttemptUtcKey, nowUtc);
    settings.endGroup();
  }

  runCommand({QStringLiteral("lib"), QStringLiteral("update-index")}, false, false, {},
             [this, nowUtc, automatic](int exitCode, const QByteArray& out) {
               QString errorText;
               if (exitCode == 0) {
                 QSettings settings;
                 settings.beginGroup(kLibraryManagerSettingsGroup);
                 settings.setValue(kLibIndexLastSuccessUtcKey, nowUtc);
                 settings.remove(kLibIndexLastErrorUtcKey);
                 settings.remove(kLibIndexLastErrorMessageKey);
                 settings.endGroup();
                 emit librariesChanged();
               } else {
                 errorText = truncateMessage(QString::fromLocal8Bit(out), 2048);
                 if (errorText.trimmed().isEmpty()) {
                   errorText = tr("Unknown error.");
                 }
                 QSettings settings;
                 settings.beginGroup(kLibraryManagerSettingsGroup);
                 settings.setValue(kLibIndexLastErrorUtcKey, nowUtc);
                 settings.setValue(kLibIndexLastErrorMessageKey, errorText);
                 settings.endGroup();

                 if (output_) {
                   output_->appendLine("[Library Manager] Index update failed.");
                   if (!errorText.trimmed().isEmpty()) {
                     output_->appendLine(errorText);
                   }
                 }

                 if (!automatic) {
                   QMessageBox::warning(
                       this, tr("Index Update Failed"),
                       tr("Could not update the Library Manager index.\n\n%1").arg(errorText));
                 }
               }

               updateIndexStatusLabel();
               refreshInstalled();
             });
}
