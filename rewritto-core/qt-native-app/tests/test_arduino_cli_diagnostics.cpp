#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>

#include "arduino_cli.h"

class TestArduinoCliDiagnostics final : public QObject {
  Q_OBJECT

 private slots:
  void parsesGccStyleDiagnostics();
  void parsesFatalErrorDiagnostics();
  void parsesExitStatusAndCompilationError();
  void parsesPlatformNotInstalledDiagnostics();
  void parsesMissingToolDiagnostics();
  void parsesLibraryConflictDiagnostics();
  void parsesLinkerDiagnostics();
  void parsesUndefinedReferenceWithLine();
  void prefixesIncludeChainContext();
  void parsesUploadToolErrors();
};

static bool makeExecutable(const QString& path) {
  QFile f(path);
  QFile::Permissions p = f.permissions();
  p |= QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther;
  return f.setPermissions(p);
}

void TestArduinoCliDiagnostics::parsesGccStyleDiagnostics() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  {
    QSettings settings;
    settings.beginGroup("Preferences");
    settings.remove("additionalUrls");
    settings.endGroup();
  }

  // Ensure the code path that prepends --config-file is stable.
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
    f.write("echo \"sketch.ino:12:3: error: bad stuff\"\n");
    f.write("echo \"  foo();\"\n");
    f.write("echo \"  ^~~~\"\n");
    f.write("echo \"util.cpp:9: warning: beware\"\n");
  }
  QVERIFY(makeExecutable(script));

  ArduinoCli cli;
  cli.setArduinoCliPath(script);

  QSignalSpy diagSpy(&cli, &ArduinoCli::diagnosticFound);
  QSignalSpy finishedSpy(&cli, &ArduinoCli::finished);

  cli.run({QStringLiteral("compile")});
  QVERIFY(finishedSpy.wait(2000));

  QVERIFY(diagSpy.count() >= 2);

  const auto d0 = diagSpy.at(0);
  QCOMPARE(d0.at(0).toString(), QStringLiteral("sketch.ino"));
  QCOMPARE(d0.at(1).toInt(), 12);
  QCOMPARE(d0.at(2).toInt(), 3);
  QCOMPARE(d0.at(3).toString(), QStringLiteral("error"));
  QVERIFY(d0.at(4).toString().contains(QStringLiteral("bad stuff")));
  QVERIFY(d0.at(4).toString().contains(QStringLiteral("foo();")));
  QVERIFY(d0.at(4).toString().contains(QStringLiteral("^~~~")));

  const auto d1 = diagSpy.at(1);
  QCOMPARE(d1.at(0).toString(), QStringLiteral("util.cpp"));
  QCOMPARE(d1.at(1).toInt(), 9);
  QCOMPARE(d1.at(2).toInt(), 0);
  QCOMPARE(d1.at(3).toString(), QStringLiteral("warning"));
  QCOMPARE(d1.at(4).toString(), QStringLiteral("beware"));

  qunsetenv("ARDUINO_CLI_CONFIG_FILE");
}

void TestArduinoCliDiagnostics::parsesLibraryConflictDiagnostics() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  {
    QSettings settings;
    settings.beginGroup("Preferences");
    settings.remove("additionalUrls");
    settings.endGroup();
  }

  // Ensure the code path that prepends --config-file is stable.
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
    f.write("echo \"Multiple libraries were found for \\\"WiFi.h\\\"\"\n");
    f.write("echo \"  Used: /opt/arduino/libraries/WiFi\"\n");
    f.write("echo \"  Not used: /home/user/Rewritto-ide/libraries/WiFi\"\n");
  }
  QVERIFY(makeExecutable(script));

  ArduinoCli cli;
  cli.setArduinoCliPath(script);

  QSignalSpy diagSpy(&cli, &ArduinoCli::diagnosticFound);
  QSignalSpy finishedSpy(&cli, &ArduinoCli::finished);

  cli.run({QStringLiteral("compile")});
  QVERIFY(finishedSpy.wait(2000));

  QVERIFY(diagSpy.count() >= 1);
  const auto d0 = diagSpy.at(0);
  QCOMPARE(d0.at(0).toString(), QString{});
  QCOMPARE(d0.at(1).toInt(), 0);
  QCOMPARE(d0.at(2).toInt(), 0);
  QCOMPARE(d0.at(3).toString(), QStringLiteral("note"));
  QVERIFY(d0.at(4).toString().contains(QStringLiteral("Multiple libraries were found")));
  QVERIFY(d0.at(4).toString().contains(QStringLiteral("Used: /opt/arduino/libraries/WiFi")));
  QVERIFY(d0.at(4).toString().contains(
      QStringLiteral("Not used: /home/user/Rewritto-ide/libraries/WiFi")));

  qunsetenv("ARDUINO_CLI_CONFIG_FILE");
}

