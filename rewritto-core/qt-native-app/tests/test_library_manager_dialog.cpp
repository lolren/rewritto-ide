#include <QtTest/QtTest>

#include <QApplication>
#include <QDateTime>
#include <QFile>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QTableView>
#include <QTemporaryDir>

#include "arduino_cli.h"
#include "library_manager_dialog.h"

class TestLibraryManagerDialog final : public QObject {
  Q_OBJECT

 private slots:
  void showSearchForRunsSearchAndPopulatesResults();
  void installedIncludeButtonEmitsProvidesIncludes();
};

static bool makeExecutable(const QString& path) {
  QFile f(path);
  QFile::Permissions p = f.permissions();
  p |= QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther;
  return f.setPermissions(p);
}

void TestLibraryManagerDialog::showSearchForRunsSearchAndPopulatesResults() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  {
    QSettings settings;
    settings.beginGroup("Preferences");
    settings.remove("additionalUrls");
    settings.endGroup();
  }

  const QString cfg = dir.filePath("arduino-cli.yaml");
  {
    QFile f(cfg);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write("# test\n");
  }
  qputenv("ARDUINO_CLI_CONFIG_FILE", cfg.toUtf8());

  const QString script = dir.filePath("fake-arduino-cli.sh");
  {
    QFile f(script);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write("#!/usr/bin/env bash\n");
    f.write("echo '{\"libraries\":[{\"name\":\"WiFi\",\"latest\":\"1.0.0\",\"available_versions\":[\"1.0.0\"]}]}'\n");
  }
  QVERIFY(makeExecutable(script));

  ArduinoCli cli;
  cli.setArduinoCliPath(script);

  LibraryManagerDialog dlg(&cli, nullptr);
  dlg.showSearchFor("WiFi");

  auto* edit = dlg.findChild<QLineEdit*>("libraryManagerSearchEdit");
  QVERIFY(edit);
  QCOMPARE(edit->text(), QStringLiteral("WiFi"));

  auto* view = dlg.findChild<QTableView*>("libraryManagerSearchView");
  QVERIFY(view);
  QVERIFY(view->model());

  QTRY_COMPARE(view->model()->rowCount(), 1);
  QCOMPARE(view->model()->index(0, 0).data().toString(), QStringLiteral("WiFi"));

  qunsetenv("ARDUINO_CLI_CONFIG_FILE");
}

void TestLibraryManagerDialog::installedIncludeButtonEmitsProvidesIncludes() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  {
    QSettings settings;
    settings.beginGroup("Preferences");
    settings.remove("additionalUrls");
    settings.endGroup();
  }
  {
    QSettings settings;
    settings.beginGroup("LibraryManager");
    settings.setValue("libIndexLastSuccessUtc", QDateTime::currentDateTimeUtc());
    settings.endGroup();
  }

  const QString cfg = dir.filePath("arduino-cli.yaml");
  {
    QFile f(cfg);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write("# test\n");
  }
  qputenv("ARDUINO_CLI_CONFIG_FILE", cfg.toUtf8());

  const QString script = dir.filePath("fake-arduino-cli.sh");
  {
    QFile f(script);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write("#!/usr/bin/env bash\n");
    f.write("echo '{\"installed_libraries\":[{\"library\":{\"name\":\"WiFi\",\"version\":\"1.0.0\",\"location\":\"user\",\"sentence\":\"A test lib\",\"provides_includes\":[\"WiFi.h\",\"WiFiUdp.h\"]}}]}'\n");
  }
  QVERIFY(makeExecutable(script));

  ArduinoCli cli;
  cli.setArduinoCliPath(script);

  LibraryManagerDialog dlg(&cli, nullptr);
  dlg.refresh();

  auto* includeBtn =
      dlg.findChild<QPushButton*>("libraryManagerInstalledIncludeButton");
  QVERIFY(includeBtn);

  QTRY_VERIFY(includeBtn->isEnabled());

  QSignalSpy includeSpy(&dlg, &LibraryManagerDialog::includeLibraryRequested);
  includeBtn->click();

  QCOMPARE(includeSpy.count(), 1);
  const auto args = includeSpy.takeFirst();
  QCOMPARE(args.at(0).toString(), QStringLiteral("WiFi"));
  QCOMPARE(args.at(1).toStringList(),
           (QStringList{QStringLiteral("WiFi.h"), QStringLiteral("WiFiUdp.h")}));

  qunsetenv("ARDUINO_CLI_CONFIG_FILE");
}

int main(int argc, char** argv) {
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);
  QCoreApplication::setOrganizationName("RewrittoIdeTests");
  QCoreApplication::setApplicationName("test_library_manager_dialog");
  TestLibraryManagerDialog tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "test_library_manager_dialog.moc"
