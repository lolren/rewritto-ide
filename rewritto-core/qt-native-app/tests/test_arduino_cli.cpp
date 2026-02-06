#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>

#include "arduino_cli.h"

class TestArduinoCli final : public QObject {
  Q_OBJECT

 private slots:
  void prependsConfigFileFlag();
  void doesNotPrependAdditionalUrlsFlag();
};

static bool makeExecutable(const QString& path) {
  QFile f(path);
  QFile::Permissions p = f.permissions();
  p |= QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther;
  return f.setPermissions(p);
}

void TestArduinoCli::prependsConfigFileFlag() {
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
    f.write("echo \"$@\"\n");
  }
  QVERIFY(makeExecutable(script));

  ArduinoCli cli;
  cli.setArduinoCliPath(script);

  QString output;
  QObject::connect(&cli, &ArduinoCli::outputReceived, &cli,
                   [&output](const QString& chunk) { output += chunk; });

  QSignalSpy finishedSpy(&cli, &ArduinoCli::finished);
  cli.run({QStringLiteral("version")});

  QVERIFY(finishedSpy.wait(2000));
  QVERIFY(output.contains("--config-file"));
  QVERIFY(output.contains(cfg));
  QVERIFY(output.contains("version"));

  qunsetenv("ARDUINO_CLI_CONFIG_FILE");
}

void TestArduinoCli::doesNotPrependAdditionalUrlsFlag() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  {
    QSettings settings;
    settings.beginGroup("Preferences");
    settings.setValue(
        "additionalUrls",
        QStringList{QStringLiteral("https://example.com/package_a.json"),
                    QStringLiteral("https://example.com/package_b.json")});
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
    f.write("echo \"$@\"\n");
  }
  QVERIFY(makeExecutable(script));

  ArduinoCli cli;
  cli.setArduinoCliPath(script);

  QString output;
  QObject::connect(&cli, &ArduinoCli::outputReceived, &cli,
                   [&output](const QString& chunk) { output += chunk; });

  QSignalSpy finishedSpy(&cli, &ArduinoCli::finished);
  cli.run({QStringLiteral("version")});
  QVERIFY(finishedSpy.wait(2000));

  QVERIFY(output.contains("--config-file"));
  QVERIFY(output.contains(cfg));
  QVERIFY(!output.contains("--additional-urls"));

  {
    QSettings settings;
    settings.beginGroup("Preferences");
    settings.remove("additionalUrls");
    settings.endGroup();
  }

  qunsetenv("ARDUINO_CLI_CONFIG_FILE");
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  QCoreApplication::setOrganizationName("RewrittoIdeTests");
  QCoreApplication::setApplicationName("test_arduino_cli");
  TestArduinoCli tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "test_arduino_cli.moc"
