#include "preferences_dialog.h"

#include <algorithm>
#include <cmath>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFontComboBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QInputDialog>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QSignalBlocker>
#include <QTimer>

PreferencesDialog::PreferencesDialog(QWidget* parent) : QDialog(parent) {
  setWindowTitle(tr("Preferences"));

  tabWidget_ = new QTabWidget(this);

  // === General Tab ===
  themeCombo_ = new QComboBox(this);
  themeCombo_->addItem(tr("System"), "system");
  themeCombo_->addItem(tr("Light"), "light");
  themeCombo_->addItem(tr("Dark"), "dark");
  themeCombo_->addItem(tr("Arduino"), "arduino");
  themeCombo_->addItem(tr("Oceanic"), "oceanic");
  themeCombo_->addItem(tr("Cyber"), "cyber");
  themeCombo_->addItem(tr("Graphite"), "graphite");
  themeCombo_->addItem(tr("Nord"), "nord");
  themeCombo_->addItem(tr("Everforest"), "everforest");
  themeCombo_->addItem(tr("Dawn"), "dawn");
  themeCombo_->addItem(tr("Y2K"), "y2k");
  connect(themeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int) { emit themePreviewRequested(theme()); });

  languageCombo_ = new QComboBox(this);
  languageCombo_->addItem(tr("System Default"), "system");
  languageCombo_->addItem(tr("English"), "en");
  languageCombo_->addItem(tr("Spanish"), "es");
  languageCombo_->setCurrentIndex(0);

  uiScaleCombo_ = new QComboBox(this);
  uiScaleCombo_->addItem("80%", 0.8);
  uiScaleCombo_->addItem("90%", 0.9);
  uiScaleCombo_->addItem("100%", 1.0);
  uiScaleCombo_->addItem("110%", 1.1);
  uiScaleCombo_->addItem("125%", 1.25);
  uiScaleCombo_->addItem("150%", 1.5);
  uiScaleCombo_->setCurrentIndex(uiScaleCombo_->findData(1.0));
  connect(uiScaleCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int) { emit uiScalePreviewRequested(uiScale()); });

  sketchbookEdit_ = new QLineEdit(this);
  sketchbookEdit_->setPlaceholderText(tr("e.g. %1").arg(QDir::homePath() + "/Rewritto"));
  auto* browseSketchbook = new QPushButton(tr("Browse…"), this);
  connect(browseSketchbook, &QPushButton::clicked, this, [this] {
    const QString current = sketchbookEdit_ ? sketchbookEdit_->text().trimmed()
                                            : QString{};
    const QString initial = current.isEmpty() ? (QDir::homePath() + "/Rewritto")
                                              : current;
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Select Sketchbook Folder"),
                                                          initial);
    if (!dir.isEmpty() && sketchbookEdit_) {
      sketchbookEdit_->setText(dir);
    }
  });
  auto* sketchbookRow = new QWidget(this);
  auto* sketchbookLayout = new QHBoxLayout(sketchbookRow);
  sketchbookLayout->setContentsMargins(0, 0, 0, 0);
  sketchbookLayout->addWidget(sketchbookEdit_, 1);
  sketchbookLayout->addWidget(browseSketchbook);

  auto* generalForm = new QFormLayout();
  generalForm->addRow(tr("Theme:"), themeCombo_);
  generalForm->addRow(tr("Language:"), languageCombo_);
  generalForm->addRow(tr("Interface scale:"), uiScaleCombo_);
  generalForm->addRow(tr("Sketchbook:"), sketchbookRow);

  auto* generalTab = new QWidget(this);
  generalTab->setLayout(generalForm);
  tabWidget_->addTab(generalTab, tr("General"));

  // === Editor Tab ===
  editorFontCombo_ = new QFontComboBox(this);
  editorFontCombo_->setFontFilters(QFontComboBox::MonospacedFonts);
  editorFontCombo_->setMaximumWidth(360);

  editorFontSizeSpin_ = new QSpinBox(this);
  editorFontSizeSpin_->setRange(6, 48);
  int defaultFontSize = QFontDatabase::systemFont(QFontDatabase::FixedFont).pointSize();
  if (defaultFontSize <= 0) {
    defaultFontSize = 12;
  }
  editorFontSizeSpin_->setValue(defaultFontSize);

  auto* editorFontRow = new QWidget(this);
  auto* editorFontLayout = new QHBoxLayout(editorFontRow);
  editorFontLayout->setContentsMargins(0, 0, 0, 0);
  editorFontLayout->addWidget(editorFontCombo_, 1);
  editorFontLayout->addWidget(new QLabel(tr("Size:"), editorFontRow));
  editorFontLayout->addWidget(editorFontSizeSpin_);

  tabSizeSpin_ = new QSpinBox(this);
  tabSizeSpin_->setRange(1, 8);
  tabSizeSpin_->setValue(2);

  insertSpacesCheck_ = new QCheckBox(tr("Insert spaces for tabs"), this);
  insertSpacesCheck_->setChecked(true);

  showIndentGuidesCheck_ = new QCheckBox(tr("Show indent guides"), this);
  showIndentGuidesCheck_->setChecked(true);

  showWhitespaceCheck_ = new QCheckBox(tr("Show whitespace characters"), this);
  showWhitespaceCheck_->setChecked(false);

  defaultLineEndingCombo_ = new QComboBox(this);
  defaultLineEndingCombo_->addItem(tr("LF"), QStringLiteral("LF"));
  defaultLineEndingCombo_->addItem(tr("CRLF"), QStringLiteral("CRLF"));
  defaultLineEndingCombo_->setCurrentIndex(
      defaultLineEndingCombo_->findData(QStringLiteral("LF")));

  trimTrailingWhitespaceCheck_ =
      new QCheckBox(tr("Trim trailing whitespace on save"), this);
  trimTrailingWhitespaceCheck_->setChecked(false);

  autosaveCheck_ = new QCheckBox(tr("Autosave files"), this);
  autosaveCheck_->setChecked(false);

  autosaveIntervalSpin_ = new QSpinBox(this);
  autosaveIntervalSpin_->setRange(5, 600);
  autosaveIntervalSpin_->setValue(30);
  autosaveIntervalSpin_->setSuffix(tr(" s"));
  autosaveIntervalSpin_->setEnabled(false);
  connect(autosaveCheck_, &QCheckBox::toggled, this, [this](bool enabled) {
    if (autosaveIntervalSpin_) {
      autosaveIntervalSpin_->setEnabled(enabled);
    }
  });

  auto* editorForm = new QFormLayout();
  editorForm->addRow(tr("Editor font:"), editorFontRow);
  editorForm->addRow(tr("Tab size:"), tabSizeSpin_);
  editorForm->addRow(QString{}, insertSpacesCheck_);
  editorForm->addRow(QString{}, showIndentGuidesCheck_);
  editorForm->addRow(QString{}, showWhitespaceCheck_);
  editorForm->addRow(tr("Default line ending:"), defaultLineEndingCombo_);
  editorForm->addRow(QString{}, trimTrailingWhitespaceCheck_);
  editorForm->addRow(QString{}, autosaveCheck_);
  editorForm->addRow(tr("Autosave interval:"), autosaveIntervalSpin_);

  auto* editorTab = new QWidget(this);
  editorTab->setLayout(editorForm);
  tabWidget_->addTab(editorTab, tr("Editor"));

  // === Network Tab (Proxy Settings) ===
  proxyTypeCombo_ = new QComboBox(this);
  proxyTypeCombo_->addItem(tr("No Proxy"), "none");
  proxyTypeCombo_->addItem(tr("HTTP"), "http");
  proxyTypeCombo_->addItem(tr("HTTPS"), "https");
  proxyTypeCombo_->addItem(tr("SOCKS5"), "socks5");

  proxyHostEdit_ = new QLineEdit(this);
  proxyHostEdit_->setPlaceholderText(tr("proxy.example.com"));

  proxyPortSpin_ = new QSpinBox(this);
  proxyPortSpin_->setRange(1, 65535);
  proxyPortSpin_->setValue(8080);
  proxyPortSpin_->setEnabled(false);

  proxyUsernameEdit_ = new QLineEdit(this);
  proxyUsernameEdit_->setPlaceholderText(tr("(optional)"));
  proxyUsernameEdit_->setEnabled(false);

  proxyPasswordEdit_ = new QLineEdit(this);
  proxyPasswordEdit_->setEchoMode(QLineEdit::Password);
  proxyPasswordEdit_->setPlaceholderText(tr("(optional)"));
  proxyPasswordEdit_->setEnabled(false);

  noProxyHostsEdit_ = new QPlainTextEdit(this);
  noProxyHostsEdit_->setPlaceholderText(
      tr("One hostname or pattern per line\ne.g., localhost, *.local, 192.168.*"));
  noProxyHostsEdit_->setMinimumHeight(80);

  testConnectionButton_ = new QPushButton(tr("Test Connection"), this);
  testConnectionButton_->setEnabled(false);
  connect(testConnectionButton_, &QPushButton::clicked, this, [this]() {
    const QString type = proxyType();
    const QString host = proxyHost();
    if (type != QStringLiteral("none") && host.isEmpty()) {
      QMessageBox::warning(this, tr("Connection Test"),
                           tr("Please set a proxy host first."));
      return;
    }

    const QString originalButtonText = testConnectionButton_->text();
    testConnectionButton_->setEnabled(false);
    testConnectionButton_->setText(tr("Testing…"));

    auto* manager = new QNetworkAccessManager(this);
    if (type != QStringLiteral("none")) {
      QNetworkProxy::ProxyType proxyType = QNetworkProxy::HttpProxy;
      if (type == QStringLiteral("socks5")) {
        proxyType = QNetworkProxy::Socks5Proxy;
      }
      QNetworkProxy proxy(proxyType, host, proxyPort(), proxyUsername(),
                          proxyPassword());
      manager->setProxy(proxy);
    } else {
      manager->setProxy(QNetworkProxy::NoProxy);
    }

    QNetworkRequest req(QUrl(QStringLiteral("https://downloads.arduino.cc/")));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(12000);

    QNetworkReply* reply = manager->head(req);
    auto* timeoutTimer = new QTimer(reply);
    timeoutTimer->setSingleShot(true);
    connect(timeoutTimer, &QTimer::timeout, reply, [reply] {
      if (reply->isRunning()) {
        reply->abort();
      }
    });
    timeoutTimer->start(13000);

    QPointer<QPushButton> buttonGuard = testConnectionButton_;
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, manager, buttonGuard, originalButtonText]() {
              const int statusCode =
                  reply->attribute(QNetworkRequest::HttpStatusCodeAttribute)
                      .toInt();
              const bool success =
                  (reply->error() == QNetworkReply::NoError &&
                   (statusCode == 0 ||
                    (statusCode >= 200 && statusCode < 400)));

              if (buttonGuard) {
                buttonGuard->setText(originalButtonText);
                const bool hasProxy =
                    proxyTypeCombo_ &&
                    proxyTypeCombo_->currentData().toString() !=
                        QStringLiteral("none");
                buttonGuard->setEnabled(hasProxy);
              }

              if (success) {
                QMessageBox::information(
                    this, tr("Connection Test"),
                    tr("Connection successful.\nHTTP status: %1")
                        .arg(statusCode == 0 ? tr("OK") : QString::number(statusCode)));
              } else {
                const QString errorText = reply->errorString().trimmed().isEmpty()
                                              ? tr("Unknown network error.")
                                              : reply->errorString().trimmed();
                QMessageBox::warning(
                    this, tr("Connection Test Failed"),
                    tr("Could not reach Arduino package servers.\n\nStatus: %1\nError: %2")
                        .arg(statusCode > 0 ? QString::number(statusCode) : tr("n/a"),
                             errorText));
              }

              reply->deleteLater();
              manager->deleteLater();
            });
  });

  connect(proxyTypeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int index) {
            const QString type = proxyTypeCombo_->itemData(index).toString();
            const bool hasProxy = (type != "none");
            proxyPortSpin_->setEnabled(hasProxy);
            proxyUsernameEdit_->setEnabled(hasProxy);
            proxyPasswordEdit_->setEnabled(hasProxy);
            testConnectionButton_->setEnabled(hasProxy);
          });

  auto* networkForm = new QFormLayout();
  networkForm->addRow(tr("Proxy Type:"), proxyTypeCombo_);
  networkForm->addRow(tr("Proxy Host:"), proxyHostEdit_);
  networkForm->addRow(tr("Proxy Port:"), proxyPortSpin_);
  networkForm->addRow(tr("Proxy Username:"), proxyUsernameEdit_);
  networkForm->addRow(tr("Proxy Password:"), proxyPasswordEdit_);
  networkForm->addRow(tr("No Proxy For:"), noProxyHostsEdit_);
  networkForm->addRow(QString{}, testConnectionButton_);

  auto* networkTab = new QWidget(this);
  networkTab->setLayout(networkForm);
  tabWidget_->addTab(networkTab, tr("Network"));

  // === Compilation Tab ===
  additionalUrlsEdit_ = new QPlainTextEdit(this);
  additionalUrlsEdit_->setPlaceholderText(
      tr("One URL per line (used for Boards Manager)"));
  additionalUrlsEdit_->setMinimumHeight(80);

  warningsCombo_ = new QComboBox(this);
  warningsCombo_->addItem(tr("None"), "none");
  warningsCombo_->addItem(tr("Default"), "default");
  warningsCombo_->addItem(tr("More"), "more");
  warningsCombo_->addItem(tr("All"), "all");
  warningsCombo_->setCurrentIndex(warningsCombo_->findData("none"));

  verboseCompileCheck_ = new QCheckBox(tr("Show verbose output during compilation"),
                                      this);
  verboseCompileCheck_->setChecked(false);

  verboseUploadCheck_ =
      new QCheckBox(tr("Show verbose output during upload"), this);
  verboseUploadCheck_->setChecked(false);

  auto* compilationForm = new QFormLayout();
  compilationForm->addRow(tr("Additional Boards Manager URLs:"), additionalUrlsEdit_);
  compilationForm->addRow(tr("Compiler warnings:"), warningsCombo_);
  compilationForm->addRow(QString{}, verboseCompileCheck_);
  compilationForm->addRow(QString{}, verboseUploadCheck_);

  auto* compilationTab = new QWidget(this);
  compilationTab->setLayout(compilationForm);
  tabWidget_->addTab(compilationTab, tr("Compilation"));

  // Dialog buttons
  auto* buttons =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  auto* layout = new QVBoxLayout(this);
  layout->addWidget(tabWidget_);
  layout->addWidget(buttons);
}

