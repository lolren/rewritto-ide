#pragma once

#include <QString>
#include <QVariantMap>

class SketchBuildSettingsStore final {
 public:
  enum class BuildProfile {
    Release,
    Debug,
  };

  struct BuildProfileSettings final {
    QString name;
    QString optimizationLevel;  // -O0, -O1, -O2, -O3, -Os
    QString debugLevel;         // -g0, -g1, -g2, -g3
    bool enableLto = false;     // Link-time optimization
    QString customFlags;        // Additional compiler flags
  };

  struct Settings final {
    bool hasEntry = false;
    QString fqbn;
    QString port;
    BuildProfile currentProfile = BuildProfile::Release;
    BuildProfileSettings releaseProfile;
    BuildProfileSettings debugProfile;
  };

  static Settings loadForSketch(const QString& sketchFolder);
  static void saveForSketch(const QString& sketchFolder, const Settings& settings);

  // Legacy method for backward compatibility
  static void saveForSketch(const QString& sketchFolder, const QString& fqbn,
                           const QString& port, bool optimizeForDebug);

  static QString profileName(BuildProfile profile);
  static BuildProfileSettings defaultProfileSettings(BuildProfile profile);
};