void TestArduinoCliDiagnostics::parsesUndefinedReferenceWithLine() {
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
    // Avoid shell command-substitution via backticks; the parser only cares
    // about the "undefined reference" substring.
    f.write("echo \"sketch.ino:42: undefined reference to foo\" \n");
  }
  QVERIFY(makeExecutable(script));

  ArduinoCli cli;
  cli.setArduinoCliPath(script);

  QSignalSpy diagSpy(&cli, &ArduinoCli::diagnosticFound);
  QSignalSpy finishedSpy(&cli, &ArduinoCli::finished);

  cli.run({QStringLiteral("compile")});
  QVERIFY(finishedSpy.wait(2000));

  QVERIFY(diagSpy.count() >= 1);
  const auto d0 = diagSpy.at(0);
  QCOMPARE(d0.at(0).toString(), QStringLiteral("sketch.ino"));
  QCOMPARE(d0.at(1).toInt(), 42);
  QCOMPARE(d0.at(2).toInt(), 0);
  QCOMPARE(d0.at(3).toString(), QStringLiteral("error"));
  QVERIFY(d0.at(4).toString().contains(QStringLiteral("undefined reference")));

  qunsetenv("ARDUINO_CLI_CONFIG_FILE");
}

void TestArduinoCliDiagnostics::prefixesIncludeChainContext() {
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
    f.write("echo \"In file included from /tmp/a.h:1:\" \n");
    f.write("echo \"                 from /tmp/b.cpp:2:\" \n");
    f.write("echo \"b.cpp:3:1: error: boom\" \n");
  }
  QVERIFY(makeExecutable(script));

  ArduinoCli cli;
  cli.setArduinoCliPath(script);

  QSignalSpy diagSpy(&cli, &ArduinoCli::diagnosticFound);
  QSignalSpy finishedSpy(&cli, &ArduinoCli::finished);

  cli.run({QStringLiteral("compile")});
  QVERIFY(finishedSpy.wait(2000));

  QVERIFY(diagSpy.count() >= 1);
  const auto d0 = diagSpy.at(0);
  QCOMPARE(d0.at(0).toString(), QStringLiteral("b.cpp"));
  QCOMPARE(d0.at(1).toInt(), 3);
  QCOMPARE(d0.at(2).toInt(), 1);
  QCOMPARE(d0.at(3).toString(), QStringLiteral("error"));
  const QString msg = d0.at(4).toString();
  QVERIFY(msg.contains(QStringLiteral("In file included from /tmp/a.h:1:")));
  QVERIFY(msg.contains(QStringLiteral("from /tmp/b.cpp:2:")));
  QVERIFY(msg.contains(QStringLiteral("boom")));

  qunsetenv("ARDUINO_CLI_CONFIG_FILE");
}

void TestArduinoCliDiagnostics::parsesFatalErrorDiagnostics() {
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
    f.write("echo \"main.cpp:7:10: fatal error: missing.h: No such file or directory\"\n");
  }
  QVERIFY(makeExecutable(script));

  ArduinoCli cli;
  cli.setArduinoCliPath(script);

  QSignalSpy diagSpy(&cli, &ArduinoCli::diagnosticFound);
  QSignalSpy finishedSpy(&cli, &ArduinoCli::finished);

  cli.run({QStringLiteral("compile")});
  QVERIFY(finishedSpy.wait(2000));

  QVERIFY(diagSpy.count() >= 1);
  const auto d0 = diagSpy.at(0);
  QCOMPARE(d0.at(0).toString(), QStringLiteral("main.cpp"));
  QCOMPARE(d0.at(1).toInt(), 7);
  QCOMPARE(d0.at(2).toInt(), 10);
  QCOMPARE(d0.at(3).toString(), QStringLiteral("error"));
  QVERIFY(d0.at(4).toString().contains(QStringLiteral("missing.h")));

  qunsetenv("ARDUINO_CLI_CONFIG_FILE");
}

void TestArduinoCliDiagnostics::parsesExitStatusAndCompilationError() {
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
    f.write("echo \"exit status 1\"\n");
    f.write("echo \"Compilation error: something went wrong\"\n");
  }
  QVERIFY(makeExecutable(script));

  ArduinoCli cli;
  cli.setArduinoCliPath(script);

  QSignalSpy diagSpy(&cli, &ArduinoCli::diagnosticFound);
  QSignalSpy finishedSpy(&cli, &ArduinoCli::finished);

  cli.run({QStringLiteral("compile")});
  QVERIFY(finishedSpy.wait(2000));

  QVERIFY(diagSpy.count() >= 2);
  const auto d0 = diagSpy.at(0);
  QCOMPARE(d0.at(0).toString(), QString{});
  QCOMPARE(d0.at(1).toInt(), 0);
  QCOMPARE(d0.at(2).toInt(), 0);
  QCOMPARE(d0.at(3).toString(), QStringLiteral("error"));
  QCOMPARE(d0.at(4).toString(), QStringLiteral("exit status 1"));

  const auto d1 = diagSpy.at(1);
  QCOMPARE(d1.at(0).toString(), QString{});
  QCOMPARE(d1.at(1).toInt(), 0);
  QCOMPARE(d1.at(2).toInt(), 0);
  QCOMPARE(d1.at(3).toString(), QStringLiteral("error"));
  QVERIFY(d1.at(4).toString().contains(QStringLiteral("Compilation error")));

  qunsetenv("ARDUINO_CLI_CONFIG_FILE");
}