QString PreferencesDialog::theme() const {
  return themeCombo_->currentData().toString();
}

QString PreferencesDialog::language() const {
  return languageCombo_ ? languageCombo_->currentData().toString() : QStringLiteral("system");
}

double PreferencesDialog::uiScale() const {
  return uiScaleCombo_ ? uiScaleCombo_->currentData().toDouble() : 1.0;
}

QString PreferencesDialog::sketchbookDir() const {
  return sketchbookEdit_ ? sketchbookEdit_->text().trimmed() : QString{};
}

QStringList PreferencesDialog::additionalUrls() const {
  if (!additionalUrlsEdit_) {
    return {};
  }
  const QString text = additionalUrlsEdit_->toPlainText();
  QStringList urls;
  const QStringList lines =
      text.split(QRegularExpression(R"([\r\n]+)"), Qt::SkipEmptyParts);
  for (const QString& line : lines) {
    const QStringList parts =
        line.split(QRegularExpression(R"([,]+)"), Qt::SkipEmptyParts);
    for (const QString& p : parts) {
      const QString url = p.trimmed();
      if (!url.isEmpty()) {
        urls << url;
      }
    }
  }
  urls.removeDuplicates();
  return urls;
}

QFont PreferencesDialog::editorFont() const {
  QFont font = editorFontCombo_ ? editorFontCombo_->currentFont() : QFont{};
  if (editorFontSizeSpin_) {
    font.setPointSize(editorFontSizeSpin_->value());
  }
  return font;
}

