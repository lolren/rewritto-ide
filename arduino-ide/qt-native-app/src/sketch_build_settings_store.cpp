#include "sketch_build_settings_store.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QSettings>

namespace {
static constexpr auto kMainGroup = "MainWindow";
static constexpr auto kSketchBuildGroup = "SketchBuild";
static constexpr auto kPathKey = "path";
static constexpr auto kFqbnKey = "fqbn";
static constexpr auto kPortKey = "port";
static constexpr auto kCurrentProfileKey = "currentProfile";
static constexpr auto kOptimizeForDebugKey = "optimizeForDebug";  // Legacy
static constexpr auto kUpdatedUtcKey = "updatedUtc";

// Build profile keys
static constexpr auto kReleaseProfileGroup = "ReleaseProfile";
static constexpr auto kDebugProfileGroup = "DebugProfile";
static constexpr auto kOptimizationLevelKey = "optimizationLevel";
static constexpr auto kDebugLevelKey = "debugLevel";
static constexpr auto kEnableLtoKey = "enableLto";
static constexpr auto kCustomFlagsKey = "customFlags";

QString normalizeSketchFolder(const QString& folder) {
  if (folder.trimmed().isEmpty()) {
    return {};
  }
  return QDir(folder).absolutePath();
}

QString sketchIdForPath(const QString& sketchFolder) {
  const QByteArray bytes = normalizeSketchFolder(sketchFolder).toUtf8();
  if (bytes.isEmpty()) {
    return {};
  }
  return QString::fromLatin1(
      QCryptographicHash::hash(bytes, QCryptographicHash::Sha1).toHex());
}

void loadBuildProfileSettings(QSettings& settings, const QString& group,
                             SketchBuildSettingsStore::BuildProfileSettings& out) {
  settings.beginGroup(group);
  out.optimizationLevel = settings.value(kOptimizationLevelKey).toString();
  out.debugLevel = settings.value(kDebugLevelKey).toString();
  out.enableLto = settings.value(kEnableLtoKey, false).toBool();
  out.customFlags = settings.value(kCustomFlagsKey).toString();
  settings.endGroup();
}

void saveBuildProfileSettings(QSettings& settings, const QString& group,
                             const SketchBuildSettingsStore::BuildProfileSettings& in) {
  settings.beginGroup(group);
  settings.setValue(kOptimizationLevelKey, in.optimizationLevel);
  settings.setValue(kDebugLevelKey, in.debugLevel);
  settings.setValue(kEnableLtoKey, in.enableLto);
  settings.setValue(kCustomFlagsKey, in.customFlags);
  settings.endGroup();
}
}  // namespace

SketchBuildSettingsStore::Settings SketchBuildSettingsStore::loadForSketch(
    const QString& sketchFolder) {
  Settings out;

  const QString normalized = normalizeSketchFolder(sketchFolder);
  const QString id = sketchIdForPath(normalized);
  if (normalized.isEmpty() || id.isEmpty()) {
    return out;
  }

  QSettings settings;
  settings.beginGroup(kMainGroup);
  settings.beginGroup(kSketchBuildGroup);
  settings.beginGroup(id);
  const QString storedPath = settings.value(kPathKey).toString();
  if (storedPath == normalized) {
    out.hasEntry = true;
    out.fqbn = settings.value(kFqbnKey).toString();
    out.port = settings.value(kPortKey).toString();

    // Load current profile (default to Release for new entries)
    const QString profileStr = settings.value(kCurrentProfileKey, "Release").toString();
    out.currentProfile = (profileStr == "Debug") ? BuildProfile::Debug : BuildProfile::Release;

    // Load profile settings
    loadBuildProfileSettings(settings, kReleaseProfileGroup, out.releaseProfile);
    loadBuildProfileSettings(settings, kDebugProfileGroup, out.debugProfile);

    // Legacy migration: if optimizeForDebug was set, update debug profile
    const bool legacyOptimizeForDebug = settings.value(kOptimizeForDebugKey, false).toBool();
    if (legacyOptimizeForDebug && out.debugProfile.optimizationLevel.isEmpty()) {
      out.debugProfile.optimizationLevel = "-Og";  // Optimize for debugging
    }
  }
  settings.endGroup();
  settings.endGroup();
  settings.endGroup();

  return out;
}

void SketchBuildSettingsStore::saveForSketch(const QString& sketchFolder,
                                            const Settings& settings) {
  const QString normalized = normalizeSketchFolder(sketchFolder);
  const QString id = sketchIdForPath(normalized);
  if (normalized.isEmpty() || id.isEmpty()) {
    return;
  }

  QSettings qsettings;
  qsettings.beginGroup(kMainGroup);
  qsettings.beginGroup(kSketchBuildGroup);
  qsettings.beginGroup(id);
  qsettings.setValue(kPathKey, normalized);
  qsettings.setValue(kFqbnKey, settings.fqbn);
  qsettings.setValue(kPortKey, settings.port);
  qsettings.setValue(kCurrentProfileKey,
                    settings.currentProfile == BuildProfile::Debug ? "Debug" : "Release");
  qsettings.setValue(kUpdatedUtcKey, QDateTime::currentDateTimeUtc());

  // Save profile settings
  saveBuildProfileSettings(qsettings, kReleaseProfileGroup, settings.releaseProfile);
  saveBuildProfileSettings(qsettings, kDebugProfileGroup, settings.debugProfile);

  qsettings.endGroup();
  qsettings.endGroup();
  qsettings.endGroup();
}

QString SketchBuildSettingsStore::profileName(BuildProfile profile) {
  return (profile == BuildProfile::Debug) ? QObject::tr("Debug") : QObject::tr("Release");
}

SketchBuildSettingsStore::BuildProfileSettings
SketchBuildSettingsStore::defaultProfileSettings(BuildProfile profile) {
  BuildProfileSettings settings;
  if (profile == BuildProfile::Debug) {
    settings.name = profileName(profile);
    settings.optimizationLevel = "-Og";  // Optimize for debugging
    settings.debugLevel = "-g3";
    settings.enableLto = false;
  } else {
    settings.name = profileName(profile);
    settings.optimizationLevel = "-Os";  // Optimize for size
    settings.debugLevel = "-g2";
    settings.enableLto = true;
  }
  return settings;
}

// Legacy method for backward compatibility
void SketchBuildSettingsStore::saveForSketch(const QString& sketchFolder,
                                            const QString& fqbn,
                                            const QString& port,
                                            bool optimizeForDebug) {
  Settings settings;
  settings.fqbn = fqbn;
  settings.port = port;
  settings.currentProfile = optimizeForDebug ? BuildProfile::Debug : BuildProfile::Release;
  settings.releaseProfile = defaultProfileSettings(BuildProfile::Release);
  settings.debugProfile = defaultProfileSettings(BuildProfile::Debug);
  saveForSketch(sketchFolder, settings);
}