void TestArduinoCliDiagnostics::parsesPlatformNotInstalledDiagnostics() {
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
    f.write("echo \"platform not installed: arduino:avr\"\n");
    f.write("echo \"Error: platform \\\"esp32:esp32\\\" is not installed\"\n");
  }
  QVERIFY(makeExecutable(script));

  ArduinoCli cli;
  cli.setArduinoCliPath(script);

  QSignalSpy diagSpy(&cli, &ArduinoCli::diagnosticFound);
  QSignalSpy finishedSpy(&cli, &ArduinoCli::finished);

  cli.run({QStringLiteral("compile")});
  QVERIFY(finishedSpy.wait(2000));

  QVERIFY(diagSpy.count() >= 2);

  const auto d0 = diagSpy.at(0);
  QCOMPARE(d0.at(0).toString(), QStringLiteral("Platform"));
  QCOMPARE(d0.at(1).toInt(), 0);
  QCOMPARE(d0.at(2).toInt(), 0);
  QCOMPARE(d0.at(3).toString(), QStringLiteral("error"));
  QVERIFY(d0.at(4).toString().contains(QStringLiteral("platform not installed")));

  const auto d1 = diagSpy.at(1);
  QCOMPARE(d1.at(0).toString(), QStringLiteral("Platform"));
  QCOMPARE(d1.at(1).toInt(), 0);
  QCOMPARE(d1.at(2).toInt(), 0);
  QCOMPARE(d1.at(3).toString(), QStringLiteral("error"));
  QVERIFY(d1.at(4).toString().contains(QStringLiteral("is not installed")));

  qunsetenv("ARDUINO_CLI_CONFIG_FILE");
}

void TestArduinoCliDiagnostics::parsesMissingToolDiagnostics() {
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
    f.write("echo \"exec: \\\"openocd\\\": executable file not found in \\$PATH\"\n");
    f.write("echo \"fork/exec esptool.py: no such file or directory\"\n");
    f.write("echo \"bash: 1: picotool: not found\"\n");
    f.write("echo \"bossac: command not found\"\n");
  }
  QVERIFY(makeExecutable(script));

  ArduinoCli cli;
  cli.setArduinoCliPath(script);

  QSignalSpy diagSpy(&cli, &ArduinoCli::diagnosticFound);
  QSignalSpy finishedSpy(&cli, &ArduinoCli::finished);

  cli.run({QStringLiteral("compile")});
  QVERIFY(finishedSpy.wait(2000));

  QVERIFY(diagSpy.count() >= 4);

  const auto d0 = diagSpy.at(0);
  QCOMPARE(d0.at(0).toString(), QStringLiteral("openocd"));
  QCOMPARE(d0.at(1).toInt(), 0);
  QCOMPARE(d0.at(2).toInt(), 0);
  QCOMPARE(d0.at(3).toString(), QStringLiteral("error"));
  QVERIFY(d0.at(4).toString().contains(QStringLiteral("executable file not found")));

  const auto d1 = diagSpy.at(1);
  QCOMPARE(d1.at(0).toString(), QStringLiteral("esptool.py"));
  QCOMPARE(d1.at(1).toInt(), 0);
  QCOMPARE(d1.at(2).toInt(), 0);
  QCOMPARE(d1.at(3).toString(), QStringLiteral("error"));
  QVERIFY(d1.at(4).toString().contains(QStringLiteral("no such file")));

  const auto d2 = diagSpy.at(2);
  QCOMPARE(d2.at(0).toString(), QStringLiteral("picotool"));
  QCOMPARE(d2.at(1).toInt(), 0);
  QCOMPARE(d2.at(2).toInt(), 0);
  QCOMPARE(d2.at(3).toString(), QStringLiteral("error"));
  QVERIFY(d2.at(4).toString().contains(QStringLiteral("not found")));

  const auto d3 = diagSpy.at(3);
  QCOMPARE(d3.at(0).toString(), QStringLiteral("bossac"));
  QCOMPARE(d3.at(1).toInt(), 0);
  QCOMPARE(d3.at(2).toInt(), 0);
  QCOMPARE(d3.at(3).toString(), QStringLiteral("error"));
  QVERIFY(d3.at(4).toString().contains(QStringLiteral("command not found")));

  qunsetenv("ARDUINO_CLI_CONFIG_FILE");
}