int PreferencesDialog::tabSize() const {
  return tabSizeSpin_->value();
}

bool PreferencesDialog::insertSpaces() const {
  return insertSpacesCheck_->isChecked();
}

bool PreferencesDialog::showIndentGuides() const {
  return showIndentGuidesCheck_ && showIndentGuidesCheck_->isChecked();
}

bool PreferencesDialog::showWhitespace() const {
  return showWhitespaceCheck_ && showWhitespaceCheck_->isChecked();
}

QString PreferencesDialog::defaultLineEnding() const {
  if (!defaultLineEndingCombo_) {
    return QStringLiteral("LF");
  }
  const QString val = defaultLineEndingCombo_->currentData().toString().trimmed().toUpper();
  return val == QStringLiteral("CRLF") ? QStringLiteral("CRLF") : QStringLiteral("LF");
}

bool PreferencesDialog::trimTrailingWhitespace() const {
  return trimTrailingWhitespaceCheck_ && trimTrailingWhitespaceCheck_->isChecked();
}

bool PreferencesDialog::autosaveEnabled() const {
  return autosaveCheck_ && autosaveCheck_->isChecked();
}

int PreferencesDialog::autosaveIntervalSeconds() const {
  if (!autosaveIntervalSpin_) {
    return 30;
  }
  return autosaveIntervalSpin_->value();
}

