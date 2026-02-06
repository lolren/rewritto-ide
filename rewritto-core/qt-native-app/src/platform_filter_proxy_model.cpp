#include "platform_filter_proxy_model.h"

#include <QAbstractItemModel>
#include <QJsonArray>
#include <QJsonObject>
#include <QtGlobal>

namespace {
constexpr int kRolePlatformJson = Qt::UserRole + 2;

struct ParsedPlatformId {
  QString vendor;
  QString arch;
};

ParsedPlatformId parsePlatformId(const QString& platformId) {
  ParsedPlatformId out;
  const int colon = platformId.indexOf(':');
  if (colon < 0) {
    out.vendor = platformId.trimmed();
    return out;
  }
  out.vendor = platformId.left(colon).trimmed();
  out.arch = platformId.mid(colon + 1).trimmed();
  return out;
}
}  // namespace

PlatformFilterProxyModel::PlatformFilterProxyModel(QObject* parent)
    : QSortFilterProxyModel(parent) {
}

void PlatformFilterProxyModel::refreshFilter() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
  beginFilterChange();
  endFilterChange();
#else
  invalidateFilter();
#endif
}

void PlatformFilterProxyModel::setIdColumn(int column) {
  idColumn_ = column;
  refreshFilter();
}

void PlatformFilterProxyModel::setInstalledColumn(int column) {
  installedColumn_ = column;
  refreshFilter();
}

void PlatformFilterProxyModel::setLatestColumn(int column) {
  latestColumn_ = column;
  refreshFilter();
}

void PlatformFilterProxyModel::setShowMode(ShowMode mode) {
  if (showMode_ == mode) {
    return;
  }
  showMode_ = mode;
  refreshFilter();
}

PlatformFilterProxyModel::ShowMode PlatformFilterProxyModel::showMode() const {
  return showMode_;
}

void PlatformFilterProxyModel::setVendorFilter(QString vendor) {
  if (vendorFilter_ == vendor) {
    return;
  }
  vendorFilter_ = std::move(vendor);
  refreshFilter();
}

QString PlatformFilterProxyModel::vendorFilter() const {
  return vendorFilter_;
}

void PlatformFilterProxyModel::setArchitectureFilter(QString architecture) {
  if (architectureFilter_ == architecture) {
    return;
  }
  architectureFilter_ = std::move(architecture);
  refreshFilter();
}

QString PlatformFilterProxyModel::architectureFilter() const {
  return architectureFilter_;
}

void PlatformFilterProxyModel::setTypeFilter(QString type) {
  if (typeFilter_ == type) {
    return;
  }
  typeFilter_ = std::move(type);
  refreshFilter();
}

QString PlatformFilterProxyModel::typeFilter() const {
  return typeFilter_;
}

bool PlatformFilterProxyModel::filterAcceptsRow(
    int sourceRow,
    const QModelIndex& sourceParent) const {
  const QAbstractItemModel* model = sourceModel();
  if (!model) {
    return false;
  }

  const QString platformId =
      model->index(sourceRow, idColumn_, sourceParent).data().toString().trimmed();
  const ParsedPlatformId parsedId = parsePlatformId(platformId);

  const QString installed =
      model->index(sourceRow, installedColumn_, sourceParent).data().toString().trimmed();
  const QString latest =
      model->index(sourceRow, latestColumn_, sourceParent).data().toString().trimmed();

  const bool isInstalled = !installed.isEmpty();
  const bool isUpdatable = isInstalled && !latest.isEmpty() && installed != latest;

  switch (showMode_) {
    case ShowMode::All:
      break;
    case ShowMode::Installed:
      if (!isInstalled) {
        return false;
      }
      break;
    case ShowMode::Updatable:
      if (!isUpdatable) {
        return false;
      }
      break;
    case ShowMode::NotInstalled:
      if (isInstalled) {
        return false;
      }
      break;
  }

  if (!vendorFilter_.isEmpty() &&
      QString::compare(parsedId.vendor, vendorFilter_, Qt::CaseInsensitive) !=
          0) {
    return false;
  }

  if (!architectureFilter_.isEmpty() &&
      QString::compare(parsedId.arch, architectureFilter_, Qt::CaseInsensitive) !=
          0) {
    return false;
  }

  if (!typeFilter_.isEmpty()) {
    const QJsonObject platform =
        model->index(sourceRow, idColumn_, sourceParent).data(kRolePlatformJson).toJsonObject();
    if (platform.isEmpty()) {
      return false;
    }

    QString version = isInstalled ? installed : latest;
    if (version.isEmpty()) {
      version = platform.value(QStringLiteral("latest_version")).toString().trimmed();
    }
    const QJsonObject releases = platform.value(QStringLiteral("releases")).toObject();
    const QJsonObject rel = releases.value(version).toObject();
    const QJsonArray types = rel.value(QStringLiteral("types")).toArray();

    bool match = false;
    for (const QJsonValue& tv : types) {
      const QString t = tv.toString().trimmed();
      if (t.isEmpty()) {
        continue;
      }
      if (QString::compare(t, typeFilter_, Qt::CaseInsensitive) == 0) {
        match = true;
        break;
      }
    }
    if (!match) {
      return false;
    }
  }

  return true;
}
