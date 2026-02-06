#include <QtTest/QtTest>

#include <QJsonArray>
#include <QJsonObject>
#include <QStandardItemModel>

#include "platform_filter_proxy_model.h"

namespace {
constexpr int kRolePlatformJson = Qt::UserRole + 2;

void addPlatform(QStandardItemModel* model,
                 QString id,
                 QString installed,
                 QString latest,
                 QString name = QStringLiteral("Platform")) {
  QList<QStandardItem*> row;
  row << new QStandardItem(std::move(id)) << new QStandardItem(std::move(installed))
      << new QStandardItem(std::move(latest)) << new QStandardItem(std::move(name));
  model->appendRow(row);
}

void addPlatformWithTypes(QStandardItemModel* model,
                          QString id,
                          QString installed,
                          QString latest,
                          QStringList types,
                          QString name = QStringLiteral("Platform")) {
  QList<QStandardItem*> row;
  auto* idItem = new QStandardItem(id);
  idItem->setData(id, Qt::DisplayRole);

  QString version = !installed.trimmed().isEmpty() ? installed : latest;
  if (version.trimmed().isEmpty()) {
    version = QStringLiteral("0.0.0");
  }

  QJsonObject rel;
  QJsonArray typeArray;
  for (const QString& t : types) {
    typeArray.append(t);
  }
  rel.insert(QStringLiteral("types"), typeArray);
  QJsonObject releases;
  releases.insert(version, rel);

  QJsonObject platform;
  platform.insert(QStringLiteral("id"), id);
  platform.insert(QStringLiteral("installed_version"), installed);
  platform.insert(QStringLiteral("latest_version"), latest);
  platform.insert(QStringLiteral("releases"), releases);

  idItem->setData(platform, kRolePlatformJson);
  row << idItem << new QStandardItem(std::move(installed))
      << new QStandardItem(std::move(latest)) << new QStandardItem(std::move(name));
  model->appendRow(row);
}
}  // namespace

class TestPlatformFilterProxyModel final : public QObject {
  Q_OBJECT

 private slots:
  void filtersByShowMode();
  void filtersByVendorAndArchitecture();
  void filtersByType();
};

void TestPlatformFilterProxyModel::filtersByShowMode() {
  QStandardItemModel model;
  model.setColumnCount(4);
  addPlatform(&model, QStringLiteral("rewritto:avr"), QStringLiteral("1.8.6"),
              QStringLiteral("1.8.6"));
  addPlatform(&model, QStringLiteral("rewritto:samd"), QStringLiteral("1.8.13"),
              QStringLiteral("1.8.14"));
  addPlatform(&model, QStringLiteral("stm32duino:stm32"), QStringLiteral("2.0.0"),
              QString{});
  addPlatform(&model, QStringLiteral("espressif:esp32"), QString{},
              QStringLiteral("3.0.0"));

  PlatformFilterProxyModel proxy;
  proxy.setIdColumn(0);
  proxy.setInstalledColumn(1);
  proxy.setLatestColumn(2);
  proxy.setSourceModel(&model);

  QCOMPARE(proxy.rowCount(), 4);

  proxy.setShowMode(PlatformFilterProxyModel::ShowMode::Installed);
  QCOMPARE(proxy.rowCount(), 3);

  proxy.setShowMode(PlatformFilterProxyModel::ShowMode::NotInstalled);
  QCOMPARE(proxy.rowCount(), 1);
  QCOMPARE(proxy.index(0, 0).data().toString(), QStringLiteral("espressif:esp32"));

  proxy.setShowMode(PlatformFilterProxyModel::ShowMode::Updatable);
  QCOMPARE(proxy.rowCount(), 1);
  QCOMPARE(proxy.index(0, 0).data().toString(), QStringLiteral("rewritto:samd"));
}

void TestPlatformFilterProxyModel::filtersByVendorAndArchitecture() {
  QStandardItemModel model;
  model.setColumnCount(4);
  addPlatform(&model, QStringLiteral("rewritto:avr"), QStringLiteral("1.8.6"),
              QStringLiteral("1.8.6"));
  addPlatform(&model, QStringLiteral("rewritto:samd"), QStringLiteral("1.8.13"),
              QStringLiteral("1.8.14"));
  addPlatform(&model, QStringLiteral("espressif:esp32"), QString{},
              QStringLiteral("3.0.0"));

  PlatformFilterProxyModel proxy;
  proxy.setIdColumn(0);
  proxy.setInstalledColumn(1);
  proxy.setLatestColumn(2);
  proxy.setSourceModel(&model);

  proxy.setVendorFilter(QStringLiteral("rewritto"));
  QCOMPARE(proxy.rowCount(), 2);

  proxy.setArchitectureFilter(QStringLiteral("avr"));
  QCOMPARE(proxy.rowCount(), 1);
  QCOMPARE(proxy.index(0, 0).data().toString(), QStringLiteral("rewritto:avr"));

  proxy.setShowMode(PlatformFilterProxyModel::ShowMode::Updatable);
  QCOMPARE(proxy.rowCount(), 0);
}

void TestPlatformFilterProxyModel::filtersByType() {
  QStandardItemModel model;
  model.setColumnCount(4);
  addPlatformWithTypes(&model, QStringLiteral("rewritto:avr"), QStringLiteral("1.8.6"),
                       QStringLiteral("1.8.6"), {QStringLiteral("Arduino")});
  addPlatformWithTypes(&model, QStringLiteral("rewritto:samd"), QStringLiteral("1.8.13"),
                       QStringLiteral("1.8.14"),
                       {QStringLiteral("Arduino"), QStringLiteral("Contributed")});
  addPlatformWithTypes(&model, QStringLiteral("espressif:esp32"), QString{},
                       QStringLiteral("3.0.0"),
                       {QStringLiteral("ESP32"), QStringLiteral("Contributed")});

  PlatformFilterProxyModel proxy;
  proxy.setIdColumn(0);
  proxy.setInstalledColumn(1);
  proxy.setLatestColumn(2);
  proxy.setSourceModel(&model);

  proxy.setTypeFilter(QStringLiteral("esp32"));
  QCOMPARE(proxy.rowCount(), 1);
  QCOMPARE(proxy.index(0, 0).data().toString(), QStringLiteral("espressif:esp32"));

  proxy.setTypeFilter(QStringLiteral("ARDUINO"));
  QCOMPARE(proxy.rowCount(), 2);
}

QTEST_MAIN(TestPlatformFilterProxyModel)

#include "test_platform_filter_proxy_model.moc"
