#pragma once

#include <QDialog>
#include "update_manager.h"

class QLabel;
class QCheckBox;
class QTextEdit;
class QPushButton;

class UpdateDialog : public QDialog {
  Q_OBJECT

 public:
  explicit UpdateDialog(const UpdateManager::ReleaseInfo& release, QWidget* parent = nullptr);
  ~UpdateDialog() override;

  // User's choice
  bool downloadRequested() const;
  bool skipThisVersion() const;
  bool remindLater() const;

 private slots:
  void onDownloadClicked();
  void onSkipClicked();
  void onRemindLaterClicked();

 private:
  void setupUi();

  UpdateManager::ReleaseInfo release_;
  QLabel* versionLabel_ = nullptr;
  QLabel* dateLabel_ = nullptr;
  QTextEdit* changelogText_ = nullptr;
  QCheckBox* skipVersionCheck_ = nullptr;
  QPushButton* downloadButton_ = nullptr;
  QPushButton* skipButton_ = nullptr;
  QPushButton* remindButton_ = nullptr;

  bool downloadRequested_ = false;
};
