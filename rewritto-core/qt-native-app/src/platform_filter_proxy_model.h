#pragma once

#include <QSortFilterProxyModel>
#include <QString>

class PlatformFilterProxyModel final : public QSortFilterProxyModel {
 public:
  enum class ShowMode { All = 0, Installed, Updatable, NotInstalled };

  explicit PlatformFilterProxyModel(QObject* parent = nullptr);

  void setIdColumn(int column);
  void setInstalledColumn(int column);
  void setLatestColumn(int column);

  void setShowMode(ShowMode mode);
  ShowMode showMode() const;

  void setVendorFilter(QString vendor);
  QString vendorFilter() const;

  void setArchitectureFilter(QString architecture);
  QString architectureFilter() const;

  void setTypeFilter(QString type);
  QString typeFilter() const;

 protected:
  bool filterAcceptsRow(int sourceRow,
                        const QModelIndex& sourceParent) const override;

 private:
  int idColumn_ = 0;
  int installedColumn_ = 1;
  int latestColumn_ = 2;

  ShowMode showMode_ = ShowMode::All;
  QString vendorFilter_;
  QString architectureFilter_;
  QString typeFilter_;
};
