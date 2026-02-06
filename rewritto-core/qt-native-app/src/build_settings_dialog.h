#pragma once

#include <QDialog>
#include <QVariantMap>

#include "sketch_build_settings_store.h"

class QComboBox;
class QLineEdit;
class QCheckBox;
class QSpinBox;
class QTabWidget;

class BuildSettingsDialog final : public QDialog {
  Q_OBJECT

 public:
  explicit BuildSettingsDialog(QWidget* parent = nullptr);
  ~BuildSettingsDialog() override;

  void setSettings(const SketchBuildSettingsStore::Settings& settings);
  SketchBuildSettingsStore::Settings settings() const;

 private slots:
  void onProfileChanged(int index);
  void onResetToDefaults();

 private:
  void setupUi();
  void loadCurrentProfile();
  void saveCurrentProfile();
  SketchBuildSettingsStore::BuildProfileSettings currentProfileSettings() const;
  void setCurrentProfileSettings(const SketchBuildSettingsStore::BuildProfileSettings& settings);

  // Profile selector
  QComboBox* profileCombo_ = nullptr;

  // Profile settings widgets
  QLineEdit* optimizationEdit_ = nullptr;
  QComboBox* debugLevelCombo_ = nullptr;
  QCheckBox* enableLtoCheck_ = nullptr;
  QLineEdit* customFlagsEdit_ = nullptr;

  SketchBuildSettingsStore::Settings settings_;
  SketchBuildSettingsStore::BuildProfile currentProfile_ = SketchBuildSettingsStore::BuildProfile::Release;
};