QString PreferencesDialog::warningsLevel() const {
  return warningsCombo_ ? warningsCombo_->currentData().toString() : QString{};
}

bool PreferencesDialog::verboseCompile() const {
  return verboseCompileCheck_->isChecked();
}

bool PreferencesDialog::verboseUpload() const {
  return verboseUploadCheck_->isChecked();
}

void PreferencesDialog::setTheme(QString theme) {
  theme = theme.trimmed().toLower();
  const int idx = themeCombo_->findData(theme);
  const QSignalBlocker blocker(themeCombo_);
  themeCombo_->setCurrentIndex(idx >= 0 ? idx : 0);
}

void PreferencesDialog::setLanguage(QString language) {
  if (!languageCombo_) {
    return;
  }
  language = language.trimmed();
  if (language.isEmpty()) {
    language = "system";
  }
  const int idx = languageCombo_->findData(language);
  const QSignalBlocker blocker(languageCombo_);
  languageCombo_->setCurrentIndex(idx >= 0 ? idx : 0);
}

void PreferencesDialog::setUiScale(double scale) {
  if (!uiScaleCombo_) {
    return;
  }
  const double s = std::clamp(scale, 0.5, 2.0);

  int best = 0;
  double bestDiff = 1e9;
  for (int i = 0; i < uiScaleCombo_->count(); ++i) {
    const double v = uiScaleCombo_->itemData(i).toDouble();
    const double d = std::abs(v - s);
    if (d < bestDiff) {
      bestDiff = d;
      best = i;
    }
  }
  const QSignalBlocker blocker(uiScaleCombo_);
  uiScaleCombo_->setCurrentIndex(best);
}

