#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QTemporaryDir>

#include "sketch_build_settings_store.h"

class TestSketchBuildSettingsStore final : public QObject {
  Q_OBJECT

 private slots:
  void initTestCase();
  void savesAndLoadsBySketchFolder();
  void profileSettings();
  void defaultProfiles();
  void profilePersistence();
};

void TestSketchBuildSettingsStore::initTestCase() {
  // Clean up any test settings
  QSettings settings("Rewritto", "Rewritto Ide");
  settings.remove("MainWindow/SketchBuild");
}

void TestSketchBuildSettingsStore::savesAndLoadsBySketchFolder() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path());

  QCoreApplication::setOrganizationName("Rewritto");
  QCoreApplication::setApplicationName("Rewritto Ide");

  const QString sketchA = QDir(dir.path()).absoluteFilePath("sketchA");
  const QString sketchB = QDir(dir.path()).absoluteFilePath("sketchB");

  // Create test sketch directories
  QDir().mkpath(sketchA);
  QDir().mkpath(sketchB);

  QFile fileA(sketchA + "/sketchA.ino");
  fileA.open(QIODevice::WriteOnly);
  fileA.write("void setup() {}\nvoid loop() {}\n");
  fileA.close();

  QFile fileB(sketchB + "/sketchB.ino");
  fileB.open(QIODevice::WriteOnly);
  fileB.write("void setup() {}\nvoid loop() {}\n");
  fileB.close();

  // Save settings
  SketchBuildSettingsStore::Settings settingsA;
  settingsA.fqbn = "arduino:avr:uno";
  settingsA.port = "/dev/ttyACM0";
  settingsA.currentProfile = SketchBuildSettingsStore::BuildProfile::Debug;
  SketchBuildSettingsStore::saveForSketch(sketchA, settingsA);

  SketchBuildSettingsStore::Settings settingsB;
  settingsB.fqbn = "arduino:avr:nano";
  settingsB.port = "/dev/ttyUSB0";
  settingsB.currentProfile = SketchBuildSettingsStore::BuildProfile::Release;
  SketchBuildSettingsStore::saveForSketch(sketchB, settingsB);

  // Load and verify
  const auto a = SketchBuildSettingsStore::loadForSketch(sketchA);
  QVERIFY(a.hasEntry);
  QCOMPARE(a.fqbn, QStringLiteral("arduino:avr:uno"));
  QCOMPARE(a.port, QStringLiteral("/dev/ttyACM0"));
  QCOMPARE(a.currentProfile, SketchBuildSettingsStore::BuildProfile::Debug);

  const auto b = SketchBuildSettingsStore::loadForSketch(sketchB);
  QVERIFY(b.hasEntry);
  QCOMPARE(b.fqbn, QStringLiteral("arduino:avr:nano"));
  QCOMPARE(b.port, QStringLiteral("/dev/ttyUSB0"));
  QCOMPARE(b.currentProfile, SketchBuildSettingsStore::BuildProfile::Release);

  // Test missing sketch
  const auto missing = SketchBuildSettingsStore::loadForSketch(dir.filePath("does-not-exist"));
  QVERIFY(!missing.hasEntry);
  QVERIFY(missing.fqbn.isEmpty());
  QVERIFY(missing.port.isEmpty());
}

