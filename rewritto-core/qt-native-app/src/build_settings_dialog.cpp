#include "build_settings_dialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QMessageBox>

namespace {
constexpr int kNameRole = Qt::UserRole;
}

BuildSettingsDialog::BuildSettingsDialog(QWidget* parent) : QDialog(parent) {
  setupUi();
}

BuildSettingsDialog::~BuildSettingsDialog() = default;

void BuildSettingsDialog::setupUi() {
  setWindowTitle(tr("Build Settings"));
  setModal(true);

  auto* layout = new QVBoxLayout(this);

  // Profile selector
  auto* profileLayout = new QHBoxLayout();
  profileLayout->addWidget(new QLabel(tr("Build Profile:")));
  profileCombo_ = new QComboBox(this);
  profileCombo_->addItem(tr("Release"), static_cast<int>(SketchBuildSettingsStore::BuildProfile::Release));
  profileCombo_->addItem(tr("Debug"), static_cast<int>(SketchBuildSettingsStore::BuildProfile::Debug));
  connect(profileCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &BuildSettingsDialog::onProfileChanged);
  profileLayout->addWidget(profileCombo_, 1);
  layout->addLayout(profileLayout);

  // Profile settings
  auto* form = new QFormLayout();

  // Optimization level
  optimizationEdit_ = new QLineEdit(this);
  optimizationEdit_->setPlaceholderText(tr("e.g., -Os, -O2, -Og"));
  form->addRow(tr("Optimization Level:"), optimizationEdit_);

  // Debug level
  debugLevelCombo_ = new QComboBox(this);
  debugLevelCombo_->addItem(tr("None"), "0");
  debugLevelCombo_->addItem(tr("Minimal (-g1)", "1");
  debugLevelCombo_->addItem(tr("Default (-g2)", "2");
  debugLevelCombo_->addItem(tr("Maximum (-g3)", "3");
  debugLevelCombo_->setCurrentIndex(2);
  form->addRow(tr("Debug Information:"), debugLevelCombo_);

  // Link-time optimization
  enableLtoCheck_ = new QCheckBox(tr("Enable Link-Time Optimization (LTO)"), this);
  form->addRow(QString{}, enableLtoCheck_);

  // Custom flags
  customFlagsEdit_ = new QLineEdit(this);
  customFlagsEdit_->setPlaceholderText(tr("Additional compiler flags (optional)"));
  form->addRow(tr("Custom Flags:"), customFlagsEdit_);

  layout->addLayout(form);

  // Info label
  auto* infoLabel = new QLabel(
      tr("<i>Note: Optimization and debug flags are passed to the compiler. "
        "Settings are per-sketch and profile-specific.</i>"),
      this);
  infoLabel->setWordWrap(true);
  layout->addWidget(infoLabel);

  // Reset button
  auto* resetButton = new QPushButton(tr("Reset to Defaults"), this);
  connect(resetButton, &QPushButton::clicked, this, &BuildSettingsDialog::onResetToDefaults);
  layout->addWidget(resetButton);

  // Dialog buttons
  auto* buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  layout->addWidget(buttons);

  // Set initial state
  onProfileChanged(profileCombo_->currentIndex());
}

void BuildSettingsDialog::setSettings(const SketchBuildSettingsStore::Settings& settings) {
  settings_ = settings;
  currentProfile_ = settings.currentProfile;

  const int profileIndex = (currentProfile_ == SketchBuildSettingsStore::BuildProfile::Debug) ? 1 : 0;
  profileCombo_->setCurrentIndex(profileIndex);

  loadCurrentProfile();
}

SketchBuildSettingsStore::Settings BuildSettingsDialog::settings() const {
  return settings_;
}

void BuildSettingsDialog::onProfileChanged(int index) {
  if (index < 0) {
    return;
  }

  // Save current profile settings before switching
  saveCurrentProfile();

  // Load new profile
  const int profileValue = profileCombo_->itemData(index).toInt();
  currentProfile_ = static_cast<SketchBuildSettingsStore::BuildProfile>(profileValue);

  loadCurrentProfile();
}

void BuildSettingsDialog::loadCurrentProfile() {
  SketchBuildSettingsStore::BuildProfileSettings profileSettings;

  if (currentProfile_ == SketchBuildSettingsStore::BuildProfile::Debug) {
    profileSettings = settings_.debugProfile;
    if (profileSettings.optimizationLevel.isEmpty()) {
      profileSettings = SketchBuildSettingsStore::defaultProfileSettings(
          SketchBuildSettingsStore::BuildProfile::Debug);
    }
  } else {
    profileSettings = settings_.releaseProfile;
    if (profileSettings.optimizationLevel.isEmpty()) {
      profileSettings = SketchBuildSettingsStore::defaultProfileSettings(
          SketchBuildSettingsStore::BuildProfile::Release);
    }
  }

  optimizationEdit_->setText(profileSettings.optimizationLevel);

  // Set debug level combo
  const QString debugLevel = profileSettings.debugLevel;
  if (debugLevel == "-g1" || debugLevel == "1") {
    debugLevelCombo_->setCurrentIndex(1);
  } else if (debugLevel == "-g2" || debugLevel == "2") {
    debugLevelCombo_->setCurrentIndex(2);
  } else if (debugLevel == "-g3" || debugLevel == "3") {
    debugLevelCombo_->setCurrentIndex(3);
  } else {
    debugLevelCombo_->setCurrentIndex(0);
  }

  enableLtoCheck_->setChecked(profileSettings.enableLto);
  customFlagsEdit_->setText(profileSettings.customFlags);
}

void BuildSettingsDialog::saveCurrentProfile() {
  const SketchBuildSettingsStore::BuildProfileSettings profileSettings = currentProfileSettings();

  if (currentProfile_ == SketchBuildSettingsStore::BuildProfile::Debug) {
    settings_.debugProfile = profileSettings;
  } else {
    settings_.releaseProfile = profileSettings;
  }
}

SketchBuildSettingsStore::BuildProfileSettings BuildSettingsDialog::currentProfileSettings() const {
  SketchBuildSettingsStore::BuildProfileSettings settings;

  settings.optimizationLevel = optimizationEdit_->text().trimmed();
  settings.debugLevel = "-g" + debugLevelCombo_->currentData().toString();
  settings.enableLto = enableLtoCheck_->isChecked();
  settings.customFlags = customFlagsEdit_->text().trimmed();

  return settings;
}

void BuildSettingsDialog::setCurrentProfileSettings(
    const SketchBuildSettingsStore::BuildProfileSettings& settings) {

  optimizationEdit_->setText(settings.optimizationLevel);

  const QString debugLevel = settings.debugLevel;
  if (debugLevel == "-g1" || debugLevel == "1") {
    debugLevelCombo_->setCurrentIndex(1);
  } else if (debugLevel == "-g2" || debugLevel == "2") {
    debugLevelCombo_->setCurrentIndex(2);
  } else if (debugLevel == "-g3" || debugLevel == "3") {
    debugLevelCombo_->setCurrentIndex(3);
  } else {
    debugLevelCombo_->setCurrentIndex(0);
  }

  enableLtoCheck_->setChecked(settings.enableLto);
  customFlagsEdit_->setText(settings.customFlags);
}

void BuildSettingsDialog::onResetToDefaults() {
  const auto defaultSettings = SketchBuildSettingsStore::defaultProfileSettings(currentProfile_);
  setCurrentProfileSettings(defaultSettings);
}
