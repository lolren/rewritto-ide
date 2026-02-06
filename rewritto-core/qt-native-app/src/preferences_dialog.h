#pragma once

#include <QDialog>
#include <QFont>
#include <QStringList>

class QCheckBox;
class QComboBox;
class QFontComboBox;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;
class QPushButton;
class QTabWidget;

class PreferencesDialog final : public QDialog {
  Q_OBJECT

 public:
  explicit PreferencesDialog(QWidget* parent = nullptr);

  QString theme() const;
  QString language() const;
  double uiScale() const;
  QString sketchbookDir() const;
  QStringList additionalUrls() const;
  QFont editorFont() const;
  int tabSize() const;
  bool insertSpaces() const;
  bool showIndentGuides() const;
  bool showWhitespace() const;
  QString defaultLineEnding() const;
  bool trimTrailingWhitespace() const;
  bool autosaveEnabled() const;
  int autosaveIntervalSeconds() const;
  QString warningsLevel() const;
  bool verboseCompile() const;
  bool verboseUpload() const;

  // Proxy settings
  QString proxyType() const;
  QString proxyHost() const;
  int proxyPort() const;
  QString proxyUsername() const;
  QString proxyPassword() const;
  QStringList noProxyHosts() const;

  void setTheme(QString theme);
  void setLanguage(QString language);
  void setUiScale(double scale);
  void setSketchbookDir(QString directory);
  void setAdditionalUrls(QStringList urls);
  void setEditorFont(QFont font);
  void setTabSize(int tabSize);
  void setInsertSpaces(bool insertSpaces);
  void setShowIndentGuides(bool enabled);
  void setShowWhitespace(bool enabled);
  void setDefaultLineEnding(QString lineEnding);
  void setTrimTrailingWhitespace(bool enabled);
  void setAutosaveEnabled(bool enabled);
  void setAutosaveIntervalSeconds(int seconds);
  void setWarningsLevel(QString level);
  void setVerboseCompile(bool verboseCompile);
  void setVerboseUpload(bool verboseUpload);

  // Proxy setters
  void setProxyType(QString type);
  void setProxyHost(QString host);
  void setProxyPort(int port);
  void setProxyUsername(QString username);
  void setProxyPassword(QString password);
  void setNoProxyHosts(QStringList hosts);

 signals:
  void themePreviewRequested(QString theme);
  void uiScalePreviewRequested(double scale);

 private:
  QComboBox* themeCombo_ = nullptr;
  QComboBox* languageCombo_ = nullptr;
  QComboBox* uiScaleCombo_ = nullptr;
  QLineEdit* sketchbookEdit_ = nullptr;
  QPlainTextEdit* additionalUrlsEdit_ = nullptr;
  QFontComboBox* editorFontCombo_ = nullptr;
  QSpinBox* editorFontSizeSpin_ = nullptr;
  QSpinBox* tabSizeSpin_ = nullptr;
  QCheckBox* insertSpacesCheck_ = nullptr;
  QCheckBox* showIndentGuidesCheck_ = nullptr;
  QCheckBox* showWhitespaceCheck_ = nullptr;
  QComboBox* defaultLineEndingCombo_ = nullptr;
  QCheckBox* trimTrailingWhitespaceCheck_ = nullptr;
  QCheckBox* autosaveCheck_ = nullptr;
  QSpinBox* autosaveIntervalSpin_ = nullptr;
  QComboBox* warningsCombo_ = nullptr;
  QCheckBox* verboseCompileCheck_ = nullptr;
  QCheckBox* verboseUploadCheck_ = nullptr;

  // Proxy settings widgets
  QTabWidget* tabWidget_ = nullptr;
  QComboBox* proxyTypeCombo_ = nullptr;
  QLineEdit* proxyHostEdit_ = nullptr;
  QSpinBox* proxyPortSpin_ = nullptr;
  QLineEdit* proxyUsernameEdit_ = nullptr;
  QLineEdit* proxyPasswordEdit_ = nullptr;
  QPlainTextEdit* noProxyHostsEdit_ = nullptr;
  QPushButton* testConnectionButton_ = nullptr;
};