void TestSketchBuildSettingsStore::profileSettings() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path());

  QCoreApplication::setOrganizationName("Rewritto");
  QCoreApplication::setApplicationName("Rewritto Ide");

  const QString sketchPath = QDir(dir.path()).absoluteFilePath("test_profile");
  QDir().mkpath(sketchPath);

  QFile file(sketchPath + "/test.ino");
  file.open(QIODevice::WriteOnly);
  file.write("void setup() {}\nvoid loop() {}\n");
  file.close();

  // Create settings with custom profile options
  SketchBuildSettingsStore::Settings settings;
  settings.fqbn = "arduino:avr:uno";
  settings.currentProfile = SketchBuildSettingsStore::BuildProfile::Debug;

  // Customize debug profile
  settings.debugProfile.optimizationLevel = "-O0";
  settings.debugProfile.debugLevel = "-g3";
  settings.debugProfile.enableLto = false;
  settings.debugProfile.customFlags = "-DDEBUG=1";

  // Customize release profile
  settings.releaseProfile.optimizationLevel = "-O3";
  settings.releaseProfile.debugLevel = "-g0";
  settings.releaseProfile.enableLto = true;
  settings.releaseProfile.customFlags = "-flto";

  SketchBuildSettingsStore::saveForSketch(sketchPath, settings);

  // Load and verify
  const auto loaded = SketchBuildSettingsStore::loadForSketch(sketchPath);
  QVERIFY(loaded.hasEntry);
  QCOMPARE(loaded.currentProfile, SketchBuildSettingsStore::BuildProfile::Debug);
  QCOMPARE(loaded.debugProfile.optimizationLevel, QString("-O0"));
  QCOMPARE(loaded.debugProfile.debugLevel, QString("-g3"));
  QCOMPARE(loaded.debugProfile.enableLto, false);
  QCOMPARE(loaded.debugProfile.customFlags, QString("-DDEBUG=1"));
  QCOMPARE(loaded.releaseProfile.optimizationLevel, QString("-O3"));
  QCOMPARE(loaded.releaseProfile.debugLevel, QString("-g0"));
  QCOMPARE(loaded.releaseProfile.enableLto, true);
  QCOMPARE(loaded.releaseProfile.customFlags, QString("-flto"));
}

void TestSketchBuildSettingsStore::defaultProfiles() {
  const auto releaseDefaults =
      SketchBuildSettingsStore::defaultProfileSettings(
          SketchBuildSettingsStore::BuildProfile::Release);
  const auto debugDefaults =
      SketchBuildSettingsStore::defaultProfileSettings(
          SketchBuildSettingsStore::BuildProfile::Debug);

  // Release defaults should optimize for size
  QVERIFY(!releaseDefaults.optimizationLevel.isEmpty());
  QVERIFY(releaseDefaults.optimizationLevel.contains("O"));

  // Debug defaults should have debug info
  QVERIFY(!debugDefaults.debugLevel.isEmpty());
  QVERIFY(debugDefaults.debugLevel.contains("g"));

  // Release typically uses LTO, debug doesn't
  QVERIFY(releaseDefaults.enableLto);
  QVERIFY(!debugDefaults.enableLto);
}

void TestSketchBuildSettingsStore::profilePersistence() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path());

  QCoreApplication::setOrganizationName("Rewritto");
  QCoreApplication::setApplicationName("Rewritto Ide");

  const QString sketchPath = QDir(dir.path()).absoluteFilePath("test_persist");
  QDir().mkpath(sketchPath);

  QFile file(sketchPath + "/test.ino");
  file.open(QIODevice::WriteOnly);
  file.write("void setup() {}\nvoid loop() {}\n");
  file.close();

  // Save with debug profile selected
  SketchBuildSettingsStore::Settings settings;
  settings.fqbn = "arduino:avr:uno";
  settings.currentProfile = SketchBuildSettingsStore::BuildProfile::Debug;
  settings.debugProfile = SketchBuildSettingsStore::defaultProfileSettings(
      SketchBuildSettingsStore::BuildProfile::Debug);
  settings.releaseProfile = SketchBuildSettingsStore::defaultProfileSettings(
      SketchBuildSettingsStore::BuildProfile::Release);

  // Customize some settings
  settings.debugProfile.customFlags = "-DTEST";
  settings.releaseProfile.enableLto = false;

  SketchBuildSettingsStore::saveForSketch(sketchPath, settings);

  // Reload and verify persistence
  const auto loaded = SketchBuildSettingsStore::loadForSketch(sketchPath);
  QVERIFY(loaded.hasEntry);
  QCOMPARE(loaded.currentProfile, SketchBuildSettingsStore::BuildProfile::Debug);
  QCOMPARE(loaded.debugProfile.customFlags, QString("-DTEST"));
  QCOMPARE(loaded.releaseProfile.enableLto, false);
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  TestSketchBuildSettingsStore tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "test_sketch_build_settings_store.moc"