void PreferencesDialog::setSketchbookDir(QString directory) {
  if (!sketchbookEdit_) {
    return;
  }
  directory = directory.trimmed();
  sketchbookEdit_->setText(std::move(directory));
}

void PreferencesDialog::setAdditionalUrls(QStringList urls) {
  if (!additionalUrlsEdit_) {
    return;
  }
  for (QString& u : urls) {
    u = u.trimmed();
  }
  urls.removeAll(QString{});
  urls.removeDuplicates();
  additionalUrlsEdit_->setPlainText(urls.join('\n'));
}

void PreferencesDialog::setEditorFont(QFont font) {
  if (editorFontCombo_) {
    editorFontCombo_->setCurrentFont(font);
  }
  if (editorFontSizeSpin_) {
    int size = font.pointSize();
    if (size <= 0) {
      size = editorFontSizeSpin_->value();
    }
    editorFontSizeSpin_->setValue(std::clamp(size, 6, 48));
  }
}

void PreferencesDialog::setTabSize(int tabSize) {
  tabSizeSpin_->setValue(tabSize);
}

void PreferencesDialog::setInsertSpaces(bool insertSpaces) {
  insertSpacesCheck_->setChecked(insertSpaces);
}

void PreferencesDialog::setShowIndentGuides(bool enabled) {
  if (showIndentGuidesCheck_) {
    showIndentGuidesCheck_->setChecked(enabled);
  }
}

void PreferencesDialog::setShowWhitespace(bool enabled) {
  if (showWhitespaceCheck_) {
    showWhitespaceCheck_->setChecked(enabled);
  }
}

void PreferencesDialog::setDefaultLineEnding(QString lineEnding) {
  if (!defaultLineEndingCombo_) {
    return;
  }
  lineEnding = lineEnding.trimmed().toUpper();
  const int idx = defaultLineEndingCombo_->findData(lineEnding);
  defaultLineEndingCombo_->setCurrentIndex(
      idx >= 0 ? idx : defaultLineEndingCombo_->findData(QStringLiteral("LF")));
}