void TestArduinoCliDiagnostics::parsesLinkerDiagnostics() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  {
    QSettings settings;
    settings.beginGroup("Preferences");
    settings.remove("additionalUrls");
    settings.endGroup();
  }

  // Ensure the code path that prepends --config-file is stable.
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
    f.write("echo \"/tmp/build/sketch/sketch.ino.cpp.o:(.text.setup+0x0): undefined reference to 'foo'\"\n");
    f.write("echo \"collect2: error: ld returned 1 exit status\"\n");
    f.write("echo \"/usr/bin/ld: cannot find -lmissinglib\"\n");
  }
  QVERIFY(makeExecutable(script));

  ArduinoCli cli;
  cli.setArduinoCliPath(script);

  QSignalSpy diagSpy(&cli, &ArduinoCli::diagnosticFound);
  QSignalSpy finishedSpy(&cli, &ArduinoCli::finished);

  cli.run({QStringLiteral("compile")});
  QVERIFY(finishedSpy.wait(2000));

  QVERIFY(diagSpy.count() >= 3);

  const auto d0 = diagSpy.at(0);
  QCOMPARE(d0.at(0).toString(), QStringLiteral("/tmp/build/sketch/sketch.ino.cpp.o"));
  QCOMPARE(d0.at(1).toInt(), 0);
  QCOMPARE(d0.at(2).toInt(), 0);
  QCOMPARE(d0.at(3).toString(), QStringLiteral("error"));
  QVERIFY(d0.at(4).toString().contains(QStringLiteral("undefined reference to")));

  const auto d1 = diagSpy.at(1);
  QCOMPARE(d1.at(0).toString(), QStringLiteral("collect2"));
  QCOMPARE(d1.at(1).toInt(), 0);
  QCOMPARE(d1.at(2).toInt(), 0);
  QCOMPARE(d1.at(3).toString(), QStringLiteral("error"));
  QVERIFY(d1.at(4).toString().contains(QStringLiteral("ld returned")));

  const auto d2 = diagSpy.at(2);
  QCOMPARE(d2.at(0).toString(), QStringLiteral("ld"));
  QCOMPARE(d2.at(1).toInt(), 0);
  QCOMPARE(d2.at(2).toInt(), 0);
  QCOMPARE(d2.at(3).toString(), QStringLiteral("error"));
  QVERIFY(d2.at(4).toString().contains(QStringLiteral("cannot find -lmissinglib")));

  qunsetenv("ARDUINO_CLI_CONFIG_FILE");
}

void TestArduinoCliDiagnostics::parsesUploadToolErrors() {
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
    f.write("echo \"avrdude: ser_open(): can't open device \\\"/dev/ttyACM0\\\": No such file or directory\"\n");
    f.write("echo \"Error during upload: uploading error: exit status 1\"\n");
  }
  QVERIFY(makeExecutable(script));

  ArduinoCli cli;
  cli.setArduinoCliPath(script);

  QSignalSpy diagSpy(&cli, &ArduinoCli::diagnosticFound);
  QSignalSpy finishedSpy(&cli, &ArduinoCli::finished);

  cli.run({QStringLiteral("upload")});
  QVERIFY(finishedSpy.wait(2000));

  QVERIFY(diagSpy.count() >= 2);

  const auto d0 = diagSpy.at(0);
  QCOMPARE(d0.at(0).toString(), QStringLiteral("avrdude"));
  QCOMPARE(d0.at(1).toInt(), 0);
  QCOMPARE(d0.at(2).toInt(), 0);
  QCOMPARE(d0.at(3).toString(), QStringLiteral("error"));
  QVERIFY(d0.at(4).toString().contains(QStringLiteral("can't open device")));

  const auto d1 = diagSpy.at(1);
  QCOMPARE(d1.at(0).toString(), QStringLiteral("Upload"));
  QCOMPARE(d1.at(1).toInt(), 0);
  QCOMPARE(d1.at(2).toInt(), 0);
  QCOMPARE(d1.at(3).toString(), QStringLiteral("error"));
  QVERIFY(d1.at(4).toString().contains(QStringLiteral("Error during upload")));

  qunsetenv("ARDUINO_CLI_CONFIG_FILE");
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  QCoreApplication::setOrganizationName("RewrittoIdeTests");
  QCoreApplication::setApplicationName("test_arduino_cli_diagnostics");
  TestArduinoCliDiagnostics tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "test_arduino_cli_diagnostics.moc"