void PreferencesDialog::setTrimTrailingWhitespace(bool enabled) {
  if (trimTrailingWhitespaceCheck_) {
    trimTrailingWhitespaceCheck_->setChecked(enabled);
  }
}

void PreferencesDialog::setAutosaveEnabled(bool enabled) {
  if (autosaveCheck_) {
    autosaveCheck_->setChecked(enabled);
  }
  if (autosaveIntervalSpin_) {
    autosaveIntervalSpin_->setEnabled(enabled);
  }
}

void PreferencesDialog::setAutosaveIntervalSeconds(int seconds) {
  if (!autosaveIntervalSpin_) {
    return;
  }
  autosaveIntervalSpin_->setValue(std::clamp(seconds, 5, 600));
}

void PreferencesDialog::setWarningsLevel(QString level) {
  if (!warningsCombo_) {
    return;
  }
  level = level.trimmed().toLower();
  const int idx = warningsCombo_->findData(level);
  warningsCombo_->setCurrentIndex(idx >= 0 ? idx : warningsCombo_->findData("none"));
}

void PreferencesDialog::setVerboseCompile(bool verboseCompile) {
  verboseCompileCheck_->setChecked(verboseCompile);
}

void PreferencesDialog::setVerboseUpload(bool verboseUpload) {
  verboseUploadCheck_->setChecked(verboseUpload);
}

// Proxy settings getters/setters
QString PreferencesDialog::proxyType() const {
  return proxyTypeCombo_ ? proxyTypeCombo_->currentData().toString() : QStringLiteral("none");
}

QString PreferencesDialog::proxyHost() const {
  return proxyHostEdit_ ? proxyHostEdit_->text().trimmed() : QString{};
}

int PreferencesDialog::proxyPort() const {
  return proxyPortSpin_ ? proxyPortSpin_->value() : 8080;
}

QString PreferencesDialog::proxyUsername() const {
  return proxyUsernameEdit_ ? proxyUsernameEdit_->text().trimmed() : QString{};
}

QString PreferencesDialog::proxyPassword() const {
  return proxyPasswordEdit_ ? proxyPasswordEdit_->text() : QString{};
}

QStringList PreferencesDialog::noProxyHosts() const {
  if (!noProxyHostsEdit_) {
    return {};
  }
  const QString text = noProxyHostsEdit_->toPlainText();
  QStringList hosts;
  const QStringList lines =
      text.split(QRegularExpression(R"([\r\n]+)"), Qt::SkipEmptyParts);
  for (const QString& line : lines) {
    const QString host = line.trimmed();
    if (!host.isEmpty()) {
      hosts << host;
    }
  }
  hosts.removeDuplicates();
  return hosts;
}

void PreferencesDialog::setProxyType(QString type) {
  if (!proxyTypeCombo_) {
    return;
  }
  type = type.trimmed().toLower();
  const int idx = proxyTypeCombo_->findData(type);
  proxyTypeCombo_->setCurrentIndex(idx >= 0 ? idx : 0);
}

void PreferencesDialog::setProxyHost(QString host) {
  if (proxyHostEdit_) {
    proxyHostEdit_->setText(host.trimmed());
  }
}

void PreferencesDialog::setProxyPort(int port) {
  if (proxyPortSpin_) {
    proxyPortSpin_->setValue(qBound(1, port, 65535));
  }
}

void PreferencesDialog::setProxyUsername(QString username) {
  if (proxyUsernameEdit_) {
    proxyUsernameEdit_->setText(username.trimmed());
  }
}

void PreferencesDialog::setProxyPassword(QString password) {
  if (proxyPasswordEdit_) {
    proxyPasswordEdit_->setText(password);
  }
}

void PreferencesDialog::setNoProxyHosts(QStringList hosts) {
  if (!noProxyHostsEdit_) {
    return;
  }
  for (QString& h : hosts) {
    h = h.trimmed();
  }
  hosts.removeAll(QString{});
  hosts.removeDuplicates();
  noProxyHostsEdit_->setPlainText(hosts.join('\n'));
}
