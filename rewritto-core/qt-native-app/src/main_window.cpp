#include "main_window.h"

#include "arduino_cli.h"
#include "board_selector_dialog.h"
#include "boards_manager_dialog.h"
#include "build_output_parser.h"
#include "code_editor.h"
#include "code_snapshot_compare_dialog.h"
#include "code_snapshot_store.h"
#include "code_snapshots_dialog.h"
#include "editor_widget.h"
#include "examples_dialog.h"
#include "examples_scanner.h"
#include "find_in_files_dialog.h"
#include "find_replace_dialog.h"
#include "library_manager_dialog.h"
#include "lsp_client.h"
#include "lsp_code_action_utils.h"
#include "output_widget.h"
#include "preferences_dialog.h"
#include "quick_pick_dialog.h"
#include "replace_in_files_dialog.h"
#include "problems_widget.h"
#include "serial_monitor_widget.h"
#include "serial_plotter_widget.h"
#include "serial_port.h"
#include "sketch_manager.h"
#include "sketch_build_settings_store.h"
#include "theme_manager.h"
#include "toast_widget.h"
#include "interface_scale_manager.h"
#include "welcome_widget.h"

#include <QDesktopServices>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <limits>
#include <memory>
#include <QAbstractButton>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QCloseEvent>
#include <QColor>
#include <QComboBox>
#include <QCommandLinkButton>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDockWidget>
#include <QCompleter>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QDataStream>
#include <QDateTime>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLocale>
#include <QLineEdit>
#include <QMap>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QInputDialog>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPointer>
#include <QProgressBar>
#include <QProgressDialog>
#include <QPushButton>
#include <QRegularExpression>
#include <QRadioButton>
#include <QScrollBar>
#include <QSaveFile>
#include <QSortFilterProxyModel>
#include <QSet>
#include <QStyleHints>
#include <QToolButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QStorageInfo>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QStyle>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QToolBar>
#include <QToolTip>
#include <QTreeView>
#include <QTreeWidget>
#include <QTextBlock>
#include <QTextDocument>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWizard>

#ifndef REWRITTO_IDE_VERSION
#define REWRITTO_IDE_VERSION "0.4.4"
#endif

static constexpr auto kSettingsGroup = "MainWindow";
static constexpr auto kLastSketchKey = "lastSketch";
static constexpr auto kRecentSketchesKey = "recentSketches";
static constexpr auto kPinnedSketchesKey = "pinnedSketches";
static constexpr auto kGeometryKey = "geometry";
static constexpr auto kStateKey = "state";
static constexpr auto kFqbnKey = "fqbn";
static constexpr auto kPortKey = "port";
static constexpr auto kProgrammerKey = "programmer";
static constexpr auto kOptimizeForDebugKey = "optimizeForDebug";
static constexpr auto kMcpServerCommandKey = "mcpServerCommand";
static constexpr auto kMcpAutoStartKey = "mcpAutoStart";
static constexpr auto kOpenFilesKey = "openFiles";
static constexpr auto kActiveFileKey = "activeFile";
static constexpr auto kEditorViewStatesKey = "editorViewStates";
static constexpr auto kBreakpointsKey = "breakpoints";
static constexpr auto kDebugWatchesKey = "debugWatches";
static constexpr auto kStateVersionKey = "stateVersion";
static constexpr auto kSketchBoardSelectionsKey = "sketchBoardSelections";
static constexpr auto kBoardSetupWizardCompletedKey = "boardSetupWizardCompleted";
static constexpr auto kPrefCheckIndexesOnStartupKey = "checkIndexesOnStartup";
static constexpr int kCurrentStateVersion = 1;

namespace {
constexpr int kCompletionRoleInsertText = Qt::UserRole + 100;
constexpr int kCompletionRoleTextEdit = Qt::UserRole + 101;
constexpr int kCompletionRoleAdditionalEdits = Qt::UserRole + 102;
constexpr int kCompletionRoleInsertTextFormat = Qt::UserRole + 103;
constexpr int kCompletionRoleLabel = Qt::UserRole + 104;

constexpr int kOutlineRoleFilePath = Qt::UserRole + 200;
constexpr int kOutlineRoleLine = Qt::UserRole + 201;
constexpr int kOutlineRoleColumn = Qt::UserRole + 202;

constexpr int kPortRoleDetectedFqbn = Qt::UserRole + 301;
constexpr int kPortRoleDetectedBoardName = Qt::UserRole + 302;
constexpr int kPortRoleProtocol = Qt::UserRole + 303;
constexpr int kPortRoleMissing = Qt::UserRole + 304;

constexpr int kRoleIsFavorite = Qt::UserRole + 1;

QStringList normalizeStringList(QStringList values);
QStringList parseAdditionalBoardsText(const QString& text);

enum BoardSetupWizardPageId {
  kBoardWizardPageMode = 0,
  kBoardWizardPagePresets = 1,
  kBoardWizardPageCustom = 2,
  kBoardWizardPageReview = 3,
};

struct BoardSetupPreset final {
  QString id;
  QString name;
  QString description;
  QString url;
  QStringList recommendedCores;
  QString themeIconName;
  QStyle::StandardPixmap fallbackIcon = QStyle::SP_ComputerIcon;
  bool defaultSelected = false;
};

QIcon boardSetupPresetIcon(const BoardSetupPreset& preset, const QWidget* widget) {
  const QIcon themed = QIcon::fromTheme(preset.themeIconName.trimmed());
  if (!themed.isNull()) {
    return themed;
  }
  const QStyle* style = widget ? widget->style() : QApplication::style();
  return style ? style->standardIcon(preset.fallbackIcon) : QIcon{};
}

QStringList parseBoardSetupCoreIdsText(QString text) {
  text.replace(QLatin1Char(','), QLatin1Char('\n'));
  text.replace(QLatin1Char(';'), QLatin1Char('\n'));
  return normalizeStringList(text.split(QLatin1Char('\n')));
}

QVector<BoardSetupPreset> boardSetupPresetCatalog() {
  return {
      {
          QStringLiteral("esp32"),
          QObject::tr("Espressif ESP32"),
          QObject::tr("ESP32 family (ESP32, S2, S3, C3, C6, H2, P4)."),
          QStringLiteral(
              "https://espressif.github.io/arduino-esp32/package_esp32_index.json"),
          {QStringLiteral("esp32:esp32")},
          QStringLiteral("network-wireless"),
          QStyle::SP_DriveNetIcon,
          true,
      },
      {
          QStringLiteral("esp8266"),
          QObject::tr("ESP8266 Community"),
          QObject::tr("ESP8266 boards from the official community core."),
          QStringLiteral("https://arduino.esp8266.com/stable/package_esp8266com_index.json"),
          {QStringLiteral("esp8266:esp8266")},
          QStringLiteral("network-wireless"),
          QStyle::SP_DriveNetIcon,
          true,
      },
      {
          QStringLiteral("rp2040"),
          QObject::tr("Raspberry Pi RP2040 (arduino-pico)"),
          QObject::tr("Raspberry Pi Pico and RP2040-compatible boards."),
          QStringLiteral(
              "https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json"),
          {QStringLiteral("rp2040:rp2040")},
          QStringLiteral("cpu"),
          QStyle::SP_ComputerIcon,
          true,
      },
      {
          QStringLiteral("stm32"),
          QObject::tr("STM32 Official Core"),
          QObject::tr("STMicroelectronics STM32 boards."),
          QStringLiteral(
              "https://github.com/stm32duino/BoardManagerFiles/raw/main/package_stmicroelectronics_index.json"),
          {QStringLiteral("STMicroelectronics:stm32")},
          QStringLiteral("applications-engineering"),
          QStyle::SP_DesktopIcon,
          true,
      },
      {
          QStringLiteral("nrf52"),
          QObject::tr("Adafruit nRF52 + SAMD"),
          QObject::tr("Adafruit boards, including Feather nRF52 and many SAMD boards."),
          QStringLiteral("https://adafruit.github.io/arduino-board-index/package_adafruit_index.json"),
          {QStringLiteral("adafruit:nrf52"), QStringLiteral("adafruit:samd")},
          QStringLiteral("bluetooth"),
          QStyle::SP_DriveHDIcon,
          true,
      },
      {
          QStringLiteral("teensy"),
          QObject::tr("PJRC Teensy"),
          QObject::tr("Teensy boards (Teensy 4.x and others)."),
          QStringLiteral("https://www.pjrc.com/teensy/package_teensy_index.json"),
          {QStringLiteral("teensy:avr")},
          QStringLiteral("media-flash"),
          QStyle::SP_DriveFDIcon,
          true,
      },
      {
          QStringLiteral("sparkfun"),
          QObject::tr("SparkFun Boards"),
          QObject::tr("SparkFun cores (AVR, SAMD, nRF52, Apollo3, ESP, STM32)."),
          QStringLiteral(
              "https://raw.githubusercontent.com/sparkfun/Arduino_Boards/main/IDE_Board_Manager/package_sparkfun_index.json"),
          {QStringLiteral("SparkFun:nrf52"), QStringLiteral("SparkFun:apollo3")},
          QStringLiteral("applications-engineering"),
          QStyle::SP_DriveHDIcon,
          false,
      },
      {
          QStringLiteral("seeed"),
          QObject::tr("Seeed Studio (XIAO and more)"),
          QObject::tr("Seeed cores including nRF52, SAMD, STM32, mbed and others."),
          QStringLiteral("https://files.seeedstudio.com/arduino/package_seeeduino_boards_index.json"),
          {QStringLiteral("Seeeduino:nrf52"), QStringLiteral("Seeeduino:samd")},
          QStringLiteral("applications-engineering"),
          QStyle::SP_DriveHDIcon,
          false,
      },
      {
          QStringLiteral("attinycore"),
          QObject::tr("ATTinyCore"),
          QObject::tr("Classic ATtiny support from Spence Konde."),
          QStringLiteral("http://drazzy.com/package_drazzy.com_index.json"),
          {QStringLiteral("ATTinyCore:avr")},
          QStringLiteral("cpu"),
          QStyle::SP_FileDialogDetailedView,
          false,
      },
      {
          QStringLiteral("megatinycore"),
          QObject::tr("megaTinyCore"),
          QObject::tr("ATtiny 0/1/2-series support from Spence Konde."),
          QStringLiteral("http://drazzy.com/package_drazzy.com_index.json"),
          {QStringLiteral("megaTinyCore:megaavr")},
          QStringLiteral("cpu"),
          QStyle::SP_FileDialogDetailedView,
          false,
      },
      {
          QStringLiteral("megacore"),
          QObject::tr("MCUdude MegaCore"),
          QObject::tr("ATmega chips beyond default AVR support."),
          QStringLiteral("https://mcudude.github.io/MegaCore/package_MCUdude_MegaCore_index.json"),
          {QStringLiteral("MegaCore:avr")},
          QStringLiteral("cpu"),
          QStyle::SP_FileDialogListView,
          false,
      },
      {
          QStringLiteral("minicore"),
          QObject::tr("MCUdude MiniCore"),
          QObject::tr("ATmega8/48/88/168/328 family support."),
          QStringLiteral("https://mcudude.github.io/MiniCore/package_MCUdude_MiniCore_index.json"),
          {QStringLiteral("MiniCore:avr")},
          QStringLiteral("cpu"),
          QStyle::SP_FileDialogListView,
          false,
      },
      {
          QStringLiteral("mightycore"),
          QObject::tr("MCUdude MightyCore"),
          QObject::tr("ATmega16/32/64/128 family support."),
          QStringLiteral(
              "https://mcudude.github.io/MightyCore/package_MCUdude_MightyCore_index.json"),
          {QStringLiteral("MightyCore:avr")},
          QStringLiteral("cpu"),
          QStyle::SP_FileDialogListView,
          false,
      },
      {
          QStringLiteral("microcore"),
          QObject::tr("MCUdude MicroCore"),
          QObject::tr("ATtiny13 support."),
          QStringLiteral(
              "https://mcudude.github.io/MicroCore/package_MCUdude_MicroCore_index.json"),
          {QStringLiteral("MicroCore:avr")},
          QStringLiteral("cpu"),
          QStyle::SP_FileDialogListView,
          false,
      },
  };
}

class BoardSetupModePage final : public QWizardPage {
 public:
  explicit BoardSetupModePage(QWidget* parent = nullptr) : QWizardPage(parent) {
    setTitle(QObject::tr("Choose Setup Mode"));
    setSubTitle(QObject::tr("Pick a preset catalog or add your own board manager URL."));

    auto* layout = new QVBoxLayout(this);

    const QString cardStyle = QStringLiteral(
        "QCommandLinkButton { border: 1px solid palette(mid); border-radius: 10px; "
        "padding: 12px; text-align: left; }"
        "QCommandLinkButton:checked { border: 2px solid palette(highlight); "
        "background-color: palette(alternate-base); }");

    presetButton_ = new QCommandLinkButton(
        QObject::tr("Browse Presets"),
        QObject::tr("Choose from curated popular cores (ESP32, nRF52, STM32, RP2040, Teensy, etc.)."),
        this);
    presetButton_->setCheckable(true);
    presetButton_->setIcon(style()->standardIcon(QStyle::SP_DriveNetIcon));
    presetButton_->setStyleSheet(cardStyle);

    customButton_ = new QCommandLinkButton(
        QObject::tr("Add Custom URL"),
        QObject::tr("Paste one or more additional board manager URLs and optional core IDs."),
        this);
    customButton_->setCheckable(true);
    customButton_->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    customButton_->setStyleSheet(cardStyle);

    auto* modeGroup = new QButtonGroup(this);
    modeGroup->setExclusive(true);
    modeGroup->addButton(presetButton_);
    modeGroup->addButton(customButton_);
    presetButton_->setChecked(true);

    connect(modeGroup, &QButtonGroup::buttonToggled, this,
            [this](QAbstractButton*, bool) { emit completeChanged(); });

    layout->addWidget(presetButton_);
    layout->addWidget(customButton_);
    layout->addStretch(1);
  }

  int nextId() const override {
    return customButton_ && customButton_->isChecked() ? kBoardWizardPageCustom
                                                       : kBoardWizardPagePresets;
  }

 private:
  QCommandLinkButton* presetButton_ = nullptr;
  QCommandLinkButton* customButton_ = nullptr;
};

class BoardSetupPresetPage final : public QWizardPage {
 public:
  explicit BoardSetupPresetPage(QVector<BoardSetupPreset> presets,
                                QWidget* parent = nullptr)
      : QWizardPage(parent), presets_(std::move(presets)) {
    setTitle(QObject::tr("Browse Presets"));
    setSubTitle(QObject::tr("Select one or more presets. Each preset shows its exact URL."));

    auto* layout = new QVBoxLayout(this);

    filterEdit_ = new QLineEdit(this);
    filterEdit_->setPlaceholderText(QObject::tr("Filter presets (name, URL, board family)..."));
    layout->addWidget(filterEdit_);

    presetTree_ = new QTreeWidget(this);
    presetTree_->setHeaderLabels({QObject::tr("Preset"), QObject::tr("Details")});
    presetTree_->setRootIsDecorated(true);
    presetTree_->setAlternatingRowColors(true);
    presetTree_->setSelectionMode(QAbstractItemView::NoSelection);
    presetTree_->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    presetTree_->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    layout->addWidget(presetTree_, 1);

    auto* controlsLayout = new QHBoxLayout();
    auto* selectPopularButton = new QPushButton(QObject::tr("Select Popular"), this);
    auto* selectAllButton = new QPushButton(QObject::tr("Select All"), this);
    auto* clearButton = new QPushButton(QObject::tr("Clear"), this);
    controlsLayout->addWidget(selectPopularButton);
    controlsLayout->addWidget(selectAllButton);
    controlsLayout->addWidget(clearButton);
    controlsLayout->addStretch(1);
    layout->addLayout(controlsLayout);

    rebuildTree();

    connect(filterEdit_, &QLineEdit::textChanged, this,
            [this](const QString& value) { applyFilter(value); });
    connect(presetTree_, &QTreeWidget::itemChanged, this,
            [this](QTreeWidgetItem* item, int) {
              if (!item || item->parent() != nullptr) {
                return;
              }
              emit completeChanged();
            });

    connect(selectPopularButton, &QPushButton::clicked, this, [this] {
      for (int i = 0; i < presetTree_->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = presetTree_->topLevelItem(i);
        if (!item) {
          continue;
        }
        const int presetIndex = item->data(0, Qt::UserRole).toInt();
        if (presetIndex >= 0 && presetIndex < presets_.size()) {
          item->setCheckState(0,
                              presets_.at(presetIndex).defaultSelected ? Qt::Checked
                                                                       : Qt::Unchecked);
        }
      }
      emit completeChanged();
    });
    connect(selectAllButton, &QPushButton::clicked, this, [this] {
      for (int i = 0; i < presetTree_->topLevelItemCount(); ++i) {
        if (QTreeWidgetItem* item = presetTree_->topLevelItem(i)) {
          item->setCheckState(0, Qt::Checked);
        }
      }
      emit completeChanged();
    });
    connect(clearButton, &QPushButton::clicked, this, [this] {
      for (int i = 0; i < presetTree_->topLevelItemCount(); ++i) {
        if (QTreeWidgetItem* item = presetTree_->topLevelItem(i)) {
          item->setCheckState(0, Qt::Unchecked);
        }
      }
      emit completeChanged();
    });
  }

  int nextId() const override { return kBoardWizardPageReview; }

  bool isComplete() const override { return !selectedUrls().isEmpty(); }

  QStringList selectedUrls() const {
    QStringList urls;
    for (int i = 0; i < presetTree_->topLevelItemCount(); ++i) {
      const QTreeWidgetItem* item = presetTree_->topLevelItem(i);
      if (!item || item->checkState(0) != Qt::Checked) {
        continue;
      }
      const int presetIndex = item->data(0, Qt::UserRole).toInt();
      if (presetIndex >= 0 && presetIndex < presets_.size()) {
        urls << presets_.at(presetIndex).url;
      }
    }
    return normalizeStringList(urls);
  }

  QStringList selectedCores() const {
    QStringList cores;
    for (int i = 0; i < presetTree_->topLevelItemCount(); ++i) {
      const QTreeWidgetItem* item = presetTree_->topLevelItem(i);
      if (!item || item->checkState(0) != Qt::Checked) {
        continue;
      }
      const int presetIndex = item->data(0, Qt::UserRole).toInt();
      if (presetIndex >= 0 && presetIndex < presets_.size()) {
        cores.append(presets_.at(presetIndex).recommendedCores);
      }
    }
    return normalizeStringList(cores);
  }

 private:
  void rebuildTree() {
    presetTree_->clear();
    for (int i = 0; i < presets_.size(); ++i) {
      const BoardSetupPreset& preset = presets_.at(i);

      auto* presetItem = new QTreeWidgetItem(presetTree_);
      presetItem->setText(0, preset.name);
      presetItem->setText(1, preset.description);
      presetItem->setIcon(0, boardSetupPresetIcon(preset, this));
      presetItem->setFlags((presetItem->flags() | Qt::ItemIsUserCheckable) &
                           ~Qt::ItemIsSelectable);
      presetItem->setCheckState(0, preset.defaultSelected ? Qt::Checked : Qt::Unchecked);
      presetItem->setData(0, Qt::UserRole, i);

      auto* urlItem = new QTreeWidgetItem(presetItem);
      urlItem->setText(0, QObject::tr("URL"));
      urlItem->setText(1, preset.url);
      urlItem->setIcon(0, style()->standardIcon(QStyle::SP_DialogHelpButton));
      urlItem->setFlags((urlItem->flags() | Qt::ItemIsSelectable) &
                        ~Qt::ItemIsUserCheckable);

      auto* coresItem = new QTreeWidgetItem(presetItem);
      coresItem->setText(0, QObject::tr("Recommended Cores"));
      coresItem->setText(1, preset.recommendedCores.isEmpty()
                                ? QObject::tr("(none pre-selected)")
                                : preset.recommendedCores.join(QStringLiteral(", ")));
      coresItem->setFlags((coresItem->flags() | Qt::ItemIsSelectable) &
                          ~Qt::ItemIsUserCheckable);

      presetItem->setExpanded(true);
    }
  }

  void applyFilter(QString value) {
    value = value.trimmed();
    for (int i = 0; i < presetTree_->topLevelItemCount(); ++i) {
      QTreeWidgetItem* item = presetTree_->topLevelItem(i);
      if (!item) {
        continue;
      }
      const int presetIndex = item->data(0, Qt::UserRole).toInt();
      if (presetIndex < 0 || presetIndex >= presets_.size()) {
        item->setHidden(false);
        continue;
      }

      const BoardSetupPreset& preset = presets_.at(presetIndex);
      const QString haystack = (preset.name + QLatin1Char('\n') + preset.description +
                                QLatin1Char('\n') + preset.url +
                                QLatin1Char('\n') + preset.recommendedCores.join(QLatin1Char(' ')))
                                   .toLower();
      const bool visible =
          value.isEmpty() || haystack.contains(value.toLower());
      item->setHidden(!visible);
    }
  }

  QVector<BoardSetupPreset> presets_;
  QLineEdit* filterEdit_ = nullptr;
  QTreeWidget* presetTree_ = nullptr;
};

class BoardSetupCustomPage final : public QWizardPage {
 public:
  explicit BoardSetupCustomPage(QWidget* parent = nullptr) : QWizardPage(parent) {
    setTitle(QObject::tr("Add Custom Board Manager URL"));
    setSubTitle(QObject::tr("Paste one or more URLs. One URL per line."));

    auto* layout = new QVBoxLayout(this);

    auto* urlsLabel = new QLabel(QObject::tr("Board manager URL(s)"), this);
    layout->addWidget(urlsLabel);
    urlsEdit_ = new QPlainTextEdit(this);
    urlsEdit_->setPlaceholderText(
        QStringLiteral("https://example.com/package_vendor_index.json"));
    urlsEdit_->setMinimumHeight(150);
    layout->addWidget(urlsEdit_);

    auto* coreLabel = new QLabel(
        QObject::tr("Optional core IDs to install now (packager:architecture, one per line)"),
        this);
    layout->addWidget(coreLabel);
    coresEdit_ = new QPlainTextEdit(this);
    coresEdit_->setPlaceholderText(QStringLiteral("esp32:esp32"));
    coresEdit_->setMaximumHeight(110);
    layout->addWidget(coresEdit_);

    auto* hintLabel = new QLabel(
        QObject::tr("Example core IDs: esp32:esp32, STMicroelectronics:stm32, "
                    "adafruit:nrf52"),
        this);
    hintLabel->setWordWrap(true);
    hintLabel->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(hintLabel);
    layout->addStretch(1);

    connect(urlsEdit_, &QPlainTextEdit::textChanged, this,
            [this] { emit completeChanged(); });
  }

  int nextId() const override { return kBoardWizardPageReview; }

  bool isComplete() const override { return !customUrls().isEmpty(); }

  QStringList customUrls() const {
    return parseAdditionalBoardsText(urlsEdit_->toPlainText());
  }

  QStringList customCores() const {
    return parseBoardSetupCoreIdsText(coresEdit_->toPlainText());
  }

 private:
  QPlainTextEdit* urlsEdit_ = nullptr;
  QPlainTextEdit* coresEdit_ = nullptr;
};

class BoardSetupReviewPage final : public QWizardPage {
 public:
  explicit BoardSetupReviewPage(QWidget* parent = nullptr) : QWizardPage(parent) {
    setTitle(QObject::tr("Review and Apply"));
    setSubTitle(QObject::tr("Confirm URLs and optional core installations."));

    auto* layout = new QVBoxLayout(this);
    summaryEdit_ = new QPlainTextEdit(this);
    summaryEdit_->setReadOnly(true);
    summaryEdit_->setMinimumHeight(220);
    layout->addWidget(summaryEdit_, 1);

    installRecommendedCheck_ =
        new QCheckBox(QObject::tr("Install recommended core(s) now"), this);
    installRecommendedCheck_->setChecked(true);
    layout->addWidget(installRecommendedCheck_);

    openBoardsManagerCheck_ =
        new QCheckBox(QObject::tr("Open Boards Manager after finishing"), this);
    openBoardsManagerCheck_->setChecked(true);
    layout->addWidget(openBoardsManagerCheck_);
  }

  int nextId() const override { return -1; }

  void initializePage() override {
    const QStringList urls = selectedUrls();
    const QStringList cores = selectedCores();

    QStringList lines;
    lines << QObject::tr("Board manager URL(s):");
    if (urls.isEmpty()) {
      lines << QObject::tr("  (none)");
    } else {
      for (const QString& url : urls) {
        lines << QStringLiteral("  - %1").arg(url);
      }
    }
    lines << QString{};
    lines << QObject::tr("Core(s) queued for install:");
    if (cores.isEmpty()) {
      lines << QObject::tr("  (none)");
    } else {
      for (const QString& core : cores) {
        lines << QStringLiteral("  - %1").arg(core);
      }
    }
    summaryEdit_->setPlainText(lines.join(QLatin1Char('\n')));

    installRecommendedCheck_->setEnabled(!cores.isEmpty());
    if (cores.isEmpty()) {
      installRecommendedCheck_->setChecked(false);
    }
  }

  QStringList selectedUrls() const {
    QStringList urls;
    if (!wizard()) {
      return urls;
    }

    if (const auto* presetPage = dynamic_cast<const BoardSetupPresetPage*>(
            wizard()->page(kBoardWizardPagePresets))) {
      urls.append(presetPage->selectedUrls());
    }
    if (const auto* customPage = dynamic_cast<const BoardSetupCustomPage*>(
            wizard()->page(kBoardWizardPageCustom))) {
      urls.append(customPage->customUrls());
    }
    return normalizeStringList(urls);
  }

  QStringList selectedCores() const {
    QStringList cores;
    if (!wizard()) {
      return cores;
    }

    if (const auto* presetPage = dynamic_cast<const BoardSetupPresetPage*>(
            wizard()->page(kBoardWizardPagePresets))) {
      cores.append(presetPage->selectedCores());
    }
    if (const auto* customPage = dynamic_cast<const BoardSetupCustomPage*>(
            wizard()->page(kBoardWizardPageCustom))) {
      cores.append(customPage->customCores());
    }
    return normalizeStringList(cores);
  }

  bool installRecommendedNow() const {
    return installRecommendedCheck_ && installRecommendedCheck_->isChecked();
  }

  bool openBoardsManagerAfterFinish() const {
    return openBoardsManagerCheck_ && openBoardsManagerCheck_->isChecked();
  }

 private:
  QPlainTextEdit* summaryEdit_ = nullptr;
  QCheckBox* installRecommendedCheck_ = nullptr;
  QCheckBox* openBoardsManagerCheck_ = nullptr;
};

QColor blendColors(const QColor& first, const QColor& second, qreal secondWeight) {
  const qreal clampedWeight = std::clamp(secondWeight, 0.0, 1.0);
  const qreal firstWeight = 1.0 - clampedWeight;
  return QColor::fromRgbF(
      first.redF() * firstWeight + second.redF() * clampedWeight,
      first.greenF() * firstWeight + second.greenF() * clampedWeight,
      first.blueF() * firstWeight + second.blueF() * clampedWeight,
      first.alphaF() * firstWeight + second.alphaF() * clampedWeight);
}

QString colorHex(const QColor& color) {
  return color.name(QColor::HexRgb);
}

QColor readableForeground(const QColor& background) {
  return background.lightnessF() >= 0.55 ? QColor(QStringLiteral("#0f172a"))
                                         : QColor(QStringLiteral("#f8fafc"));
}

QString normalizeSnapshotRelativePath(QString rel) {
  rel = rel.trimmed();
  rel.replace('\\', '/');
  rel = QDir::cleanPath(rel);
  if (rel == QStringLiteral(".")) {
    return {};
  }
  while (rel.startsWith(QStringLiteral("./"))) {
    rel = rel.mid(2);
  }
  return rel;
}

bool shouldIgnoreSnapshotComparePath(const QString& relPath) {
  const QString rel = normalizeSnapshotRelativePath(relPath);
  if (rel.isEmpty()) {
    return true;
  }
  if (rel == QStringLiteral(".rewritto") || rel.startsWith(QStringLiteral(".rewritto/"))) {
    return true;
  }
  if (rel == QStringLiteral(".git") || rel.startsWith(QStringLiteral(".git/"))) {
    return true;
  }
  return false;
}

struct CommandResult final {
  bool started = false;
  bool timedOut = false;
  int exitCode = -1;
  QProcess::ExitStatus exitStatus = QProcess::NormalExit;
  QString stdoutText;
  QString stderrText;
};

CommandResult runCommandBlocking(const QString& program,
                                 const QStringList& args,
                                 const QString& workingDirectory = {},
                                 const QByteArray& stdinPayload = {},
                                 int timeoutMs = 120000) {
  CommandResult result;
  if (program.trimmed().isEmpty()) {
    return result;
  }

  QProcess process;
  if (!workingDirectory.trimmed().isEmpty()) {
    process.setWorkingDirectory(workingDirectory);
  }
  process.start(program, args);
  if (!process.waitForStarted(5000)) {
    result.stderrText = QObject::tr("Failed to start '%1'.").arg(program);
    return result;
  }

  result.started = true;
  if (!stdinPayload.isEmpty()) {
    process.write(stdinPayload);
  }
  process.closeWriteChannel();

  if (!process.waitForFinished(timeoutMs)) {
    result.timedOut = true;
    process.kill();
    process.waitForFinished(1000);
  }

  result.exitStatus = process.exitStatus();
  result.exitCode = process.exitCode();
  result.stdoutText = QString::fromUtf8(process.readAllStandardOutput());
  result.stderrText = QString::fromUtf8(process.readAllStandardError());
  return result;
}

QString commandErrorSummary(const CommandResult& result) {
  if (!result.started) {
    return QObject::tr("Process could not be started.");
  }
  if (result.timedOut) {
    return QObject::tr("Process timed out.");
  }
  if (result.exitStatus != QProcess::NormalExit) {
    return QObject::tr("Process crashed.");
  }
  const QString stderrText = result.stderrText.trimmed();
  const QString stdoutText = result.stdoutText.trimmed();
  if (!stderrText.isEmpty()) {
    return stderrText;
  }
  if (!stdoutText.isEmpty()) {
    return stdoutText;
  }
  return QObject::tr("Command failed (exit code %1).").arg(result.exitCode);
}

QString defaultArduinoDataDirPath() {
#if defined(Q_OS_WIN)
  QString localAppData = qEnvironmentVariable("LOCALAPPDATA").trimmed();
  if (localAppData.isEmpty()) {
    localAppData = QDir(QDir::homePath())
                       .absoluteFilePath(QStringLiteral("AppData/Local"));
  }
  return QDir(localAppData).absoluteFilePath(QStringLiteral("Arduino15"));
#else
  return QDir(QDir::homePath()).absoluteFilePath(QStringLiteral(".arduino15"));
#endif
}

bool removePathIfExists(const QString& path) {
  if (path.trimmed().isEmpty()) {
    return true;
  }
  const QFileInfo info(path);
  if (!info.exists() && !info.isSymLink()) {
    return true;
  }
  if (info.isDir() && !info.isSymLink()) {
    return QDir(path).removeRecursively();
  }
  return QFile::remove(path);
}

bool copyDirectoryRecursivelyMerged(const QString& sourceDir,
                                    const QString& targetDir,
                                    QString* outError) {
  const QDir srcRoot(sourceDir);
  if (!srcRoot.exists()) {
    if (outError) {
      *outError = QObject::tr("Source folder does not exist: %1").arg(sourceDir);
    }
    return false;
  }
  if (!QDir().mkpath(targetDir)) {
    if (outError) {
      *outError =
          QObject::tr("Could not create destination folder: %1").arg(targetDir);
    }
    return false;
  }

  QDirIterator it(sourceDir,
                  QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden |
                      QDir::System,
                  QDirIterator::Subdirectories);
  while (it.hasNext()) {
    const QString srcPath = it.next();
    const QFileInfo srcInfo(srcPath);
    const QString rel = srcRoot.relativeFilePath(srcPath);
    if (rel.trimmed().isEmpty() || rel == QStringLiteral(".")) {
      continue;
    }
    const QString dstPath = QDir(targetDir).absoluteFilePath(rel);

    if (srcInfo.isDir() && !srcInfo.isSymLink()) {
      if (!QDir().mkpath(dstPath)) {
        if (outError) {
          *outError = QObject::tr("Could not create folder: %1").arg(dstPath);
        }
        return false;
      }
      continue;
    }

    if (!srcInfo.isFile() && !srcInfo.isSymLink()) {
      continue;
    }

    if (!QDir().mkpath(QFileInfo(dstPath).absolutePath())) {
      if (outError) {
        *outError = QObject::tr("Could not create folder: %1")
                        .arg(QFileInfo(dstPath).absolutePath());
      }
      return false;
    }
    if (!removePathIfExists(dstPath)) {
      if (outError) {
        *outError = QObject::tr("Could not replace path: %1").arg(dstPath);
      }
      return false;
    }

    bool copied = false;
    if (srcInfo.isSymLink()) {
      const QString linkTarget = srcInfo.symLinkTarget();
      if (!linkTarget.isEmpty()) {
        copied = QFile::link(linkTarget, dstPath);
      }
    }
    if (!copied) {
      copied = QFile::copy(srcPath, dstPath);
    }
    if (!copied) {
      if (outError) {
        *outError = QObject::tr("Could not copy file: %1").arg(rel);
      }
      return false;
    }
  }

  if (outError) {
    outError->clear();
  }
  return true;
}

bool extractZipArchive(const QString& zipPath,
                       const QString& destinationDir,
                       QString* outError) {
  if (!QFileInfo(zipPath).isFile()) {
    if (outError) {
      *outError = QObject::tr("Archive file does not exist.");
    }
    return false;
  }
  if (!QDir().mkpath(destinationDir)) {
    if (outError) {
      *outError = QObject::tr("Could not create extraction folder.");
    }
    return false;
  }

  const QString unzipPath =
      QStandardPaths::findExecutable(QStringLiteral("unzip"));
  if (!unzipPath.isEmpty()) {
    const CommandResult result = runCommandBlocking(
        unzipPath,
        {QStringLiteral("-oq"), zipPath, QStringLiteral("-d"), destinationDir},
        {}, {}, 900000);
    if (result.started && !result.timedOut &&
        result.exitStatus == QProcess::NormalExit && result.exitCode == 0) {
      if (outError) {
        outError->clear();
      }
      return true;
    }
    if (outError) {
      *outError = commandErrorSummary(result);
    }
  }

  const QString bsdtarPath =
      QStandardPaths::findExecutable(QStringLiteral("bsdtar"));
  if (!bsdtarPath.isEmpty()) {
    const CommandResult result = runCommandBlocking(
        bsdtarPath,
        {QStringLiteral("-xf"), zipPath, QStringLiteral("-C"), destinationDir},
        {}, {}, 900000);
    if (result.started && !result.timedOut &&
        result.exitStatus == QProcess::NormalExit && result.exitCode == 0) {
      if (outError) {
        outError->clear();
      }
      return true;
    }
    if (outError) {
      *outError = commandErrorSummary(result);
    }
  }

  const QString tarPath = QStandardPaths::findExecutable(QStringLiteral("tar"));
  if (!tarPath.isEmpty()) {
    const CommandResult result = runCommandBlocking(
        tarPath,
        {QStringLiteral("-xf"), zipPath, QStringLiteral("-C"), destinationDir},
        {}, {}, 900000);
    if (result.started && !result.timedOut &&
        result.exitStatus == QProcess::NormalExit && result.exitCode == 0) {
      if (outError) {
        outError->clear();
      }
      return true;
    }
    if (outError) {
      *outError = commandErrorSummary(result);
    }
  }

  if (outError && outError->trimmed().isEmpty()) {
    *outError = QObject::tr("No archive extractor available (need unzip or tar).");
  }
  return false;
}

QString findBundleRootDirectory(const QString& extractedRoot,
                                const QString& manifestFileName) {
  const QString directManifest =
      QDir(extractedRoot).absoluteFilePath(manifestFileName);
  if (QFileInfo::exists(directManifest)) {
    return QDir(extractedRoot).absolutePath();
  }

  QDirIterator it(extractedRoot, QDir::Dirs | QDir::NoDotAndDotDot,
                  QDirIterator::Subdirectories);
  while (it.hasNext()) {
    const QString dirPath = it.next();
    if (QFileInfo::exists(QDir(dirPath).absoluteFilePath(manifestFileName))) {
      return QDir(dirPath).absolutePath();
    }
  }
  return {};
}

struct ArduinoCliDirectories final {
  QString dataDir;
  QString userDir;
};

ArduinoCliDirectories readArduinoCliDirectories(const QString& cliPath,
                                                QStringList globalFlags) {
  ArduinoCliDirectories out;
  const QString trimmedCli = cliPath.trimmed();
  if (trimmedCli.isEmpty()) {
    return out;
  }

  globalFlags << QStringLiteral("config")
              << QStringLiteral("dump")
              << QStringLiteral("--format")
              << QStringLiteral("json");
  const CommandResult result =
      runCommandBlocking(trimmedCli, globalFlags, {}, {}, 15000);
  if (!(result.started && !result.timedOut &&
        result.exitStatus == QProcess::NormalExit && result.exitCode == 0)) {
    return out;
  }

  const QJsonDocument doc =
      QJsonDocument::fromJson(result.stdoutText.toUtf8());
  if (!doc.isObject()) {
    return out;
  }
  const QJsonObject directories =
      doc.object().value(QStringLiteral("directories")).toObject();
  out.dataDir =
      directories.value(QStringLiteral("data")).toString().trimmed();
  out.userDir =
      directories.value(QStringLiteral("user")).toString().trimmed();
  return out;
}

QString formatByteSize(qint64 bytes) {
  if (bytes < 0) {
    return QObject::tr("unknown size");
  }
  static const QStringList units = {QStringLiteral("B"), QStringLiteral("KB"),
                                    QStringLiteral("MB"), QStringLiteral("GB"),
                                    QStringLiteral("TB")};
  double size = static_cast<double>(bytes);
  int unitIndex = 0;
  while (size >= 1024.0 && unitIndex < units.size() - 1) {
    size /= 1024.0;
    ++unitIndex;
  }
  const int precision = (unitIndex == 0 || size >= 100.0) ? 0 : 1;
  return QStringLiteral("%1 %2")
      .arg(QString::number(size, 'f', precision), units.at(unitIndex));
}

QString formatElapsedTimeMs(qint64 elapsedMs) {
  const qint64 totalSeconds = qMax<qint64>(0, elapsedMs / 1000);
  const qint64 minutes = totalSeconds / 60;
  const qint64 seconds = totalSeconds % 60;
  if (minutes <= 0) {
    return QObject::tr("%1s").arg(seconds);
  }
  return QObject::tr("%1m %2s").arg(minutes).arg(seconds);
}

qint64 directorySizeBytes(const QString& rootPath) {
  if (rootPath.trimmed().isEmpty() || !QFileInfo(rootPath).isDir()) {
    return 0;
  }
  qint64 total = 0;
  QDirIterator it(rootPath, QDir::Files | QDir::NoDotAndDotDot,
                  QDirIterator::Subdirectories);
  while (it.hasNext()) {
    const QFileInfo info(it.next());
    if (info.isFile()) {
      total += info.size();
    }
  }
  return total;
}

bool isLikelyBuildArtifactFile(const QString& fileName) {
  const QString lower = fileName.trimmed().toLower();
  if (lower.isEmpty()) {
    return false;
  }
  static const QStringList suffixes = {
      QStringLiteral(".uf2"), QStringLiteral(".hex"), QStringLiteral(".bin"),
      QStringLiteral(".elf"), QStringLiteral(".map"), QStringLiteral(".eep"),
      QStringLiteral(".zip"),
  };
  for (const QString& suffix : suffixes) {
    if (lower.endsWith(suffix)) {
      return true;
    }
  }
  return lower == QStringLiteral("build.options.json");
}

bool copyBuildArtifactsForSketch(const QString& buildRoot,
                                 const QString& sketchName,
                                 const QString& destinationRoot,
                                 QStringList* outCopiedRelativeFiles,
                                 QString* outError) {
  const QString normalizedSketchName = sketchName.trimmed().toLower();
  const QDir srcRoot(buildRoot);
  if (!srcRoot.exists()) {
    if (outError) {
      *outError = QObject::tr("Build artifacts folder does not exist.");
    }
    return false;
  }
  if (!QDir().mkpath(destinationRoot)) {
    if (outError) {
      *outError = QObject::tr("Could not create destination artifacts folder.");
    }
    return false;
  }

  QStringList candidates;
  QHash<QString, QString> absByRel;
  QDirIterator it(buildRoot, QDir::Files | QDir::NoDotAndDotDot,
                  QDirIterator::Subdirectories);
  while (it.hasNext()) {
    const QString absPath = it.next();
    const QFileInfo info(absPath);
    if (!info.isFile() || !isLikelyBuildArtifactFile(info.fileName())) {
      continue;
    }
    const QString rel = srcRoot.relativeFilePath(absPath);
    if (rel.trimmed().isEmpty()) {
      continue;
    }
    candidates << rel;
    absByRel.insert(rel, absPath);
  }
  if (candidates.isEmpty()) {
    if (outError) {
      *outError = QObject::tr("No build artifact files were found.");
    }
    return false;
  }

  QStringList prioritized;
  QStringList fallback;
  for (const QString& rel : candidates) {
    const QString fileName = QFileInfo(rel).fileName().toLower();
    const bool sketchMatch = normalizedSketchName.isEmpty() ||
                             fileName.startsWith(normalizedSketchName + QLatin1Char('.')) ||
                             fileName.startsWith(normalizedSketchName + QLatin1Char('_')) ||
                             fileName.contains(normalizedSketchName);
    if (sketchMatch) {
      prioritized << rel;
    } else {
      fallback << rel;
    }
  }
  QStringList chosen = prioritized.isEmpty() ? fallback : prioritized;
  std::sort(chosen.begin(), chosen.end(), [](const QString& a, const QString& b) {
    return QString::localeAwareCompare(a, b) < 0;
  });

  QStringList copied;
  for (const QString& rel : chosen) {
    const QString srcPath = absByRel.value(rel);
    if (srcPath.isEmpty()) {
      continue;
    }
    const QString dstPath = QDir(destinationRoot).absoluteFilePath(rel);
    if (!QDir().mkpath(QFileInfo(dstPath).absolutePath())) {
      if (outError) {
        *outError =
            QObject::tr("Could not create destination for artifact '%1'.").arg(rel);
      }
      return false;
    }
    (void)removePathIfExists(dstPath);
    if (!QFile::copy(srcPath, dstPath)) {
      if (outError) {
        *outError = QObject::tr("Could not copy artifact '%1'.").arg(rel);
      }
      return false;
    }
    copied << rel;
  }

  if (copied.isEmpty()) {
    if (outError) {
      *outError = QObject::tr("No build artifacts were copied.");
    }
    return false;
  }

  if (outCopiedRelativeFiles) {
    *outCopiedRelativeFiles = copied;
  }
  if (outError) {
    outError->clear();
  }
  return true;
}

bool isGitRepository(const QString& gitPath, const QString& workingDirectory) {
  const CommandResult result = runCommandBlocking(
      gitPath, {QStringLiteral("rev-parse"), QStringLiteral("--is-inside-work-tree")},
      workingDirectory, {}, 10000);
  return result.started && !result.timedOut &&
         result.exitStatus == QProcess::NormalExit &&
         result.exitCode == 0 &&
         result.stdoutText.trimmed() == QStringLiteral("true");
}

QString githubBrowseUrlForRemote(QString remoteUrl) {
  remoteUrl = remoteUrl.trimmed();
  if (remoteUrl.isEmpty()) {
    return {};
  }

  if (remoteUrl.startsWith(QStringLiteral("git@github.com:"))) {
    remoteUrl.replace(QStringLiteral("git@github.com:"), QStringLiteral("https://github.com/"));
  }
  if (remoteUrl.startsWith(QStringLiteral("ssh://git@github.com/"))) {
    remoteUrl.replace(QStringLiteral("ssh://git@github.com/"), QStringLiteral("https://github.com/"));
  }
  if (!remoteUrl.startsWith(QStringLiteral("https://github.com/")) &&
      !remoteUrl.startsWith(QStringLiteral("http://github.com/"))) {
    return {};
  }

  if (remoteUrl.endsWith(QStringLiteral(".git"), Qt::CaseInsensitive)) {
    remoteUrl.chop(4);
  }
  while (remoteUrl.endsWith(QLatin1Char('/'))) {
    remoteUrl.chop(1);
  }
  return remoteUrl;
}

bool sendLinuxDesktopNotification(const QString& summary,
                                  const QString& body,
                                  int timeoutMs) {
#if defined(Q_OS_LINUX)
  static const QString notifySendPath =
      QStandardPaths::findExecutable(QStringLiteral("notify-send"));
  if (notifySendPath.isEmpty()) {
    return false;
  }

  QStringList args;
  args << QStringLiteral("--app-name") << QStringLiteral("Rewritto IDE");
  args << QStringLiteral("--icon") << QStringLiteral("applications-development");
  if (timeoutMs >= 0) {
    args << QStringLiteral("--expire-time") << QString::number(timeoutMs);
  }
  args << summary;
  args << body;
  return QProcess::startDetached(notifySendPath, args);
#else
  Q_UNUSED(summary);
  Q_UNUSED(body);
  Q_UNUSED(timeoutMs);
  return false;
#endif
}

bool linuxNotifySendSupportsActions() {
#if defined(Q_OS_LINUX)
  static int cached = -1;
  if (cached >= 0) {
    return cached == 1;
  }

  static const QString notifySendPath =
      QStandardPaths::findExecutable(QStringLiteral("notify-send"));
  if (notifySendPath.isEmpty()) {
    cached = 0;
    return false;
  }

  QProcess helpProcess;
  helpProcess.start(notifySendPath, {QStringLiteral("--help")});
  if (!helpProcess.waitForFinished(500)) {
    helpProcess.kill();
    cached = 0;
    return false;
  }

  const QString helpText =
      QString::fromUtf8(helpProcess.readAllStandardOutput()) +
      QString::fromUtf8(helpProcess.readAllStandardError());
  cached = helpText.contains(QStringLiteral("--action")) ? 1 : 0;
  return cached == 1;
#else
  return false;
#endif
}

bool sendLinuxDesktopNotificationWithAction(QObject* owner,
                                            const QString& summary,
                                            const QString& body,
                                            const QString& actionText,
                                            std::function<void()> action,
                                            int timeoutMs) {
#if defined(Q_OS_LINUX)
  if (!owner || actionText.trimmed().isEmpty() || !action ||
      !linuxNotifySendSupportsActions()) {
    return false;
  }

  static const QString notifySendPath =
      QStandardPaths::findExecutable(QStringLiteral("notify-send"));
  if (notifySendPath.isEmpty()) {
    return false;
  }

  const QString cleanActionText = actionText.trimmed().remove(QLatin1Char('&'));

  auto* process = new QProcess(owner);
  QStringList args;
  args << QStringLiteral("--app-name") << QStringLiteral("Rewritto IDE");
  args << QStringLiteral("--icon") << QStringLiteral("applications-development");
  if (timeoutMs >= 0) {
    args << QStringLiteral("--expire-time") << QString::number(timeoutMs);
  }
  args << QStringLiteral("--action=rewritto-action=%1").arg(cleanActionText);
  args << summary;
  args << body;

  process->start(notifySendPath, args);
  if (!process->waitForStarted(200)) {
    process->deleteLater();
    return false;
  }

  // Early failure detection (e.g. bad flags or missing service) without blocking
  // action-capable notifications, which wait for user interaction.
  if (process->waitForFinished(100)) {
    const bool ok = (process->exitStatus() == QProcess::NormalExit &&
                     process->exitCode() == 0);
    const QString selectedAction =
        QString::fromUtf8(process->readAllStandardOutput()).trimmed();
    process->deleteLater();
    if (ok && selectedAction == QStringLiteral("rewritto-action") && action) {
      action();
    }
    return ok;
  }

  QObject::connect(process, &QProcess::errorOccurred, process,
                   [process](QProcess::ProcessError) { process->deleteLater(); });
  QObject::connect(
      process,
      QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
      owner,
      [process, action = std::move(action)](int exitCode, QProcess::ExitStatus) mutable {
        const QString selectedAction =
            QString::fromUtf8(process->readAllStandardOutput()).trimmed();
        process->deleteLater();
        if (exitCode == 0 && selectedAction == QStringLiteral("rewritto-action") &&
            action) {
          action();
        }
      });
  return true;
#else
  Q_UNUSED(owner);
  Q_UNUSED(summary);
  Q_UNUSED(body);
  Q_UNUSED(actionText);
  Q_UNUSED(action);
  Q_UNUSED(timeoutMs);
  return false;
#endif
}

bool isLikelyUf2VolumeName(QString name) {
  name = name.trimmed().toLower();
  if (name.isEmpty()) {
    return false;
  }
  return name == QStringLiteral("rpi-rp2") ||
         name.contains(QStringLiteral("rp2040")) ||
         name.contains(QStringLiteral("rp2350")) ||
         name.startsWith(QStringLiteral("rp2")) ||
         name.contains(QStringLiteral("uf2"));
}

class BoardItemDelegate final : public QStyledItemDelegate {
 public:
  using QStyledItemDelegate::QStyledItemDelegate;

  void paint(QPainter* painter,
             const QStyleOptionViewItem& option,
             const QModelIndex& index) const override {
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    // Draw background
    painter->fillRect(opt.rect, (opt.state & QStyle::State_Selected) 
        ? opt.palette.highlight() : opt.palette.base());

    // Draw Text
    const QString text = index.data(Qt::DisplayRole).toString();
    const bool isFav = index.data(kRoleIsFavorite).toBool();
    
    QRect textRect = opt.rect.adjusted(8, 0, -32, 0);
    painter->setPen((opt.state & QStyle::State_Selected) 
        ? opt.palette.highlightedText().color() : opt.palette.text().color());
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text);

    // Draw Star
    const int starSize = 16;
    const QRect starRect(opt.rect.right() - 24, opt.rect.center().y() - starSize/2, starSize, starSize);
    
    QPainterPath path;
    path.moveTo(8, 0);
    path.lineTo(10.5, 5);
    path.lineTo(16, 6);
    path.lineTo(12, 10);
    path.lineTo(13, 16);
    path.lineTo(8, 13);
    path.lineTo(3, 16);
    path.lineTo(4, 10);
    path.lineTo(0, 6);
    path.lineTo(5.5, 5);
    path.closeSubpath();

    painter->translate(starRect.topLeft());
    if (isFav) {
        painter->fillPath(path, QColor("#FFD700")); // Gold
        painter->drawPath(path);
    } else {
        painter->setPen(QPen(opt.palette.color(QPalette::Text), 1));
        painter->drawPath(path);
    }

    painter->restore();
  }

  QSize sizeHint(const QStyleOptionViewItem& option,
                const QModelIndex& index) const override {
    QSize s = QStyledItemDelegate::sizeHint(option, index);
    s.setHeight(32);
    return s;
  }
};

class BoardFilterProxyModel final : public QSortFilterProxyModel {
 public:
  using QSortFilterProxyModel::QSortFilterProxyModel;

  void setPinnedFqbn(QString fqbn) { pinnedFqbn_ = fqbn.trimmed(); }

 protected:
  bool filterAcceptsRow(int sourceRow,
                        const QModelIndex& sourceParent) const override {
    if (filterRegularExpression().pattern().isEmpty()) {
      return true;
    }
    const QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
    const QString name = sourceModel()->data(idx, Qt::DisplayRole).toString();
    const QString fqbn = sourceModel()->data(idx, Qt::UserRole).toString();
    if (!pinnedFqbn_.isEmpty() && fqbn.trimmed() == pinnedFqbn_) {
      return true;
    }
    return name.contains(filterRegularExpression()) ||
           fqbn.contains(filterRegularExpression());
  }

  bool lessThan(const QModelIndex& source_left, const QModelIndex& source_right) const override {
      const bool favLeft = sourceModel()->data(source_left, kRoleIsFavorite).toBool();
      const bool favRight = sourceModel()->data(source_right, kRoleIsFavorite).toBool();

      if (favLeft != favRight) return favLeft;

      return QSortFilterProxyModel::lessThan(source_left, source_right);
  }
 
 private:
  QString pinnedFqbn_;
};

QString defaultSketchbookDir() {
  const QString home = QDir::homePath();
  const QString preferred = home + QStringLiteral("/Rewritto-ide");
  const QString previous = home + QStringLiteral("/Rewritto");
  const QString legacy = home + QStringLiteral("/Arduino");
  if (QDir(legacy).exists() && !QDir(preferred).exists() && !QDir(previous).exists()) {
    return legacy;
  }
  if (QDir(previous).exists() && !QDir(preferred).exists()) {
    return previous;
  }
  return preferred;
}

QStringList normalizeStringList(QStringList values) {
  for (QString& value : values) {
    value = value.trimmed();
  }
  values.removeAll(QString{});
  values.removeDuplicates();
  return values;
}

QStringList parseAdditionalBoardsText(const QString& text) {
  QStringList urls;
  const QStringList lines = text.split(QLatin1Char('\n'));
  urls.reserve(lines.size());

  for (QString line : lines) {
    const int commentAt = line.indexOf(QLatin1Char('#'));
    if (commentAt >= 0) {
      line = line.left(commentAt);
    }
    const QString trimmed = line.trimmed();
    if (!trimmed.isEmpty()) {
      urls << trimmed;
    }
  }

  return normalizeStringList(urls);
}

constexpr auto kSetupProfileFormat = "rewritto.setup.profile";
constexpr auto kSetupProfileVersion = 1;
constexpr auto kProjectLockFormat = "rewritto.lock";
constexpr auto kProjectLockVersion = 1;
constexpr auto kProjectBundleFormat = "rewritto.project.bundle";
constexpr auto kProjectBundleVersion = 1;
constexpr auto kProjectBundleManifestFile = "rewritto-project-bundle.json";

struct InstalledCoreSnapshot final {
  QString id;
  QString packager;
  QString architecture;
  QString installedVersion;
  QString latestVersion;
  QString name;
};

struct InstalledLibrarySnapshot final {
  QString name;
  QString version;
  QString location;
  QString installDir;
  QString sourceDir;
  QStringList providesIncludes;
};

struct CoreToolDependency final {
  QString packager;
  QString name;
  QString version;
};

bool commandSucceeded(const CommandResult& result) {
  return result.started && !result.timedOut &&
         result.exitStatus == QProcess::NormalExit && result.exitCode == 0;
}

QString normalizeIncludeToken(QString includeToken) {
  includeToken = includeToken.trimmed();
  if (includeToken.startsWith(QLatin1Char('<')) &&
      includeToken.endsWith(QLatin1Char('>')) && includeToken.size() >= 2) {
    includeToken = includeToken.mid(1, includeToken.size() - 2).trimmed();
  } else if (includeToken.startsWith(QLatin1Char('"')) &&
             includeToken.endsWith(QLatin1Char('"')) && includeToken.size() >= 2) {
    includeToken = includeToken.mid(1, includeToken.size() - 2).trimmed();
  }
  return includeToken;
}

QSet<QString> parseIncludesFromSourceText(const QString& text) {
  static const QRegularExpression includeLineRe(
      QStringLiteral(R"(^\s*#\s*include\s*[<"]([^>"]+)[>"])"));
  QSet<QString> out;
  const QStringList lines = text.split(QLatin1Char('\n'));
  for (const QString& line : lines) {
    const QRegularExpressionMatch match = includeLineRe.match(line);
    if (!match.hasMatch()) {
      continue;
    }
    const QString header = normalizeIncludeToken(match.captured(1));
    if (header.isEmpty()) {
      continue;
    }
    out.insert(header);
    out.insert(QFileInfo(header).fileName());
  }
  return out;
}

QSet<QString> collectSketchIncludeHeaders(const QString& sketchFolder) {
  QSet<QString> headers;
  if (sketchFolder.trimmed().isEmpty() || !QFileInfo(sketchFolder).isDir()) {
    return headers;
  }
  const QStringList nameFilters = {QStringLiteral("*.ino"), QStringLiteral("*.pde"),
                                   QStringLiteral("*.h"), QStringLiteral("*.hpp"),
                                   QStringLiteral("*.c"), QStringLiteral("*.cc"),
                                   QStringLiteral("*.cpp"), QStringLiteral("*.cxx")};
  QDirIterator it(sketchFolder, nameFilters, QDir::Files,
                  QDirIterator::Subdirectories);
  while (it.hasNext()) {
    const QString path = it.next();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      continue;
    }
    headers.unite(
        parseIncludesFromSourceText(QString::fromUtf8(file.readAll())));
  }
  return headers;
}

QVector<InstalledCoreSnapshot> parseInstalledCoresFromJson(const QByteArray& jsonBytes) {
  QVector<InstalledCoreSnapshot> out;
  const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes);
  if (!doc.isObject()) {
    return out;
  }
  const QJsonArray platforms = doc.object().value(QStringLiteral("platforms")).toArray();
  out.reserve(platforms.size());
  for (const QJsonValue& value : platforms) {
    if (!value.isObject()) {
      continue;
    }
    const QJsonObject platform = value.toObject();
    InstalledCoreSnapshot core;
    core.id = platform.value(QStringLiteral("id")).toString().trimmed();
    const QStringList idParts =
        core.id.split(QLatin1Char(':'), Qt::SkipEmptyParts);
    if (idParts.size() >= 2) {
      core.packager = idParts.at(0).trimmed();
      core.architecture = idParts.at(1).trimmed();
    }
    core.installedVersion =
        platform.value(QStringLiteral("installed_version")).toString().trimmed();
    core.latestVersion =
        platform.value(QStringLiteral("latest_version")).toString().trimmed();
    const QJsonObject releases = platform.value(QStringLiteral("releases")).toObject();
    if (!core.installedVersion.isEmpty()) {
      core.name = releases.value(core.installedVersion)
                      .toObject()
                      .value(QStringLiteral("name"))
                      .toString()
                      .trimmed();
    }
    if (core.name.isEmpty() && !core.latestVersion.isEmpty()) {
      core.name = releases.value(core.latestVersion)
                      .toObject()
                      .value(QStringLiteral("name"))
                      .toString()
                      .trimmed();
    }
    if (core.id.isEmpty()) {
      continue;
    }
    out.push_back(std::move(core));
  }
  std::sort(out.begin(), out.end(), [](const InstalledCoreSnapshot& left,
                                       const InstalledCoreSnapshot& right) {
    return QString::localeAwareCompare(left.id, right.id) < 0;
  });
  return out;
}

QVector<InstalledLibrarySnapshot> parseInstalledLibrariesFromJson(const QByteArray& jsonBytes) {
  QVector<InstalledLibrarySnapshot> out;
  const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes);
  if (!doc.isObject()) {
    return out;
  }
  const QJsonArray libs = doc.object().value(QStringLiteral("installed_libraries")).toArray();
  out.reserve(libs.size());
  for (const QJsonValue& value : libs) {
    if (!value.isObject()) {
      continue;
    }
    const QJsonObject lib = value.toObject().value(QStringLiteral("library")).toObject();
    InstalledLibrarySnapshot entry;
    entry.name = lib.value(QStringLiteral("name")).toString().trimmed();
    entry.version = lib.value(QStringLiteral("version")).toString().trimmed();
    entry.location = lib.value(QStringLiteral("location")).toString().trimmed();
    entry.installDir = lib.value(QStringLiteral("install_dir")).toString().trimmed();
    entry.sourceDir = lib.value(QStringLiteral("source_dir")).toString().trimmed();
    const QJsonArray includes = lib.value(QStringLiteral("provides_includes")).toArray();
    for (const QJsonValue& includeValue : includes) {
      const QString include = normalizeIncludeToken(includeValue.toString());
      if (!include.isEmpty()) {
        entry.providesIncludes << include;
        entry.providesIncludes << QFileInfo(include).fileName();
      }
    }
    entry.providesIncludes = normalizeStringList(entry.providesIncludes);
    if (entry.name.isEmpty()) {
      continue;
    }
    out.push_back(std::move(entry));
  }
  std::sort(out.begin(), out.end(), [](const InstalledLibrarySnapshot& left,
                                       const InstalledLibrarySnapshot& right) {
    return QString::localeAwareCompare(left.name, right.name) < 0;
  });
  return out;
}

QVector<CoreToolDependency> parseCoreToolDependenciesFromInstalledJson(
    const QString& installedJsonPath,
    const QString& packager,
    const QString& architecture,
    const QString& version) {
  QVector<CoreToolDependency> out;
  QFile file(installedJsonPath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return out;
  }

  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  if (!doc.isObject()) {
    return out;
  }

  const QJsonArray packages = doc.object().value(QStringLiteral("packages")).toArray();
  for (const QJsonValue& packageValue : packages) {
    if (!packageValue.isObject()) {
      continue;
    }
    const QJsonObject packageObj = packageValue.toObject();
    const QString packageName =
        packageObj.value(QStringLiteral("name")).toString().trimmed();
    if (!packager.isEmpty() && !packageName.isEmpty() &&
        packageName.compare(packager, Qt::CaseInsensitive) != 0) {
      continue;
    }

    const QJsonArray platforms = packageObj.value(QStringLiteral("platforms")).toArray();
    for (const QJsonValue& platformValue : platforms) {
      if (!platformValue.isObject()) {
        continue;
      }
      const QJsonObject platformObj = platformValue.toObject();
      const QString arch =
          platformObj.value(QStringLiteral("architecture")).toString().trimmed();
      const QString ver =
          platformObj.value(QStringLiteral("version")).toString().trimmed();
      if (!architecture.isEmpty() &&
          arch.compare(architecture, Qt::CaseInsensitive) != 0) {
        continue;
      }
      if (!version.isEmpty() && ver != version) {
        continue;
      }

      const QJsonArray dependencies =
          platformObj.value(QStringLiteral("toolsDependencies")).toArray();
      for (const QJsonValue& depValue : dependencies) {
        if (!depValue.isObject()) {
          continue;
        }
        const QJsonObject depObj = depValue.toObject();
        CoreToolDependency dep;
        dep.packager =
            depObj.value(QStringLiteral("packager")).toString().trimmed();
        dep.name = depObj.value(QStringLiteral("name")).toString().trimmed();
        dep.version =
            depObj.value(QStringLiteral("version")).toString().trimmed();
        if (!dep.name.isEmpty() && !dep.version.isEmpty()) {
          out.push_back(dep);
        }
      }
      return out;
    }
  }

  return out;
}

QStringList parseLibraryDependenciesFromProperties(const QString& libraryInstallDir) {
  if (libraryInstallDir.trimmed().isEmpty()) {
    return {};
  }
  const QString propertiesPath =
      QDir(libraryInstallDir).absoluteFilePath(QStringLiteral("library.properties"));
  QFile file(propertiesPath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return {};
  }

  const QString text = QString::fromUtf8(file.readAll());
  const QStringList lines = text.split(QLatin1Char('\n'));
  for (QString line : lines) {
    line = line.trimmed();
    if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
      continue;
    }
    if (!line.startsWith(QStringLiteral("depends="))) {
      continue;
    }
    line = line.mid(QStringLiteral("depends=").size()).trimmed();
    if (line.isEmpty()) {
      return {};
    }

    QStringList deps;
    const QStringList parts = line.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (QString part : parts) {
      part = part.trimmed();
      if (part.isEmpty()) {
        continue;
      }
      part.remove(QRegularExpression(QStringLiteral(R"(\s*\(.*\)\s*)")));
      part = part.trimmed();
      if (!part.isEmpty()) {
        deps << part;
      }
    }
    return normalizeStringList(deps);
  }

  return {};
}

QString settingsRootForAuxFiles() {
  QSettings settings;
  QFileInfo settingsInfo(settings.fileName());
  QDir dir = settingsInfo.absoluteDir();

  const QString orgName = QCoreApplication::organizationName().trimmed();
  if (!orgName.isEmpty() &&
      dir.dirName().compare(orgName, Qt::CaseInsensitive) == 0) {
    dir.cdUp();
  }

  return dir.absolutePath();
}

QString additionalBoardsFilePath() {
  const QString root = settingsRootForAuxFiles();
  if (!root.trimmed().isEmpty()) {
    return QDir(root).absoluteFilePath(QStringLiteral("additional-boards.txt"));
  }
  return QDir(QDir::homePath())
      .absoluteFilePath(QStringLiteral("rewritto/additional-boards.txt"));
}

QString readTextFileBestEffort(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return {};
  }
  return QString::fromUtf8(file.readAll());
}

bool writeTextFileBestEffort(const QString& path, const QByteArray& data) {
  if (path.trimmed().isEmpty()) {
    return false;
  }
  const QFileInfo info(path);
  if (!QDir().mkpath(info.absolutePath())) {
    return false;
  }
  QSaveFile save(path);
  if (!save.open(QIODevice::WriteOnly | QIODevice::Text)) {
    return false;
  }
  if (save.write(data) != data.size()) {
    return false;
  }
  return save.commit();
}

QStringList loadSeededAdditionalBoardsUrls() {
  static const QString resourcePath =
      QStringLiteral(":/config/additional-boards.txt");
  const QString userPath = additionalBoardsFilePath();

  if (!userPath.trimmed().isEmpty() && !QFileInfo::exists(userPath)) {
    QFile seededResource(resourcePath);
    if (seededResource.open(QIODevice::ReadOnly | QIODevice::Text)) {
      const QByteArray data = seededResource.readAll();
      (void)writeTextFileBestEffort(userPath, data);
    }
  }

  if (!userPath.trimmed().isEmpty()) {
    const QString userText = readTextFileBestEffort(userPath);
    const QStringList parsedUser = parseAdditionalBoardsText(userText);
    if (!parsedUser.isEmpty()) {
      return parsedUser;
    }
  }

  QFile resource(resourcePath);
  if (!resource.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return {};
  }
  return parseAdditionalBoardsText(QString::fromUtf8(resource.readAll()));
}

bool isThemeDark(QString theme) {
  theme = theme.trimmed().toLower();
  if (theme == QStringLiteral("system")) {
    if (QGuiApplication::styleHints()) {
      return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    }
    return false;
  }
  if (theme == QStringLiteral("dark") || theme == QStringLiteral("oceanic") ||
      theme == QStringLiteral("cyber") || theme == QStringLiteral("graphite") ||
      theme == QStringLiteral("nord") || theme == QStringLiteral("everforest") ||
      theme == QStringLiteral("midnight") || theme == QStringLiteral("terra")) {
    return true;
  }
  return false;
}

struct ArduinoCliConfigSnapshot final {
  QString userDir;
  QStringList additionalUrls;
};

int leadingSpaces(const QString& line) {
  int count = 0;
  while (count < line.size() && line.at(count) == QLatin1Char(' ')) {
    ++count;
  }
  return count;
}

QString unquoteYamlScalar(QString value) {
  value = value.trimmed();
  if (value.size() < 2) {
    return value;
  }
  const QChar first = value.front();
  const QChar last = value.back();
  if ((first == QLatin1Char('"') && last == QLatin1Char('"')) ||
      (first == QLatin1Char('\'') && last == QLatin1Char('\''))) {
    value.chop(1);
    value.remove(0, 1);
  }
  return value.trimmed();
}

ArduinoCliConfigSnapshot readArduinoCliConfigSnapshot(const QString& configPath) {
  ArduinoCliConfigSnapshot out;
  if (configPath.trimmed().isEmpty()) {
    return out;
  }

  QFile f(configPath);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return out;
  }

  const QString text = QString::fromUtf8(f.readAll());
  const QStringList lines = text.split('\n');

  QString currentTop;
  bool inAdditionalUrls = false;
  int additionalUrlsIndent = -1;

  for (const QString& raw : lines) {
    const int indent = leadingSpaces(raw);
    const QString trimmed = raw.trimmed();
    if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#'))) {
      continue;
    }

    if (indent == 0 && trimmed.endsWith(QLatin1Char(':')) && !trimmed.startsWith(QLatin1Char('-'))) {
      currentTop = trimmed.left(trimmed.size() - 1).trimmed();
      inAdditionalUrls = false;
      additionalUrlsIndent = -1;
      continue;
    }

    if (currentTop == QStringLiteral("directories")) {
      if (trimmed.startsWith(QStringLiteral("user:"))) {
        out.userDir = unquoteYamlScalar(trimmed.mid(QStringLiteral("user:").size()));
      }
      continue;
    }

    if (currentTop != QStringLiteral("board_manager")) {
      continue;
    }

    if (!inAdditionalUrls && trimmed == QStringLiteral("additional_urls:")) {
      inAdditionalUrls = true;
      additionalUrlsIndent = indent;
      continue;
    }

    if (inAdditionalUrls) {
      if (indent <= additionalUrlsIndent) {
        inAdditionalUrls = false;
        additionalUrlsIndent = -1;
        continue;
      }
      if (trimmed.startsWith(QLatin1Char('-'))) {
        const QString url = unquoteYamlScalar(trimmed.mid(1));
        if (!url.isEmpty()) {
          out.additionalUrls << url;
        }
      }
    }
  }

  out.additionalUrls.removeAll(QString{});
  out.additionalUrls.removeDuplicates();
  return out;
}

QString quoteYamlSingle(QString value) {
  value.replace(QStringLiteral("'"), QStringLiteral("''"));
  return QStringLiteral("'") + value + QStringLiteral("'");
}

bool updateArduinoCliConfig(const QString& configPath,
                            QString userDir,
                            QStringList additionalUrls,
                            QString* outError) {
  userDir = QDir::cleanPath(userDir.trimmed());
  additionalUrls.removeAll(QString{});
  for (QString& u : additionalUrls) {
    u = u.trimmed();
  }
  additionalUrls.removeAll(QString{});
  additionalUrls.removeDuplicates();

  if (configPath.trimmed().isEmpty()) {
    if (outError) {
      *outError = QStringLiteral("arduino-cli config path is empty.");
    }
    return false;
  }

  QFile in(configPath);
  if (!in.open(QIODevice::ReadOnly | QIODevice::Text)) {
    if (outError) {
      *outError = QStringLiteral("Could not read arduino-cli config: %1")
                      .arg(configPath);
    }
    return false;
  }

  const QString text = QString::fromUtf8(in.readAll());
  const QStringList lines = text.split('\n');

  QStringList out;
  out.reserve(lines.size() + 32);

  QString currentTop;
  bool sawDirectories = false;
  bool wroteUserDir = false;
  int directoriesIndent = 4;  // common arduino-cli config indent

  bool sawBoardManager = false;
  bool handledAdditionalUrls = false;
  bool inAdditionalUrls = false;
  int additionalUrlsIndent = -1;
  int boardManagerIndent = 2;

  auto flushMissingFor = [&](const QString& leavingTop) {
    if (leavingTop == QStringLiteral("directories")) {
      if (!userDir.isEmpty() && !wroteUserDir) {
        out << QString(directoriesIndent, QLatin1Char(' ')) +
                   QStringLiteral("user: ") + quoteYamlSingle(userDir);
        wroteUserDir = true;
      }
      return;
    }
    if (leavingTop == QStringLiteral("board_manager")) {
      if (!additionalUrls.isEmpty() && !handledAdditionalUrls) {
        out << QString(boardManagerIndent, QLatin1Char(' ')) +
                   QStringLiteral("additional_urls:");
        for (const QString& url : additionalUrls) {
          out << QString(boardManagerIndent + 2, QLatin1Char(' ')) +
                     QStringLiteral("- ") + quoteYamlSingle(url);
        }
        handledAdditionalUrls = true;
      }
      return;
    }
  };

  for (const QString& raw : lines) {
    const int indent = leadingSpaces(raw);
    const QString trimmed = raw.trimmed();

    // Track new top-level keys.
    if (indent == 0 && trimmed.endsWith(QLatin1Char(':')) &&
        !trimmed.startsWith(QLatin1Char('-'))) {
      flushMissingFor(currentTop);
      currentTop = trimmed.left(trimmed.size() - 1).trimmed();
      inAdditionalUrls = false;
      additionalUrlsIndent = -1;
      out << raw;
      continue;
    }

    if (currentTop == QStringLiteral("directories")) {
      sawDirectories = true;
      if (!userDir.isEmpty() && indent > 0 && trimmed.startsWith(QStringLiteral("user:"))) {
        out << QString(indent, QLatin1Char(' ')) +
                   QStringLiteral("user: ") + quoteYamlSingle(userDir);
        wroteUserDir = true;
        continue;
      }
      if (indent > 0 && directoriesIndent <= 0 && trimmed.contains(QLatin1Char(':')) &&
          !trimmed.startsWith(QLatin1Char('-'))) {
        directoriesIndent = indent;
      }
      out << raw;
      continue;
    }

    if (currentTop == QStringLiteral("board_manager")) {
      sawBoardManager = true;

      if (!inAdditionalUrls && trimmed == QStringLiteral("additional_urls:")) {
        handledAdditionalUrls = true;
        inAdditionalUrls = true;
        additionalUrlsIndent = indent;

        if (additionalUrls.isEmpty()) {
          out << QString(indent, QLatin1Char(' ')) + QStringLiteral("additional_urls: []");
        } else {
          out << raw;
          for (const QString& url : additionalUrls) {
            out << QString(indent + 2, QLatin1Char(' ')) +
                       QStringLiteral("- ") + quoteYamlSingle(url);
          }
        }
        continue;
      }

      if (inAdditionalUrls) {
        if (indent > additionalUrlsIndent) {
          // Drop the old list items / children.
          continue;
        }
        inAdditionalUrls = false;
        additionalUrlsIndent = -1;
        // fall through to handle the current line (next key under board_manager)
      }

      if (indent > 0 && boardManagerIndent <= 0 &&
          trimmed.contains(QLatin1Char(':')) &&
          !trimmed.startsWith(QLatin1Char('-'))) {
        boardManagerIndent = indent;
      }

      out << raw;
      continue;
    }

    out << raw;
  }

  flushMissingFor(currentTop);

  if (!userDir.isEmpty() && !sawDirectories) {
    if (!out.isEmpty() && !out.last().trimmed().isEmpty()) {
      out << QString{};
    }
    out << QStringLiteral("directories:");
    out << QString(directoriesIndent, QLatin1Char(' ')) +
               QStringLiteral("user: ") + quoteYamlSingle(userDir);
    wroteUserDir = true;
  }

  if (!additionalUrls.isEmpty() && !sawBoardManager) {
    if (!out.isEmpty() && !out.last().trimmed().isEmpty()) {
      out << QString{};
    }
    out << QStringLiteral("board_manager:");
    out << QString(boardManagerIndent, QLatin1Char(' ')) + QStringLiteral("additional_urls:");
    for (const QString& url : additionalUrls) {
      out << QString(boardManagerIndent + 2, QLatin1Char(' ')) +
                 QStringLiteral("- ") + quoteYamlSingle(url);
    }
    handledAdditionalUrls = true;
  }

  QSaveFile save(configPath);
  if (!save.open(QIODevice::WriteOnly | QIODevice::Text)) {
    if (outError) {
      *outError = QStringLiteral("Could not write arduino-cli config: %1")
                      .arg(configPath);
    }
    return false;
  }

  QString outText = out.join(QStringLiteral("\n"));
  if (!outText.endsWith(QLatin1Char('\n'))) {
    outText += QLatin1Char('\n');
  }
  const QByteArray bytes = outText.toUtf8();
  if (save.write(bytes) != bytes.size()) {
    if (outError) {
      *outError = QStringLiteral("Failed writing arduino-cli config.");
    }
    return false;
  }
  if (!save.commit()) {
    if (outError) {
      *outError = QStringLiteral("Failed saving arduino-cli config.");
    }
    return false;
  }

  if (outError) {
    outError->clear();
  }
  return true;
}

QString normalizeDiagnosticPath(QString filePath, int line, const QString& sketchFolder) {
  filePath = filePath.trimmed();
  if (filePath.isEmpty() || line <= 0) {
    return filePath;
  }

  QFileInfo info(filePath);
  if (info.isRelative() && !sketchFolder.trimmed().isEmpty()) {
    filePath = QDir(sketchFolder).absoluteFilePath(filePath);
    info = QFileInfo(filePath);
  }

  return info.absoluteFilePath();
}

bool isIdentifierChar(QChar ch) {
  return ch.isLetterOrNumber() || ch == QLatin1Char('_');
}

QString identifierNearColumn(const QString& lineText, int column) {
  if (lineText.isEmpty()) {
    return {};
  }

  int index = column > 0 ? column - 1 : 0;
  index = qBound(0, index, lineText.size() - 1);

  if (!isIdentifierChar(lineText.at(index))) {
    int nearest = -1;
    int nearestDistance = std::numeric_limits<int>::max();
    for (int i = 0; i < lineText.size(); ++i) {
      if (!isIdentifierChar(lineText.at(i))) {
        continue;
      }
      const int distance = std::abs(i - index);
      if (distance < nearestDistance) {
        nearestDistance = distance;
        nearest = i;
      }
    }
    if (nearest < 0) {
      return {};
    }
    index = nearest;
  }

  int start = index;
  while (start > 0 && isIdentifierChar(lineText.at(start - 1))) {
    --start;
  }
  int end = index;
  while (end + 1 < lineText.size() && isIdentifierChar(lineText.at(end + 1))) {
    ++end;
  }

  const QString token = lineText.mid(start, end - start + 1);
  if (token.isEmpty()) {
    return {};
  }

  static const QSet<QString> kKeywords = {
      QStringLiteral("if"),        QStringLiteral("else"),
      QStringLiteral("for"),       QStringLiteral("while"),
      QStringLiteral("switch"),    QStringLiteral("case"),
      QStringLiteral("return"),    QStringLiteral("class"),
      QStringLiteral("struct"),    QStringLiteral("enum"),
      QStringLiteral("namespace"), QStringLiteral("template"),
      QStringLiteral("typename"),  QStringLiteral("void"),
      QStringLiteral("int"),       QStringLiteral("long"),
      QStringLiteral("short"),     QStringLiteral("float"),
      QStringLiteral("double"),    QStringLiteral("char"),
      QStringLiteral("bool"),      QStringLiteral("const"),
      QStringLiteral("static"),    QStringLiteral("volatile"),
      QStringLiteral("unsigned"),  QStringLiteral("signed"),
      QStringLiteral("auto"),      QStringLiteral("new"),
      QStringLiteral("delete"),
  };

  if (kKeywords.contains(token)) {
    return {};
  }
  return token;
}

int declarationInsertPosition(QTextDocument* doc) {
  if (!doc) {
    return 0;
  }

  int insertPos = 0;
  QTextBlock block = doc->begin();
  while (block.isValid()) {
    const QString trimmed = block.text().trimmed();
    if (trimmed.isEmpty() ||
        trimmed.startsWith(QStringLiteral("//")) ||
        trimmed.startsWith(QStringLiteral("/*")) ||
        trimmed.startsWith(QStringLiteral("*")) ||
        trimmed.startsWith(QStringLiteral("*/")) ||
        trimmed.startsWith(QStringLiteral("#include")) ||
        trimmed.startsWith(QStringLiteral("#pragma once"))) {
      insertPos = block.position() + block.length();
      block = block.next();
      continue;
    }
    break;
  }

  return qBound(0, insertPos, std::max(0, doc->characterCount() - 1));
}

QString includeGuardSymbolFromPath(const QString& filePath) {
  QString base = QFileInfo(filePath).fileName().trimmed();
  if (base.isEmpty()) {
    base = QStringLiteral("HEADER");
  }
  QString symbol = base.toUpper();
  for (QChar& ch : symbol) {
    if (!ch.isLetterOrNumber()) {
      ch = QLatin1Char('_');
    }
  }
  while (symbol.contains(QStringLiteral("__"))) {
    symbol.replace(QStringLiteral("__"), QStringLiteral("_"));
  }
  if (symbol.startsWith(QLatin1Char('_'))) {
    symbol.remove(0, 1);
  }
  if (symbol.endsWith(QLatin1Char('_'))) {
    symbol.chop(1);
  }
  if (!symbol.endsWith(QStringLiteral("_H")) &&
      !symbol.endsWith(QStringLiteral("_HPP"))) {
    symbol += QStringLiteral("_H");
  }
  return QStringLiteral("REWRITTO_%1").arg(symbol);
}

QString pathFromUriOrPath(QString uriOrPath) {
  uriOrPath = uriOrPath.trimmed();
  if (uriOrPath.isEmpty()) {
    return {};
  }

  const QUrl url(uriOrPath);
  if (url.isValid() && url.isLocalFile()) {
    return QFileInfo(url.toLocalFile()).absoluteFilePath();
  }
  return QFileInfo(uriOrPath).absoluteFilePath();
}

QString languageIdForFilePath(const QString& filePath) {
  const QString ext = QFileInfo(filePath).suffix().trimmed().toLower();
  if (ext == QStringLiteral("ino") || ext == QStringLiteral("pde") ||
      ext == QStringLiteral("c") || ext == QStringLiteral("cc") ||
      ext == QStringLiteral("cpp") || ext == QStringLiteral("cxx") ||
      ext == QStringLiteral("h") || ext == QStringLiteral("hh") ||
      ext == QStringLiteral("hpp") || ext == QStringLiteral("hxx")) {
    return QStringLiteral("cpp");
  }
  return QStringLiteral("plaintext");
}

QString severityLabelForLspSeverity(int severity) {
  switch (severity) {
    case 1:
      return QStringLiteral("error");
    case 2:
      return QStringLiteral("warning");
    case 4:
      return QStringLiteral("hint");
    case 3:
    default:
      return QStringLiteral("info");
  }
}

QVector<CodeEditor::Diagnostic> editorDiagnosticsFromLsp(const QJsonArray& diagnostics) {
  QVector<CodeEditor::Diagnostic> out;
  out.reserve(diagnostics.size());
  for (const QJsonValue& value : diagnostics) {
    if (!value.isObject()) {
      continue;
    }
    const QJsonObject obj = value.toObject();
    const QJsonObject range = obj.value(QStringLiteral("range")).toObject();
    const QJsonObject start = range.value(QStringLiteral("start")).toObject();
    const QJsonObject end = range.value(QStringLiteral("end")).toObject();

    CodeEditor::Diagnostic d;
    d.startLine = qMax(0, start.value(QStringLiteral("line")).toInt());
    d.startCharacter = qMax(0, start.value(QStringLiteral("character")).toInt());
    d.endLine = qMax(d.startLine, end.value(QStringLiteral("line")).toInt());
    d.endCharacter = qMax(
        d.startCharacter + 1, end.value(QStringLiteral("character")).toInt());
    d.severity = qBound(1, obj.value(QStringLiteral("severity")).toInt(3), 4);
    out.push_back(d);
  }
  return out;
}

QVector<ProblemsWidget::Diagnostic> problemsDiagnosticsFromLsp(
    const QString& filePath,
    const QJsonArray& diagnostics) {
  QVector<ProblemsWidget::Diagnostic> out;
  out.reserve(diagnostics.size());
  for (const QJsonValue& value : diagnostics) {
    if (!value.isObject()) {
      continue;
    }
    const QJsonObject obj = value.toObject();
    const QJsonObject range = obj.value(QStringLiteral("range")).toObject();
    const QJsonObject start = range.value(QStringLiteral("start")).toObject();

    ProblemsWidget::Diagnostic d;
    d.filePath = filePath;
    d.line = qMax(0, start.value(QStringLiteral("line")).toInt() + 1);
    d.column = qMax(0, start.value(QStringLiteral("character")).toInt() + 1);
    d.severity =
        severityLabelForLspSeverity(obj.value(QStringLiteral("severity")).toInt(3));
    d.message = obj.value(QStringLiteral("message")).toString().trimmed();
    out.push_back(d);
  }
  return out;
}

QString hoverContentsToText(const QJsonValue& contentsValue) {
  auto stripMarkdown = [](QString text) {
    text.replace(QRegularExpression(QStringLiteral("```[\\s\\S]*?```")),
                 QStringLiteral(" "));
    text.replace(QRegularExpression(QStringLiteral("`([^`]+)`")),
                 QStringLiteral("\\1"));
    text.replace(QRegularExpression(QStringLiteral("\\*\\*([^*]+)\\*\\*")),
                 QStringLiteral("\\1"));
    text.replace(QRegularExpression(QStringLiteral("\\*([^*]+)\\*")),
                 QStringLiteral("\\1"));
    return text.simplified();
  };

  if (contentsValue.isString()) {
    return stripMarkdown(contentsValue.toString());
  }
  if (contentsValue.isArray()) {
    QStringList parts;
    const QJsonArray arr = contentsValue.toArray();
    for (const QJsonValue& v : arr) {
      const QString piece = hoverContentsToText(v).trimmed();
      if (!piece.isEmpty()) {
        parts << piece;
      }
    }
    return parts.join(QStringLiteral("\n"));
  }
  if (contentsValue.isObject()) {
    const QJsonObject obj = contentsValue.toObject();
    if (obj.contains(QStringLiteral("value"))) {
      return stripMarkdown(obj.value(QStringLiteral("value")).toString());
    }
    if (obj.contains(QStringLiteral("language")) ||
        obj.contains(QStringLiteral("kind"))) {
      return stripMarkdown(obj.value(QStringLiteral("value")).toString());
    }
  }
  return {};
}

bool lspRangeToDocumentOffsets(QTextDocument* doc,
                               const QJsonObject& rangeObj,
                               int* outStart,
                               int* outEnd) {
  if (!doc || !outStart || !outEnd) {
    return false;
  }
  const QJsonObject startObj = rangeObj.value(QStringLiteral("start")).toObject();
  const QJsonObject endObj = rangeObj.value(QStringLiteral("end")).toObject();
  if (startObj.isEmpty() || endObj.isEmpty()) {
    return false;
  }

  auto positionFor = [doc](const QJsonObject& posObj) {
    const int line = qMax(0, posObj.value(QStringLiteral("line")).toInt());
    const int character = qMax(0, posObj.value(QStringLiteral("character")).toInt());
    QTextBlock block = doc->findBlockByNumber(line);
    if (!block.isValid()) {
      return qMax(0, doc->characterCount() - 1);
    }
    const int offsetInLine = qBound(0, character, block.text().size());
    return block.position() + offsetInLine;
  };

  int start = positionFor(startObj);
  int end = positionFor(endObj);
  if (end < start) {
    std::swap(start, end);
  }
  *outStart = start;
  *outEnd = end;
  return true;
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setWindowTitle("Rewritto-ide");
  setWindowIcon(
      QIcon::fromTheme(QStringLiteral("com.rewritto.ide"),
                       QIcon(QStringLiteral(":/icons/app-icon.svg"))));
  resize(1200, 800);  // Set a sensible default size

  sketchManager_ = new SketchManager(this);
  arduinoCli_ = new ArduinoCli(this);
  lsp_ = new LspClient(this);
  lspRestartTimer_ = new QTimer(this);
  lspRestartTimer_->setSingleShot(true);
  lspRestartTimer_->setInterval(600);
  connect(lspRestartTimer_, &QTimer::timeout, this,
          [this] { restartLanguageServer(); });

  boardOptionsRefreshTimer_ = new QTimer(this);
  boardOptionsRefreshTimer_->setSingleShot(true);
  boardOptionsRefreshTimer_->setInterval(250);
  connect(boardOptionsRefreshTimer_, &QTimer::timeout, this,
          [this] { refreshBoardOptions(); });

  serialReconnectTimer_ = new QTimer(this);
  serialReconnectTimer_->setSingleShot(true);

  loadFavorites();
  createActions();
  createMenus();
  createLayout();
  if (auto* app = qobject_cast<QApplication*>(QCoreApplication::instance())) {
    app->installEventFilter(this);
  }
	updateSketchbookView();
  (void)loadSeededAdditionalBoardsUrls();
  {
    QSettings settings;
    settings.beginGroup("Preferences");
	    const QString theme = settings.value("theme", "system").toString().toLower();
	    const int tabSize = settings.value("tabSize", 2).toInt();
	    const bool insertSpaces = settings.value("insertSpaces", true).toBool();
	    const bool showIndentGuides = settings.value("showIndentGuides", true).toBool();
	    const bool showWhitespace = settings.value("showWhitespace", false).toBool();
	    const bool wordWrap = settings.value("wordWrap", false).toBool();
	    const int zoomSteps = settings.value("editorZoomSteps", 0).toInt();
	    const bool autosaveEnabled = settings.value("autosaveEnabled", false).toBool();
	    const int autosaveInterval = settings.value("autosaveInterval", 30).toInt();
	    const QString editorFontFamily =
	        settings.value("editorFontFamily").toString().trimmed();
	    const int editorFontSize = settings.value("editorFontSize", 0).toInt();
	    settings.endGroup();
	    if (editor_) {
	      QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
	      if (!editorFontFamily.isEmpty()) {
	        font.setFamily(editorFontFamily);
	      }
	      if (editorFontSize > 0) {
	        font.setPointSize(editorFontSize);
	      }
              editor_->setTheme(isThemeDark(theme));
	      editor_->setEditorFont(font);
	      editor_->setEditorSettings(tabSize, insertSpaces);
	      editor_->setShowIndentGuides(showIndentGuides);
	      editor_->setShowWhitespace(showWhitespace);
	      editor_->setAutosaveEnabled(autosaveEnabled);
	      editor_->setAutosaveIntervalSeconds(autosaveInterval);
	      editor_->setWordWrapEnabled(wordWrap);
	      editor_->applyZoomDelta(zoomSteps);
	      if (actionZoomReset_) {
	        actionZoomReset_->setEnabled(editor_->zoomSteps() != 0);
      }
    }
    if (actionWordWrap_) {
      actionWordWrap_->setChecked(wordWrap);
    }
  }
  wireSignals();
  restoreStateFromSettings();
  migrateSketchListsToFolders();

  // Restore (or initialize) programmer selection.
  {
    const QString savedProgrammer = currentProgrammer();
    QAction* selected = nullptr;
    auto matchesSaved = [&savedProgrammer](QAction* a) -> bool {
      if (!a) return false;
      return a->data().toString().trimmed() == savedProgrammer;
    };

    if (!savedProgrammer.isEmpty()) {
      if (matchesSaved(actionProgrammerAVRISP_)) selected = actionProgrammerAVRISP_;
      else if (matchesSaved(actionProgrammerUSBasp_)) selected = actionProgrammerUSBasp_;
      else if (matchesSaved(actionProgrammerArduinoISP_)) selected = actionProgrammerArduinoISP_;
      else if (matchesSaved(actionProgrammerUSBTinyISP_)) selected = actionProgrammerUSBTinyISP_;
    }

    if (!selected) {
      if (actionProgrammerAVRISP_) selected = actionProgrammerAVRISP_;
      else if (actionProgrammerUSBasp_) selected = actionProgrammerUSBasp_;
      else if (actionProgrammerArduinoISP_) selected = actionProgrammerArduinoISP_;
      else if (actionProgrammerUSBTinyISP_) selected = actionProgrammerUSBTinyISP_;
    }

    if (selected) {
      selected->setChecked(true);
      if (savedProgrammer.isEmpty()) {
        QSettings settings;
        settings.beginGroup(kSettingsGroup);
        settings.setValue(kProgrammerKey, selected->data().toString().trimmed());
        settings.endGroup();
      }
    }
  }

  const QString restoredSketchFolder =
      normalizeSketchFolderPath(editor_ ? editor_->currentFilePath() : QString{});
  if (!restoredSketchFolder.isEmpty()) {
    if (sketchManager_) {
      sketchManager_->openSketchFolder(restoredSketchFolder);
    }
    if (fileModel_ && fileTree_) {
      fileModel_->setRootPath(restoredSketchFolder);
      const QModelIndex root = fileModel_->index(restoredSketchFolder);
      fileTree_->setRootIndex(root);
      fileTree_->expand(root);
    }
    if (editor_) {
      editor_->setDefaultSaveDirectory(restoredSketchFolder);
    }
    applyPreferredFqbnForSketch(restoredSketchFolder);
    if (actionPinSketch_) {
      const QSignalBlocker blocker(actionPinSketch_);
      actionPinSketch_->setChecked(isSketchPinned(restoredSketchFolder));
    }
  }
  updateWelcomePage();

  portsWatchDebounceTimer_ = new QTimer(this);
  portsWatchDebounceTimer_->setSingleShot(true);
  portsWatchDebounceTimer_->setInterval(250);
  connect(portsWatchDebounceTimer_, &QTimer::timeout, this, [this] {
    if (arduinoCli_ && arduinoCli_->isRunning()) {
      portsRefreshQueued_ = true;
      return;
    }
    refreshConnectedPorts();
  });

  portsAutoRefreshTimer_ = new QTimer(this);
  portsAutoRefreshTimer_->setInterval(2500);
  connect(portsAutoRefreshTimer_, &QTimer::timeout, this, [this] {
    if (portsWatchProcess_ && portsWatchProcess_->state() != QProcess::NotRunning) {
      return;
    }
    if (arduinoCli_ && arduinoCli_->isRunning()) {
      return;
    }
    refreshConnectedPorts();
  });
  portsAutoRefreshTimer_->start();

  startPortWatcher();

  // Initial refresh of boards and ports
  QTimer::singleShot(500, this, [this] {
    refreshInstalledBoards();
    refreshConnectedPorts();
  });

  bool checkIndexesOnStartup = false;
  {
    QSettings settings;
    settings.beginGroup("Preferences");
    checkIndexesOnStartup =
        settings.value(kPrefCheckIndexesOnStartupKey, false).toBool();
    settings.endGroup();
  }
  if (checkIndexesOnStartup) {
    // Optional background index updates (boards + libraries) when stale.
    QTimer::singleShot(1000, this, [this] {
      if (boardsManager_) {
        boardsManager_->refresh();
      }
      if (libraryManager_) {
        libraryManager_->refresh();
      }
    });
  }

  if (actionMcpAutostart_ && actionMcpAutostart_->isChecked()) {
    QTimer::singleShot(1200, this, [this] { startMcpServer(); });
  }

  statusBar()->showMessage("Ready");
}

MainWindow::~MainWindow() {
  if (mcpServerProcess_ && mcpServerProcess_->state() != QProcess::NotRunning) {
    mcpStopRequested_ = true;
    mcpServerProcess_->terminate();
    if (!mcpServerProcess_->waitForFinished(1200)) {
      mcpServerProcess_->kill();
      (void)mcpServerProcess_->waitForFinished(400);
    }
  }
  clearPendingUploadFlow();
  stopRefreshProcesses();
  stopPortWatcher();
  stopLanguageServer();
  if (serialPort_) {
    serialPort_->closePort();
  }
}

void MainWindow::openPaths(const QStringList& paths) {
  QStringList absPaths;
  for (const QString& p : paths) {
    if (p.trimmed().isEmpty()) continue;
    absPaths << QDir(p).absolutePath();
  }
  if (absPaths.isEmpty()) return;

  for (const QString& p : absPaths) {
    const QFileInfo fi(p);
    if (fi.isDir() && SketchManager::isSketchFolder(p)) {
      (void)openSketchFolderInUi(p);
      continue;
    }

    if (fi.isFile()) {
      const QString suffix = fi.suffix().toLower();
      if (suffix == QStringLiteral("ino") || suffix == QStringLiteral("pde")) {
        (void)openSketchFolderInUi(fi.absolutePath());
      } else if (editor_) {
        editor_->openFile(p);
        updateWelcomeVisibility();
      }
    }
  }
}

void MainWindow::createActions() {
  actionNewSketch_ = new QAction(tr("New Sketch\u2026"), this);
  actionNewSketch_->setShortcut(QKeySequence::New);

  actionOpenSketch_ = new QAction(tr("Open Sketch\u2026"), this);
  actionOpenSketch_->setShortcut(QKeySequence::Open);

  actionOpenSketchFolder_ = new QAction(tr("Open Sketch Folder\u2026"), this);

  actionNewTab_ = new QAction(tr("New Tab\u2026"), this);

  actionCloseTab_ = new QAction(tr("Close Tab"), this);
  actionCloseTab_->setShortcut(QKeySequence::Close);

  actionReopenClosedTab_ = new QAction(tr("Reopen Closed Tab"), this);
  actionReopenClosedTab_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T));

  actionCloseAllTabs_ = new QAction(tr("Close All Tabs"), this);
  actionCloseAllTabs_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_W));

  actionSave_ = new QAction(tr("Save"), this);
  actionSave_->setShortcut(QKeySequence::Save);

  actionSaveAs_ = new QAction(tr("Save As\u2026"), this);
  actionSaveAs_->setShortcut(QKeySequence::SaveAs);

  actionSaveCopyAs_ = new QAction(tr("Save Copy As\u2026"), this);

  actionSaveAll_ = new QAction(tr("Save All"), this);
  actionSaveAll_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));

  actionExportProjectZip_ = new QAction(tr("Export Project ZIP\u2026"), this);
  actionImportProjectZip_ = new QAction(tr("Import Project ZIP\u2026"), this);

  actionExamples_ = new QAction(tr("Examples\u2026"), this);

  actionPreferences_ = new QAction(tr("Preferences\u2026"), this);
  actionPreferences_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma));

  actionPinSketch_ = new QAction(tr("Pin Current Sketch"), this);
  actionPinSketch_->setCheckable(true);

  actionQuickOpen_ = new QAction(tr("Quick Open\u2026"), this);
  actionQuickOpen_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_P));

  actionCommandPalette_ = new QAction(tr("Command Palette\u2026"), this);
  actionCommandPalette_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P));

  actionQuit_ = new QAction(tr("Quit"), this);
  actionQuit_->setShortcut(QKeySequence::Quit);

  actionUndo_ = new QAction(tr("Undo"), this);
  actionUndo_->setShortcut(QKeySequence::Undo);

  actionRedo_ = new QAction(tr("Redo"), this);
  actionRedo_->setShortcut(QKeySequence::Redo);

  actionCut_ = new QAction(tr("Cut"), this);
  actionCut_->setShortcut(QKeySequence::Cut);

  actionCopy_ = new QAction(tr("Copy"), this);
  actionCopy_->setShortcut(QKeySequence::Copy);

  actionPaste_ = new QAction(tr("Paste"), this);
  actionPaste_->setShortcut(QKeySequence::Paste);

  actionSelectAll_ = new QAction(tr("Select All"), this);
  actionSelectAll_->setShortcut(QKeySequence::SelectAll);

  actionToggleComment_ = new QAction(tr("Toggle Comment"), this);
  actionToggleComment_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Slash));

  actionIncreaseIndent_ = new QAction(tr("Increase Indent"), this);
  actionIncreaseIndent_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_BracketRight));

  actionDecreaseIndent_ = new QAction(tr("Decrease Indent"), this);
  actionDecreaseIndent_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_BracketLeft));

  actionFind_ = new QAction(tr("Find\u2026"), this);
  actionFind_->setShortcut(QKeySequence::Find);

  actionFindNext_ = new QAction(tr("Find Next"), this);
  actionFindNext_->setShortcut(QKeySequence::FindNext);

  actionFindPrevious_ = new QAction(tr("Find Previous"), this);
  actionFindPrevious_->setShortcut(QKeySequence::FindPrevious);

  actionReplace_ = new QAction(tr("Replace\u2026"), this);
  actionReplace_->setShortcut(QKeySequence::Replace);

  actionFindInFiles_ = new QAction(tr("Find in Files\u2026"), this);
  actionFindInFiles_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F));

  actionReplaceInFiles_ = new QAction(tr("Replace in Files\u2026"), this);
  actionReplaceInFiles_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_H));

  actionSidebarSearch_ = new QAction(tr("Search"), this);
  actionSidebarSearch_->setCheckable(true);

  actionGoToLine_ = new QAction(tr("Go to Line\u2026"), this);
  actionGoToLine_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));

  actionGoToSymbol_ = new QAction(tr("Go to Symbol\u2026"), this);
  actionGoToSymbol_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));

  actionCompletion_ = new QAction(tr("Trigger Completion"), this);
  actionCompletion_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Space));

  actionShowHover_ = new QAction(tr("Show Hover"), this);

  actionGoToDefinition_ = new QAction(tr("Go to Definition"), this);
  actionGoToDefinition_->setShortcut(QKeySequence(Qt::Key_F12));

  actionFindReferences_ = new QAction(tr("Find References"), this);
  actionFindReferences_->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F12));

  actionRenameSymbol_ = new QAction(tr("Rename Symbol"), this);
  actionRenameSymbol_->setShortcut(QKeySequence(Qt::Key_F2));

  actionCodeActions_ = new QAction(tr("Code Actions"), this);
  actionCodeActions_->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Return));

  actionOrganizeImports_ = new QAction(tr("Organize Imports"), this);

  actionFormatDocument_ = new QAction(tr("Format Document"), this);
  actionFormatDocument_->setShortcut(
      QKeySequence(Qt::SHIFT | Qt::ALT | Qt::Key_F));

  actionVerify_ = new QAction(tr("Verify"), this);
  actionVerify_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
  actionVerify_->setIcon(QIcon(":/icons/verify.png"));
  actionVerify_->setIconVisibleInMenu(false);

  actionUpload_ = new QAction(tr("Verify and Upload"), this);
  actionUpload_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_U));
  actionUpload_->setIcon(QIcon(":/icons/upload.png"));
  actionUpload_->setIconVisibleInMenu(false);
  actionUpload_->setToolTip(tr("Compile and upload sketch"));

  actionJustUpload_ = new QAction(tr("Upload"), this);
  actionJustUpload_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_U));
  actionJustUpload_->setIcon(QIcon(":/icons/upload.png"));
  actionJustUpload_->setIconVisibleInMenu(false);
  actionJustUpload_->setToolTip(tr("Upload prebuilt binary"));
  actionJustUpload_->setEnabled(false);

  actionStop_ = new QAction(tr("Stop"), this);
  actionStop_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Period));
  actionStop_->setToolTip(tr("Stop (cancel)"));
  actionStop_->setIcon(QIcon(":/icons/stop.png"));
  actionStop_->setIconVisibleInMenu(false);
  actionStop_->setEnabled(false);

  actionUploadUsingProgrammer_ = new QAction(tr("Upload Using Programmer"), this);

  actionExportCompiledBinary_ = new QAction(tr("Export Compiled Binary"), this);

  actionOptimizeForDebug_ = new QAction(tr("Optimize for Debugging"), this);
  actionOptimizeForDebug_->setCheckable(true);
  {
    QSettings settings;
    settings.beginGroup(kSettingsGroup);
    const bool optimizeForDebug =
        settings.value(kOptimizeForDebugKey, false).toBool();
    settings.endGroup();
    actionOptimizeForDebug_->setChecked(optimizeForDebug);
  }

  actionShowSketchFolder_ = new QAction(tr("Show Sketch Folder"), this);

  actionRenameSketch_ = new QAction(tr("Rename Sketch\u2026"), this);

  actionAddFileToSketch_ = new QAction(tr("Add File to Sketch\u2026"), this);

  actionAddZipLibrary_ = new QAction(tr("Add .ZIP Library\u2026"), this);

  actionManageLibraries_ = new QAction(tr("Manage Libraries\u2026"), this);
  actionManageLibraries_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_I));

  actionRefreshBoards_ = new QAction(tr("Refresh Boards"), this);
  actionRefreshPorts_ = new QAction(tr("Refresh Ports"), this);
  actionSelectBoard_ = new QAction(tr("Select Board\u2026"), this);
  actionBoardSetupWizard_ = new QAction(tr("Board Setup Wizard\u2026"), this);

  actionBoardsManager_ = new QAction(tr("Boards Manager\u2026"), this);
  actionBoardsManager_->setCheckable(true);
  actionLibraryManager_ = new QAction(tr("Library Manager\u2026"), this);
  actionLibraryManager_->setCheckable(true);

  actionToggleFontToolBar_ = new QAction(tr("Context Toolbar"), this);
  actionToggleFontToolBar_->setCheckable(true);
  actionToggleFontToolBar_->setChecked(true);

  actionToggleBold_ = new QAction(tr("Bold"), this);
  actionToggleBold_->setCheckable(true);
  actionToggleBold_->setIconText(tr("B"));
  actionToggleBold_->setToolTip(tr("Bold"));

  actionContextFontsMode_ = new QAction(tr("Fonts"), this);
  actionContextFontsMode_->setCheckable(true);
  actionContextSnapshotsMode_ = new QAction(tr("Snapshots"), this);
  actionContextSnapshotsMode_->setCheckable(true);
  actionContextGithubMode_ = new QAction(tr("GitHub"), this);
  actionContextGithubMode_->setCheckable(true);
  actionContextMcpMode_ = new QAction(tr("MCP"), this);
  actionContextMcpMode_->setCheckable(true);

  actionSnapshotCapture_ = new QAction(tr("Capture"), this);
  actionSnapshotCompare_ = new QAction(tr("Compare"), this);
  actionSnapshotGallery_ = new QAction(tr("Gallery"), this);
  actionGithubLogin_ = new QAction(tr("Login"), this);
  actionGitInitRepo_ = new QAction(tr("Init Repo"), this);
  actionGitCommit_ = new QAction(tr("Commit"), this);
  actionGitPush_ = new QAction(tr("Push"), this);
  actionMcpConfigure_ = new QAction(tr("Configure"), this);
  actionMcpStart_ = new QAction(tr("Start"), this);
  actionMcpStop_ = new QAction(tr("Stop"), this);
  actionMcpRestart_ = new QAction(tr("Restart"), this);
  actionMcpAutostart_ = new QAction(tr("Auto-start"), this);
  actionMcpAutostart_->setCheckable(true);
  {
    QSettings settings;
    settings.beginGroup(kSettingsGroup);
    mcpServerCommand_ =
        settings.value(kMcpServerCommandKey).toString().trimmed();
    const bool mcpAutoStart =
        settings.value(kMcpAutoStartKey, false).toBool();
    settings.endGroup();
    actionMcpAutostart_->setChecked(mcpAutoStart);
  }

  actionSerialMonitor_ = new QAction(tr("Serial Monitor"), this);
  actionSerialMonitor_->setCheckable(true);
  actionSerialMonitor_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_M));
  actionSerialMonitor_->setIcon(QIcon(":/icons/serial-monitor.png"));
  actionSerialMonitor_->setIconVisibleInMenu(false);

  actionSerialPlotter_ = new QAction(tr("Serial Plotter"), this);
  actionSerialPlotter_->setCheckable(true);
  actionSerialPlotter_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_L));
  actionSerialPlotter_->setIcon(QIcon(":/icons/serial-plotter.png"));
  actionSerialPlotter_->setIconVisibleInMenu(false);

  actionBurnBootloader_ = new QAction(tr("Burn Bootloader"), this);

  actionGetBoardInfo_ = new QAction(tr("Get Board Info"), this);

  actionAbout_ = new QAction(tr("About"), this);
  actionGithubPage_ = new QAction(tr("Github page"), this);

  // Additional Tools menu actions for Rewritto-ide parity
  actionAutoFormat_ = new QAction(tr("Auto Format"), this);
  actionAutoFormat_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
  actionAutoFormat_->setStatusTip(tr("Automatically format the current sketch code"));

  actionArchiveSketch_ = new QAction(tr("Archive Sketch"), this);
  actionArchiveSketch_->setStatusTip(tr("Create a zip archive of the current sketch"));

  actionWiFiFirmwareUpdater_ = new QAction(tr("Firmware Updater"), this);
  actionWiFiFirmwareUpdater_->setStatusTip(tr("Update the firmware on compatible boards"));

  actionUploadSSL_ = new QAction(tr("Upload SSL Root Certificates"), this);
  actionUploadSSL_->setStatusTip(tr("Upload SSL Root Certificates to the board"));

  actionExportSetupProfile_ = new QAction(tr("Export Setup Profile…"), this);
  actionImportSetupProfile_ = new QAction(tr("Import Setup Profile…"), this);
  actionGenerateProjectLockfile_ = new QAction(tr("Generate Project Lockfile"), this);
  actionBootstrapProjectLockfile_ = new QAction(tr("Bootstrap Project from Lockfile"), this);
  actionEnvironmentDoctor_ = new QAction(tr("Environment Doctor…"), this);

  // Programmer submenu actions
  actionProgrammerAVRISP_ = new QAction(tr("AVRISP mkII"), this);
  actionProgrammerAVRISP_->setCheckable(true);
  actionProgrammerAVRISP_->setData("avr:atmel:avrissa");

  actionProgrammerUSBasp_ = new QAction(tr("USBasp"), this);
  actionProgrammerUSBasp_->setCheckable(true);
  actionProgrammerUSBasp_->setData("usbasp");

  actionProgrammerArduinoISP_ = new QAction(tr("Arduino as ISP"), this);
  actionProgrammerArduinoISP_->setCheckable(true);
  actionProgrammerArduinoISP_->setData("arduino:arduinoisp");

  actionProgrammerUSBTinyISP_ = new QAction(tr("USBtinyISP"), this);
  actionProgrammerUSBTinyISP_->setCheckable(true);
  actionProgrammerUSBTinyISP_->setData("usbtiny");

  // Core debugging actions (placeholders)
  actionStartDebugging_ = new QAction(tr("Start Debugging"), this);
  actionStartDebugging_->setShortcut(QKeySequence(Qt::Key_F5));

  actionStepOver_ = new QAction(tr("Step Over"), this);
  actionStepOver_->setShortcut(QKeySequence(Qt::Key_F10));

  actionStepInto_ = new QAction(tr("Step Into"), this);
  actionStepInto_->setShortcut(QKeySequence(Qt::Key_F11));

  actionStepOut_ = new QAction(tr("Step Out"), this);
  actionStepOut_->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F11));

  actionContinue_ = new QAction(tr("Continue"), this);
  actionContinue_->setShortcut(QKeySequence(Qt::Key_F5));

  actionStopDebugging_ = new QAction(tr("Stop Debugging"), this);
  actionStopDebugging_->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F5));
}

void MainWindow::createMenus() {
  QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
  fileMenu->addAction(actionNewSketch_);
  fileMenu->addAction(actionOpenSketch_);
  fileMenu->addAction(actionOpenSketchFolder_);
  fileMenu->addAction(actionQuickOpen_);
  recentSketchesMenu_ = fileMenu->addMenu(tr("Open Recent"));
  connect(recentSketchesMenu_, &QMenu::aboutToShow, this,
          [this] { rebuildRecentSketchesMenu(); });
  fileMenu->addAction(actionPinSketch_);
  examplesMenu_ = fileMenu->addMenu(tr("Examples"));
  connect(examplesMenu_, &QMenu::aboutToShow, this, &MainWindow::rebuildExamplesMenu);
  fileMenu->addSeparator();
  fileMenu->addAction(actionCloseTab_);
  fileMenu->addAction(actionReopenClosedTab_);
  fileMenu->addAction(actionCloseAllTabs_);
  fileMenu->addSeparator();
  fileMenu->addAction(actionSave_);
  fileMenu->addAction(actionSaveAs_);
  fileMenu->addAction(actionSaveCopyAs_);
  fileMenu->addAction(actionSaveAll_);
  fileMenu->addSeparator();
  fileMenu->addAction(actionExportProjectZip_);
  fileMenu->addAction(actionImportProjectZip_);
  fileMenu->addSeparator();
  fileMenu->addAction(actionPreferences_);
  fileMenu->addSeparator();
  fileMenu->addAction(actionQuit_);

  QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));
  editMenu->addAction(actionUndo_);
  editMenu->addAction(actionRedo_);
  editMenu->addAction(actionCommandPalette_);
  editMenu->addSeparator();
  editMenu->addAction(actionCut_);
  editMenu->addAction(actionCopy_);
  editMenu->addAction(actionPaste_);
  editMenu->addSeparator();
  editMenu->addAction(actionSelectAll_);
  editMenu->addSeparator();
  editMenu->addAction(actionToggleComment_);
  editMenu->addAction(actionIncreaseIndent_);
  editMenu->addAction(actionDecreaseIndent_);
  editMenu->addSeparator();
  editMenu->addAction(actionFind_);
  editMenu->addAction(actionFindNext_);
  editMenu->addAction(actionFindPrevious_);
  editMenu->addAction(actionReplace_);
  editMenu->addSeparator();
  editMenu->addAction(actionFindInFiles_);
  editMenu->addAction(actionReplaceInFiles_);

  QMenu* viewMenu = menuBar()->addMenu(tr("&View"));
  viewMenu->addAction(actionGoToLine_);
  viewMenu->addAction(actionGoToSymbol_);
  viewMenu->addSeparator();
  QMenu* codeIntelligenceMenu = viewMenu->addMenu(tr("Code Intelligence"));
  codeIntelligenceMenu->addAction(actionCompletion_);
  codeIntelligenceMenu->addAction(actionShowHover_);
  codeIntelligenceMenu->addAction(actionGoToDefinition_);
  codeIntelligenceMenu->addAction(actionFindReferences_);
  codeIntelligenceMenu->addAction(actionRenameSymbol_);
  codeIntelligenceMenu->addSeparator();
  codeIntelligenceMenu->addAction(actionCodeActions_);
  codeIntelligenceMenu->addAction(actionOrganizeImports_);
  codeIntelligenceMenu->addAction(actionFormatDocument_);
  viewMenu->addSeparator();
  viewMenu->addAction(actionSidebarSearch_);
  viewMenu->addSeparator();
  
  toolbarsMenu_ = viewMenu->addMenu(tr("Toolbars"));
  toolbarsMenu_->addAction(actionToggleFontToolBar_);

  QMenu* sketchMenu = menuBar()->addMenu(tr("&Sketch"));
  sketchMenu->addAction(actionVerify_);
  sketchMenu->addAction(actionUpload_);
  sketchMenu->addAction(actionJustUpload_);
  sketchMenu->addAction(actionUploadUsingProgrammer_);
  sketchMenu->addAction(actionExportCompiledBinary_);
  sketchMenu->addAction(actionOptimizeForDebug_);
  sketchMenu->addSeparator();
  sketchMenu->addAction(actionShowSketchFolder_);
  sketchMenu->addSeparator();
  includeLibraryMenu_ = sketchMenu->addMenu(tr("Include Library"));
  connect(includeLibraryMenu_, &QMenu::aboutToShow, this,
          &MainWindow::rebuildIncludeLibraryMenu);

  toolsMenu_ = menuBar()->addMenu(tr("&Tools"));
  // Group 0: Main
  toolsMenu_->addAction(actionAutoFormat_);
  toolsMenu_->addAction(actionArchiveSketch_);
  toolsMenu_->addAction(actionManageLibraries_);
  toolsMenu_->addAction(actionSerialMonitor_);
  toolsMenu_->addAction(actionSerialPlotter_);
  toolsMenu_->addSeparator();

  // Group 1: Firmware Uploader
  toolsMenu_->addAction(actionWiFiFirmwareUpdater_);
  toolsMenu_->addAction(actionUploadSSL_);
  QMenu* environmentMenu = toolsMenu_->addMenu(tr("Setup && Environment"));
  environmentMenu->addAction(actionExportSetupProfile_);
  environmentMenu->addAction(actionImportSetupProfile_);
  environmentMenu->addSeparator();
  environmentMenu->addAction(actionGenerateProjectLockfile_);
  environmentMenu->addAction(actionBootstrapProjectLockfile_);
  environmentMenu->addSeparator();
  environmentMenu->addAction(actionEnvironmentDoctor_);
  toolsMenu_->addSeparator();

  // Group 2: Board selection section
  boardMenu_ = new QMenu(tr("Board"), toolsMenu_);
  boardMenu_->setObjectName("BoardMenu");
  toolsMenu_->addMenu(boardMenu_);
  connect(boardMenu_, &QMenu::aboutToShow, this, [this] { rebuildBoardMenu(); });

  portMenu_ = new QMenu(tr("Port"), toolsMenu_);
  portMenu_->setObjectName("PortMenu");
  portMenuAction_ = toolsMenu_->addMenu(portMenu_);
  connect(portMenu_, &QMenu::aboutToShow, this, [this] { rebuildPortMenu(); });

  toolsMenu_->addAction(actionGetBoardInfo_);
  toolsMenu_->addSeparator();

  // Group 3: Programmer and Burn Bootloader
  programmerMenu_ = new QMenu(tr("Programmer"), toolsMenu_);
  programmerMenuAction_ = toolsMenu_->addMenu(programmerMenu_);
  programmerMenu_->addAction(actionProgrammerAVRISP_);
  programmerMenu_->addAction(actionProgrammerUSBasp_);
  programmerMenu_->addAction(actionProgrammerArduinoISP_);
  programmerMenu_->addAction(actionProgrammerUSBTinyISP_);
  // Programmer selection group
  QActionGroup* programmerGroup = new QActionGroup(this);
  programmerGroup->addAction(actionProgrammerAVRISP_);
  programmerGroup->addAction(actionProgrammerUSBasp_);
  programmerGroup->addAction(actionProgrammerArduinoISP_);
  programmerGroup->addAction(actionProgrammerUSBTinyISP_);
  programmerGroup->setExclusive(true);

  toolsMenu_->addAction(actionBurnBootloader_);

  QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
  helpMenu->addAction(actionGithubPage_);
  helpMenu->addAction(actionAbout_);

  // Connect actions to their handlers
  connect(actionPreferences_, &QAction::triggered, this, [this] {
    auto* dialog = new PreferencesDialog(this);

    // Load current settings
    QSettings settings;
    settings.beginGroup("Preferences");

    const QString initialTheme = settings.value("theme", "system").toString();
    dialog->setTheme(initialTheme);

    QString initialLocale = settings.value("locale").toString();
    if (initialLocale.trimmed().isEmpty()) {
      initialLocale = settings.value("language", "system").toString();
    }
    dialog->setLanguage(initialLocale);

    const double initialUiScale = settings.value("uiScale", 1.0).toDouble();
    dialog->setUiScale(initialUiScale);

    const ArduinoCliConfigSnapshot cliSnapshot = readArduinoCliConfigSnapshot(
        arduinoCli_ ? arduinoCli_->arduinoCliConfigPath() : QString{});

    QString sketchbookDir;
    if (settings.contains("sketchbookDir")) {
      sketchbookDir = settings.value("sketchbookDir").toString();
    } else {
      sketchbookDir = cliSnapshot.userDir;
    }
    if (sketchbookDir.trimmed().isEmpty()) {
      sketchbookDir = defaultSketchbookDir();
    }
    dialog->setSketchbookDir(sketchbookDir);

    QStringList additionalUrls;
    if (settings.contains("additionalUrls")) {
      additionalUrls = settings.value("additionalUrls").toStringList();
    } else {
      additionalUrls = cliSnapshot.additionalUrls;
    }
    dialog->setAdditionalUrls(additionalUrls);

    // Editor settings
    QFont editorFont;
    const QString fontFamily = settings.value("editorFontFamily").toString();
    if (!fontFamily.isEmpty()) {
      editorFont.setFamily(fontFamily);
    } else {
      editorFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    }
    const int fontSize = settings.value("editorFontSize", 0).toInt();
    if (fontSize > 0) {
      editorFont.setPointSize(fontSize);
    }
    dialog->setEditorFont(editorFont);

    dialog->setTabSize(settings.value("tabSize", 2).toInt());
    dialog->setInsertSpaces(settings.value("insertSpaces", true).toBool());
    dialog->setShowIndentGuides(settings.value("showIndentGuides", true).toBool());
    dialog->setShowWhitespace(settings.value("showWhitespace", false).toBool());
    dialog->setDefaultLineEnding(settings.value("defaultLineEnding", "LF").toString());
    dialog->setTrimTrailingWhitespace(settings.value("trimTrailingWhitespace", false).toBool());
    dialog->setAutosaveEnabled(settings.value("autosaveEnabled", false).toBool());
    dialog->setAutosaveIntervalSeconds(settings.value("autosaveInterval", 30).toInt());

    // Compiler settings
    dialog->setWarningsLevel(settings.value("compilerWarnings", "none").toString());
    dialog->setVerboseCompile(settings.value("verboseCompile", false).toBool());
    dialog->setVerboseUpload(settings.value("verboseUpload", false).toBool());
    dialog->setCheckIndexesOnStartup(
        settings.value(kPrefCheckIndexesOnStartupKey, false).toBool());

    // Proxy settings
    dialog->setProxyType(settings.value("proxyType", "none").toString());
    dialog->setProxyHost(settings.value("proxyHost").toString());
    dialog->setProxyPort(settings.value("proxyPort", 8080).toInt());
    dialog->setProxyUsername(settings.value("proxyUsername").toString());
    dialog->setProxyPassword(settings.value("proxyPassword").toString());
    dialog->setNoProxyHosts(settings.value("noProxyHosts").toStringList());

    settings.endGroup();

    // Live preview (theme + UI scale).
    connect(dialog, &PreferencesDialog::themePreviewRequested, this,
            [this](const QString& theme) {
              ThemeManager::apply(theme);
              rebuildContextToolbar();
              if (editor_) {
                editor_->setTheme(isThemeDark(theme));
              }
            });
    connect(dialog, &PreferencesDialog::uiScalePreviewRequested, this,
            [this](double scale) {
              UiScaleManager::apply(scale);
            });

    const int result = dialog->exec();
    if (result == QDialog::Accepted) {
      // Apply settings
      settings.beginGroup("Preferences");
      settings.setValue("theme", dialog->theme());
      settings.setValue("locale", dialog->language());
      settings.setValue("uiScale", dialog->uiScale());
      const QString sketchDir = dialog->sketchbookDir();
      if (!sketchDir.isEmpty()) {
        settings.setValue("sketchbookDir", sketchDir);
      }
      settings.setValue("additionalUrls", dialog->additionalUrls());

      const QFont newFont = dialog->editorFont();
      settings.setValue("editorFontFamily", newFont.family());
      settings.setValue("editorFontSize", newFont.pointSize());
      settings.setValue("tabSize", dialog->tabSize());
      settings.setValue("insertSpaces", dialog->insertSpaces());
      settings.setValue("showIndentGuides", dialog->showIndentGuides());
      settings.setValue("showWhitespace", dialog->showWhitespace());
      settings.setValue("defaultLineEnding", dialog->defaultLineEnding());
      settings.setValue("trimTrailingWhitespace", dialog->trimTrailingWhitespace());
      settings.setValue("autosaveEnabled", dialog->autosaveEnabled());
      settings.setValue("autosaveInterval", dialog->autosaveIntervalSeconds());
      settings.setValue("compilerWarnings", dialog->warningsLevel());
      settings.setValue("verboseCompile", dialog->verboseCompile());
      settings.setValue("verboseUpload", dialog->verboseUpload());
      settings.setValue(kPrefCheckIndexesOnStartupKey,
                        dialog->checkIndexesOnStartup());

      settings.setValue("proxyType", dialog->proxyType());
      settings.setValue("proxyHost", dialog->proxyHost());
      settings.setValue("proxyPort", dialog->proxyPort());
      settings.setValue("proxyUsername", dialog->proxyUsername());
      settings.setValue("proxyPassword", dialog->proxyPassword());
      settings.setValue("noProxyHosts", dialog->noProxyHosts());
      settings.endGroup();

      // Apply theme + UI scale
      UiScaleManager::apply(dialog->uiScale());
      ThemeManager::apply(dialog->theme());
      rebuildContextToolbar();

      // Apply editor settings
      if (editor_) {
        editor_->setTheme(isThemeDark(dialog->theme()));
        editor_->setEditorFont(newFont);
        editor_->setEditorSettings(dialog->tabSize(), dialog->insertSpaces());
        editor_->setShowIndentGuides(dialog->showIndentGuides());
        editor_->setShowWhitespace(dialog->showWhitespace());
        editor_->setAutosaveEnabled(dialog->autosaveEnabled());
        editor_->setAutosaveIntervalSeconds(dialog->autosaveIntervalSeconds());
      }

      updateSketchbookView();

      // Keep arduino-cli config in sync with Preferences.
      if (arduinoCli_) {
        const QString configPath = arduinoCli_->arduinoCliConfigPath();
        QString configError;
        const QString userDir =
            sketchDir.trimmed().isEmpty() ? defaultSketchbookDir() : sketchDir;
        const QStringList urls = dialog->additionalUrls();
        if (configPath.trimmed().isEmpty()) {
          if (output_) {
            output_->appendLine(
                tr("[Preferences] Arduino CLI config path is unavailable; skipping config sync."));
          }
        } else if (!updateArduinoCliConfig(configPath, userDir, urls, &configError)) {
          if (output_) {
            output_->appendLine(tr("[Preferences] Failed to update Arduino CLI config: %1")
                                    .arg(configError.trimmed()));
          }
          showToast(tr("Failed to update Arduino CLI configuration"));
        } else {
          showToast(tr("Arduino CLI configuration updated"));
        }
      }

      if (dialog->language().trimmed() != initialLocale.trimmed()) {
        QMessageBox::information(this, tr("Restart Required"),
                                 tr("Language changes will take effect after restarting the IDE."));
      }
    } else {
      UiScaleManager::apply(initialUiScale);
      ThemeManager::apply(initialTheme);
      rebuildContextToolbar();
      if (editor_) {
        editor_->setTheme(isThemeDark(initialTheme));
      }
    }

    dialog->deleteLater();
  });

  // === File Menu Actions ===
  connect(actionNewSketch_, &QAction::triggered, this, [this] {
    newSketch();
  });

  connect(actionOpenSketch_, &QAction::triggered, this, [this] {
    openSketch();
  });

  connect(actionOpenSketchFolder_, &QAction::triggered, this, [this] {
    openSketchFolder();
  });

  connect(actionQuickOpen_, &QAction::triggered, this, [this] {
    showQuickOpen();
  });

  connect(actionPinSketch_, &QAction::toggled, this, [this](bool pinned) {
    if (!actionPinSketch_) {
      return;
    }
    const QString folder = currentSketchFolderPath();
    if (folder.isEmpty()) {
      const QSignalBlocker blocker(actionPinSketch_);
      actionPinSketch_->setChecked(false);
      return;
    }
    setSketchPinned(folder, pinned);
    showToast(pinned ? tr("Sketch pinned") : tr("Sketch unpinned"));
  });

  connect(actionCloseTab_, &QAction::triggered, this, [this] {
    if (editor_) {
      editor_->closeCurrentTab();
      updateWelcomeVisibility();
    }
  });

  connect(actionReopenClosedTab_, &QAction::triggered, this, [this] {
    if (editor_) {
      editor_->reopenLastClosedTab();
      updateWelcomeVisibility();
    }
  });

  connect(actionCloseAllTabs_, &QAction::triggered, this, [this] {
    if (editor_) {
      editor_->closeAllTabs();
      updateWelcomeVisibility();
    }
  });

  connect(actionSave_, &QAction::triggered, this, [this] {
    if (editor_) {
      editor_->save();
    }
  });

  connect(actionSaveAs_, &QAction::triggered, this, [this] {
    if (!editor_) {
      return;
    }

    QString initialPath = editor_->currentFilePath().trimmed();
    if (initialPath.isEmpty()) {
      QString baseDir = currentSketchFolderPath();
      if (baseDir.isEmpty()) {
        baseDir = defaultSketchbookDir();
      }
      if (QFileInfo(baseDir).isDir()) {
        initialPath =
            QDir(baseDir).absoluteFilePath(QStringLiteral("Untitled.ino"));
      }
    }

    const QString chosen = QFileDialog::getSaveFileName(
        this, tr("Save As"), initialPath,
        tr("Rewritto Sketch (*.ino);;C/C++ Files (*.c *.cc *.cpp *.cxx *.h *.hh *.hpp *.hxx);;All Files (*)"));
    if (chosen.trimmed().isEmpty()) {
      return;
    }
    if (!editor_->saveAs(chosen)) {
      QMessageBox::warning(this, tr("Save Failed"), tr("Could not save file."));
    }
  });

  connect(actionSaveCopyAs_, &QAction::triggered, this, [this] {
    if (editor_) {
      QString path = editor_->currentFilePath();
      if (path.isEmpty()) {
        if (editor_->saveCurrentWithDialog()) {
          path = editor_->currentFilePath();
        }
      }
      if (!path.isEmpty()) {
        const QString baseName = QFileInfo(path).completeBaseName();
        QString suggested = QFileInfo(path).absolutePath() + "/" + baseName + "_copy." + QFileInfo(path).suffix();
        const QString chosen = QFileDialog::getSaveFileName(this, tr("Save Copy As"), suggested);
        if (!chosen.isEmpty()) {
          editor_->saveCopyAs(chosen);
        }
      }
    }
  });

  connect(actionSaveAll_, &QAction::triggered, this, [this] {
    if (editor_) {
      editor_->saveAll();
    }
  });

  connect(actionExportProjectZip_, &QAction::triggered, this, [this] {
    exportProjectZip();
  });

  connect(actionImportProjectZip_, &QAction::triggered, this, [this] {
    importProjectZip();
  });

  connect(actionQuit_, &QAction::triggered, this, [this] {
    close();
  });

  // === Edit Menu Actions ===
  connect(actionUndo_, &QAction::triggered, this, [this] {
    if (auto* ed = qobject_cast<QPlainTextEdit*>(editor_->currentEditorWidget())) {
      ed->undo();
    }
  });

  connect(actionRedo_, &QAction::triggered, this, [this] {
    if (auto* ed = qobject_cast<QPlainTextEdit*>(editor_->currentEditorWidget())) {
      ed->redo();
    }
  });

  connect(actionCut_, &QAction::triggered, this, [this] {
    if (auto* ed = qobject_cast<QPlainTextEdit*>(editor_->currentEditorWidget())) {
      ed->cut();
    }
  });

  connect(actionCopy_, &QAction::triggered, this, [this] {
    if (auto* ed = qobject_cast<QPlainTextEdit*>(editor_->currentEditorWidget())) {
      ed->copy();
    }
  });

  connect(actionPaste_, &QAction::triggered, this, [this] {
    if (auto* ed = qobject_cast<QPlainTextEdit*>(editor_->currentEditorWidget())) {
      ed->paste();
    }
  });

  connect(actionSelectAll_, &QAction::triggered, this, [this] {
    if (auto* ed = qobject_cast<QPlainTextEdit*>(editor_->currentEditorWidget())) {
      ed->selectAll();
    }
  });

  connect(actionToggleComment_, &QAction::triggered, this, [this] {
    if (auto* ed = qobject_cast<CodeEditor*>(editor_->currentEditorWidget())) {
      // Toggle comment on current line or selection
      QTextCursor cursor = ed->textCursor();
      if (!cursor.hasSelection()) {
        cursor.select(QTextCursor::LineUnderCursor);
      }
      QString text = cursor.selectedText();
      if (text.trimmed().startsWith("//")) {
        // Remove comment
        text.remove(QRegularExpression("^\\s*//"));
        cursor.insertText(text);
      } else {
        // Add comment
        text.prepend("// ");
        cursor.insertText(text);
      }
    }
  });

  connect(actionIncreaseIndent_, &QAction::triggered, this, [this] {
    if (auto* ed = qobject_cast<QPlainTextEdit*>(editor_->currentEditorWidget())) {
      QTextCursor cursor = ed->textCursor();
      if (!cursor.hasSelection()) {
        cursor.select(QTextCursor::LineUnderCursor);
      }
      const int startPos = cursor.selectionStart();
      const int endPos = cursor.selectionEnd();
      QTextDocument* doc = ed->document();
      for (int i = startPos; i <= endPos; ) {
        QTextBlock block = doc->findBlock(i);
        if (!block.isValid()) break;
        QTextCursor insertCursor(doc);
        insertCursor.setPosition(block.position());
        insertCursor.insertText("\t");
        i = block.position() + block.length();
      }
    }
  });

  connect(actionDecreaseIndent_, &QAction::triggered, this, [this] {
    if (auto* ed = qobject_cast<QPlainTextEdit*>(editor_->currentEditorWidget())) {
      QTextCursor cursor = ed->textCursor();
      if (!cursor.hasSelection()) {
        cursor.select(QTextCursor::LineUnderCursor);
      }
      const int startPos = cursor.selectionStart();
      const int endPos = cursor.selectionEnd();
      QTextDocument* doc = ed->document();
      for (int i = startPos; i <= endPos; ) {
        QTextBlock block = doc->findBlock(i);
        if (!block.isValid()) break;
        QString text = block.text();
        if (text.startsWith("\t")) {
          QTextCursor removeCursor(doc);
          removeCursor.setPosition(block.position());
          removeCursor.setPosition(block.position() + 1, QTextCursor::KeepAnchor);
          removeCursor.removeSelectedText();
        } else if (text.startsWith("  ")) {
          QTextCursor removeCursor(doc);
          removeCursor.setPosition(block.position());
          removeCursor.setPosition(block.position() + 2, QTextCursor::KeepAnchor);
          removeCursor.removeSelectedText();
        }
        i = block.position() + block.length();
      }
    }
  });

  connect(actionFind_, &QAction::triggered, this, [this] {
    showFindReplaceDialog();
  });

  connect(actionFindNext_, &QAction::triggered, this, [this] {
    if (findReplaceDialog_ && findReplaceDialog_->isVisible() && !findReplaceDialog_->findText().isEmpty()) {
      if (editor_) {
        editor_->findNext(findReplaceDialog_->findText());
      }
    }
  });

  connect(actionFindPrevious_, &QAction::triggered, this, [this] {
    if (findReplaceDialog_ && findReplaceDialog_->isVisible() && !findReplaceDialog_->findText().isEmpty()) {
      if (editor_) {
        editor_->find(findReplaceDialog_->findText(), QTextDocument::FindBackward);
      }
    }
  });

  connect(actionReplace_, &QAction::triggered, this, [this] {
    showFindReplaceDialog();
  });

  connect(actionFindInFiles_, &QAction::triggered, this, [this] {
    showFindInFilesDialog();
  });

  connect(actionReplaceInFiles_, &QAction::triggered, this, [this] {
    showReplaceInFilesDialog();
  });

  // === View Menu Actions ===
  connect(actionGoToLine_, &QAction::triggered, this, [this] {
    goToLine();
  });

  connect(actionGoToSymbol_, &QAction::triggered, this, [this] {
    showGoToSymbol();
  });

  connect(actionCompletion_, &QAction::triggered, this,
          [this] { requestCompletion(); });
  connect(actionShowHover_, &QAction::triggered, this,
          [this] { showHover(); });
  connect(actionGoToDefinition_, &QAction::triggered, this,
          [this] { goToDefinition(); });
  connect(actionFindReferences_, &QAction::triggered, this,
          [this] { findReferences(); });
  connect(actionRenameSymbol_, &QAction::triggered, this,
          [this] { renameSymbol(); });
  connect(actionCodeActions_, &QAction::triggered, this,
          [this] { showCodeActions(); });
  connect(actionOrganizeImports_, &QAction::triggered, this, [this] {
    showCodeActions({QStringLiteral("source.organizeImports")});
  });
  connect(actionFormatDocument_, &QAction::triggered, this,
          [this] { formatDocument(); });

  connect(actionCommandPalette_, &QAction::triggered, this, [this] {
    showCommandPalette();
  });

  connect(actionSidebarSearch_, &QAction::triggered, this, [this] {
    if (searchDock_) {
      searchDock_->show();
      searchDock_->raise();
    }
  });

  // === Sketch Menu Actions ===
  connect(actionVerify_, &QAction::triggered, this, [this] {
    verifySketch();
  });

  connect(actionUpload_, &QAction::triggered, this, [this] {
    uploadSketch();
  });

  connect(actionJustUpload_, &QAction::triggered, this, [this] {
    fastUploadSketch();
  });

  connect(actionStop_, &QAction::triggered, this, [this] {
    stopOperation();
  });

  connect(actionUploadUsingProgrammer_, &QAction::triggered, this, [this] {
    uploadUsingProgrammer();
  });

  connect(actionExportCompiledBinary_, &QAction::triggered, this, [this] {
    exportCompiledBinary();
  });

  connect(actionOptimizeForDebug_, &QAction::toggled, this, [this](bool enabled) {
    QSettings settings;
    settings.beginGroup(kSettingsGroup);
    settings.setValue(kOptimizeForDebugKey, enabled);
    settings.endGroup();
    showToast(enabled ? tr("Optimize for Debugging enabled")
                      : tr("Optimize for Debugging disabled"));
  });

  connect(actionShowSketchFolder_, &QAction::triggered, this, [this] {
    showSketchFolder();
  });

  connect(actionRenameSketch_, &QAction::triggered, this, [this] {
    renameSketch();
  });

  connect(actionAddFileToSketch_, &QAction::triggered, this, [this] {
    addFileToSketch();
  });

  connect(actionAddZipLibrary_, &QAction::triggered, this, [this] {
    addZipLibrary();
  });

  connect(actionBoardsManager_, &QAction::triggered, this, [this] {
    if (boardsManagerDock_) {
      boardsManagerDock_->show();
      boardsManagerDock_->raise();
    }
  });

  connect(actionLibraryManager_, &QAction::triggered, this, [this] {
    if (libraryManagerDock_) {
      libraryManagerDock_->show();
      libraryManagerDock_->raise();
    }
  });

  connect(actionManageLibraries_, &QAction::triggered, this, [this] {
    if (libraryManagerDock_) {
      libraryManagerDock_->show();
      libraryManagerDock_->raise();
    }
  });

  // === Board Selection Actions ===
  connect(actionSelectBoard_, &QAction::triggered, this, [this] {
    showSelectBoardDialog();
  });

  connect(actionBoardSetupWizard_, &QAction::triggered, this, [this] {
    runBoardSetupWizard();
  });

  connect(actionRefreshBoards_, &QAction::triggered, this, [this] {
    scheduleBoardListRefresh();
    showToast(tr("Boards list refreshed"));
  });

  connect(actionRefreshPorts_, &QAction::triggered, this, [this] {
    refreshConnectedPorts();
    showToast(tr("Ports list refreshed"));
  });

  // === Tools Menu Actions ===
  connect(actionSerialMonitor_, &QAction::triggered, this, [this] {
    toggleSerialMonitor();
  });

  connect(actionSerialPlotter_, &QAction::triggered, this, [this] {
    toggleSerialPlotter();
  });

  connect(actionGetBoardInfo_, &QAction::triggered, this, [this] {
    getBoardInfo();
  });

  connect(actionBurnBootloader_, &QAction::triggered, this, [this] {
    burnBootloader();
  });

  // === Additional Tools Menu Actions ===
  connect(actionAutoFormat_, &QAction::triggered, this, [this] {
    autoFormatSketch();
  });

  connect(actionArchiveSketch_, &QAction::triggered, this, [this] {
    archiveSketch();
  });

  connect(actionWiFiFirmwareUpdater_, &QAction::triggered, this, [this] {
    showWiFiFirmwareUpdater();
  });

  connect(actionUploadSSL_, &QAction::triggered, this, [this] {
    uploadSslRootCertificates();
  });

  connect(actionExportSetupProfile_, &QAction::triggered, this, [this] {
    exportSetupProfile();
  });
  connect(actionImportSetupProfile_, &QAction::triggered, this, [this] {
    importSetupProfile();
  });
  connect(actionGenerateProjectLockfile_, &QAction::triggered, this, [this] {
    generateProjectLockfile();
  });
  connect(actionBootstrapProjectLockfile_, &QAction::triggered, this, [this] {
    bootstrapProjectLockfile();
  });
  connect(actionEnvironmentDoctor_, &QAction::triggered, this, [this] {
    runEnvironmentDoctor();
  });

  // Programmer selection
  connect(actionProgrammerAVRISP_, &QAction::triggered, this, [this] {
    if (actionProgrammerAVRISP_) {
      setProgrammer(actionProgrammerAVRISP_->data().toString());
    }
  });
  connect(actionProgrammerUSBasp_, &QAction::triggered, this, [this] {
    if (actionProgrammerUSBasp_) {
      setProgrammer(actionProgrammerUSBasp_->data().toString());
    }
  });
  connect(actionProgrammerArduinoISP_, &QAction::triggered, this, [this] {
    if (actionProgrammerArduinoISP_) {
      setProgrammer(actionProgrammerArduinoISP_->data().toString());
    }
  });
  connect(actionProgrammerUSBTinyISP_, &QAction::triggered, this, [this] {
    if (actionProgrammerUSBTinyISP_) {
      setProgrammer(actionProgrammerUSBTinyISP_->data().toString());
    }
  });

  // Debug actions
  connect(actionStartDebugging_, &QAction::triggered, this, [this] {
    startDebugging();
  });
  connect(actionStepOver_, &QAction::triggered, this, [this] {
    debugStepOver();
  });
  connect(actionStepInto_, &QAction::triggered, this, [this] {
    debugStepInto();
  });
  connect(actionStepOut_, &QAction::triggered, this, [this] {
    debugStepOut();
  });
  connect(actionContinue_, &QAction::triggered, this, [this] {
    debugContinue();
  });
  connect(actionStopDebugging_, &QAction::triggered, this, [this] {
    stopDebugging();
  });

  // === Help Menu Actions ===
  connect(actionAbout_, &QAction::triggered, this, [this] {
    showAbout();
  });

  connect(actionGithubPage_, &QAction::triggered, this, [this] {
    QDesktopServices::openUrl(QUrl("https://github.com/lolren/rewritto-ide"));
  });

  // Moved to wireSignals()
}

void MainWindow::createLayout() {
  centralStack_ = new QStackedWidget(this);
  centralStack_->setObjectName("CentralStack");

  welcome_ = new WelcomeWidget(centralStack_);
  welcome_->setObjectName("WelcomeWidget");
  editor_ = new EditorWidget(centralStack_);
  editor_->setObjectName("EditorWidget");

  centralStack_->addWidget(welcome_);
  centralStack_->addWidget(editor_);
  setCentralWidget(centralStack_);

  toast_ = new ToastWidget(centralStack_);

  completionModel_ = new QStandardItemModel(this);
  completer_ = new QCompleter(completionModel_, this);
  completer_->setCompletionMode(QCompleter::PopupCompletion);
  completer_->setCaseSensitivity(Qt::CaseInsensitive);
  completer_->setCompletionRole(kCompletionRoleLabel);

  // Remove default margins and spacing for a more compact, modern look
  setContentsMargins(0, 0, 0, 0);

  // --- Header ToolBar (Rewritto-ide style) ---
  buildToolBar_ = new QToolBar(tr("Build"), this);
  buildToolBar_->setObjectName("HeaderToolBar");
  buildToolBar_->setMovable(false);
  buildToolBar_->setFloatable(false);
  buildToolBar_->setToolButtonStyle(Qt::ToolButtonIconOnly);
  buildToolBar_->setIconSize(QSize(24, 24));

  // Main action buttons
  buildToolBar_->addAction(actionVerify_);
  buildToolBar_->addAction(actionUpload_);
  buildToolBar_->addAction(actionJustUpload_);
  buildToolBar_->addAction(actionStop_);
  buildToolBar_->addSeparator();

  // Custom Board/Port selector styling - more compact
  QWidget* boardSelectorContainer = new QWidget(buildToolBar_);
  QHBoxLayout* boardLayout = new QHBoxLayout(boardSelectorContainer);
  boardLayout->setContentsMargins(4, 0, 4, 0);
  boardLayout->setSpacing(4);

  boardCombo_ = new QComboBox(boardSelectorContainer);
  boardCombo_->setEditable(false);
  boardCombo_->setMinimumWidth(200);
  boardCombo_->setMaximumWidth(280);
  boardCombo_->setInsertPolicy(QComboBox::NoInsert);
  boardCombo_->setPlaceholderText(tr("Select Board..."));
  boardCombo_->setToolTip(tr("Click to open searchable board selector"));
  
  auto* boardModel = new QStandardItemModel(boardCombo_);
  auto* boardProxy = new BoardFilterProxyModel(boardCombo_); 
  boardProxy->setSourceModel(boardModel);
  boardProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
  
  boardCombo_->setModel(boardProxy);
  boardCombo_->setItemDelegate(new BoardItemDelegate(this));

  // Handle clicking the star in the dropdown
  boardCombo_->installEventFilter(this);
  if (auto* view = boardCombo_->view()) {
      view->viewport()->installEventFilter(this);
  }
  boardLayout->addWidget(boardCombo_);

  portCombo_ = new QComboBox(boardSelectorContainer);
  portCombo_->setEditable(false);
  portCombo_->setMinimumWidth(120);
  portCombo_->setMaximumWidth(180);
  portCombo_->setInsertPolicy(QComboBox::NoInsert);
  portCombo_->setPlaceholderText(tr("Select Port..."));
  boardLayout->addWidget(portCombo_);

  // Connect board combo selection changes
  connect(boardCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int index) {
            const QString fqbn = boardCombo_->itemData(index).toString().trimmed();
            if (auto* proxy = static_cast<BoardFilterProxyModel*>(boardCombo_->model())) {
              proxy->setPinnedFqbn(fqbn);
            }

            QSettings settings;
            settings.beginGroup(kSettingsGroup);
            if (fqbn.isEmpty()) {
              settings.remove(kFqbnKey);
            } else {
              settings.setValue(kFqbnKey, fqbn);
            }
            settings.endGroup();
            storeFqbnForCurrentSketch(fqbn);

            updateBoardPortIndicator();
            scheduleRefreshBoardOptions();
            updateUploadActionStates();
            scheduleRestartLanguageServer();
          });

  // Connect port combo selection changes
  connect(portCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int index) {
            const QString port = portCombo_->itemData(index).toString().trimmed();

            QSettings settings;
            settings.beginGroup(kSettingsGroup);
            if (port.isEmpty()) {
              settings.remove(kPortKey);
            } else {
              settings.setValue(kPortKey, port);
            }
	    settings.endGroup();

	    maybeAutoSelectBoardForCurrentPort();
	    updateBoardPortIndicator();
	  });

  buildToolBar_->addWidget(boardSelectorContainer);

  // Spacer to push Serial tools to the right edge
  QWidget* spacer = new QWidget(buildToolBar_);
  spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  buildToolBar_->addWidget(spacer);

  actionSerialMonitor_->setText(tr("Serial Monitor"));
  actionSerialPlotter_->setText(tr("Serial Plotter"));
  buildToolBar_->addAction(actionSerialMonitor_);
  buildToolBar_->addAction(actionSerialPlotter_);
  buildToolBar_->addSeparator();

  // --- Context Toolbar (Top, under build actions) ---
  fontToolBar_ = new QToolBar(tr("Context"), this);
  fontToolBar_->setObjectName("ContextToolBar");
  fontToolBar_->setMovable(false);
  fontToolBar_->setFloatable(false);
  fontToolBar_->setIconSize(QSize(18, 18));
  fontToolBar_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  fontToolBar_->setAllowedAreas(Qt::TopToolBarArea);

  // --- Context Mode Bar (Right slim toolbar) ---
  contextModeToolBar_ = new QToolBar(tr("Context Modes"), this);
  contextModeToolBar_->setObjectName("ContextModeBar");
  contextModeToolBar_->setMovable(false);
  contextModeToolBar_->setFloatable(false);
  contextModeToolBar_->setOrientation(Qt::Vertical);
  contextModeToolBar_->setToolButtonStyle(Qt::ToolButtonIconOnly);
  contextModeToolBar_->setIconSize(QSize(18, 18));

  auto themedModeIcon = [this](const QString& iconName, QStyle::StandardPixmap fallback) {
    QIcon icon = QIcon::fromTheme(iconName);
    if (icon.isNull()) {
      icon = style()->standardIcon(fallback);
    }
    return icon;
  };

  actionContextFontsMode_->setIcon(themedModeIcon("preferences-desktop-font", QStyle::SP_FileDialogDetailedView));
  actionContextFontsMode_->setToolTip(tr("Font Controls"));
  actionContextSnapshotsMode_->setIcon(themedModeIcon("camera-photo", QStyle::SP_DialogSaveButton));
  actionContextSnapshotsMode_->setToolTip(tr("Snapshots"));
  actionContextGithubMode_->setIcon(themedModeIcon("github", QStyle::SP_DriveNetIcon));
  actionContextGithubMode_->setToolTip(tr("GitHub"));
  actionContextMcpMode_->setIcon(themedModeIcon("applications-system", QStyle::SP_ComputerIcon));
  actionContextMcpMode_->setToolTip(tr("MCP"));

  actionSnapshotCapture_->setIcon(themedModeIcon("camera-photo", QStyle::SP_DialogSaveButton));
  actionSnapshotCapture_->setToolTip(tr("Capture code snapshot"));
  actionSnapshotCompare_->setIcon(themedModeIcon("view-sort-descending", QStyle::SP_FileDialogListView));
  actionSnapshotCompare_->setToolTip(tr("Compare snapshots"));
  actionSnapshotGallery_->setIcon(themedModeIcon("folder-pictures", QStyle::SP_DirIcon));
  actionSnapshotGallery_->setToolTip(tr("Open code snapshots"));
  actionGithubLogin_->setIcon(themedModeIcon("dialog-password", QStyle::SP_DialogYesButton));
  actionGithubLogin_->setToolTip(tr("Authenticate with GitHub"));
  actionGitInitRepo_->setIcon(themedModeIcon("document-new", QStyle::SP_FileDialogNewFolder));
  actionGitInitRepo_->setToolTip(tr("Initialize a local Git repository"));
  actionGitCommit_->setIcon(themedModeIcon("document-save", QStyle::SP_DialogSaveButton));
  actionGitCommit_->setToolTip(tr("Create a commit for current sketch changes"));
  actionGitPush_->setIcon(themedModeIcon("go-up", QStyle::SP_ArrowUp));
  actionGitPush_->setToolTip(tr("Push current sketch repository to GitHub"));
  actionMcpConfigure_->setIcon(themedModeIcon("preferences-system", QStyle::SP_FileDialogDetailedView));
  actionMcpConfigure_->setToolTip(tr("Configure MCP server command"));
  actionMcpStart_->setIcon(themedModeIcon("media-playback-start", QStyle::SP_MediaPlay));
  actionMcpStart_->setToolTip(tr("Start MCP server"));
  actionMcpStop_->setIcon(themedModeIcon("media-playback-stop", QStyle::SP_MediaStop));
  actionMcpStop_->setToolTip(tr("Stop MCP server"));
  actionMcpRestart_->setIcon(themedModeIcon("view-refresh", QStyle::SP_BrowserReload));
  actionMcpRestart_->setToolTip(tr("Restart MCP server"));
  actionMcpAutostart_->setIcon(themedModeIcon("system-run", QStyle::SP_BrowserReload));
  actionMcpAutostart_->setToolTip(tr("Start MCP server on launch"));
  connect(actionSnapshotCapture_, &QAction::triggered, this,
          [this] { captureCodeSnapshot(false); });
  connect(actionSnapshotCompare_, &QAction::triggered, this,
          [this] { showCodeSnapshotCompare(); });
  connect(actionSnapshotGallery_, &QAction::triggered, this,
          [this] { showCodeSnapshotsGallery(); });
  connect(actionGithubLogin_, &QAction::triggered, this,
          &MainWindow::loginToGithub);
  connect(actionGitInitRepo_, &QAction::triggered, this,
          &MainWindow::initGitRepositoryForCurrentSketch);
  connect(actionGitCommit_, &QAction::triggered, this,
          &MainWindow::commitCurrentSketchToGit);
  connect(actionGitPush_, &QAction::triggered, this,
          &MainWindow::pushCurrentSketchToRemote);
  connect(actionMcpConfigure_, &QAction::triggered, this,
          &MainWindow::configureMcpServer);
  connect(actionMcpStart_, &QAction::triggered, this,
          &MainWindow::startMcpServer);
  connect(actionMcpStop_, &QAction::triggered, this,
          &MainWindow::stopMcpServer);
  connect(actionMcpRestart_, &QAction::triggered, this,
          &MainWindow::restartMcpServer);
  connect(actionMcpAutostart_, &QAction::toggled, this, [this](bool checked) {
    QSettings settings;
    settings.beginGroup(kSettingsGroup);
    settings.setValue(kMcpAutoStartKey, checked);
    settings.endGroup();
    updateMcpUiState();
  });

  contextModeToolBar_->addAction(actionContextFontsMode_);
  contextModeToolBar_->addAction(actionContextSnapshotsMode_);
  contextModeToolBar_->addAction(actionContextGithubMode_);
  contextModeToolBar_->addAction(actionContextMcpMode_);

  if (QWidget* widget = contextModeToolBar_->widgetForAction(actionContextFontsMode_)) {
    widget->setObjectName("ContextModeFontsButton");
  }
  if (QWidget* widget = contextModeToolBar_->widgetForAction(actionContextSnapshotsMode_)) {
    widget->setObjectName("ContextModeSnapshotsButton");
  }
  if (QWidget* widget = contextModeToolBar_->widgetForAction(actionContextGithubMode_)) {
    widget->setObjectName("ContextModeGithubButton");
  }
  if (QWidget* widget = contextModeToolBar_->widgetForAction(actionContextMcpMode_)) {
    widget->setObjectName("ContextModeMcpButton");
  }

  restyleContextModeToolBar();

  contextModeGroup_ = new QActionGroup(this);
  contextModeGroup_->setExclusive(true);
  contextModeGroup_->addAction(actionContextFontsMode_);
  contextModeGroup_->addAction(actionContextSnapshotsMode_);
  contextModeGroup_->addAction(actionContextGithubMode_);
  contextModeGroup_->addAction(actionContextMcpMode_);
  connect(contextModeGroup_, &QActionGroup::triggered, this,
          [this](QAction* action) {
            if (action == actionContextFontsMode_) {
              setContextToolbarMode(ContextToolbarMode::Fonts);
            } else if (action == actionContextSnapshotsMode_) {
              setContextToolbarMode(ContextToolbarMode::Snapshots);
            } else if (action == actionContextGithubMode_) {
              setContextToolbarMode(ContextToolbarMode::Github);
            } else if (action == actionContextMcpMode_) {
              setContextToolbarMode(ContextToolbarMode::Mcp);
            }
            if (actionToggleFontToolBar_ && !actionToggleFontToolBar_->isChecked()) {
              actionToggleFontToolBar_->setChecked(true);
            } else if (fontToolBar_) {
              fontToolBar_->show();
            }
          });

  actionContextFontsMode_->setChecked(true);
  setContextToolbarMode(ContextToolbarMode::Fonts);

  // Add the toolbars to the window in strict order
  addToolBar(Qt::TopToolBarArea, buildToolBar_);
  addToolBar(Qt::TopToolBarArea, fontToolBar_);
  addToolBar(Qt::RightToolBarArea, contextModeToolBar_);
  enforceToolbarLayout();
  if (actionToggleFontToolBar_) {
    connect(actionToggleFontToolBar_, &QAction::toggled, this,
            [this](bool visible) {
              if (fontToolBar_) {
                fontToolBar_->setVisible(visible);
              }
              syncContextModeSelection(visible);
            });
    const bool contextVisible = actionToggleFontToolBar_->isChecked();
    fontToolBar_->setVisible(contextVisible);
    syncContextModeSelection(contextVisible);
  } else if (fontToolBar_) {
    fontToolBar_->hide();
    syncContextModeSelection(false);
  }

  // --- Activity Bar (Left Vertical Bar) ---
  sideBarToolBar_ = new QToolBar(tr("Activity Bar"), this);
  sideBarToolBar_->setObjectName("ActivityBar");
  sideBarToolBar_->setMovable(false);
  sideBarToolBar_->setFloatable(false);
  sideBarToolBar_->setOrientation(Qt::Vertical);
  sideBarToolBar_->setToolButtonStyle(Qt::ToolButtonIconOnly);
  sideBarToolBar_->setIconSize(QSize(24, 24));
  addToolBar(Qt::LeftToolBarArea, sideBarToolBar_);

  // ... (rest of sidebar icon logic stays the same) ...
  auto createMonoIcon = [this](QStyle::StandardPixmap stdPixmap, const QString&) {
    return style()->standardIcon(stdPixmap);
  };

  QAction* sketchbookToggle = sideBarToolBar_->addAction(createMonoIcon(QStyle::SP_DialogOpenButton, "folder"), "");
  sketchbookToggle->setCheckable(true);
  sketchbookToggle->setChecked(false);
  sketchbookToggle->setToolTip(tr("Sketchbook"));

  QAction* boardsToggle = sideBarToolBar_->addAction(createMonoIcon(QStyle::SP_ComputerIcon, "applications-engineering"), "");
  boardsToggle->setCheckable(true);
  boardsToggle->setToolTip(tr("Boards Manager"));

  QAction* libsToggle = sideBarToolBar_->addAction(createMonoIcon(QStyle::SP_FileIcon, "book"), "");
  libsToggle->setCheckable(true);
  libsToggle->setToolTip(tr("Library Manager"));

  QAction* searchToggle = sideBarToolBar_->addAction(createMonoIcon(QStyle::SP_FileDialogDetailedView, "edit-find"), "");
  searchToggle->setCheckable(true);
  searchToggle->setToolTip(tr("Search"));

  QActionGroup* activityGroup = new QActionGroup(this);
  activityGroup->addAction(sketchbookToggle);
  activityGroup->addAction(boardsToggle);
  activityGroup->addAction(libsToggle);
  activityGroup->addAction(searchToggle);
  activityGroup->setExclusive(true);

  output_ = new OutputWidget(this);
  output_->setObjectName("OutputPanel");

  // --- Dockable Panels ---
  fileModel_ = new QFileSystemModel(this);
  fileModel_->setReadOnly(false);
  fileModel_->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
  fileTree_ = new QTreeView(this);
  fileTree_->setModel(fileModel_);
  fileTree_->setHeaderHidden(true);
  connect(fileTree_, &QTreeView::doubleClicked, this, [this](const QModelIndex& index) {
    if (!fileModel_ || !fileTree_) {
      return;
    }
    const QFileInfo fi = fileModel_->fileInfo(index);
    if (!fi.exists()) {
      return;
    }
    if (fi.isDir()) {
      fileTree_->setExpanded(index, !fileTree_->isExpanded(index));
      return;
    }
    if (editor_) {
      editor_->openFile(fi.absoluteFilePath());
      updateWelcomeVisibility();
    }
  });

  sketchbookModel_ = new QFileSystemModel(this);
  sketchbookModel_->setFilter(QDir::AllDirs | QDir::NoDotAndDotDot);
  sketchbookTree_ = new QTreeView(this);
  sketchbookTree_->setModel(sketchbookModel_);
  sketchbookTree_->setHeaderHidden(true);
  connect(sketchbookTree_, &QTreeView::doubleClicked, this, [this](const QModelIndex& index) {
    if (!sketchbookModel_ || !sketchbookTree_) {
      return;
    }
    const QFileInfo fi = sketchbookModel_->fileInfo(index);
    if (!fi.exists() || !fi.isDir()) {
      return;
    }
    const QString folder = fi.absoluteFilePath();
    if (SketchManager::isSketchFolder(folder)) {
      (void)openSketchFolderInUi(folder);
      return;
    }
    sketchbookTree_->setExpanded(index, !sketchbookTree_->isExpanded(index));
  });

  sketchbookTabs_ = new QTabWidget(this);
  sketchbookTabs_->addTab(fileTree_, tr("Sketch"));
  sketchbookTabs_->addTab(sketchbookTree_, tr("Sketchbook"));

  fileDock_ = new QDockWidget(tr("Sketchbook"), this);
  fileDock_->setObjectName("SketchbookDock");
  fileDock_->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
  fileDock_->setWidget(sketchbookTabs_);
  addDockWidget(Qt::LeftDockWidgetArea, fileDock_);
  resizeDocks({fileDock_}, {250}, Qt::Horizontal);

  boardsManagerDock_ = new QDockWidget(tr("Boards Manager"), this);
  boardsManagerDock_->setObjectName("BoardsManagerDock");
  boardsManagerDock_->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
  boardsManager_ = new BoardsManagerDialog(arduinoCli_, output_, boardsManagerDock_);
  boardsManagerDock_->setWidget(boardsManager_);
  addDockWidget(Qt::LeftDockWidgetArea, boardsManagerDock_);
  tabifyDockWidget(fileDock_, boardsManagerDock_);
  boardsManagerDock_->hide();

  libraryManagerDock_ = new QDockWidget(tr("Library Manager"), this);
  libraryManagerDock_->setObjectName("LibraryManagerDock");
  libraryManagerDock_->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
  libraryManager_ = new LibraryManagerDialog(arduinoCli_, output_, libraryManagerDock_);
  libraryManagerDock_->setWidget(libraryManager_);
  addDockWidget(Qt::LeftDockWidgetArea, libraryManagerDock_);
  tabifyDockWidget(fileDock_, libraryManagerDock_);
  libraryManagerDock_->hide();

  connect(boardsManagerDock_, &QDockWidget::visibilityChanged, this,
          [this](bool visible) {
            if (visible && boardsManager_) {
              boardsManager_->refresh();
            }
          });
  connect(libraryManagerDock_, &QDockWidget::visibilityChanged, this,
          [this](bool visible) {
            if (visible && libraryManager_) {
              libraryManager_->refresh();
            }
          });

  searchDock_ = new QDockWidget(tr("Search"), this);
  searchDock_->setObjectName("SearchDock");
  searchDock_->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
  searchTabs_ = new QTabWidget(searchDock_);
  findInFiles_ = new FindInFilesDialog("", searchDock_);
  replaceInFiles_ = new ReplaceInFilesDialog("", searchDock_);
  searchTabs_->addTab(findInFiles_, tr("Find"));
  searchTabs_->addTab(replaceInFiles_, tr("Replace"));
  searchDock_->setWidget(searchTabs_);
  addDockWidget(Qt::LeftDockWidgetArea, searchDock_);
  tabifyDockWidget(fileDock_, searchDock_);
  searchDock_->hide();
  fileDock_->hide();

  // Activity Bar Logic
  auto hideLeftPanel = [this] {
    if (fileDock_) fileDock_->hide();
    if (boardsManagerDock_) boardsManagerDock_->hide();
    if (libraryManagerDock_) libraryManagerDock_->hide();
    if (searchDock_) searchDock_->hide();
  };

  auto uncheckAllActivities = [activityGroup] {
    if (!activityGroup) return;
    for (QAction* a : activityGroup->actions()) {
      if (a) a->setChecked(false);
    }
  };

  auto toggleDockFromActivity = [this, hideLeftPanel, uncheckAllActivities](QDockWidget* dock,
                                                                           QAction* action) {
    if (!dock || !action) return;

    // Clicking the active icon collapses the entire left panel (IDE 2.x behavior).
    if (action->isChecked() && dock->isVisible()) {
      hideLeftPanel();
      uncheckAllActivities();
      return;
    }

    hideLeftPanel();
    dock->show();
    dock->raise();
    action->setChecked(true);
  };

  connect(sketchbookToggle, &QAction::triggered, this, [this, toggleDockFromActivity, sketchbookToggle] {
    toggleDockFromActivity(fileDock_, sketchbookToggle);
  });
  connect(boardsToggle, &QAction::triggered, this, [this, toggleDockFromActivity, boardsToggle] {
    toggleDockFromActivity(boardsManagerDock_, boardsToggle);
  });
  connect(libsToggle, &QAction::triggered, this, [this, toggleDockFromActivity, libsToggle] {
    toggleDockFromActivity(libraryManagerDock_, libsToggle);
  });
  connect(searchToggle, &QAction::triggered, this, [this, toggleDockFromActivity, searchToggle] {
    toggleDockFromActivity(searchDock_, searchToggle);
  });

  auto syncActivityCheck = [uncheckAllActivities](QDockWidget* dock, QAction* action) {
    if (!dock || !action) return;
    QObject::connect(dock, &QDockWidget::visibilityChanged, dock,
                     [action, uncheckAllActivities](bool visible) {
                       if (!visible) {
                         if (action->isChecked()) action->setChecked(false);
                         return;
                       }
                       uncheckAllActivities();
                       action->setChecked(true);
                     });
  };
  syncActivityCheck(fileDock_, sketchbookToggle);
  syncActivityCheck(boardsManagerDock_, boardsToggle);
  syncActivityCheck(libraryManagerDock_, libsToggle);
  syncActivityCheck(searchDock_, searchToggle);

  // --- Bottom Area ---
  outputDock_ = new QDockWidget(tr("Output"), this);
  outputDock_->setObjectName("OutputDock");
  outputDock_->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
  outputDock_->setWidget(output_);
  addDockWidget(Qt::BottomDockWidgetArea, outputDock_);
  resizeDocks({outputDock_}, {180}, Qt::Vertical);

  outlineRefreshTimer_ = new QTimer(this);
  outlineRefreshTimer_->setSingleShot(true);
  outlineRefreshTimer_->setInterval(400);
  connect(outlineRefreshTimer_, &QTimer::timeout, this, [this] { refreshOutline(); });

  boardPortLabel_ = new QLabel(this);
  boardPortLabel_->setObjectName("BoardPortLabel");
  boardPortLabel_->setText(tr("Board: (none) | Port: (none)"));
  statusBar()->addPermanentWidget(boardPortLabel_, 1);

  cursorPosLabel_ = new QLabel(this);
  cursorPosLabel_->setText(tr("Ln 1, Col 1"));
  statusBar()->addPermanentWidget(cursorPosLabel_);

  cliBusyLabel_ = new QLabel(this);
  cliBusyLabel_->setObjectName("CliBusyLabel");
  cliBusyLabel_->setStyleSheet(
      "QLabel#CliBusyLabel { font-weight: 600; letter-spacing: 0.2px; }");
  cliBusyLabel_->hide();
  statusBar()->addPermanentWidget(cliBusyLabel_);

  cliBusy_ = new QProgressBar(this);
  cliBusy_->setRange(0, 100);
  cliBusy_->setValue(0);
  cliBusy_->setTextVisible(false);
  cliBusy_->setFixedSize(180, 12);
  cliBusy_->setStyleSheet(
      "QProgressBar {"
      "  border: 1px solid rgba(127, 127, 127, 0.35);"
      "  border-radius: 6px;"
      "  background: rgba(127, 127, 127, 0.12);"
      "  padding: 1px;"
      "}"
      "QProgressBar::chunk {"
      "  border-radius: 5px;"
      "  background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
      "    stop:0 #00a7b5, stop:1 #2dd4bf);"
      "}");
  cliBusy_->hide();
  statusBar()->addPermanentWidget(cliBusy_);

  updateStopActionState();
  defaultDockState_ = saveState();

  updateWelcomePage();
  updateWelcomeVisibility();
}

void MainWindow::wireSignals() {
  if (boardsManager_) {
    connect(boardsManager_, &BoardsManagerDialog::platformsChanged, this,
            [this] { refreshInstalledBoards(); });
    connect(boardsManager_, &BoardsManagerDialog::busyChanged, this,
            [this](bool) { updateStopActionState(); });
  }
  if (libraryManager_) {
    connect(libraryManager_, &LibraryManagerDialog::librariesChanged, this,
            [this] { clearIncludeLibraryMenuActions(); });
    connect(libraryManager_, &LibraryManagerDialog::includeLibraryRequested, this,
            &MainWindow::insertLibraryIncludes);
    connect(libraryManager_, &LibraryManagerDialog::openLibraryExamplesRequested, this,
            [this](const QString& libraryName) { showExamplesDialog(libraryName); });
    connect(libraryManager_, &LibraryManagerDialog::busyChanged, this,
            [this](bool) { updateStopActionState(); });
  }

  problems_ = new ProblemsWidget(this);
  problemsDock_ = new QDockWidget(tr("Problems"), this);
  problemsDock_->setObjectName("ProblemsDock");
  problemsDock_->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
  problemsDock_->setWidget(problems_);
  addDockWidget(Qt::BottomDockWidgetArea, problemsDock_);
  tabifyDockWidget(outputDock_, problemsDock_);
  problemsDock_->raise();

  connect(problems_, &ProblemsWidget::openLocationRequested, this,
          [this](const QString& filePath, int line, int column) {
            if (!editor_ || filePath.trimmed().isEmpty() || line <= 0) {
              return;
            }
            if (!editor_->openLocation(filePath, line, column)) {
              showToast(tr("Could not open diagnostic location."));
            }
          });
  connect(problems_, &ProblemsWidget::searchBoardsRequested, this,
          [this](const QString& query) { focusBoardsManagerSearch(query); });
  connect(problems_, &ProblemsWidget::searchLibrariesRequested, this,
          [this](const QString& query) { focusLibraryManagerSearch(query); });
  connect(problems_, &ProblemsWidget::quickFixRequested, this,
          &MainWindow::handleQuickFix);
  connect(problems_, &ProblemsWidget::showDocsRequested, this,
          [this](const QString& message) {
            QString query = message.simplified();
            if (query.isEmpty()) {
              return;
            }
            if (query.size() > 220) {
              query = query.left(220);
            }
            const QString encoded = QString::fromLatin1(
                QUrl::toPercentEncoding(QStringLiteral("arduino %1").arg(query)));
            QDesktopServices::openUrl(
                QUrl(QStringLiteral("https://duckduckgo.com/?q=%1").arg(encoded)));
          });

  auto applyMergedEditorDiagnostics = [this](const QString& filePath) {
    if (!editor_) {
      return;
    }
    const QString normalized = QFileInfo(filePath).absoluteFilePath();
    QVector<CodeEditor::Diagnostic> merged =
        compilerDiagnostics_.value(normalized);
    const QJsonArray lspDiags = lspDiagnosticsByFilePath_.value(normalized);
    const QVector<CodeEditor::Diagnostic> lspEditorDiags =
        editorDiagnosticsFromLsp(lspDiags);
    for (const CodeEditor::Diagnostic& d : lspEditorDiags) {
      merged.push_back(d);
    }
    editor_->setDiagnostics(normalized, merged);
  };

  if (lsp_) {
    connect(lsp_, &LspClient::logMessage, this, [this](const QString& message) {
      if (!output_) {
        return;
      }
      const QString trimmed = message.trimmed();
      if (trimmed.isEmpty()) {
        return;
      }
      output_->appendLine(tr("[LSP] %1").arg(trimmed));
    });

    connect(lsp_, &LspClient::readyChanged, this, [this](bool ready) {
      if (!ready) {
        if (editor_) {
          editor_->clearAllDiagnostics();
          for (auto it = compilerDiagnostics_.cbegin();
               it != compilerDiagnostics_.cend(); ++it) {
            editor_->setDiagnostics(it.key(), it.value());
          }
        }
        lspDiagnosticsByFilePath_.clear();
        if (problems_) {
          problems_->clearSource(QStringLiteral("LSP"));
        }
        return;
      }

      if (!editor_) {
        return;
      }
      const QVector<QString> files = editor_->openedFiles();
      for (const QString& filePath : files) {
        if (filePath.trimmed().isEmpty()) {
          continue;
        }
        lsp_->didOpen(toFileUri(filePath),
                      languageIdForFilePath(filePath),
                      editor_->textForFile(filePath));
      }
      scheduleOutlineRefresh();
    });

    connect(lsp_, &LspClient::publishDiagnostics, this,
            [this, applyMergedEditorDiagnostics](const QString& uri,
                                                 const QJsonArray& diagnostics) {
              const QString filePath = pathFromUriOrPath(uri);
              if (filePath.trimmed().isEmpty()) {
                return;
              }
              const QString normalized = QFileInfo(filePath).absoluteFilePath();
              if (diagnostics.isEmpty()) {
                lspDiagnosticsByFilePath_.remove(normalized);
              } else {
                lspDiagnosticsByFilePath_[normalized] = diagnostics;
              }
              applyMergedEditorDiagnostics(normalized);

              if (problems_) {
                const QVector<ProblemsWidget::Diagnostic> lspProblems =
                    problemsDiagnosticsFromLsp(normalized, diagnostics);
                problems_->setDiagnostics(QStringLiteral("LSP"), normalized, lspProblems);
              }
            });
  }

  debugDock_ = new QDockWidget(tr("Debug"), this);
  debugDock_->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
  debugDock_->setObjectName("DebugDock");
  {
    auto* page = new QWidget(debugDock_);
    auto* v = new QVBoxLayout(page);
    v->setContentsMargins(6, 6, 6, 6);

    auto* topRow = new QHBoxLayout();
    topRow->addWidget(new QLabel(tr("Programmer:"), page));
    debugProgrammerEdit_ = new QLineEdit(page);
    debugProgrammerEdit_->setPlaceholderText(tr("e.g. atmel_ice"));
    topRow->addWidget(debugProgrammerEdit_, 1);
    debugCheckButton_ = new QPushButton(tr("Check Support"), page);
    topRow->addWidget(debugCheckButton_);
    v->addLayout(topRow);

    auto* actionsRow = new QHBoxLayout();
    debugStartButton_ = new QPushButton(tr("Start Debug"), page);
    debugStopButton_ = new QPushButton(tr("Stop"), page);
    debugStopButton_->setEnabled(false);
    debugClearButton_ = new QPushButton(tr("Clear"), page);
    debugSyncBreakpointsButton_ = new QPushButton(tr("Sync Breakpoints"), page);
    actionsRow->addWidget(debugStartButton_);
    actionsRow->addWidget(debugStopButton_);
    actionsRow->addWidget(debugClearButton_);
    actionsRow->addWidget(debugSyncBreakpointsButton_);
    actionsRow->addStretch(1);
    v->addLayout(actionsRow);

    auto* stepRow = new QHBoxLayout();
    debugContinueButton_ = new QPushButton(tr("Continue"), page);
    debugInterruptButton_ = new QPushButton(tr("Interrupt"), page);
    debugNextButton_ = new QPushButton(tr("Step Over"), page);
    debugStepButton_ = new QPushButton(tr("Step Into"), page);
    debugFinishButton_ = new QPushButton(tr("Step Out"), page);
    debugContinueButton_->setEnabled(false);
    debugInterruptButton_->setEnabled(false);
    debugNextButton_->setEnabled(false);
    debugStepButton_->setEnabled(false);
    debugFinishButton_->setEnabled(false);
    stepRow->addWidget(debugContinueButton_);
    stepRow->addWidget(debugInterruptButton_);
    stepRow->addWidget(debugNextButton_);
    stepRow->addWidget(debugStepButton_);
    stepRow->addWidget(debugFinishButton_);
    stepRow->addStretch(1);
    v->addLayout(stepRow);

    debugInfoTabs_ = new QTabWidget(page);
    debugInfoTabs_->setObjectName("DebugInfoTabs");
    debugInfoTabs_->setDocumentMode(true);
    debugInfoTabs_->setUsesScrollButtons(true);
    debugInfoTabs_->setMinimumHeight(160);

    debugBreakpointsTree_ = new QTreeWidget(debugInfoTabs_);
    debugBreakpointsTree_->setObjectName("DebugBreakpointsTree");
    debugBreakpointsTree_->setHeaderLabels(
        {tr("File"), tr("Line"), tr("Condition")});
    debugBreakpointsTree_->setRootIsDecorated(true);
    debugBreakpointsTree_->setAlternatingRowColors(true);
    debugBreakpointsTree_->setSelectionMode(QAbstractItemView::SingleSelection);
    debugBreakpointsTree_->setContextMenuPolicy(Qt::CustomContextMenu);
    debugInfoTabs_->addTab(debugBreakpointsTree_, tr("Breakpoints"));

    debugThreadsTree_ = new QTreeWidget(debugInfoTabs_);
    debugThreadsTree_->setObjectName("DebugThreadsTree");
    debugThreadsTree_->setHeaderLabels({tr("ID"), tr("Target ID"), tr("Details")});
    debugInfoTabs_->addTab(debugThreadsTree_, tr("Threads"));

    debugCallStackTree_ = new QTreeWidget(debugInfoTabs_);
    debugCallStackTree_->setObjectName("DebugCallStackTree");
    debugCallStackTree_->setHeaderLabels({tr("Level"), tr("Function"), tr("File"), tr("Line")});
    debugCallStackTree_->setRootIsDecorated(false);
    debugCallStackTree_->setAlternatingRowColors(true);
    debugCallStackTree_->setSelectionMode(QAbstractItemView::SingleSelection);
    debugInfoTabs_->addTab(debugCallStackTree_, tr("Call Stack"));

    debugLocalsTree_ = new QTreeWidget(debugInfoTabs_);
    debugLocalsTree_->setObjectName("DebugLocalsTree");
    debugLocalsTree_->setHeaderLabels({tr("Name"), tr("Value"), tr("Type")});
    debugLocalsTree_->setRootIsDecorated(false);
    debugLocalsTree_->setAlternatingRowColors(true);
    debugLocalsTree_->setSelectionMode(QAbstractItemView::SingleSelection);
    debugInfoTabs_->addTab(debugLocalsTree_, tr("Locals"));

    debugWatchesTree_ = new QTreeWidget(debugInfoTabs_);
    debugWatchesTree_->setObjectName("DebugWatchesTree");
    debugWatchesTree_->setHeaderLabels({tr("Expression"), tr("Value"), tr("Type")});
    debugWatchesTree_->setRootIsDecorated(false);
    debugWatchesTree_->setAlternatingRowColors(true);
    debugWatchesTree_->setSelectionMode(QAbstractItemView::SingleSelection);
    debugInfoTabs_->addTab(debugWatchesTree_, tr("Watches"));

    auto* watchControls = new QWidget(page);
    auto* watchLayout = new QHBoxLayout(watchControls);
    watchLayout->setContentsMargins(0, 0, 0, 0);
    debugWatchEdit_ = new QLineEdit(watchControls);
    debugWatchEdit_->setPlaceholderText(tr("Add watch expression"));
    debugWatchAddButton_ = new QPushButton(tr("Add"), watchControls);
    debugWatchRemoveButton_ = new QPushButton(tr("Remove"), watchControls);
    debugWatchClearButton_ = new QPushButton(tr("Clear All"), watchControls);
    watchLayout->addWidget(debugWatchEdit_, 1);
    watchLayout->addWidget(debugWatchAddButton_);
    watchLayout->addWidget(debugWatchRemoveButton_);
    watchLayout->addWidget(debugWatchClearButton_);
    v->addWidget(watchControls);

    v->addWidget(debugInfoTabs_, 1);

    debugConsole_ = new QPlainTextEdit(page);
    debugConsole_->setReadOnly(true);
    debugConsole_->setPlaceholderText(
        tr("Debug output (arduino-cli debug --interpreter mi2)\u2026"));
    debugConsole_->setMinimumHeight(160);
    v->addWidget(debugConsole_, 1);

    auto* cmdRow = new QHBoxLayout();
    debugCommandEdit_ = new QLineEdit(page);
    debugCommandEdit_->setPlaceholderText(tr("MI command (e.g. -exec-continue)"));
    debugSendButton_ = new QPushButton(tr("Send"), page);
    cmdRow->addWidget(debugCommandEdit_, 1);
    cmdRow->addWidget(debugSendButton_);
    v->addLayout(cmdRow);

    debugDock_->setWidget(page);
  }
  addDockWidget(Qt::BottomDockWidgetArea, debugDock_);
  tabifyDockWidget(outputDock_, debugDock_);
  debugDock_->hide();

  serialPort_ = new SerialPort(this);
  serialMonitor_ = new SerialMonitorWidget(this);
  serialDock_ = new QDockWidget(tr("Serial Monitor"), this);
  serialDock_->setObjectName("SerialMonitorDock");
  serialDock_->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
  serialDock_->setWidget(serialMonitor_);
  addDockWidget(Qt::BottomDockWidgetArea, serialDock_);
  tabifyDockWidget(outputDock_, serialDock_);
  serialDock_->hide();

  serialPlotter_ = new SerialPlotterWidget(this);
  serialPlotterDock_ = new QDockWidget(tr("Serial Plotter"), this);
  serialPlotterDock_->setObjectName("SerialPlotterDock");
  serialPlotterDock_->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
  serialPlotterDock_->setWidget(serialPlotter_);
  addDockWidget(Qt::BottomDockWidgetArea, serialPlotterDock_);
  tabifyDockWidget(outputDock_, serialPlotterDock_);
  serialPlotterDock_->hide();

  connect(arduinoCli_, &ArduinoCli::outputReceived, this,
          [this](const QString& text) {
            processOutputChunk(text);
            if (!capturingCliOutput_) {
              return;
            }
            cliOutputCapture_.append(text);
            constexpr int kMaxChars = 512 * 1024;
            if (cliOutputCapture_.size() > kMaxChars) {
              cliOutputCapture_.remove(0, cliOutputCapture_.size() - kMaxChars);
            }
          });
  connect(arduinoCli_, &ArduinoCli::started, this, [this] {
    cliCancelRequested_ = false;
    capturingCliOutput_ = true;
    cliOutputCapture_.clear();
    beginCliProgress(lastCliJobKind_);
    updateStopActionState();
    updateUploadActionStates();

    compilerDiagnostics_.clear();
    if (editor_) {
      editor_->clearAllDiagnostics();
      for (auto it = lspDiagnosticsByFilePath_.cbegin();
           it != lspDiagnosticsByFilePath_.cend(); ++it) {
        const QVector<CodeEditor::Diagnostic> lspOnly =
            editorDiagnosticsFromLsp(it.value());
        editor_->setDiagnostics(it.key(), lspOnly);
      }
    }
    if (problems_) {
      problems_->clearSource(QStringLiteral("Compiler"));
    }
  });
  connect(arduinoCli_, &ArduinoCli::diagnosticFound, this,
          [this, applyMergedEditorDiagnostics](const QString& filePath, int line, int column,
                                               const QString& severity, const QString& message) {
            const QString normalizedFilePath = normalizeDiagnosticPath(
                filePath, line, currentSketchFolderPath());
            CodeEditor::Diagnostic d;
            d.startLine = qMax(0, line - 1);
            d.startCharacter = qMax(0, column - 1);
            d.endLine = d.startLine;
            d.endCharacter = d.startCharacter + 1;

            if (severity == QStringLiteral("error")) {
              d.severity = 1;
            } else if (severity == QStringLiteral("warning")) {
              d.severity = 2;
            } else {
              d.severity = 3;
            }

            if (line > 0 && !normalizedFilePath.trimmed().isEmpty()) {
              compilerDiagnostics_[normalizedFilePath].push_back(d);
            }
            if (editor_ && line > 0 && !normalizedFilePath.trimmed().isEmpty()) {
              applyMergedEditorDiagnostics(normalizedFilePath);
            }

            if (problems_) {
              ProblemsWidget::Diagnostic pd;
              pd.filePath = normalizedFilePath;
              pd.line = qMax(0, line);
              pd.column = qMax(0, column);
              pd.severity = severity;
              pd.message = message;
              problems_->addDiagnostic(QStringLiteral("Compiler"), pd);
            }

            if (problemsDock_ &&
                !problemsDock_->isVisible() &&
                severity.trimmed().compare(QStringLiteral("error"),
                                           Qt::CaseInsensitive) == 0) {
              problemsDock_->show();
              problemsDock_->raise();
            }
          });
  connect(arduinoCli_, &ArduinoCli::finished, this,
          [this](int exitCode, QProcess::ExitStatus) {
            capturingCliOutput_ = false;
            const bool cancelled = cliCancelRequested_;
            cliCancelRequested_ = false;
            const bool uploadCancelled = pendingUploadCancelled_;
            pendingUploadCancelled_ = false;
            const QString output = cliOutputCapture_;
            cliOutputCapture_.clear();
            const CliJobKind job = lastCliJobKind_;
            lastCliJobKind_ = CliJobKind::None;

            auto maybeRefreshPorts = [this] {
              if (!portsRefreshQueued_) {
                return;
              }
              if (arduinoCli_ && arduinoCli_->isRunning()) {
                return;
              }
              portsRefreshQueued_ = false;
              refreshConnectedPorts();
            };

	            if (job == CliJobKind::UploadCompile) {
	              if (cancelled || uploadCancelled) {
	                output_->appendLine(tr("Upload cancelled."));
                  finishCliProgress(false, true);
	                clearPendingUploadFlow();
	                updateStopActionState();
                  updateUploadActionStates();
	                maybeRefreshPorts();
	                return;
	              }

	              if (exitCode == 0) {
                  rememberSuccessfulCompileArtifact(
                      pendingUploadFlow_.sketchFolder, pendingUploadFlow_.fqbn,
                      pendingUploadFlow_.buildPath);
                  finishCliProgress(true, false);
	                output_->appendHtml(QString("<span style=\"color:#388e3c;\"><b>%1</b></span>")
	                                        .arg(tr("Compile finished. Uploading\u2026")));
	                updateStopActionState();
                  updateUploadActionStates();
	                // Some arduino-cli builds report `finished` slightly before
	                // QProcess reaches NotRunning; retry briefly instead of failing.
	                auto retryStartUpload = std::make_shared<std::function<void(int)>>();
	                *retryStartUpload = [this, maybeRefreshPorts, retryStartUpload](int retriesLeft) {
	                  if (pendingUploadCancelled_ || cliCancelRequested_) {
	                    output_->appendLine(tr("Upload cancelled."));
                      finishCliProgress(false, true);
	                    clearPendingUploadFlow();
	                    updateStopActionState();
                      updateUploadActionStates();
	                    maybeRefreshPorts();
	                    return;
	                  }

	                  if (startUploadFromPendingFlow()) {
	                    return;
	                  }

	                  if (arduinoCli_ && arduinoCli_->isRunning() && retriesLeft > 0) {
	                    QTimer::singleShot(120, this, [retryStartUpload, retriesLeft] {
	                      (*retryStartUpload)(retriesLeft - 1);
	                    });
	                    return;
	                  }

	                  output_->appendHtml(QString("<span style=\"color:#d32f2f;\"><b>%1</b></span>")
	                                          .arg(tr("Upload failed: could not start upload step.")));
                    finishCliProgress(false, false);
	                  clearPendingUploadFlow();
	                  updateStopActionState();
                    updateUploadActionStates();
	                  maybeRefreshPorts();
	                };
	                QTimer::singleShot(0, this, [retryStartUpload] { (*retryStartUpload)(10); });
	                return;
	              }

	              // Compile failed: clear the pending flow so we don't keep stale state.
                finishCliProgress(false, false);
	              clearPendingUploadFlow();
	            }

	            if (cancelled) {
                finishCliProgress(false, true);
	              output_->appendLine(tr("Cancelled."));
	            } else {
		              if (exitCode == 0) {
			                if (job == CliJobKind::Compile) {
                      const QString buildPath =
                          QStandardPaths::writableLocation(
                              QStandardPaths::TempLocation) +
                          "/rewritto/build";
                      rememberSuccessfulCompileArtifact(
                          currentSketchFolderPath(), currentFqbn(), buildPath);
			                }
                    finishCliProgress(true, false);
			                output_->appendHtml(QString("<span style=\"color:#388e3c;\"><b>%1</b></span>")
			                                        .arg(tr("arduino-cli finished successfully.")));
		              } else {
	                if ((job == CliJobKind::Upload ||
	                     job == CliJobKind::UploadUsingProgrammer) &&
	                    pendingUploadFlow_.useInputDir &&
	                    !pendingUploadFlow_.buildPath.trimmed().isEmpty()) {
	                  pendingUploadFlow_.useInputDir = false;
                    setCliProgressValue(40, tr("Upload · retrying"));
	                  output_->appendHtml(
	                      QString("<span style=\"color:#fbc02d;\"><b>%1</b></span>")
	                          .arg(tr("Upload failed with prebuilt artifacts. Retrying without --input-dir…")));
	                  updateStopActionState();

	                  QTimer::singleShot(0, this, [this, maybeRefreshPorts] {
	                    if (pendingUploadCancelled_ || cliCancelRequested_) {
	                      output_->appendLine(tr("Upload cancelled."));
                        finishCliProgress(false, true);
	                      clearPendingUploadFlow();
	                      updateStopActionState();
                        updateUploadActionStates();
	                      maybeRefreshPorts();
	                      return;
	                    }

	                    if (!startUploadFromPendingFlow()) {
	                      output_->appendHtml(
	                          QString("<span style=\"color:#d32f2f;\"><b>%1</b></span>")
	                              .arg(tr("Upload failed: could not start fallback upload step.")));
                        finishCliProgress(false, false);
	                      clearPendingUploadFlow();
	                      updateStopActionState();
                        updateUploadActionStates();
	                      maybeRefreshPorts();
	                    }
	                  });
	                  return;
	                }

	                if (job == CliJobKind::Upload && tryUf2UploadFallback(output)) {
                  finishCliProgress(true, false);
	                  clearPendingUploadFlow();
	                  updateStopActionState();
                    updateUploadActionStates();
	                  maybeRefreshPorts();
	                  return;
	                }

                    finishCliProgress(false, false);
			                output_->appendHtml(QString("<span style=\"color:#d32f2f;\"><b>%1</b></span>")
			                                        .arg(QString("arduino-cli finished with exit code %1.").arg(exitCode)));
			              }
		            }
		            if (job == CliJobKind::Upload ||
		                job == CliJobKind::UploadUsingProgrammer) {
		              clearPendingUploadFlow();
		            }
		            updateStopActionState();
                updateUploadActionStates();
		            maybeRefreshPorts();
		          });

  // === Welcome Widget Connections ===
  connect(welcome_, &WelcomeWidget::newSketchRequested, this, [this] {
    newSketch();
  });

  connect(welcome_, &WelcomeWidget::openSketchRequested, this, [this] {
    openSketch();
  });

  connect(welcome_, &WelcomeWidget::openSketchFolderRequested, this, [this] {
    openSketchFolder();
  });

  connect(welcome_, &WelcomeWidget::openSketchSelected, this, [this](const QString& folder) {
    (void)openSketchFolderInUi(folder);
  });

  connect(welcome_, &WelcomeWidget::pinSketchRequested, this, [this](const QString& folder, bool pinned) {
    setSketchPinned(folder, pinned);
  });

  connect(welcome_, &WelcomeWidget::clearPinnedRequested, this, [this] {
    setPinnedSketches(QStringList());
  });

  connect(welcome_, &WelcomeWidget::clearRecentRequested, this, [this] {
    setRecentSketches(QStringList());
  });

  connect(welcome_, &WelcomeWidget::removeRecentRequested, this, [this](const QString& folder) {
    QStringList recent = recentSketches();
    recent.removeAll(folder);
    setRecentSketches(recent);
  });

  // === Editor Widget Connections ===
  connect(editor_, &EditorWidget::newTabRequested, this, [this] {
    newSketch();
  });

  connect(editor_, &EditorWidget::currentFileChanged, this, [this](const QString& path) {
    updateWindowTitleForFile(path);
    updateBoardPortIndicator();
    updateUploadActionStates();
    scheduleOutlineRefresh();
  });

  connect(editor_, &EditorWidget::documentOpened, this,
          [this](const QString& path, const QString& text) {
            const QString folder = normalizeSketchFolderPath(path);
            if (folder.isEmpty()) {
              return;
            }

            if (sketchManager_ && sketchManager_->lastSketchPath().trimmed().isEmpty()) {
              sketchManager_->openSketchFolder(folder);
            }

            if (fileModel_ && fileTree_ && fileModel_->rootPath().trimmed().isEmpty()) {
              fileModel_->setRootPath(folder);
              const QModelIndex root = fileModel_->index(folder);
              fileTree_->setRootIndex(root);
              fileTree_->expand(root);
            }
            updateUploadActionStates();
            scheduleOutlineRefresh();
            scheduleRestartLanguageServer();
            if (lsp_ && lsp_->isReady()) {
              lsp_->didOpen(toFileUri(path), languageIdForFilePath(path), text);
            }
          });

  connect(editor_, &EditorWidget::documentChanged, this,
          [this](const QString& path, const QString& text) {
            markSketchAsChanged(path);
            updateUploadActionStates();
            scheduleOutlineRefresh();
            if (lsp_ && lsp_->isReady() && !path.trimmed().isEmpty()) {
              lsp_->didChange(toFileUri(path), text);
            }
          });

  connect(editor_, &EditorWidget::documentClosed, this, [this](const QString& path) {
    updateUploadActionStates();
    scheduleOutlineRefresh();
    if (!path.trimmed().isEmpty()) {
      const QString normalized = QFileInfo(path).absoluteFilePath();
      lspDiagnosticsByFilePath_.remove(normalized);
      if (problems_) {
        problems_->setDiagnostics(QStringLiteral("LSP"), normalized, {});
      }
      if (editor_) {
        editor_->setDiagnostics(normalized, compilerDiagnostics_.value(normalized));
      }
    }
    if (lsp_ && lsp_->isReady() && !path.trimmed().isEmpty()) {
      lsp_->didClose(toFileUri(path));
    }
  });

  connect(editor_, &EditorWidget::breakpointsChanged, this,
          [this](const QString& path, const QVector<int>& lines) {
    Q_UNUSED(path);
    Q_UNUSED(lines);
    // Breakpoints changed - sync with debugger if needed
  });

  scheduleRestartLanguageServer();
  scheduleOutlineRefresh();
}

void MainWindow::processOutputChunk(const QString& chunk) {
    cliOutputBuffer_.append(chunk);
    
    QSettings settings;
    settings.beginGroup("Preferences");
    bool verbose = false;
    if (lastCliJobKind_ == CliJobKind::Upload || lastCliJobKind_ == CliJobKind::UploadUsingProgrammer || lastCliJobKind_ == CliJobKind::BurnBootloader) {
        verbose = settings.value("verboseUpload", false).toBool();
    } else {
        verbose = settings.value("verboseCompile", false).toBool();
    }
    settings.endGroup();

    while (true) {
        int idx = cliOutputBuffer_.indexOf('\n');
        if (idx < 0) break;
        
        QString line = cliOutputBuffer_.left(idx);
        cliOutputBuffer_.remove(0, idx + 1);
        
        if (line.endsWith('\r')) line.chop(1);
        
        bool print = verbose;
        QString color;
        bool bold = false;
        const QString trimmed = line.trimmed();
        const QString lower = line.toLower();

        updateCliProgressFromOutputLine(line);

        if (lower.contains("error:") || lower.contains("fatal error")) {
            color = "#d32f2f"; // Red
            print = true;
        } else if (lower.contains("warning:")) {
            color = "#fbc02d"; // Orange
            print = true;
        } else if (lower.startsWith("sketch uses") || lower.startsWith("global variables")) {
            color = "#388e3c"; // Green
            bold = true;
            print = true;
        } else if (line.contains("SUCCESS") || lower.startsWith("uploading...") || line.contains("Writing") || line.contains("Reading")) {
             print = true;
             if (line.contains("SUCCESS")) {
                 color = "#388e3c";
                 bold = true;
             }
        }

        if (!verbose) {
            if (!print) {
                const bool isUsefulStatus =
                    lower.startsWith("uploading") ||
                    lower.startsWith("writing at") ||
                    lower.startsWith("hash of data verified") ||
                    lower.startsWith("hard resetting") ||
                    lower.startsWith("waiting for upload port") ||
                    lower.startsWith("new upload port") ||
                    lower.startsWith("error during upload") ||
                    lower.startsWith("failed uploading") ||
                    lower.contains("no device found") ||
                    lower.contains("permission denied") ||
                    lower.contains("not in sync") ||
                    lower.contains("timed out") ||
                    lower.contains("timeout") ||
                    lower.contains("can not open port") ||
                    lower.contains("cannot open port");
                print = isUsefulStatus;
            }
        }

        if (print && !trimmed.isEmpty()) {
            QString html = line.toHtmlEscaped();
            if (!color.isEmpty()) {
                html = QString("<span style=\"color:%1;\">%2</span>").arg(color, html);
            }
            if (bold) {
                html = QString("<b>%1</b>").arg(html);
            }
            if (output_) output_->appendHtml(html);
        }
    }
}

void MainWindow::loadFavorites() {
  QSettings settings;
  settings.beginGroup(kSettingsGroup);
  const QStringList list = settings.value("favoriteBoards").toStringList();
  favoriteFqbns_ = QSet<QString>(list.begin(), list.end());
  settings.endGroup();
}

void MainWindow::saveFavorites() {
  QSettings settings;
  settings.beginGroup(kSettingsGroup);
  settings.setValue("favoriteBoards", QStringList(favoriteFqbns_.begin(), favoriteFqbns_.end()));
  settings.endGroup();
}

void MainWindow::toggleFavorite(const QString& fqbn) {
  if (fqbn.isEmpty()) return;
  if (favoriteFqbns_.contains(fqbn)) {
    favoriteFqbns_.remove(fqbn);
  } else {
    favoriteFqbns_.insert(fqbn);
  }
  saveFavorites();
  rebuildBoardMenu();
}

bool MainWindow::isFavorite(const QString& fqbn) const {
  return favoriteFqbns_.contains(fqbn);
}

void MainWindow::scheduleBoardListRefresh() {
  QTimer::singleShot(120, this, [this] { refreshInstalledBoards(); });
}

void MainWindow::closeEvent(QCloseEvent* event) {
  if (mcpServerProcess_ && mcpServerProcess_->state() != QProcess::NotRunning) {
    mcpStopRequested_ = true;
    mcpServerProcess_->terminate();
    if (!mcpServerProcess_->waitForFinished(1200)) {
      mcpServerProcess_->kill();
      (void)mcpServerProcess_->waitForFinished(400);
    }
  }
  stopRefreshProcesses();
  stopLanguageServer();
  if (serialPort_) {
    serialPort_->closePort();
  }
  persistStateToSettings();
  QMainWindow::closeEvent(event);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if ((watched == this || watched == qApp) &&
        (event->type() == QEvent::ApplicationPaletteChange ||
         event->type() == QEvent::PaletteChange ||
         event->type() == QEvent::StyleChange)) {
        rebuildContextToolbar();
    }

    if (boardCombo_ && watched == boardCombo_) {
        if (event->type() == QEvent::MouseButtonPress) {
            showSelectBoardDialog();
            return true;
        }
        if (event->type() == QEvent::KeyPress) {
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            const int key = keyEvent->key();
            if (key == Qt::Key_Space || key == Qt::Key_Return ||
                key == Qt::Key_Enter || key == Qt::Key_Down ||
                key == Qt::Key_F4) {
                showSelectBoardDialog();
                return true;
            }
        }
    }

    if (boardCombo_ && watched == boardCombo_->view()->viewport()) {
        if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease) {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                QModelIndex index = boardCombo_->view()->indexAt(mouseEvent->pos());
                if (index.isValid()) {
                    const QRect rect = boardCombo_->view()->visualRect(index);
                    if (mouseEvent->pos().x() > rect.right() - 32) {
                        if (event->type() == QEvent::MouseButtonPress) {
                            const QString fqbn = index.data(Qt::UserRole).toString();
                            if (!fqbn.isEmpty()) {
                                toggleFavorite(fqbn);
                                auto* proxy = static_cast<BoardFilterProxyModel*>(boardCombo_->model());
                                if (proxy) {
                                    QModelIndex srcIdx = proxy->mapToSource(index);
                                    proxy->sourceModel()->setData(srcIdx, isFavorite(fqbn), kRoleIsFavorite);
                                    proxy->invalidate();
                                }
                            }
                        }
                        return true; // Consume event (both press and release)
                    }
                }
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::rebuildRecentSketchesMenu() {
  if (!recentSketchesMenu_) return;
  recentSketchesMenu_->clear();
  const QStringList items = recentSketches();
  if (items.isEmpty()) {
    recentSketchesMenu_->addAction(tr("(No recent sketches)"))->setEnabled(false);
    return;
  }
  for (const QString& path : items) {
    QAction* a = recentSketchesMenu_->addAction(QFileInfo(path).fileName());
    connect(a, &QAction::triggered, this, [this, path] {
      (void)openSketchFolderInUi(path);
    });
  }
}

void MainWindow::rebuildExamplesMenu() {
  if (!examplesMenu_) return;
  examplesMenu_->clear();

  ExamplesScanner::Options opts = ExamplesScanner::defaultOptions();
  opts.currentFqbn = currentFqbn();
  QVector<ExampleSketch> sketches = ExamplesScanner::scan(opts);

  if (sketches.isEmpty()) {
    examplesMenu_->addAction(tr("(No examples found)"))->setEnabled(false);
    return;
  }

  QString lastCategory;
  QMap<QString, QMenu*> menuCache;

  for (const auto& s : sketches) {
    if (s.menuPath.isEmpty()) continue;

    QString category = s.menuPath.first();
    if (category != lastCategory) {
      if (!lastCategory.isEmpty()) {
        examplesMenu_->addSeparator();
      }
      lastCategory = category;
    }

    QMenu* parent = examplesMenu_;
    QString currentPath;
    for (int i = 0; i < s.menuPath.size() - 1; ++i) {
      QString segment = s.menuPath.at(i);
      currentPath += (currentPath.isEmpty() ? "" : ":::") + segment;
      if (!menuCache.contains(currentPath)) {
        QMenu* sub = parent->addMenu(segment);
        menuCache[currentPath] = sub;
      }
      parent = menuCache[currentPath];
    }

    QString title = s.menuPath.last();
    QAction* a = parent->addAction(title);
    connect(a, &QAction::triggered, this, [this, s] {
      (void)openSketchFolderInUi(QFileInfo(s.inoPath).absolutePath());
    });
  }
}

QStringList MainWindow::recentSketches() const {
  QSettings settings;
  settings.beginGroup(kSettingsGroup);
  return settings.value(kRecentSketchesKey).toStringList();
}

QStringList MainWindow::pinnedSketches() const {
  QSettings settings;
  settings.beginGroup(kSettingsGroup);
  return settings.value(kPinnedSketchesKey).toStringList();
}

void MainWindow::persistStateToSettings() {
  QSettings settings;
  settings.beginGroup(kSettingsGroup);
  settings.setValue(kGeometryKey, saveGeometry());
  settings.setValue(kStateKey, saveState());
  if (editor_) {
    const auto files = editor_->openedFiles();
    settings.setValue(kOpenFilesKey, QStringList(files.begin(), files.end()));
    settings.setValue(kActiveFileKey, editor_->currentFilePath());
  }
  settings.endGroup();
}

void MainWindow::restoreStateFromSettings() {
  QSettings settings;
  settings.beginGroup(kSettingsGroup);
  if (settings.contains(kGeometryKey)) {
    restoreGeometry(settings.value(kGeometryKey).toByteArray());
  }
  if (settings.contains(kStateKey)) {
    restoreState(settings.value(kStateKey).toByteArray());
  }

  const QStringList openFiles = settings.value(kOpenFilesKey).toStringList();
  const QString activeFile = settings.value(kActiveFileKey).toString();

  for (const QString& f : openFiles) {
    if (editor_) editor_->openFile(f);
  }
  if (!activeFile.isEmpty() && editor_) {
    editor_->openFile(activeFile);
  }

  settings.endGroup();
  enforceToolbarLayout();
  updateWelcomeVisibility();
  refreshBoardOptions();

  if (actionToggleFontToolBar_ && fontToolBar_) {
    const QSignalBlocker blocker(actionToggleFontToolBar_);
    actionToggleFontToolBar_->setChecked(fontToolBar_->isVisible());
  }
  syncContextModeSelection(fontToolBar_ && fontToolBar_->isVisible());
}

void MainWindow::refreshInstalledBoards() {
  if (!arduinoCli_ || arduinoCli_->isRunning() || !boardCombo_) return;

  QProcess* p = new QProcess(this);
  connect(p, &QProcess::finished, this, [this, p](int exitCode, QProcess::ExitStatus) {
    if (exitCode == 0) {
      const QByteArray data = p->readAllStandardOutput();
      const QJsonDocument doc = QJsonDocument::fromJson(data);

      QJsonArray arr;
      if (doc.isObject()) {
        arr = doc.object().value("boards").toArray();
      } else if (doc.isArray()) {
        arr = doc.array();
      }

      auto* proxy = static_cast<BoardFilterProxyModel*>(boardCombo_->model());
      auto* sourceModel =
          proxy ? qobject_cast<QStandardItemModel*>(proxy->sourceModel())
                : nullptr;
      if (!sourceModel) {
        p->deleteLater();
        return;
      }

      sourceModel->clear();
      auto* placeholder = new QStandardItem(tr("Select Board..."));
      placeholder->setData(QString(), Qt::UserRole);
      sourceModel->appendRow(placeholder);

      QMap<QString, QString> uniqueBoards;  // name -> fqbn
      for (const QJsonValue& v : arr) {
        const QJsonObject obj = v.toObject();
        const QString name = obj.value("name").toString();
        const QString fqbn = obj.value("fqbn").toString();
        if (!name.isEmpty() && !fqbn.isEmpty() && !uniqueBoards.contains(name)) {
          uniqueBoards[name] = fqbn;
        }
      }

      if (uniqueBoards.isEmpty()) {
        if (output_) {
          output_->appendLine(
              tr("Warning: No boards found. Please run Board Setup Wizard or install platforms via Boards Manager."));
        }
        maybeRunBoardSetupWizard();
      }

      for (auto it = uniqueBoards.begin(); it != uniqueBoards.end(); ++it) {
        auto* item = new QStandardItem(it.key());
        item->setData(it.value(), Qt::UserRole);
        item->setData(isFavorite(it.value()), kRoleIsFavorite);
        sourceModel->appendRow(item);
      }

      QString savedFqbn = preferredFqbnForSketch(currentSketchFolderPath());
      if (savedFqbn.isEmpty()) {
        QSettings settings;
        settings.beginGroup(kSettingsGroup);
        savedFqbn = settings.value(kFqbnKey).toString().trimmed();
        settings.endGroup();
      }

      if (proxy) {
        proxy->sort(0, Qt::AscendingOrder);
      }

      if (!savedFqbn.isEmpty()) {
        const int index = boardCombo_->findData(savedFqbn);
        if (index >= 0) {
          boardCombo_->setCurrentIndex(index);
        } else if (boardCombo_->count() > 0) {
          boardCombo_->setCurrentIndex(0);
        }
      } else if (boardCombo_->count() > 0) {
        boardCombo_->setCurrentIndex(0);
      }
    }
    p->deleteLater();
  });

  const QStringList args =
      arduinoCli_->withGlobalFlags({"board", "listall", "--format", "json"});
  p->start(arduinoCli_->arduinoCliPath(), args);
}

void MainWindow::maybeRunBoardSetupWizard() {
  if (boardSetupWizardShownThisSession_) {
    return;
  }

  QSettings settings;
  settings.beginGroup(kSettingsGroup);
  const bool wizardAlreadyHandled =
      settings.value(kBoardSetupWizardCompletedKey, false).toBool();
  settings.endGroup();
  if (wizardAlreadyHandled) {
    boardSetupWizardShownThisSession_ = true;
    return;
  }

  boardSetupWizardShownThisSession_ = true;
  QTimer::singleShot(0, this, [this] { runBoardSetupWizard(); });
}

bool MainWindow::mergeAdditionalBoardUrlsIntoPreferences(
    QStringList urlsToMerge,
    QString* outError,
    QStringList* outMergedUrls) {
  urlsToMerge = normalizeStringList(std::move(urlsToMerge));

  QString sketchbookDir;
  QStringList configuredUrls;
  QSettings settings;
  settings.beginGroup(QStringLiteral("Preferences"));
  sketchbookDir = settings.value(QStringLiteral("sketchbookDir")).toString();
  if (settings.contains(QStringLiteral("additionalUrls"))) {
    configuredUrls =
        settings.value(QStringLiteral("additionalUrls")).toStringList();
  }
  settings.endGroup();

  const ArduinoCliConfigSnapshot snapshot = readArduinoCliConfigSnapshot(
      arduinoCli_ ? arduinoCli_->arduinoCliConfigPath() : QString{});
  if (configuredUrls.isEmpty()) {
    configuredUrls = snapshot.additionalUrls;
  }

  QStringList merged = normalizeStringList(configuredUrls);
  merged.append(urlsToMerge);
  merged = normalizeStringList(merged);

  settings.beginGroup(QStringLiteral("Preferences"));
  settings.setValue(QStringLiteral("additionalUrls"), merged);
  settings.endGroup();

  if (outMergedUrls) {
    *outMergedUrls = merged;
  }

  if (sketchbookDir.trimmed().isEmpty()) {
    sketchbookDir = snapshot.userDir;
  }
  if (sketchbookDir.trimmed().isEmpty()) {
    sketchbookDir = defaultSketchbookDir();
  }
  QDir().mkpath(sketchbookDir);

  if (!arduinoCli_) {
    if (outError) {
      outError->clear();
    }
    return true;
  }

  const QString configPath = arduinoCli_->arduinoCliConfigPath();
  if (configPath.trimmed().isEmpty()) {
    if (outError) {
      *outError = tr("Arduino CLI config path is unavailable.");
    }
    return true;
  }

  QString configError;
  const bool ok =
      updateArduinoCliConfig(configPath, sketchbookDir, merged, &configError);
  if (!ok) {
    if (outError) {
      *outError = configError.trimmed();
    }
    return false;
  }

  if (outError) {
    outError->clear();
  }
  return true;
}

bool MainWindow::runBoardSetupCoreInstall(const QStringList& coreIds,
                                          QString* outError) {
  if (!arduinoCli_) {
    if (outError) {
      *outError = tr("Arduino CLI is not initialized.");
    }
    return false;
  }
  if (arduinoCli_->isRunning()) {
    if (outError) {
      *outError = tr("Another Arduino CLI command is currently running.");
    }
    return false;
  }

  const QString cliPath = arduinoCli_->arduinoCliPath().trimmed();
  if (cliPath.isEmpty()) {
    if (outError) {
      *outError = tr("Arduino CLI path is empty.");
    }
    return false;
  }

  if (output_) {
    output_->appendLine(tr("[Board Setup] Updating board index..."));
  }
  CommandResult updateIndexResult = runCommandBlocking(
      cliPath,
      arduinoCli_->withGlobalFlags(
          {QStringLiteral("core"), QStringLiteral("update-index")}),
      {}, {}, 600000);
  if (output_) {
    if (!updateIndexResult.stdoutText.trimmed().isEmpty()) {
      output_->appendLine(updateIndexResult.stdoutText.trimmed());
    }
    if (!updateIndexResult.stderrText.trimmed().isEmpty()) {
      output_->appendLine(updateIndexResult.stderrText.trimmed());
    }
  }
  if (!(updateIndexResult.started && !updateIndexResult.timedOut &&
        updateIndexResult.exitStatus == QProcess::NormalExit &&
        updateIndexResult.exitCode == 0)) {
    if (outError) {
      *outError = tr("Failed to update board index: %1")
                      .arg(commandErrorSummary(updateIndexResult));
    }
    return false;
  }

  QStringList failures;
  for (const QString& coreId : coreIds) {
    const QString trimmedCore = coreId.trimmed();
    if (trimmedCore.isEmpty()) {
      continue;
    }

    if (output_) {
      output_->appendLine(
          tr("[Board Setup] Installing core %1 ...").arg(trimmedCore));
    }
    const CommandResult installResult = runCommandBlocking(
        cliPath,
        arduinoCli_->withGlobalFlags({QStringLiteral("core"),
                                      QStringLiteral("install"), trimmedCore}),
        {}, {}, 900000);

    if (output_) {
      if (!installResult.stdoutText.trimmed().isEmpty()) {
        output_->appendLine(installResult.stdoutText.trimmed());
      }
      if (!installResult.stderrText.trimmed().isEmpty()) {
        output_->appendLine(installResult.stderrText.trimmed());
      }
    }

    const bool ok = installResult.started && !installResult.timedOut &&
                    installResult.exitStatus == QProcess::NormalExit &&
                    installResult.exitCode == 0;
    if (!ok) {
      failures << tr("%1 (%2)")
                      .arg(trimmedCore, commandErrorSummary(installResult));
    }
  }

  if (!failures.isEmpty()) {
    if (outError) {
      *outError = tr("Some cores failed to install:\n%1")
                      .arg(failures.join(QStringLiteral("\n")));
    }
    return false;
  }

  if (outError) {
    outError->clear();
  }
  return true;
}

void MainWindow::runBoardSetupWizard() {
  auto markWizardHandled = [] {
    QSettings settings;
    settings.beginGroup(kSettingsGroup);
    settings.setValue(kBoardSetupWizardCompletedKey, true);
    settings.endGroup();
  };

  QWizard wizard(this);
  wizard.setWindowTitle(tr("Board Setup Wizard"));
  wizard.setWizardStyle(QWizard::ModernStyle);
  wizard.setOption(QWizard::NoBackButtonOnStartPage, true);
  wizard.setOption(QWizard::HaveHelpButton, false);
  wizard.setButtonText(QWizard::FinishButton, tr("Apply"));
  wizard.resize(840, 620);

  auto* modePage = new BoardSetupModePage(&wizard);
  auto* presetsPage = new BoardSetupPresetPage(boardSetupPresetCatalog(), &wizard);
  auto* customPage = new BoardSetupCustomPage(&wizard);
  auto* reviewPage = new BoardSetupReviewPage(&wizard);

  wizard.setPage(kBoardWizardPageMode, modePage);
  wizard.setPage(kBoardWizardPagePresets, presetsPage);
  wizard.setPage(kBoardWizardPageCustom, customPage);
  wizard.setPage(kBoardWizardPageReview, reviewPage);
  wizard.setStartId(kBoardWizardPageMode);

  if (wizard.exec() != QDialog::Accepted) {
    markWizardHandled();
    return;
  }

  QStringList selectedUrls = reviewPage->selectedUrls();
  QStringList coreIds = reviewPage->selectedCores();
  selectedUrls = normalizeStringList(selectedUrls);
  coreIds = normalizeStringList(coreIds);

  if (selectedUrls.isEmpty()) {
    QMessageBox::warning(this, tr("Board Setup"),
                         tr("No board manager URL was selected."));
    return;
  }

  QString mergedError;
  if (!mergeAdditionalBoardUrlsIntoPreferences(selectedUrls, &mergedError, nullptr)) {
    QMessageBox::warning(
        this, tr("Board Setup"),
        tr("Could not update board manager URLs.\n\n%1").arg(mergedError));
    return;
  }
  if (!mergedError.trimmed().isEmpty() && output_) {
    output_->appendLine(
        tr("[Board Setup] %1").arg(mergedError.trimmed()));
  }

  if (reviewPage->installRecommendedNow() && !coreIds.isEmpty()) {
    QString installError;
    const bool installed = runBoardSetupCoreInstall(coreIds, &installError);
    markWizardHandled();

    if (!installed) {
      QMessageBox::warning(this, tr("Board Setup Failed"), installError);
      if (reviewPage->openBoardsManagerAfterFinish() && actionBoardsManager_) {
        actionBoardsManager_->trigger();
      }
      return;
    }
    showToast(tr("Board setup completed"));
  } else {
    markWizardHandled();
    showToast(tr("Board manager URLs updated"));
  }

  if (reviewPage->openBoardsManagerAfterFinish() && actionBoardsManager_) {
    actionBoardsManager_->trigger();
  }

  if (boardsManager_) {
    boardsManager_->refresh();
  }
  refreshInstalledBoards();
  refreshConnectedPorts();
}

void MainWindow::refreshConnectedPorts() {
  if (!arduinoCli_ || !portCombo_) return;
  if (arduinoCli_->isRunning()) {
    portsRefreshQueued_ = true;
    return;
  }

  QProcess* p = new QProcess(this);
  connect(p, &QProcess::finished, this, [this, p](int exitCode, QProcess::ExitStatus) {
    if (exitCode == 0) {
      const QByteArray data = p->readAllStandardOutput();
      const QJsonDocument doc = QJsonDocument::fromJson(data);
      
      QJsonArray arr;
      if (doc.isObject()) {
          arr = doc.object().value("detected_ports").toArray();
      } else if (doc.isArray()) {
          arr = doc.array();
      }

      // Note: arr can be empty if no ports detected, but doc was valid
      if (doc.isObject() || doc.isArray()) {
        const QString selectedBeforeRefresh = currentPort().trimmed();
        QSettings settings;
        settings.beginGroup(kSettingsGroup);
        const QString savedPort = settings.value(kPortKey).toString().trimmed();
        settings.endGroup();
	        const QString preferredPort = !selectedBeforeRefresh.isEmpty()
	                                           ? selectedBeforeRefresh
	                                           : savedPort;
        const QColor portTextColor = palette().color(QPalette::Text);
        const QColor portDisabledColor =
            palette().color(QPalette::Disabled, QPalette::Text);

	        const QSignalBlocker blockPortSignals(portCombo_);
	        portCombo_->clear();
	        portCombo_->addItem(tr("Select Port..."), QString{});
        portCombo_->setItemData(0, portDisabledColor, Qt::ForegroundRole);
	        int preferredIndex = -1;
        
		        for (const QJsonValue& v : arr) {
		          const QJsonObject obj = v.toObject();
	          const QJsonObject portObj = obj.value("port").toObject();
	          const QString portAddress = portObj.value("address").toString().trimmed();
	          const QString protocol = portObj.value("protocol").toString().trimmed();
	          const QString protocolLabel =
	              portObj.value("protocol_label").toString().trimmed();
	          
	          QString boardName;
	          QString boardFqbn;
	          // In 'board list', detected boards are in "boards" array if recognized
	          const QJsonArray boardsArr = obj.value("boards").toArray();
	          if (!boardsArr.isEmpty()) {
	             const QJsonObject boardObj = boardsArr.first().toObject();
	             boardName = boardObj.value("name").toString().trimmed();
	             boardFqbn = boardObj.value("fqbn").toString().trimmed();
	          }

	          if (!portAddress.isEmpty()) {
	            QString displayText = portAddress;
	            if (!boardName.isEmpty()) {
	                displayText += QString(" (%1)").arg(boardName);
	            } else {
	              const QString protoDisplay =
	                  !protocolLabel.isEmpty() ? protocolLabel : protocol;
	              if (!protoDisplay.isEmpty() &&
	                  protoDisplay != QStringLiteral("serial")) {
	                displayText += QString(" (%1)").arg(protoDisplay);
	              }
	            }
		            portCombo_->addItem(displayText, portAddress);
		            const int addedIndex = portCombo_->count() - 1;
                portCombo_->setItemData(addedIndex, portTextColor,
                                        Qt::ForegroundRole);
		            portCombo_->setItemData(
		                addedIndex,
		                protocol.isEmpty() ? QStringLiteral("serial") : protocol,
		                kPortRoleProtocol);
	            if (!boardName.isEmpty()) {
	              portCombo_->setItemData(addedIndex, boardName,
	                                      kPortRoleDetectedBoardName);
	            }
		            if (!boardFqbn.isEmpty()) {
		              portCombo_->setItemData(addedIndex, boardFqbn,
		                                      kPortRoleDetectedFqbn);
		            }
		            if (!preferredPort.isEmpty() && preferredPort == portAddress) {
		              preferredIndex = addedIndex;
		            }
		          }
		        }

	        if (preferredIndex < 0 && !preferredPort.isEmpty()) {
	          const int missingIndex = 1;  // directly under the placeholder
		          portCombo_->insertItem(missingIndex, tr("%1 (missing)").arg(preferredPort),
		                                   preferredPort);
		          portCombo_->setItemData(missingIndex, true, kPortRoleMissing);
              portCombo_->setItemData(missingIndex, portDisabledColor,
                                      Qt::ForegroundRole);
		          if (auto* model = qobject_cast<QStandardItemModel*>(portCombo_->model())) {
		            if (QStandardItem* item = model->item(missingIndex)) {
		              item->setEnabled(false);
		            }
		          }
	          preferredIndex = missingIndex;
	        }

	        portCombo_->setCurrentIndex(preferredIndex >= 0 ? preferredIndex : 0);

          const QString selectedAfterRefresh = currentPort().trimmed();
          QSettings persistedSettings;
          persistedSettings.beginGroup(kSettingsGroup);
          if (selectedAfterRefresh.isEmpty()) {
            persistedSettings.remove(kPortKey);
          } else {
            persistedSettings.setValue(kPortKey, selectedAfterRefresh);
          }
          persistedSettings.endGroup();

          maybeAutoSelectBoardForCurrentPort();
          updateBoardPortIndicator();
	      }
	    }
	    p->deleteLater();
  });

  const QStringList args =
      arduinoCli_->withGlobalFlags({"board", "list", "--format", "json"});
  p->start(arduinoCli_->arduinoCliPath(), args);
}

void MainWindow::startPortWatcher() {
  if (portsWatchProcess_) stopPortWatcher();
  
  portsWatchProcess_ = new QProcess(this);
  connect(portsWatchProcess_, &QProcess::readyReadStandardOutput, this, [this] {
    if (!portsWatchProcess_) {
      return;
    }
    (void)portsWatchProcess_->readAllStandardOutput();
    if (portsWatchDebounceTimer_) {
      portsWatchDebounceTimer_->start();
    }
  });
  const QStringList args = arduinoCli_
                               ? arduinoCli_->withGlobalFlags(
                                     {"board", "list", "--watch", "--format", "json"})
                               : QStringList{"board", "list", "--watch", "--format", "json"};
  portsWatchProcess_->start(arduinoCli_ ? arduinoCli_->arduinoCliPath() : QString{}, args);
}

void MainWindow::stopPortWatcher() {
  if (portsWatchProcess_) {
    if (portsWatchProcess_->state() != QProcess::NotRunning) {
      portsWatchProcess_->terminate();
      if (!portsWatchProcess_->waitForFinished(750)) {
        portsWatchProcess_->kill();
        (void)portsWatchProcess_->waitForFinished(750);
      }
    }
    portsWatchProcess_->deleteLater();
    portsWatchProcess_ = nullptr;
  }
}
void MainWindow::rebuildBoardMenu() {
  if (!boardMenu_) {
    return;
  }

  boardMenu_->clear();

  const QString fqbn = currentFqbn();
  QAction* current = boardMenu_->addAction(
      tr("Current: %1").arg(fqbn.isEmpty() ? tr("(none)") : fqbn));
  current->setEnabled(false);
  boardMenu_->addSeparator();

  if (actionSelectBoard_) {
    boardMenu_->addAction(actionSelectBoard_);
  }
  if (actionBoardSetupWizard_) {
    boardMenu_->addAction(actionBoardSetupWizard_);
  }
  if (actionRefreshBoards_) {
    boardMenu_->addAction(actionRefreshBoards_);
  }
  if (actionBoardsManager_) {
    boardMenu_->addAction(actionBoardsManager_);
  }
}

void MainWindow::rebuildPortMenu() {
  if (!portMenu_) {
    return;
  }

  portMenu_->clear();

  if (actionRefreshPorts_) {
    portMenu_->addAction(actionRefreshPorts_);
    if (actionSelectBoard_) {
      portMenu_->addAction(actionSelectBoard_);
    }
    if (portCombo_) {
      const int idx = portCombo_->currentIndex();
      const QString detectedFqbn =
          idx >= 0 ? portCombo_->itemData(idx, kPortRoleDetectedFqbn).toString().trimmed()
                   : QString{};
      QAction* useDetectedBoard =
          portMenu_->addAction(tr("Use Detected Board for This Port"));
      useDetectedBoard->setEnabled(!detectedFqbn.isEmpty());
      connect(useDetectedBoard, &QAction::triggered, this, [this, detectedFqbn] {
        if (detectedFqbn.isEmpty()) {
          return;
        }
        if (boardCombo_) {
          const int boardIndex = boardCombo_->findData(detectedFqbn);
          if (boardIndex >= 0) {
            boardCombo_->setCurrentIndex(boardIndex);
          } else {
            QSettings settings;
            settings.beginGroup(kSettingsGroup);
            settings.setValue(kFqbnKey, detectedFqbn);
            settings.endGroup();
          }
        }
        updateBoardPortIndicator();
        showToast(tr("Detected board selected"));
      });
    }
    const QString activePort = currentPort().trimmed();
    QAction* copyPortAddress = portMenu_->addAction(tr("Copy Port Address"));
    copyPortAddress->setEnabled(!activePort.isEmpty());
    connect(copyPortAddress, &QAction::triggered, this, [this, activePort] {
      if (activePort.isEmpty()) {
        return;
      }
      QGuiApplication::clipboard()->setText(activePort);
      showToast(tr("Port address copied"));
    });
    portMenu_->addSeparator();
  }

  const QString current = currentPort().trimmed();
  const bool missing = currentPortIsMissing();

  if (missing && !current.isEmpty()) {
    QAction* missing =
        portMenu_->addAction(tr("%1 (missing)").arg(current));
    missing->setCheckable(true);
    missing->setChecked(true);
    missing->setEnabled(false);
    portMenu_->addSeparator();
  }

  if (!portCombo_) {
    portMenu_->addAction(tr("(No ports found)"))->setEnabled(false);
    return;
  }

  QVector<int> serialPorts;
  QVector<int> networkPorts;
  QVector<int> otherPorts;
  serialPorts.reserve(portCombo_->count());
  networkPorts.reserve(portCombo_->count());
  otherPorts.reserve(portCombo_->count());

  for (int i = 0; i < portCombo_->count(); ++i) {
    const QString port = portCombo_->itemData(i).toString().trimmed();
    if (port.isEmpty()) {
      continue;
    }
    if (portCombo_->itemData(i, kPortRoleMissing).toBool()) {
      continue;
    }
    QString protocol =
        portCombo_->itemData(i, kPortRoleProtocol).toString().trimmed();
    protocol = protocol.toLower();
    if (protocol.isEmpty() || protocol == QStringLiteral("serial")) {
      serialPorts.push_back(i);
    } else if (protocol == QStringLiteral("network")) {
      networkPorts.push_back(i);
    } else {
      otherPorts.push_back(i);
    }
  }

  if (serialPorts.isEmpty() && networkPorts.isEmpty() && otherPorts.isEmpty()) {
    portMenu_->addAction(tr("(No ports found)"))->setEnabled(false);
    return;
  }

  auto addHeader = [this](const QString& title) {
    QAction* a = portMenu_->addAction(title);
    a->setEnabled(false);
    QFont f = a->font();
    f.setBold(true);
    a->setFont(f);
    return a;
  };

  auto addPortAction = [this, &current, missing](QActionGroup* group, int i) {
    const QString port = portCombo_->itemData(i).toString().trimmed();
    if (port.isEmpty()) {
      return;
    }
    const QString label = portCombo_->itemText(i).trimmed().isEmpty()
                              ? port
                              : portCombo_->itemText(i).trimmed();
    QAction* a = portMenu_->addAction(label);
    a->setCheckable(true);
    a->setData(port);
    a->setChecked(!missing && port == current);
    group->addAction(a);

    connect(a, &QAction::triggered, this, [this, port] {
      if (!portCombo_) {
        return;
      }
      const int index = portCombo_->findData(port);
      if (index >= 0) {
        portCombo_->setCurrentIndex(index);
      } else {
        QSettings settings;
        settings.beginGroup(kSettingsGroup);
        settings.setValue(kPortKey, port);
        settings.endGroup();
        updateBoardPortIndicator();
      }
    });
  };

  auto* group = new QActionGroup(portMenu_);
  group->setExclusive(true);

  const bool needsGrouping = !networkPorts.isEmpty() || !otherPorts.isEmpty();

  if (!serialPorts.isEmpty()) {
    if (needsGrouping) {
      addHeader(tr("Serial Ports"));
    }
    for (int i : serialPorts) {
      addPortAction(group, i);
    }
  }

  if (!networkPorts.isEmpty()) {
    if (!serialPorts.isEmpty()) {
      portMenu_->addSeparator();
    }
    if (needsGrouping) {
      addHeader(tr("Network Ports"));
    }
    for (int i : networkPorts) {
      addPortAction(group, i);
    }
  }

  if (!otherPorts.isEmpty()) {
    if (!serialPorts.isEmpty() || !networkPorts.isEmpty()) {
      portMenu_->addSeparator();
    }
    addHeader(tr("Other Ports"));
    for (int i : otherPorts) {
      addPortAction(group, i);
    }
  }
}

void MainWindow::maybeAutoSelectBoardForCurrentPort() {
  if (!portCombo_) {
    return;
  }
  const int idx = portCombo_->currentIndex();
  if (idx < 0) {
    return;
  }
  if (portCombo_->itemData(idx, kPortRoleMissing).toBool()) {
    return;
  }
  const QString detected =
      portCombo_->itemData(idx, kPortRoleDetectedFqbn).toString().trimmed();
  if (detected.isEmpty()) {
    return;
  }

  // Don't override a manual board selection.
  if (!currentFqbn().trimmed().isEmpty()) {
    return;
  }

  if (boardCombo_) {
    const int boardIndex = boardCombo_->findData(detected);
    if (boardIndex >= 0) {
      boardCombo_->setCurrentIndex(boardIndex);
      return;
    }
  }

  // Boards list might not be loaded yet; persist the detected board.
  QSettings settings;
  settings.beginGroup(kSettingsGroup);
  settings.setValue(kFqbnKey, detected);
  settings.endGroup();
  storeFqbnForCurrentSketch(detected);
}

void MainWindow::scheduleRefreshBoardOptions() {
  if (boardOptionsRefreshTimer_) {
    boardOptionsRefreshTimer_->start();
    return;
  }
  QTimer::singleShot(0, this, [this] { refreshBoardOptions(); });
}

void MainWindow::refreshBoardOptions() {
  clearBoardOptionMenus();
  
  const QString fqbn = currentFqbn();
  if (fqbn.isEmpty() || !arduinoCli_) return;
  
  // Extract base FQBN (without options)
  QString baseFqbn = fqbn;
  if (baseFqbn.contains(':')) {
      QStringList parts = baseFqbn.split(':');
      if (parts.size() > 3) {
          baseFqbn = parts.mid(0, 3).join(':');
      }
  }

  QProcess* p = new QProcess(this);
  connect(p, &QProcess::finished, this, [this, p, baseFqbn](int exitCode, QProcess::ExitStatus) {
    if (exitCode == 0) {
      const QByteArray data = p->readAllStandardOutput();
      const QJsonDocument doc = QJsonDocument::fromJson(data);
      const QJsonObject root = doc.object();
      const QJsonArray options = root.value("config_options").toArray();

      if (!options.isEmpty()) {
          QSettings settings;
          settings.beginGroup("BoardOptions");
          settings.beginGroup(baseFqbn);
          bool settingsChanged = false;

          for (const QJsonValue& optVal : options) {
              const QJsonObject opt = optVal.toObject();
              const QString optId = opt.value("option").toString();
              const QString optLabel = opt.value("option_label").toString();
              const QJsonArray values = opt.value("values").toArray();

              if (optId.trimmed().isEmpty() || values.isEmpty()) continue;

              QMenu* subMenu = new QMenu(optLabel, toolsMenu_);
              QAction* subMenuAction = toolsMenu_->insertMenu(programmerMenuAction_, subMenu);
              boardOptionMenuActions_.append(subMenuAction);
              auto* valueGroup = new QActionGroup(subMenu);
              valueGroup->setExclusive(true);

              const QString currentSelectedValue = settings.value(optId).toString();
              bool foundSelected = false;
              QString defaultValueId;
              QString firstValueId;

              for (const QJsonValue& valVal : values) {
                  const QJsonObject val = valVal.toObject();
                  const QString valId = val.value("value").toString();
                  const QString valLabel = val.value("value_label").toString();
                  const bool isDefault = val.value("selected").toBool();
                  if (valId.trimmed().isEmpty()) {
                      continue;
                  }
                  if (firstValueId.isEmpty()) {
                      firstValueId = valId;
                  }
                  if (isDefault && defaultValueId.isEmpty()) {
                      defaultValueId = valId;
                  }

                  QAction* act = subMenu->addAction(valLabel);
                  act->setCheckable(true);
                  act->setData(valId);
                  valueGroup->addAction(act);
                  
                  bool isChecked = false;
                  if (!currentSelectedValue.isEmpty()) {
                      isChecked = (valId == currentSelectedValue);
                  } else {
                      isChecked = isDefault;
                  }
                  act->setChecked(isChecked);
                  if (isChecked) foundSelected = true;

                  connect(act, &QAction::triggered, this, [this, optId, valId] {
                      setBoardOption(optId, valId);
                  });
              }
              
              // If nothing was checked by settings or defaults, check first
              if (!foundSelected) {
                  QString fallbackValue = defaultValueId;
                  if (fallbackValue.isEmpty()) {
                      fallbackValue = firstValueId;
                  }
                  if (!fallbackValue.isEmpty()) {
                      settings.setValue(optId, fallbackValue);
                      settingsChanged = true;
                      const QList<QAction*> actions = subMenu->actions();
                      for (QAction* action : actions) {
                          if (!action) continue;
                          if (action->isChecked()) {
                              action->setChecked(false);
                          }
                          if (action->data().toString() == fallbackValue) {
                              action->setChecked(true);
                          }
                      }
                  }
              }
          }
          settings.endGroup();
          settings.endGroup();
          if (settingsChanged) {
              updateBoardPortIndicator();
          }
      }
    }
    p->deleteLater();
  });
  const QStringList args = arduinoCli_->withGlobalFlags(
      {"board", "details", "--fqbn", baseFqbn, "--format", "json"});
  p->start(arduinoCli_->arduinoCliPath(), args);
}

void MainWindow::setBoardOption(const QString& optionId, const QString& valueId) {
    const QString fqbn = currentFqbn();
    if (fqbn.isEmpty()) return;

    // Extract base FQBN
    QString baseFqbn = fqbn;
    if (baseFqbn.contains(':')) {
        QStringList parts = baseFqbn.split(':');
        if (parts.size() > 3) {
            baseFqbn = parts.mid(0, 3).join(':');
        }
    }

    // Save selection
    QSettings settings;
    settings.beginGroup("BoardOptions");
    settings.beginGroup(baseFqbn);
    settings.setValue(optionId, valueId);
    settings.endGroup();
    settings.endGroup();

    refreshBoardOptions();
    updateBoardPortIndicator();
    updateUploadActionStates();
}
void MainWindow::updateSketchbookView() {
  QSettings settings;
  settings.beginGroup("Preferences");
  QString dir = settings.value("sketchbookDir").toString();
  settings.endGroup();
  if (dir.trimmed().isEmpty()) dir = defaultSketchbookDir();
  
  if (sketchbookModel_) {
    sketchbookModel_->setRootPath(dir);
    if (sketchbookTree_) {
      sketchbookTree_->setRootIndex(sketchbookModel_->index(dir));
    }
  }
}
void MainWindow::stopRefreshProcesses() {
  if (boardsRefreshProcess_) boardsRefreshProcess_->kill();
  if (portsRefreshProcess_) portsRefreshProcess_->kill();
  if (boardDetailsProcess_) boardDetailsProcess_->kill();
}

void MainWindow::clearBoardOptionMenus() {
  for (QAction* a : boardOptionMenuActions_) a->deleteLater();
  boardOptionMenuActions_.clear();
}

void MainWindow::clearIncludeLibraryMenuActions() {
  if (includeLibraryProcess_) {
    includeLibraryProcess_->kill();
    includeLibraryProcess_->deleteLater();
    includeLibraryProcess_ = nullptr;
  }
  for (QAction* a : includeLibraryMenuActions_) a->deleteLater();
  includeLibraryMenuActions_.clear();
}

void MainWindow::rebuildIncludeLibraryMenu() {
  if (!includeLibraryMenu_) {
    return;
  }

  clearIncludeLibraryMenuActions();
  const bool cliAvailable =
      arduinoCli_ && !arduinoCli_->arduinoCliPath().trimmed().isEmpty();

  auto addDisabledItem = [this](const QString& text) {
    if (!includeLibraryMenu_) {
      return;
    }
    QAction* action = includeLibraryMenu_->addAction(text);
    action->setEnabled(false);
    includeLibraryMenuActions_.push_back(action);
  };

  auto addFooterActions = [this, cliAvailable]() {
    if (!includeLibraryMenu_) {
      return;
    }

    if (!actionAddZipLibrary_ && !actionManageLibraries_) {
      return;
    }

    if (!includeLibraryMenu_->actions().isEmpty()) {
      QAction* separator = includeLibraryMenu_->addSeparator();
      includeLibraryMenuActions_.push_back(separator);
    }

    if (actionAddZipLibrary_) {
      QAction* addZip = includeLibraryMenu_->addAction(actionAddZipLibrary_->text());
      addZip->setEnabled(cliAvailable);
      includeLibraryMenuActions_.push_back(addZip);
      connect(addZip, &QAction::triggered, this,
              [this] { actionAddZipLibrary_->trigger(); });
    }

    if (actionManageLibraries_) {
      QAction* manage =
          includeLibraryMenu_->addAction(actionManageLibraries_->text());
      includeLibraryMenuActions_.push_back(manage);
      connect(manage, &QAction::triggered, this,
              [this] { actionManageLibraries_->trigger(); });
    }
  };

  if (!cliAvailable) {
    addDisabledItem(tr("Arduino CLI unavailable"));
    addFooterActions();
    return;
  }

  QAction* loading = includeLibraryMenu_->addAction(
      tr("Loading installed libraries..."));
  loading->setEnabled(false);
  includeLibraryMenuActions_.push_back(loading);

  QProcess* process = new QProcess(this);
  process->setProcessChannelMode(QProcess::SeparateChannels);
  includeLibraryProcess_ = process;

  connect(process,
          QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
          this,
          [this, process, addDisabledItem, addFooterActions](int exitCode,
                                                              QProcess::ExitStatus) {
            if (includeLibraryProcess_ == process) {
              includeLibraryProcess_ = nullptr;
            }

            const QByteArray out = process->readAllStandardOutput();
            const QByteArray err = process->readAllStandardError();
            process->deleteLater();

            clearIncludeLibraryMenuActions();
            if (!includeLibraryMenu_) {
              return;
            }

            if (exitCode != 0) {
              if (output_ && !err.trimmed().isEmpty()) {
                output_->appendLine(tr("[Include Library] %1")
                                        .arg(QString::fromUtf8(err).trimmed()));
              }
              addDisabledItem(tr("Failed to load installed libraries"));
              addFooterActions();
              return;
            }

            const QJsonDocument doc = QJsonDocument::fromJson(out);
            if (!doc.isObject()) {
              addDisabledItem(tr("Failed to parse installed libraries"));
              addFooterActions();
              return;
            }

            struct LibraryEntry final {
              QString name;
              QStringList includes;
            };

            QVector<LibraryEntry> entries;
            const QJsonArray installed =
                doc.object().value("installed_libraries").toArray();
            entries.reserve(installed.size());

            for (const QJsonValue& value : installed) {
              const QJsonObject lib =
                  value.toObject().value("library").toObject();
              const QString name = lib.value("name").toString().trimmed();
              if (name.isEmpty()) {
                continue;
              }

              QStringList includes;
              for (const QJsonValue& includeValue :
                   lib.value("provides_includes").toArray()) {
                const QString include = includeValue.toString().trimmed();
                if (!include.isEmpty()) {
                  includes << include;
                }
              }
              includes.removeDuplicates();

              entries.push_back({name, includes});
            }

            std::sort(entries.begin(),
                      entries.end(),
                      [](const LibraryEntry& left, const LibraryEntry& right) {
                        return QString::localeAwareCompare(left.name, right.name) < 0;
                      });

            bool addedAny = false;
            for (const LibraryEntry& entry : entries) {
              QAction* action = includeLibraryMenu_->addAction(entry.name);
              includeLibraryMenuActions_.push_back(action);
              if (entry.includes.isEmpty()) {
                action->setEnabled(false);
                action->setToolTip(
                    tr("This library does not advertise include files."));
                continue;
              }
              addedAny = true;
              connect(action, &QAction::triggered, this, [this, entry] {
                insertLibraryIncludes(entry.name, entry.includes);
              });
            }

            if (!addedAny) {
              addDisabledItem(tr("No includable installed libraries"));
            }
            addFooterActions();
          });

  connect(process, &QProcess::errorOccurred, this,
          [this, process, addFooterActions](QProcess::ProcessError) {
    if (includeLibraryProcess_ == process) {
      includeLibraryProcess_ = nullptr;
    }
    process->deleteLater();
    clearIncludeLibraryMenuActions();
    if (includeLibraryMenu_) {
      QAction* failed = includeLibraryMenu_->addAction(
          tr("Could not start Arduino CLI"));
      failed->setEnabled(false);
      includeLibraryMenuActions_.push_back(failed);
    }
    addFooterActions();
  });

  const QStringList args = arduinoCli_->withGlobalFlags(
      {QStringLiteral("lib"), QStringLiteral("list"), QStringLiteral("--json")});
  process->start(arduinoCli_->arduinoCliPath(), args);
}

void MainWindow::insertLibraryIncludes(const QString& libraryName,
                                       const QStringList& includes) {
  if (!editor_) {
    return;
  }
  auto* plain = qobject_cast<QPlainTextEdit*>(editor_->currentEditorWidget());
  if (!plain || !plain->document()) {
    showToast(tr("Open a sketch file first."));
    return;
  }

  QStringList headers;
  headers.reserve(includes.size());
  for (QString include : includes) {
    include = include.trimmed();
    if (include.startsWith(QLatin1Char('<')) && include.endsWith(QLatin1Char('>'))) {
      include = include.mid(1, include.size() - 2).trimmed();
    } else if (include.startsWith(QLatin1Char('"')) &&
               include.endsWith(QLatin1Char('"'))) {
      include = include.mid(1, include.size() - 2).trimmed();
    }
    if (!include.isEmpty()) {
      headers << include;
    }
  }
  headers.removeDuplicates();

  if (headers.isEmpty()) {
    showToast(tr("Library \"%1\" does not provide include headers.")
                  .arg(libraryName.trimmed().isEmpty()
                           ? tr("(unknown)")
                           : libraryName.trimmed()));
    return;
  }

  QSet<QString> existingHeaders;
  const QRegularExpression includeLineRe(
      QStringLiteral("^\\s*#\\s*include\\s*[<\\\"]([^>\\\"]+)[>\\\"]\\s*$"));
  const QStringList lines = plain->toPlainText().split(QLatin1Char('\n'));
  for (const QString& line : lines) {
    const QRegularExpressionMatch match = includeLineRe.match(line);
    if (match.hasMatch()) {
      existingHeaders.insert(match.captured(1).trimmed());
    }
  }

  QStringList headersToInsert;
  headersToInsert.reserve(headers.size());
  for (const QString& header : headers) {
    if (!existingHeaders.contains(header)) {
      headersToInsert << header;
    }
  }

  if (headersToInsert.isEmpty()) {
    showToast(tr("Includes for %1 are already present.").arg(libraryName));
    return;
  }

  QTextDocument* doc = plain->document();
  QTextBlock block = doc->begin();
  int insertPos = 0;
  bool sawInclude = false;

  while (block.isValid()) {
    const QString trimmed = block.text().trimmed();
    const bool isInclude = includeLineRe.match(block.text()).hasMatch();

    if (!sawInclude) {
      if (isInclude) {
        sawInclude = true;
        insertPos = block.position() + block.length();
        block = block.next();
        continue;
      }
      if (trimmed.isEmpty() || trimmed.startsWith(QStringLiteral("//")) ||
          trimmed.startsWith(QStringLiteral("/*")) ||
          trimmed.startsWith(QStringLiteral("*")) ||
          trimmed.startsWith(QStringLiteral("*/"))) {
        insertPos = block.position() + block.length();
        block = block.next();
        continue;
      }
      insertPos = block.position();
      break;
    }

    if (isInclude || trimmed.isEmpty()) {
      insertPos = block.position() + block.length();
      block = block.next();
      continue;
    }
    break;
  }

  const int maxPos = std::max(0, doc->characterCount() - 1);
  insertPos = std::clamp(insertPos, 0, maxPos);

  QStringList includeLines;
  includeLines.reserve(headersToInsert.size());
  for (const QString& header : headersToInsert) {
    includeLines << QStringLiteral("#include <%1>").arg(header);
  }
  QString snippet = includeLines.join(QLatin1Char('\n'));
  snippet += QLatin1Char('\n');

  QTextCursor cursor(doc);
  cursor.beginEditBlock();
  cursor.setPosition(insertPos);
  if (insertPos > 0) {
    const QChar before = doc->characterAt(insertPos - 1);
    if (before != QLatin1Char('\n') &&
        before != QChar::ParagraphSeparator) {
      cursor.insertText(QStringLiteral("\n"));
    }
  }
  cursor.insertText(snippet);
  cursor.endEditBlock();

  plain->setFocus(Qt::OtherFocusReason);
  showToast(tr("Inserted %1 include(s) from %2.")
                .arg(headersToInsert.size())
                .arg(libraryName));
}

void MainWindow::clearPendingUploadFlow() {
  pendingUploadFlow_ = PendingUploadFlow{};
  if (uploadBuildDir_) {
    uploadBuildDir_->remove();
    delete uploadBuildDir_;
    uploadBuildDir_ = nullptr;
  }
}
bool MainWindow::startUploadFromPendingFlow() {
  if (!arduinoCli_ || arduinoCli_->isRunning()) {
    return false;
  }

  const QString sketchFolder = pendingUploadFlow_.sketchFolder.trimmed();
  const QString fqbn = pendingUploadFlow_.fqbn.trimmed();
  const QString port = pendingUploadFlow_.port.trimmed();
  const QString protocol = pendingUploadFlow_.protocol.trimmed();
  const QString programmer = pendingUploadFlow_.programmer.trimmed();
  const QString buildPath = pendingUploadFlow_.buildPath.trimmed();

  const bool hasProgrammer = !programmer.isEmpty();
  const bool allowMissingPort =
      pendingUploadFlow_.allowMissingPort && !hasProgrammer;
  if (sketchFolder.isEmpty() || fqbn.isEmpty() ||
      (!hasProgrammer && port.isEmpty() && !allowMissingPort)) {
    return false;
  }

  QStringList args = {"upload", "--fqbn", fqbn};
  if (!port.isEmpty()) {
    args << "--port" << port;
    if (!protocol.isEmpty()) {
      args << "--protocol" << protocol;
    }
  }

  if (hasProgrammer) {
    args << "--programmer" << programmer;
  }

  if (pendingUploadFlow_.verboseUpload) {
    args << "--verbose";
  }

  if (pendingUploadFlow_.useInputDir &&
      !buildPath.isEmpty() &&
      QDir(buildPath).exists()) {
    args << "--input-dir" << buildPath;
  }

  args << sketchFolder;

  lastCliJobKind_ = pendingUploadFlow_.finalJobKind;
  arduinoCli_->run(args);
  return true;
}

void MainWindow::updateUploadActionStates() {
  if (!actionJustUpload_) {
    return;
  }
  QString reason;
  const bool canUpload = canUploadWithoutCompile(&reason);
  const bool busy = arduinoCli_ && arduinoCli_->isRunning();
  actionJustUpload_->setEnabled(canUpload && !busy);
  if (canUpload) {
    actionJustUpload_->setStatusTip(tr("Upload prebuilt binary without compiling"));
    actionJustUpload_->setToolTip(tr("Upload prebuilt binary"));
  } else {
    actionJustUpload_->setStatusTip(reason);
    actionJustUpload_->setToolTip(reason.isEmpty() ? tr("Upload prebuilt binary")
                                                   : reason);
  }
}

void MainWindow::rememberSuccessfulCompileArtifact(const QString& sketchFolder,
                                                   const QString& fqbn,
                                                   const QString& buildPath) {
  const QString normalizedSketch = normalizeSketchFolderPath(sketchFolder);
  const QString normalizedFqbn = fqbn.trimmed();
  const QString normalizedBuildPath = buildPath.trimmed();
  if (normalizedSketch.isEmpty() || normalizedFqbn.isEmpty() ||
      normalizedBuildPath.isEmpty()) {
    return;
  }

  lastSuccessfulCompile_.sketchFolder = normalizedSketch;
  lastSuccessfulCompile_.fqbn = normalizedFqbn;
  lastSuccessfulCompile_.buildPath = QDir(normalizedBuildPath).absolutePath();
  lastSuccessfulCompile_.sketchSignature =
      computeSketchSignature(normalizedSketch);
  lastSuccessfulCompile_.sketchChangedSinceCompile =
      lastSuccessfulCompile_.sketchSignature.isEmpty();
}

void MainWindow::markSketchAsChanged(const QString& filePath) {
  if (lastSuccessfulCompile_.sketchFolder.trimmed().isEmpty()) {
    return;
  }
  const QString changed = QFileInfo(filePath).absoluteFilePath().trimmed();
  if (changed.isEmpty()) {
    lastSuccessfulCompile_.sketchChangedSinceCompile = true;
    return;
  }
  const QString sketchRoot =
      QDir(lastSuccessfulCompile_.sketchFolder).absolutePath();
  const QString sketchPrefix = sketchRoot + QDir::separator();
  if (changed == sketchRoot || changed.startsWith(sketchPrefix)) {
    lastSuccessfulCompile_.sketchChangedSinceCompile = true;
  }
}

QString MainWindow::computeSketchSignature(const QString& sketchFolder) const {
  const QString normalizedSketch = normalizeSketchFolderPath(sketchFolder);
  if (normalizedSketch.isEmpty()) {
    return {};
  }

  QDir root(normalizedSketch);
  if (!root.exists()) {
    return {};
  }

  QStringList relativePaths;
  QDirIterator it(normalizedSketch, QDir::Files, QDirIterator::Subdirectories);
  while (it.hasNext()) {
    const QString absolute = it.next();
    const QFileInfo info(absolute);
    if (!info.exists() || !info.isFile()) {
      continue;
    }
    const QString relative = root.relativeFilePath(absolute);
    if (relative.startsWith(QStringLiteral("."))) {
      continue;
    }
    relativePaths.append(relative);
  }

  std::sort(relativePaths.begin(), relativePaths.end());
  QCryptographicHash hasher(QCryptographicHash::Sha256);
  for (const QString& relative : relativePaths) {
    const QFileInfo info(root.filePath(relative));
    if (!info.exists() || !info.isFile()) {
      continue;
    }
    QByteArray data = relative.toUtf8();
    data.append('\n');
    data.append(QByteArray::number(info.size()));
    data.append('\n');
    data.append(
        QByteArray::number(info.lastModified().toMSecsSinceEpoch()));
    data.append('\n');
    hasher.addData(data);
  }
  return QString::fromLatin1(hasher.result().toHex());
}

bool MainWindow::canUploadWithoutCompile(QString* reason) const {
  auto fail = [&](const QString& why) {
    if (reason) {
      *reason = why;
    }
    return false;
  };

  const QString sketchFolder = currentSketchFolderPath();
  const QString fqbn = currentFqbn().trimmed();
  if (sketchFolder.isEmpty()) {
    return fail(tr("Open a sketch first."));
  }
  if (fqbn.isEmpty()) {
    return fail(tr("Select a board first."));
  }
  if (lastSuccessfulCompile_.sketchFolder.trimmed().isEmpty() ||
      lastSuccessfulCompile_.buildPath.trimmed().isEmpty()) {
    return fail(tr("Run Verify first to build a binary."));
  }
  if (!QDir(lastSuccessfulCompile_.buildPath).exists()) {
    return fail(tr("Compiled binary is no longer available. Verify again."));
  }
  if (QDir(sketchFolder).absolutePath() !=
      QDir(lastSuccessfulCompile_.sketchFolder).absolutePath()) {
    return fail(tr("Current sketch differs from the last verified sketch."));
  }
  if (fqbn != lastSuccessfulCompile_.fqbn.trimmed()) {
    return fail(tr("Selected board differs from the last verified build."));
  }
  if (editor_ && editor_->hasUnsavedChanges()) {
    return fail(tr("Unsaved changes detected. Save and verify again."));
  }
  if (lastSuccessfulCompile_.sketchChangedSinceCompile) {
    return fail(tr("Sketch changed since last successful verify."));
  }
  const QString currentSignature = computeSketchSignature(sketchFolder);
  if (currentSignature.isEmpty() ||
      currentSignature != lastSuccessfulCompile_.sketchSignature) {
    return fail(tr("Sketch files changed since last successful verify."));
  }
  if (reason) {
    reason->clear();
  }
  return true;
}

void MainWindow::beginCliProgress(CliJobKind job) {
  if (!cliBusy_ || !cliBusyLabel_) {
    return;
  }
  currentCliPhaseText_ = cliJobLabel(job);
  if (currentCliPhaseText_.isEmpty()) {
    currentCliPhaseText_ = tr("CLI");
  }
  cliBusy_->setRange(0, 100);
  setCliProgressValue(6, tr("%1 · preparing").arg(currentCliPhaseText_));
  cliBusyLabel_->show();
  cliBusy_->show();
}

void MainWindow::updateCliProgressFromOutputLine(const QString& line) {
  if (!cliBusy_ || !cliBusyLabel_ || line.trimmed().isEmpty()) {
    return;
  }

  const CliJobKind job = lastCliJobKind_;
  if (job == CliJobKind::None) {
    return;
  }

  const QString base =
      currentCliPhaseText_.isEmpty() ? cliJobLabel(job) : currentCliPhaseText_;
  const QString lower = line.trimmed().toLower();
  auto setPhase = [this, &base](int value, const QString& phase) {
    setCliProgressValue(value, tr("%1 · %2").arg(base, phase));
  };

  if (job == CliJobKind::Compile || job == CliJobKind::UploadCompile) {
    if (lower.contains("compiling sketch")) {
      setPhase(18, tr("compiling sketch"));
    } else if (lower.startsWith("compiling ") ||
               lower.contains("compiling libraries")) {
      setPhase(38, tr("compiling libraries"));
    } else if (lower.contains("archiving")) {
      setPhase(58, tr("archiving"));
    } else if (lower.contains("linking")) {
      setPhase(76, tr("linking"));
    } else if (lower.startsWith("sketch uses") ||
               lower.startsWith("global variables")) {
      setPhase(92, tr("finalizing"));
    }
    return;
  }

  if (job == CliJobKind::Upload || job == CliJobKind::UploadUsingProgrammer ||
      job == CliJobKind::BurnBootloader) {
    if (lower.startsWith("waiting for upload port")) {
      setPhase(28, tr("waiting for port"));
      return;
    }
    if (lower.startsWith("uploading")) {
      setPhase(45, tr("uploading"));
      return;
    }
    static const QRegularExpression percentPattern(
        QStringLiteral("(\\d{1,3})%"));
    const QRegularExpressionMatch match = percentPattern.match(lower);
    if (match.hasMatch()) {
      const int rawPercent =
          qBound(0, match.captured(1).toInt(), 100);
      const int mapped = 45 + static_cast<int>(rawPercent * 0.5);
      setPhase(mapped, tr("flashing"));
      return;
    }
    if (lower.contains("hash of data verified") ||
        lower.contains("verifying")) {
      setPhase(96, tr("verifying"));
      return;
    }
    if (lower.contains("hard resetting") || lower.contains("success")) {
      setPhase(100, tr("finalizing"));
    }
  }
}

void MainWindow::finishCliProgress(bool success, bool cancelled) {
  if (!cliBusy_ || !cliBusyLabel_) {
    return;
  }
  const QString base =
      currentCliPhaseText_.isEmpty() ? tr("CLI") : currentCliPhaseText_;
  if (cancelled) {
    setCliProgressValue(0, tr("%1 · cancelled").arg(base));
  } else if (success) {
    setCliProgressValue(100, tr("%1 · done").arg(base));
  } else {
    setCliProgressValue(qMax(cliBusy_->value(), 12),
                        tr("%1 · failed").arg(base));
  }

  currentCliPhaseText_.clear();
  QTimer::singleShot(1200, this, [this] {
    if (arduinoCli_ && arduinoCli_->isRunning()) {
      return;
    }
    if (cliBusy_) {
      cliBusy_->hide();
    }
    if (cliBusyLabel_) {
      cliBusyLabel_->hide();
    }
  });
}

void MainWindow::setCliProgressValue(int value, const QString& phaseText) {
  if (!cliBusy_) {
    return;
  }
  const int bounded = qBound(0, value, 100);
  if (bounded >= cliBusy_->value() || bounded <= 6) {
    cliBusy_->setValue(bounded);
  }
  if (cliBusyLabel_ && !phaseText.trimmed().isEmpty()) {
    cliBusyLabel_->setText(phaseText);
  }
}

QString MainWindow::cliJobLabel(CliJobKind job) const {
  switch (job) {
    case CliJobKind::Compile:
      return tr("Verify");
    case CliJobKind::UploadCompile:
      return tr("Verify and Upload");
    case CliJobKind::Upload:
      return tr("Upload");
    case CliJobKind::UploadUsingProgrammer:
      return tr("Upload with Programmer");
    case CliJobKind::BurnBootloader:
      return tr("Burn Bootloader");
    case CliJobKind::IndexUpdate:
      return tr("Updating Index");
    case CliJobKind::DebugCheck:
      return tr("Debug Check");
    case CliJobKind::LibraryInstall:
      return tr("Library Install");
    default:
      break;
  }
  return {};
}

void MainWindow::updateStopActionState() {
  const bool busy = arduinoCli_ && arduinoCli_->isRunning();
  if (actionStop_) {
    actionStop_->setEnabled(busy);
    if (buildToolBar_) {
      if (QWidget* w = buildToolBar_->widgetForAction(actionStop_)) {
        w->setVisible(busy);
      }
    }
  }
  updateUploadActionStates();
}

void MainWindow::scheduleOutlineRefresh() {
  if (outlineRefreshTimer_) {
    outlineRefreshTimer_->start();
  }
}

void MainWindow::refreshOutline() {
  if (!lsp_ || !lsp_->isReady() || !editor_) return;
  const QString path = editor_->currentFilePath();
  if (path.isEmpty()) return;

  lsp_->request("textDocument/documentSymbol", 
    QJsonObject{{"textDocument", QJsonObject{{"uri", toFileUri(path)}}}},
    [this](const QJsonValue& result, const QJsonObject&) {
      Q_UNUSED(result);
      if (outlineModel_) {
        outlineModel_->clear();
        // Parsing logic for symbols would go here for full parity
      }
    });
}

void MainWindow::scheduleRestartLanguageServer() {
  if (lspRestartTimer_) {
    lspRestartTimer_->start();
  }
}

void MainWindow::restartLanguageServer() {
  stopLanguageServer();
  if (!lsp_ || !arduinoCli_ || arduinoCli_->isRunning()) {
    return;
  }

  const QString sketchFolder = currentSketchFolderPath();
  if (sketchFolder.trimmed().isEmpty()) {
    return;
  }

  auto resolveExecutable = [this](const QStringList& names) {
    const QString appDir = QCoreApplication::applicationDirPath();
    for (const QString& name : names) {
      const QString trimmed = name.trimmed();
      if (trimmed.isEmpty()) {
        continue;
      }

      QFileInfo direct(trimmed);
      if (direct.isFile() && direct.isExecutable()) {
        return direct.absoluteFilePath();
      }

      const QString fromPath = findExecutable(trimmed);
      if (!fromPath.isEmpty()) {
        return fromPath;
      }

      const QString bundled = QDir(appDir).absoluteFilePath(trimmed);
      QFileInfo bundledInfo(bundled);
      if (bundledInfo.isFile() && bundledInfo.isExecutable()) {
        return bundledInfo.absoluteFilePath();
      }
    }
    return QString{};
  };

  QString cliPath = arduinoCli_->arduinoCliPath().trimmed();
  if (!cliPath.isEmpty() && QDir::isRelativePath(cliPath)) {
    const QString resolved = findExecutable(cliPath);
    if (!resolved.isEmpty()) {
      cliPath = resolved;
    }
  }
  const QString cliConfig = arduinoCli_->arduinoCliConfigPath().trimmed();
  const QString fqbn = currentFqbn().trimmed();
  const QString rootUri = toFileUri(sketchFolder);

  const QString alsPath =
      resolveExecutable({QStringLiteral("arduino-language-server")});
  const QString clangdPath = resolveExecutable(
      {QStringLiteral("clangd"), QStringLiteral("clangd-18"),
       QStringLiteral("clangd-17"), QStringLiteral("clangd-16")});

  if (!alsPath.isEmpty() &&
      !clangdPath.isEmpty() &&
      !cliPath.isEmpty() &&
      !fqbn.isEmpty()) {
    QStringList args = {
        QStringLiteral("-clangd"),
        clangdPath,
        QStringLiteral("-cli"),
        cliPath,
        QStringLiteral("-fqbn"),
        fqbn,
    };
    if (!cliConfig.isEmpty()) {
      args << QStringLiteral("-cli-config") << cliConfig;
    }
    lsp_->start(alsPath, args, rootUri);
    if (output_) {
      output_->appendLine(tr("[LSP] Starting arduino-language-server (%1)")
                              .arg(fqbn));
    }
    lspUnavailableNoticeShown_ = false;
    return;
  }

  if (!clangdPath.isEmpty()) {
    const QStringList args = {
        QStringLiteral("--background-index"),
        QStringLiteral("--header-insertion=never"),
        QStringLiteral("--completion-style=detailed"),
    };
    lsp_->start(clangdPath, args, rootUri);
    if (output_) {
      output_->appendLine(tr("[LSP] Starting clangd fallback."));
    }
    lspUnavailableNoticeShown_ = false;
    return;
  }

  if (!lspUnavailableNoticeShown_) {
    lspUnavailableNoticeShown_ = true;
    if (output_) {
      output_->appendLine(
          tr("[LSP] No supported language server executable found. "
             "Install `clangd` or `arduino-language-server` to enable code intelligence."));
    }
    showToast(tr("Code intelligence disabled: no language server found."));
  }
}

void MainWindow::stopLanguageServer() {
  if (lsp_) lsp_->stop();
}

QString MainWindow::normalizeSketchFolderPath(const QString& path) const {
  const QString trimmed = path.trimmed();
  if (trimmed.isEmpty()) {
    return {};
  }

  const QFileInfo info(trimmed);
  const QString folder =
      info.isDir() ? info.absoluteFilePath() : info.absolutePath();
  if (folder.trimmed().isEmpty()) {
    return {};
  }
  if (!SketchManager::isSketchFolder(folder)) {
    return {};
  }
  return QDir(folder).absolutePath();
}

QString MainWindow::currentSketchFolderPath() const {
  if (sketchManager_) {
    const QString fromManager =
        normalizeSketchFolderPath(sketchManager_->lastSketchPath());
    if (!fromManager.isEmpty()) {
      return fromManager;
    }
  }
  if (editor_) {
    const QString fromEditor = normalizeSketchFolderPath(editor_->currentFilePath());
    if (!fromEditor.isEmpty()) {
      return fromEditor;
    }
  }
  return {};
}

void MainWindow::migrateSketchListsToFolders() {
  auto normalizeList = [this](QStringList items, int maxItems) {
    QStringList out;
    out.reserve(items.size());
    for (const QString& s : items) {
      const QString normalized = normalizeSketchFolderPath(s);
      if (normalized.isEmpty()) {
        continue;
      }
      if (!out.contains(normalized)) {
        out << normalized;
      }
      if (maxItems > 0 && out.size() >= maxItems) {
        break;
      }
    }
    return out;
  };

  const QStringList recent = recentSketches();
  const QStringList pinned = pinnedSketches();

  const QStringList normalizedRecent = normalizeList(recent, 20);
  const QStringList normalizedPinned = normalizeList(pinned, -1);

  if (normalizedRecent != recent) {
    setRecentSketches(normalizedRecent);
  }
  if (normalizedPinned != pinned) {
    setPinnedSketches(normalizedPinned);
  }
}

bool MainWindow::openSketchFolderInUi(const QString& folder) {
  const QString sketchFolder = normalizeSketchFolderPath(folder);
  if (sketchFolder.isEmpty()) {
    QMessageBox::warning(
        this, tr("Not a Valid Sketch"),
        tr("The selected folder does not contain a valid Arduino sketch."));
    return false;
  }

  const QString currentFolder = currentSketchFolderPath();
  const bool hasOpenFiles = editor_ && !editor_->openedFiles().isEmpty();
  const bool switchingSketch =
      hasOpenFiles && (currentFolder.isEmpty() ||
                       QDir::cleanPath(currentFolder) != QDir::cleanPath(sketchFolder));

  if (switchingSketch && editor_) {
    if (!editor_->closeAllTabs()) {
      return false;
    }
  }

  if (welcome_ && welcome_->isVisible()) {
    welcome_->hide();
    if (editor_) {
      editor_->show();
    }
  }

  if (sketchManager_) {
    sketchManager_->openSketchFolder(sketchFolder);
  }

  if (fileModel_ && fileTree_) {
    fileModel_->setRootPath(sketchFolder);
    const QModelIndex root = fileModel_->index(sketchFolder);
    fileTree_->setRootIndex(root);
    fileTree_->expand(root);
  }

  if (editor_) {
    editor_->setDefaultSaveDirectory(sketchFolder);
  }
  applyPreferredFqbnForSketch(sketchFolder);

  addRecentSketch(sketchFolder);

  if (editor_ && (switchingSketch || editor_->openedFiles().isEmpty())) {
    auto primaryInoForFolder = [](const QString& folderPath) -> QString {
      const QDir dir(folderPath);
      if (!dir.exists()) {
        return {};
      }

      const QString baseName = QFileInfo(folderPath).fileName();
      if (!baseName.isEmpty()) {
        const QString primary = dir.absoluteFilePath(baseName + ".ino");
        if (QFileInfo(primary).isFile()) {
          return QFileInfo(primary).absoluteFilePath();
        }
      }

      const QStringList inos = dir.entryList(QStringList{"*.ino", "*.pde"}, QDir::Files,
                                            QDir::Name | QDir::IgnoreCase);
      if (!inos.isEmpty()) {
        return QFileInfo(dir.absoluteFilePath(inos.first())).absoluteFilePath();
      }
      return {};
    };

    const QString primaryIno = primaryInoForFolder(sketchFolder);

    QDir dir(sketchFolder);
    const QStringList patterns = {
        "*.ino", "*.pde", "*.h",  "*.hpp", "*.hh", "*.hxx",
        "*.c",   "*.cpp", "*.cc", "*.cxx", "*.S",  "*.s",
        "*.asm",
    };

    QFileInfoList entries =
        dir.entryInfoList(patterns, QDir::Files, QDir::Name | QDir::IgnoreCase);

    QStringList files;
    files.reserve(entries.size());
    for (const QFileInfo& fi : entries) {
      const QString abs = fi.absoluteFilePath();
      if (!abs.isEmpty()) {
        files << abs;
      }
    }

    if (!primaryIno.isEmpty()) {
      files.removeAll(primaryIno);
      files.prepend(primaryIno);
    }

    for (const QString& filePath : files) {
      (void)editor_->openFile(filePath);
    }
  }

  if (actionPinSketch_) {
    const QSignalBlocker blocker(actionPinSketch_);
    actionPinSketch_->setChecked(isSketchPinned(sketchFolder));
  }

  scheduleRestartLanguageServer();
  scheduleOutlineRefresh();
  updateUploadActionStates();
  updateWelcomeVisibility();
  return true;
}

void MainWindow::updateWelcomePage() {
  if (welcome_) {
    const QStringList pinned = pinnedSketches();
    const QStringList recent = recentSketches();
    welcome_->setPinnedSketches(pinned);
    welcome_->setRecentSketches(recent);
  }
}

void MainWindow::updateWelcomeVisibility() {
  if (centralStack_ && welcome_ && editor_) {
    bool hasEditors = editor_->openedFiles().size() > 0;
    centralStack_->setCurrentWidget(hasEditors ? (QWidget*)editor_ : (QWidget*)welcome_);
  }
}

QString MainWindow::currentFqbn() const {
  QString fqbn = boardCombo_ ? boardCombo_->currentData().toString() : QString{};
  if (fqbn.isEmpty()) return fqbn;

  // Ensure we have the base FQBN
  QString baseFqbn = fqbn;
  if (baseFqbn.contains(':')) {
      QStringList parts = baseFqbn.split(':');
      if (parts.size() > 3) {
          baseFqbn = parts.mid(0, 3).join(':');
      }
  }

  // Append options from settings
  QSettings settings;
  settings.beginGroup("BoardOptions");
  settings.beginGroup(baseFqbn);
  QStringList optionKeys = settings.allKeys();
  if (!optionKeys.isEmpty()) {
      QStringList kvPairs;
      for (const QString& key : optionKeys) {
          QString val = settings.value(key).toString();
          if (!val.isEmpty()) {
              kvPairs << QString("%1=%2").arg(key, val);
          }
      }
      if (!kvPairs.isEmpty()) {
          fqbn = baseFqbn + ":" + kvPairs.join(",");
      }
  }
  settings.endGroup();
  settings.endGroup();

  return fqbn;
}

QString MainWindow::sketchSelectionKey(const QString& sketchFolder) const {
  const QString normalized = normalizeSketchFolderPath(sketchFolder);
  if (normalized.isEmpty()) {
    return {};
  }
  const QByteArray encoded = normalized.toUtf8().toBase64(
      QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
  return QString::fromLatin1(encoded);
}

QString MainWindow::preferredFqbnForSketch(const QString& sketchFolder) const {
  const QString key = sketchSelectionKey(sketchFolder);
  if (key.isEmpty()) {
    return {};
  }

  QSettings settings;
  settings.beginGroup(kSettingsGroup);
  const QVariantMap map = settings.value(kSketchBoardSelectionsKey).toMap();
  settings.endGroup();
  return map.value(key).toString().trimmed();
}

void MainWindow::storeFqbnForCurrentSketch(const QString& fqbn) {
  const QString sketchFolder = currentSketchFolderPath();
  const QString key = sketchSelectionKey(sketchFolder);
  if (key.isEmpty()) {
    return;
  }

  QSettings settings;
  settings.beginGroup(kSettingsGroup);
  QVariantMap map = settings.value(kSketchBoardSelectionsKey).toMap();
  const QString trimmed = fqbn.trimmed();
  if (trimmed.isEmpty()) {
    map.remove(key);
  } else {
    map.insert(key, trimmed);
  }
  settings.setValue(kSketchBoardSelectionsKey, map);
  settings.endGroup();
}

void MainWindow::applyPreferredFqbnForSketch(const QString& sketchFolder) {
  const QString preferred = preferredFqbnForSketch(sketchFolder);
  if (preferred.isEmpty()) {
    return;
  }

  if (boardCombo_) {
    const int index = boardCombo_->findData(preferred);
    if (index >= 0) {
      boardCombo_->setCurrentIndex(index);
      return;
    }
  }

  QSettings settings;
  settings.beginGroup(kSettingsGroup);
  settings.setValue(kFqbnKey, preferred);
  settings.endGroup();
}

QString MainWindow::currentProgrammer() const {
  QSettings settings;
  settings.beginGroup(kSettingsGroup);
  const QString programmer = settings.value(kProgrammerKey).toString().trimmed();
  settings.endGroup();
  return programmer;
}

QString MainWindow::currentPort() const {
  return portCombo_ ? portCombo_->currentData().toString() : QString{};
}

QString MainWindow::currentPortProtocol() const {
  const QString port = currentPort().trimmed();
  if (port.isEmpty() || !portCombo_) {
    return {};
  }
  const int idx = portCombo_->currentIndex();
  if (idx < 0) {
    return {};
  }
  QString protocol = portCombo_->itemData(idx, kPortRoleProtocol).toString().trimmed();
  if (protocol.isEmpty()) {
    protocol = QStringLiteral("serial");
  }
  return protocol;
}

bool MainWindow::currentPortIsMissing() const {
  if (!portCombo_) {
    return false;
  }
  const int idx = portCombo_->currentIndex();
  if (idx < 0) {
    return false;
  }
  return portCombo_->itemData(idx, kPortRoleMissing).toBool();
}

bool MainWindow::isPortOptionalForFqbn(const QString& fqbn) const {
  const QString lower = fqbn.trimmed().toLower();
  if (lower.isEmpty()) {
    return false;
  }
  return lower.contains(QStringLiteral("rp2040")) ||
         lower.contains(QStringLiteral("rp2350"));
}

QString MainWindow::findUf2ArtifactPath(const QString& buildPath,
                                        const QString& sketchFolder) const {
  const QString normalizedBuildPath = buildPath.trimmed();
  if (normalizedBuildPath.isEmpty()) {
    return {};
  }
  QDir buildDir(normalizedBuildPath);
  if (!buildDir.exists()) {
    return {};
  }

  const QString sketchName =
      QFileInfo(sketchFolder.trimmed()).fileName().trimmed().toLower();
  QString firstFound;
  QDirIterator it(normalizedBuildPath, QStringList{QStringLiteral("*.uf2")},
                  QDir::Files, QDirIterator::Subdirectories);
  while (it.hasNext()) {
    const QString candidate = it.next();
    if (firstFound.isEmpty()) {
      firstFound = candidate;
    }
    if (sketchName.isEmpty()) {
      continue;
    }
    const QString fileName = QFileInfo(candidate).fileName().toLower();
    if (fileName.startsWith(sketchName + QStringLiteral(".")) ||
        fileName.startsWith(sketchName + QStringLiteral("_")) ||
        fileName.startsWith(sketchName + QStringLiteral("-"))) {
      return candidate;
    }
  }
  return firstFound;
}

QStringList MainWindow::detectUf2MountPoints() const {
  QStringList mountPoints;
  QSet<QString> seen;

  auto addMountPoint = [&mountPoints, &seen](const QString& path) {
    const QString normalized = QDir(path).absolutePath();
    if (normalized.isEmpty() || seen.contains(normalized)) {
      return;
    }
    const QFileInfo info(normalized);
    if (!info.isDir() || !info.isWritable()) {
      return;
    }
    seen.insert(normalized);
    mountPoints.push_back(normalized);
  };

  const QList<QStorageInfo> volumes = QStorageInfo::mountedVolumes();
  for (const QStorageInfo& volume : volumes) {
    if (!volume.isValid() || !volume.isReady() || volume.isReadOnly()) {
      continue;
    }
    const QString rootPath = volume.rootPath();
    if (rootPath.trimmed().isEmpty()) {
      continue;
    }
    const QString baseName = QFileInfo(rootPath).fileName();
    if (isLikelyUf2VolumeName(baseName) ||
        isLikelyUf2VolumeName(volume.displayName()) ||
        isLikelyUf2VolumeName(volume.name())) {
      addMountPoint(rootPath);
    }
  }

  QStringList candidateRoots;
  const QString user = qEnvironmentVariable("USER").trimmed();
  if (!user.isEmpty()) {
    candidateRoots << QStringLiteral("/media/%1").arg(user)
                   << QStringLiteral("/run/media/%1").arg(user);
  }
  candidateRoots << QStringLiteral("/Volumes");

  for (const QString& root : candidateRoots) {
    QDir dir(root);
    if (!dir.exists()) {
      continue;
    }
    const QFileInfoList children = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& child : children) {
      if (isLikelyUf2VolumeName(child.fileName())) {
        addMountPoint(child.absoluteFilePath());
      }
    }
  }

  return mountPoints;
}

bool MainWindow::copyUf2ArtifactToMountPoint(const QString& uf2Path,
                                             const QString& mountPoint,
                                             QString* outError) const {
  const QFileInfo sourceInfo(uf2Path);
  if (!sourceInfo.isFile()) {
    if (outError) {
      *outError = tr("UF2 artifact not found: %1").arg(uf2Path);
    }
    return false;
  }

  const QDir mountDir(mountPoint);
  if (!mountDir.exists()) {
    if (outError) {
      *outError = tr("UF2 target volume is not available: %1").arg(mountPoint);
    }
    return false;
  }

  const QString targetPath = mountDir.absoluteFilePath(sourceInfo.fileName());
  if (QFile::exists(targetPath) && !QFile::remove(targetPath)) {
    if (outError) {
      *outError = tr("Could not overwrite existing file on UF2 volume: %1")
                      .arg(targetPath);
    }
    return false;
  }
  if (!QFile::copy(sourceInfo.absoluteFilePath(), targetPath)) {
    if (outError) {
      *outError = tr("Could not copy UF2 artifact to volume: %1").arg(targetPath);
    }
    return false;
  }

  if (outError) {
    outError->clear();
  }
  return true;
}

bool MainWindow::tryUf2UploadFallback(const QString& cliOutput) {
  if (pendingUploadFlow_.uf2FallbackAttempted) {
    return false;
  }

  const QString fqbn = pendingUploadFlow_.fqbn.trimmed();
  const QString outputLower = cliOutput.toLower();
  const bool likelyUf2Board = isPortOptionalForFqbn(fqbn);
  const bool outputSuggestsUf2 =
      outputLower.contains(QStringLiteral("uf2")) ||
      outputLower.contains(QStringLiteral("rpi-rp2")) ||
      outputLower.contains(QStringLiteral("mass storage"));
  if (!likelyUf2Board && !outputSuggestsUf2) {
    return false;
  }

  const QString uf2Path = findUf2ArtifactPath(pendingUploadFlow_.buildPath,
                                              pendingUploadFlow_.sketchFolder);
  if (uf2Path.isEmpty()) {
    return false;
  }

  pendingUploadFlow_.uf2FallbackAttempted = true;
  if (output_) {
    output_->appendHtml(QString("<span style=\"color:#fbc02d;\"><b>%1</b></span>")
                            .arg(tr("Switching to UF2 drag-and-drop upload flow…")));
  }

  const QStringList mounts = detectUf2MountPoints();
  if (mounts.size() == 1) {
    QString copyError;
    if (copyUf2ArtifactToMountPoint(uf2Path, mounts.first(), &copyError)) {
      if (output_) {
        output_->appendHtml(QString("<span style=\"color:#388e3c;\"><b>%1</b></span>")
                                .arg(tr("UF2 upload complete: copied %1 to %2")
                                         .arg(QFileInfo(uf2Path).fileName(), mounts.first())));
      }
      showToast(tr("UF2 upload completed"));
      return true;
    }
    if (output_) {
      output_->appendHtml(QString("<span style=\"color:#d32f2f;\"><b>%1</b></span>")
                              .arg(copyError));
    }
  } else if (mounts.size() > 1) {
    if (output_) {
      output_->appendLine(tr("Multiple UF2 volumes detected:"));
      for (const QString& mount : mounts) {
        output_->appendLine(QStringLiteral("  - %1").arg(mount));
      }
    }
  } else if (output_) {
    output_->appendLine(
        tr("No UF2 volume detected. Put the board in BOOT mode (RPI-RP2) and copy the UF2 file manually."));
  }

  if (output_) {
    output_->appendLine(tr("UF2 artifact ready: %1").arg(uf2Path));
  }
  showToastWithAction(
      tr("UF2 file ready for manual upload"),
      tr("Open Folder"),
      [uf2Path] {
        QDesktopServices::openUrl(
            QUrl::fromLocalFile(QFileInfo(uf2Path).absolutePath()));
      });
  return true;
}

QString MainWindow::toFileUri(const QString& filePath) {
  return QUrl::fromLocalFile(filePath).toString();
}

QString MainWindow::findExecutable(const QString& name) {
  return QStandardPaths::findExecutable(name.trimmed());
}

void MainWindow::showExamplesDialog(QString initialFilter) {
  if (!arduinoCli_) {
    return;
  }

  auto* dialog = new ExamplesDialog(this);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->setWindowModality(Qt::ApplicationModal);
  dialog->setMinimumSize(800, 600);

  if (!initialFilter.isEmpty()) {
    dialog->setFilterText(initialFilter);
  }

  dialog->show();
}

void MainWindow::showSelectBoardDialog() {
  if (!arduinoCli_) {
    QMessageBox::warning(this, tr("Arduino CLI Not Available"),
                         tr("Arduino CLI is not configured. Please set up Arduino CLI in preferences."));
    return;
  }

  if (boardSelectorDialogOpen_) {
    return;
  }
  boardSelectorDialogOpen_ = true;

  // Show a dialog that lists all available boards and allows selection
  auto* dialog = new BoardSelectorDialog(this);
  dialog->setWindowTitle(tr("Select Board"));
  dialog->setMinimumSize(700, 500);

  // Fetch all available boards
  QProcess* p = new QProcess(this);
  connect(p, &QProcess::finished, this, [this, dialog, p](int exitCode, QProcess::ExitStatus) {
    if (!boardSelectorDialogOpen_) {
      return;
    }

    if (exitCode == 0) {
      const QByteArray data = p->readAllStandardOutput();
      const QJsonDocument doc = QJsonDocument::fromJson(data);

      QVector<BoardSelectorDialog::BoardEntry> boards;
      QJsonArray arr;
      if (doc.isObject()) {
          arr = doc.object().value("boards").toArray();
      } else if (doc.isArray()) {
          arr = doc.array();
      }

      if (!arr.isEmpty()) {
        // Parse the board list
        QMap<QString, BoardSelectorDialog::BoardEntry> uniqueBoards;

        for (const QJsonValue& v : arr) {
          const QJsonObject obj = v.toObject();
          const QString name = obj.value("name").toString();
          const QString fqbn = obj.value("fqbn").toString();

          // Get the platform name if available
          QString platform;
          const QJsonObject platformObj = obj.value("platform").toObject();
          if (!platformObj.isEmpty()) {
              platform = platformObj.value("name").toString();
          }

          if (!name.isEmpty() && !fqbn.isEmpty()) {
            QString displayName = name;
            if (!platform.isEmpty()) {
              displayName = QString("%1 (%2)").arg(name, platform);
            }
            BoardSelectorDialog::BoardEntry entry;
            entry.name = displayName;
            entry.fqbn = fqbn;
            entry.isFavorite = isFavorite(fqbn);
            uniqueBoards[displayName] = entry;
          }
        }

        boards = uniqueBoards.values();
      }

      dialog->setBoards(boards);
      connect(dialog, &BoardSelectorDialog::favoriteToggled, this, &MainWindow::toggleFavorite);

      // Set current board if one is selected
      const QString currentFqbn = this->currentFqbn();
      if (!currentFqbn.isEmpty()) {
        dialog->setCurrentFqbn(currentFqbn);
      }

      if (dialog->exec() == QDialog::Accepted) {
        // If a board was selected, update the combo box and settings
        const QString selectedFqbn = dialog->selectedFqbn();
        if (!selectedFqbn.isEmpty() && selectedFqbn != currentFqbn) {
          // Find the board in the combo and select it
          const int index = boardCombo_->findData(selectedFqbn);
          if (index >= 0) {
            boardCombo_->setCurrentIndex(index);
          } else {
            // Add the board to the combo if not present
            // Need to find the name from the selected FQBN
            for (const auto& entry : boards) {
              if (entry.fqbn == selectedFqbn) {
                boardCombo_->addItem(entry.name, entry.fqbn);
                boardCombo_->setCurrentIndex(boardCombo_->count() - 1);
                break;
              }
            }
          }

          // Save to settings
	          QSettings settings;
	          settings.beginGroup(kSettingsGroup);
	          settings.setValue(kFqbnKey, selectedFqbn);
	          settings.endGroup();
              storeFqbnForCurrentSketch(selectedFqbn);

	          updateBoardPortIndicator();
	          showToast(tr("Board selected: %1").arg(selectedFqbn));
        }
      }
    } else {
      QMessageBox::warning(this, tr("Failed to Load Boards"),
                           tr("Could not retrieve board list from Arduino CLI."));
    }

    dialog->deleteLater();
    p->deleteLater();
    boardSelectorDialogOpen_ = false;
  });

  connect(p, &QProcess::errorOccurred, this, [this, dialog, p](QProcess::ProcessError) {
    if (!boardSelectorDialogOpen_) {
      return;
    }

    QMessageBox::warning(this, tr("Failed to Load Boards"),
                         tr("Could not start Arduino CLI to retrieve board list."));
    dialog->deleteLater();
    p->deleteLater();
    boardSelectorDialogOpen_ = false;
  });

  const QStringList args =
      arduinoCli_->withGlobalFlags({"board", "listall", "--format", "json"});
  p->start(arduinoCli_->arduinoCliPath(), args);
}

// === File Menu Actions ===
void MainWindow::newSketch() {
  const QString sketchbookDir = defaultSketchbookDir();
  QDir().mkpath(sketchbookDir);

  // Generate timestamp-based sketch name with collision-safe fallback.
  const QString baseName = QStringLiteral("sketch_%1")
                               .arg(QDateTime::currentDateTime().toString(
                                   QStringLiteral("yyyyMMdd_HHmmss")));
  QString sketchName;
  QString sketchPath;
  int suffix = 0;
  do {
    sketchName = suffix == 0
                     ? baseName
                     : QStringLiteral("%1_%2").arg(baseName).arg(suffix);
    sketchPath = sketchbookDir + QStringLiteral("/") + sketchName;
    ++suffix;
  } while (QDir(sketchPath).exists());

  // Create sketch folder and main .ino file
  QDir().mkpath(sketchPath);
  const QString inoPath = sketchPath + "/" + sketchName + ".ino";

  QFile file(inoPath);
  if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    file.write((QString(
      "/*\n"
      "  Sketch: %1\n"
      "  \n"
      "  Created: %2\n"
      "  \n"
      "  This is a sample sketch provided with the Rewritto-ide.\n"
      "*/\n\n"
      "void setup() {\n"
      "  // put your setup code here, to run once:\n"
      "  \n"
      "}\n\n"
      "void loop() {\n"
      "  // put your main code here, to run repeatedly:\n"
      "  \n"
      "}\n"
    ).arg(sketchName).arg(QDateTime::currentDateTime().toString(Qt::ISODate))).toUtf8());
    file.close();
  }

  (void)openSketchFolderInUi(sketchPath);
}

void MainWindow::openSketch() {
  const QString path = QFileDialog::getOpenFileName(
      this,
      tr("Open Sketch"),
      defaultSketchbookDir(),
      tr("Arduino Sketch (*.ino);;All Files (*)"));

  if (path.trimmed().isEmpty()) {
    return;
  }

  const QFileInfo info(path);
  if (!info.exists() || !info.isFile()) {
    QMessageBox::warning(this, tr("File Not Found"),
                         tr("The selected file does not exist."));
    return;
  }

  (void)openSketchFolderInUi(info.absolutePath());
}

void MainWindow::openSketchFolder() {
  const QString dir = QFileDialog::getExistingDirectory(
      this,
      tr("Open Sketch Folder"),
      defaultSketchbookDir());

  if (dir.trimmed().isEmpty()) {
    return;
  }

  (void)openSketchFolderInUi(dir);
}

void MainWindow::exportProjectZip() {
  const QString sketchFolder = currentSketchFolderPath().trimmed();
  if (sketchFolder.isEmpty()) {
    QMessageBox::warning(this, tr("Export Project ZIP"),
                         tr("Please open or create a sketch first."));
    return;
  }

  if (arduinoCli_ && arduinoCli_->isRunning()) {
    QMessageBox::information(
        this, tr("Export Project ZIP"),
        tr("Please wait for the current Arduino CLI operation to finish."));
    return;
  }

  if (editor_) {
    editor_->saveAll();
  }

  const QString sketchName = QFileInfo(sketchFolder).fileName().trimmed().isEmpty()
                                 ? QStringLiteral("sketch")
                                 : QFileInfo(sketchFolder).fileName().trimmed();
  const QString defaultZipPath = QDir(QDir::homePath())
                                     .absoluteFilePath(
                                         QStringLiteral("%1-project-%2.zip")
                                             .arg(sketchName,
                                                  QDateTime::currentDateTime().toString(
                                                      QStringLiteral("yyyyMMdd-HHmmss"))));
  const QString zipPath = QFileDialog::getSaveFileName(
      this,
      tr("Export Project ZIP"),
      defaultZipPath,
      tr("ZIP Files (*.zip);;All Files (*)"));
  if (zipPath.trimmed().isEmpty()) {
    return;
  }

  QProgressDialog progress(tr("Exporting project bundle..."), QString(), 0, 100,
                           this);
  progress.setWindowModality(Qt::ApplicationModal);
  progress.setCancelButton(nullptr);
  progress.setMinimumDuration(0);
  progress.setAutoClose(false);
  progress.setAutoReset(false);
  auto setProgress = [&progress](int value, const QString& text) {
    progress.setValue(qBound(0, value, 100));
    progress.setLabelText(text);
    QCoreApplication::processEvents();
  };
  setProgress(2, tr("Preparing staging folder..."));

  QTemporaryDir stagingDir;
  if (!stagingDir.isValid()) {
    QMessageBox::warning(this, tr("Export Project ZIP"),
                         tr("Could not create a temporary staging folder."));
    return;
  }

  const QString bundleRoot =
      QDir(stagingDir.path()).absoluteFilePath(QStringLiteral("rewritto-project-bundle"));
  if (!QDir().mkpath(bundleRoot)) {
    QMessageBox::warning(this, tr("Export Project ZIP"),
                         tr("Could not create bundle staging folder."));
    return;
  }

  QString copyError;
  const QString sketchBundlePath =
      QDir(bundleRoot).absoluteFilePath(QStringLiteral("sketch"));
  setProgress(8, tr("Copying sketch files..."));
  if (!copyDirectoryRecursivelyMerged(sketchFolder, sketchBundlePath, &copyError)) {
    QMessageBox::warning(this, tr("Export Project ZIP"),
                         tr("Could not copy sketch files.\n\n%1").arg(copyError));
    return;
  }

  QStringList notes;
  auto appendNote = [&notes](const QString& text) {
    const QString trimmed = text.trimmed();
    if (!trimmed.isEmpty()) {
      notes << trimmed;
    }
  };

  QStringList additionalUrls;
  QString sketchbookDir;
  {
    QSettings settings;
    settings.beginGroup(QStringLiteral("Preferences"));
    additionalUrls = settings.value(QStringLiteral("additionalUrls")).toStringList();
    sketchbookDir = settings.value(QStringLiteral("sketchbookDir")).toString().trimmed();
    settings.endGroup();
  }
  const ArduinoCliConfigSnapshot configSnapshot = readArduinoCliConfigSnapshot(
      arduinoCli_ ? arduinoCli_->arduinoCliConfigPath() : QString{});
  if (additionalUrls.isEmpty()) {
    additionalUrls = configSnapshot.additionalUrls;
  }
  additionalUrls = normalizeStringList(additionalUrls);
  if (sketchbookDir.isEmpty()) {
    sketchbookDir = configSnapshot.userDir.trimmed();
  }
  if (sketchbookDir.isEmpty()) {
    sketchbookDir = defaultSketchbookDir();
  }

  QString arduinoDataDir = defaultArduinoDataDirPath();
  QStringList bundledCorePackages;
  QVector<InstalledCoreSnapshot> installedCores;
  QVector<InstalledLibrarySnapshot> installedLibraries;

  const QString cliPath =
      arduinoCli_ ? arduinoCli_->arduinoCliPath().trimmed() : QString{};
  setProgress(18, tr("Reading installed cores and libraries..."));
  if (!cliPath.isEmpty() && arduinoCli_) {
    const ArduinoCliDirectories dirs =
        readArduinoCliDirectories(cliPath, arduinoCli_->withGlobalFlags({}));
    if (!dirs.dataDir.trimmed().isEmpty()) {
      arduinoDataDir = dirs.dataDir.trimmed();
    }
    if (sketchbookDir.trimmed().isEmpty() && !dirs.userDir.trimmed().isEmpty()) {
      sketchbookDir = dirs.userDir.trimmed();
    }

    const CommandResult coreListResult = runCommandBlocking(
        cliPath,
        arduinoCli_->withGlobalFlags({QStringLiteral("core"),
                                      QStringLiteral("list"),
                                      QStringLiteral("--json")}),
        {}, {}, 120000);
    if (commandSucceeded(coreListResult)) {
      installedCores = parseInstalledCoresFromJson(coreListResult.stdoutText.toUtf8());
    } else {
      appendNote(
          tr("Could not read installed cores: %1")
              .arg(commandErrorSummary(coreListResult)));
    }

    const CommandResult libListResult = runCommandBlocking(
        cliPath,
        arduinoCli_->withGlobalFlags({QStringLiteral("lib"),
                                      QStringLiteral("list"),
                                      QStringLiteral("--json")}),
        {}, {}, 120000);
    if (commandSucceeded(libListResult)) {
      installedLibraries =
          parseInstalledLibrariesFromJson(libListResult.stdoutText.toUtf8());
    } else {
      appendNote(
          tr("Could not read installed libraries: %1")
              .arg(commandErrorSummary(libListResult)));
    }
  } else {
    appendNote(tr("Arduino CLI is unavailable; exporting sketch files only."));
  }

  const QString fqbn = currentFqbn().trimmed();
  const QStringList fqbnParts = fqbn.split(QLatin1Char(':'), Qt::SkipEmptyParts);
  const QString selectedCoreId =
      fqbnParts.size() >= 2
          ? QStringLiteral("%1:%2")
                .arg(fqbnParts.at(0).trimmed(), fqbnParts.at(1).trimmed())
          : QString{};
  QString selectedPackager;
  QString selectedArchitecture;
  if (fqbnParts.size() >= 2) {
    selectedPackager = fqbnParts.at(0).trimmed();
    selectedArchitecture = fqbnParts.at(1).trimmed();
  }

  InstalledCoreSnapshot selectedCore;
  bool selectedCoreFound = false;
  if (!selectedCoreId.isEmpty()) {
    for (const InstalledCoreSnapshot& core : installedCores) {
      if (core.id == selectedCoreId) {
        selectedCore = core;
        selectedCoreFound = true;
        break;
      }
    }
  }

  QString selectedCoreHardwareRelativePath;
  QVector<CoreToolDependency> selectedCoreToolDependencies;
  QStringList bundledToolPaths;
  QSet<QString> bundledToolPathsSet;

  if (selectedCoreFound && !selectedPackager.isEmpty() &&
      !selectedArchitecture.isEmpty() &&
      !selectedCore.installedVersion.trimmed().isEmpty()) {
    setProgress(32, tr("Bundling selected board core..."));
    const QString coreHardwareRelativePath =
        QStringLiteral("arduino-data/packages/%1/hardware/%2/%3")
            .arg(selectedPackager, selectedArchitecture,
                 selectedCore.installedVersion.trimmed());
    const QString coreHardwareSrc =
        QDir(arduinoDataDir).absoluteFilePath(
            QStringLiteral("packages/%1/hardware/%2/%3")
                .arg(selectedPackager, selectedArchitecture,
                     selectedCore.installedVersion.trimmed()));
    const QString coreHardwareDst =
        QDir(bundleRoot).absoluteFilePath(coreHardwareRelativePath);

    if (QFileInfo(coreHardwareSrc).isDir()) {
      if (copyDirectoryRecursivelyMerged(coreHardwareSrc, coreHardwareDst,
                                         &copyError)) {
        selectedCoreHardwareRelativePath = coreHardwareRelativePath;
        bundledCorePackages << coreHardwareRelativePath;
      } else {
        appendNote(
            tr("Could not copy selected core files: %1").arg(copyError));
      }
    } else {
      appendNote(tr("Selected core directory was not found: %1")
                     .arg(coreHardwareSrc));
    }

    setProgress(38, tr("Bundling selected core toolchain..."));
    const QString installedJsonPath =
        QDir(coreHardwareSrc).absoluteFilePath(QStringLiteral("installed.json"));
    selectedCoreToolDependencies = parseCoreToolDependenciesFromInstalledJson(
        installedJsonPath,
        selectedPackager,
        selectedArchitecture,
        selectedCore.installedVersion.trimmed());

    if (selectedCoreToolDependencies.isEmpty()) {
      appendNote(
          tr("Could not detect core tool dependencies from installed metadata."));
    }
    for (const CoreToolDependency& dep : selectedCoreToolDependencies) {
      if (dep.packager.trimmed().isEmpty() || dep.name.trimmed().isEmpty() ||
          dep.version.trimmed().isEmpty()) {
        continue;
      }
      const QString toolRelativePath =
          QStringLiteral("arduino-data/packages/%1/tools/%2/%3")
              .arg(dep.packager, dep.name, dep.version);
      if (bundledToolPathsSet.contains(toolRelativePath)) {
        continue;
      }
      const QString toolSrc = QDir(arduinoDataDir).absoluteFilePath(
          QStringLiteral("packages/%1/tools/%2/%3")
              .arg(dep.packager, dep.name, dep.version));
      const QString toolDst = QDir(bundleRoot).absoluteFilePath(toolRelativePath);
      if (QFileInfo(toolSrc).isDir()) {
        if (copyDirectoryRecursivelyMerged(toolSrc, toolDst, &copyError)) {
          bundledToolPathsSet.insert(toolRelativePath);
          bundledToolPaths << toolRelativePath;
        } else {
          appendNote(
              tr("Could not copy tool '%1@%2': %3")
                  .arg(dep.name, dep.version, copyError));
        }
      } else {
        appendNote(tr("Required tool folder not found: %1").arg(toolSrc));
      }
    }
  } else if (selectedPackager.isEmpty() || selectedArchitecture.isEmpty()) {
    appendNote(tr("No board selected; no core/toolchain was bundled."));
  } else {
    appendNote(
        tr("Selected core metadata could not be resolved; no core files were bundled."));
  }

  const QString arduinoDataBundleDir =
      QDir(bundleRoot).absoluteFilePath(QStringLiteral("arduino-data"));
  setProgress(50, tr("Copying board/library indexes..."));
  QDir().mkpath(arduinoDataBundleDir);
  const QStringList arduinoDataIndexFiles = {
      QStringLiteral("package_index.json"),
      QStringLiteral("package_index.json.sig"),
      QStringLiteral("library_index.json"),
      QStringLiteral("library_index.json.sig"),
  };
  for (const QString& fileName : arduinoDataIndexFiles) {
    const QString src = QDir(arduinoDataDir).absoluteFilePath(fileName);
    if (!QFileInfo(src).isFile()) {
      continue;
    }
    const QString dst = QDir(arduinoDataBundleDir).absoluteFilePath(fileName);
    (void)removePathIfExists(dst);
    if (!QFile::copy(src, dst)) {
      appendNote(tr("Could not copy '%1' into bundle.").arg(fileName));
    }
  }

  const QSet<QString> usedHeaders = collectSketchIncludeHeaders(sketchFolder);
  QVector<InstalledLibrarySnapshot> selectedLibraries;
  for (const InstalledLibrarySnapshot& library : installedLibraries) {
    bool matches = false;
    for (const QString& include : library.providesIncludes) {
      if (usedHeaders.contains(include) ||
          usedHeaders.contains(QFileInfo(include).fileName())) {
        matches = true;
        break;
      }
    }
    if (!matches && !library.name.isEmpty()) {
      for (const QString& header : usedHeaders) {
        if (header.startsWith(library.name, Qt::CaseInsensitive)) {
          matches = true;
          break;
        }
      }
    }
    if (matches) {
      selectedLibraries.push_back(library);
    }
  }

  QHash<QString, InstalledLibrarySnapshot> selectedLibraryMap;
  QStringList dependencyQueue;
  for (const InstalledLibrarySnapshot& lib : selectedLibraries) {
    const QString key = lib.name.trimmed().toLower();
    if (key.isEmpty() || selectedLibraryMap.contains(key)) {
      continue;
    }
    selectedLibraryMap.insert(key, lib);
    dependencyQueue << key;
  }

  auto findInstalledLibraryByName = [&installedLibraries](
                                        const QString& requestedName)
      -> InstalledLibrarySnapshot {
    const QString wanted = requestedName.trimmed();
    if (wanted.isEmpty()) {
      return {};
    }
    for (const InstalledLibrarySnapshot& lib : installedLibraries) {
      if (lib.name.compare(wanted, Qt::CaseInsensitive) == 0) {
        return lib;
      }
    }
    for (const InstalledLibrarySnapshot& lib : installedLibraries) {
      if (lib.name.startsWith(wanted, Qt::CaseInsensitive) ||
          wanted.startsWith(lib.name, Qt::CaseInsensitive)) {
        return lib;
      }
    }
    return {};
  };

  while (!dependencyQueue.isEmpty()) {
    const QString key = dependencyQueue.takeFirst();
    if (!selectedLibraryMap.contains(key)) {
      continue;
    }
    const InstalledLibrarySnapshot lib = selectedLibraryMap.value(key);
    const QStringList deps =
        parseLibraryDependenciesFromProperties(lib.installDir);
    for (const QString& depName : deps) {
      const InstalledLibrarySnapshot depLib = findInstalledLibraryByName(depName);
      if (depLib.name.trimmed().isEmpty()) {
        appendNote(tr("Library dependency '%1' was not found locally.")
                       .arg(depName));
        continue;
      }
      const QString depKey = depLib.name.trimmed().toLower();
      if (depKey.isEmpty() || selectedLibraryMap.contains(depKey)) {
        continue;
      }
      selectedLibraryMap.insert(depKey, depLib);
      dependencyQueue << depKey;
    }
  }

  selectedLibraries = selectedLibraryMap.values().toVector();
  std::sort(selectedLibraries.begin(), selectedLibraries.end(),
            [](const InstalledLibrarySnapshot& left,
               const InstalledLibrarySnapshot& right) {
              return QString::localeAwareCompare(left.name, right.name) < 0;
            });

  bool librariesBundled = false;
  QHash<QString, QString> bundledLibraryRelativePathByName;
  setProgress(58, tr("Bundling libraries..."));
  if (!selectedLibraries.isEmpty()) {
    for (const InstalledLibrarySnapshot& lib : selectedLibraries) {
      const QString installDir = lib.installDir.trimmed();
      if (installDir.isEmpty() || !QFileInfo(installDir).isDir()) {
        appendNote(
            tr("Library '%1' is missing an install directory and was skipped.")
                .arg(lib.name));
        continue;
      }
      if (lib.location.compare(QStringLiteral("platform"), Qt::CaseInsensitive) == 0) {
        // Platform libraries are included within the bundled core directory.
        continue;
      }

      const QString folderName = QFileInfo(installDir).fileName().trimmed();
      if (folderName.isEmpty()) {
        appendNote(
            tr("Library '%1' has an invalid install directory and was skipped.")
                .arg(lib.name));
        continue;
      }
      const QString relPath = QStringLiteral("libraries/%1").arg(folderName);
      const QString dstPath = QDir(bundleRoot).absoluteFilePath(relPath);
      if (copyDirectoryRecursivelyMerged(installDir, dstPath, &copyError)) {
        librariesBundled = true;
        bundledLibraryRelativePathByName.insert(lib.name.trimmed().toLower(),
                                                relPath);
      } else {
        appendNote(
            tr("Could not bundle library '%1': %2").arg(lib.name, copyError));
      }
    }
  } else {
    appendNote(tr("No project libraries were detected from sketch includes."));
  }

  QString buildArtifactsSourceDir;
  bool buildArtifactsFromFreshCompile = false;
  setProgress(68, tr("Collecting build artifacts..."));
  const QString currentSignature = computeSketchSignature(sketchFolder);
  if (!lastSuccessfulCompile_.buildPath.trimmed().isEmpty() &&
      QFileInfo(lastSuccessfulCompile_.buildPath).isDir() &&
      QDir(lastSuccessfulCompile_.sketchFolder).absolutePath() ==
          QDir(sketchFolder).absolutePath() &&
      lastSuccessfulCompile_.fqbn.trimmed() == fqbn &&
      !lastSuccessfulCompile_.sketchChangedSinceCompile &&
      !currentSignature.isEmpty() &&
      currentSignature == lastSuccessfulCompile_.sketchSignature) {
    buildArtifactsSourceDir = lastSuccessfulCompile_.buildPath;
  }

  if (buildArtifactsSourceDir.isEmpty() && !fqbn.isEmpty() &&
      !cliPath.isEmpty() && arduinoCli_) {
    setProgress(72, tr("Compiling to capture fresh build artifacts..."));
    const QString compileBuildDir =
        QDir(stagingDir.path()).absoluteFilePath(QStringLiteral("build-artifacts-fresh"));
    QDir().mkpath(compileBuildDir);

    QSettings settings;
    settings.beginGroup(QStringLiteral("Preferences"));
    const QString warningsLevel =
        settings.value(QStringLiteral("compilerWarnings"), QStringLiteral("none"))
            .toString();
    const bool verboseCompile =
        settings.value(QStringLiteral("verboseCompile"), false).toBool();
    settings.endGroup();

    QStringList args = {QStringLiteral("compile"),
                        QStringLiteral("--fqbn"),
                        fqbn,
                        QStringLiteral("--warnings"),
                        warningsLevel,
                        QStringLiteral("--build-path"),
                        compileBuildDir,
                        sketchFolder};
    if (verboseCompile) {
      args << QStringLiteral("--verbose");
    }
    if (actionOptimizeForDebug_ && actionOptimizeForDebug_->isChecked()) {
      args << QStringLiteral("--optimize-for-debug");
    }

    const CommandResult compileResult =
        runCommandBlocking(cliPath, arduinoCli_->withGlobalFlags(args), {}, {},
                           900000);
    if (commandSucceeded(compileResult)) {
      buildArtifactsSourceDir = compileBuildDir;
      buildArtifactsFromFreshCompile = true;
    } else {
      appendNote(
          tr("Could not compile fresh build artifacts for export: %1")
              .arg(commandErrorSummary(compileResult)));
      const QString compileErrorText =
          compileResult.stderrText + QLatin1Char('\n') + compileResult.stdoutText;
      static const QRegularExpression missingHeaderRe(
          QStringLiteral(R"(fatal error:\s*([^\s:]+):\s*No such file or directory)"));
      const QRegularExpressionMatch missingHeaderMatch =
          missingHeaderRe.match(compileErrorText);
      if (missingHeaderMatch.hasMatch()) {
        appendNote(
            tr("Missing header/library detected: %1. Install it, run Verify, then export again to include fresh build artifacts.")
                .arg(missingHeaderMatch.captured(1)));
      }
    }
  }

  bool buildArtifactsBundled = false;
  QStringList bundledArtifactRelativeFiles;
  if (!buildArtifactsSourceDir.isEmpty() &&
      QFileInfo(buildArtifactsSourceDir).isDir()) {
    setProgress(82, tr("Copying build artifacts..."));
    const QString buildArtifactsDst =
        QDir(bundleRoot).absoluteFilePath(QStringLiteral("build-artifacts"));
    QString artifactCopyError;
    if (copyBuildArtifactsForSketch(buildArtifactsSourceDir, sketchName,
                                    buildArtifactsDst,
                                    &bundledArtifactRelativeFiles,
                                    &artifactCopyError)) {
      buildArtifactsBundled = true;
    } else {
      appendNote(
          tr("Could not copy build artifacts: %1")
              .arg(artifactCopyError));
    }
  } else {
    appendNote(
        tr("No build artifacts available. Run Verify to bundle compiled output."));
  }

  QJsonObject manifest;
  setProgress(90, tr("Writing bundle manifest..."));
  manifest.insert(QStringLiteral("format"),
                  QString::fromLatin1(kProjectBundleFormat));
  manifest.insert(QStringLiteral("version"), kProjectBundleVersion);
  manifest.insert(QStringLiteral("generatedAt"),
                  QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
  manifest.insert(QStringLiteral("ideVersion"),
                  QStringLiteral(REWRITTO_IDE_VERSION));

  QJsonObject sketchObj;
  sketchObj.insert(QStringLiteral("name"), sketchName);
  sketchObj.insert(QStringLiteral("relative_path"), QStringLiteral("sketch"));
  sketchObj.insert(QStringLiteral("original_path"), sketchFolder);
  manifest.insert(QStringLiteral("sketch"), sketchObj);

  QJsonObject boardObj;
  boardObj.insert(QStringLiteral("fqbn"), fqbn);
  boardObj.insert(QStringLiteral("programmer"), currentProgrammer());
  boardObj.insert(QStringLiteral("port"), currentPort());
  manifest.insert(QStringLiteral("board"), boardObj);

  QJsonArray additionalUrlsJson;
  for (const QString& url : additionalUrls) {
    additionalUrlsJson.append(url);
  }
  manifest.insert(QStringLiteral("additional_urls"), additionalUrlsJson);

  QJsonObject cliObj;
  cliObj.insert(QStringLiteral("data_dir"), arduinoDataDir);
  cliObj.insert(QStringLiteral("user_dir"), sketchbookDir);
  manifest.insert(QStringLiteral("arduino_cli"), cliObj);

  QJsonArray bundledCorePackagesJson;
  for (const QString& rel : bundledCorePackages) {
    bundledCorePackagesJson.append(rel);
  }
  manifest.insert(QStringLiteral("bundled_core_packages"),
                  bundledCorePackagesJson);

  QJsonArray bundledToolsJson;
  for (const QString& rel : bundledToolPaths) {
    bundledToolsJson.append(rel);
  }
  manifest.insert(QStringLiteral("bundled_tools"),
                  bundledToolsJson);

  QJsonArray coresJson;
  if (selectedCoreFound) {
    QJsonObject item;
    item.insert(QStringLiteral("id"), selectedCore.id);
    item.insert(QStringLiteral("version"), selectedCore.installedVersion);
    item.insert(QStringLiteral("name"), selectedCore.name);
    item.insert(QStringLiteral("relative_path"), selectedCoreHardwareRelativePath);
    QJsonArray toolDepsJson;
    for (const CoreToolDependency& dep : selectedCoreToolDependencies) {
      QJsonObject depObj;
      depObj.insert(QStringLiteral("packager"), dep.packager);
      depObj.insert(QStringLiteral("name"), dep.name);
      depObj.insert(QStringLiteral("version"), dep.version);
      toolDepsJson.append(depObj);
    }
    item.insert(QStringLiteral("tool_dependencies"), toolDepsJson);
    coresJson.append(item);
  }
  manifest.insert(QStringLiteral("selected_cores"), coresJson);

  QJsonArray selectedLibrariesJson;
  for (const InstalledLibrarySnapshot& lib : selectedLibraries) {
    QJsonObject item;
    item.insert(QStringLiteral("name"), lib.name);
    item.insert(QStringLiteral("version"), lib.version);
    QJsonArray includes;
    for (const QString& include : lib.providesIncludes) {
      includes.append(include);
    }
    item.insert(QStringLiteral("includes"), includes);
    item.insert(QStringLiteral("location"), lib.location);
    item.insert(
        QStringLiteral("bundle_relative_path"),
        bundledLibraryRelativePathByName.value(lib.name.trimmed().toLower()));
    selectedLibrariesJson.append(item);
  }
  manifest.insert(QStringLiteral("selected_libraries"), selectedLibrariesJson);
  manifest.insert(QStringLiteral("libraries_bundle_relative_path"),
                  librariesBundled ? QStringLiteral("libraries") : QString{});

  QJsonObject buildObj;
  buildObj.insert(QStringLiteral("included"), buildArtifactsBundled);
  buildObj.insert(QStringLiteral("relative_path"),
                  buildArtifactsBundled ? QStringLiteral("build-artifacts")
                                        : QString{});
  buildObj.insert(QStringLiteral("fresh_compile"),
                  buildArtifactsFromFreshCompile);
  QJsonArray buildFilesJson;
  for (const QString& rel : bundledArtifactRelativeFiles) {
    buildFilesJson.append(rel);
  }
  buildObj.insert(QStringLiteral("files"), buildFilesJson);
  manifest.insert(QStringLiteral("build_artifacts"), buildObj);

  QJsonArray notesJson;
  notes = normalizeStringList(notes);
  for (const QString& note : notes) {
    notesJson.append(note);
  }
  manifest.insert(QStringLiteral("notes"), notesJson);

  const QString manifestPath = QDir(bundleRoot).absoluteFilePath(
      QString::fromLatin1(kProjectBundleManifestFile));
  QSaveFile save(manifestPath);
  if (!save.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QMessageBox::warning(this, tr("Export Project ZIP"),
                         tr("Could not write project bundle manifest."));
    return;
  }
  const QByteArray manifestBytes =
      QJsonDocument(manifest).toJson(QJsonDocument::Indented);
  if (save.write(manifestBytes) != manifestBytes.size() || !save.commit()) {
    QMessageBox::warning(this, tr("Export Project ZIP"),
                         tr("Failed to save project bundle manifest."));
    return;
  }

  const qint64 bundleSizeBytes = directorySizeBytes(bundleRoot);
  const QString bundleSizeText = formatByteSize(bundleSizeBytes);
  if (bundleSizeBytes >= 512LL * 1024LL * 1024LL) {
    const auto reply = QMessageBox::question(
        this,
        tr("Large Bundle Warning"),
        tr("Bundle size before compression is %1.\n\n"
           "Creating the ZIP may take several minutes depending on disk speed.\n\n"
           "Continue?")
            .arg(bundleSizeText),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);
    if (reply != QMessageBox::Yes) {
      progress.close();
      return;
    }
  }

  setProgress(96, tr("Creating ZIP archive (%1 before compression)...")
                      .arg(bundleSizeText));
  progress.setRange(0, 0);
  QString archiveError;
  if (!createZipArchive(
          bundleRoot, zipPath, &archiveError,
          [this, &progress](const QString& status) {
            progress.setLabelText(status);
            QCoreApplication::processEvents();
          })) {
    progress.close();
    QMessageBox::warning(this, tr("Export Project ZIP"),
                         tr("Could not create archive.\n\n%1")
                             .arg(archiveError.trimmed().isEmpty()
                                      ? tr("No compatible archive tool found.")
                                      : archiveError.trimmed()));
    return;
  }
  progress.setRange(0, 100);
  setProgress(100, tr("Export complete."));
  progress.close();

  if (output_) {
    output_->appendLine(tr("[Project Export] Bundle saved to: %1").arg(zipPath));
  }
  if (notes.isEmpty()) {
    QMessageBox::information(
        this, tr("Export Project ZIP"),
        tr("Project bundle exported successfully.\n\n%1").arg(zipPath));
  } else {
    QMessageBox::information(
        this, tr("Export Project ZIP"),
        tr("Project bundle exported with notes.\n\n%1\n\nNotes:\n- %2")
            .arg(zipPath, notes.join(QStringLiteral("\n- "))));
  }
}

void MainWindow::importProjectZip() {
  const QString zipPath = QFileDialog::getOpenFileName(
      this,
      tr("Import Project ZIP"),
      QDir::homePath(),
      tr("ZIP Files (*.zip);;All Files (*)"));
  if (zipPath.trimmed().isEmpty()) {
    return;
  }

  const QString destinationRoot = QFileDialog::getExistingDirectory(
      this,
      tr("Choose Destination Folder for Imported Sketch"),
      defaultSketchbookDir());
  if (destinationRoot.trimmed().isEmpty()) {
    return;
  }

  if (!QFileInfo(zipPath).isFile()) {
    QMessageBox::warning(this, tr("Import Project ZIP"),
                         tr("Selected archive does not exist."));
    return;
  }

  QTemporaryDir extractionDir;
  if (!extractionDir.isValid()) {
    QMessageBox::warning(this, tr("Import Project ZIP"),
                         tr("Could not create temporary extraction folder."));
    return;
  }

  const QString extractedRoot =
      QDir(extractionDir.path()).absoluteFilePath(QStringLiteral("extracted"));
  QDir().mkpath(extractedRoot);

  QString extractError;
  if (!extractZipArchive(zipPath, extractedRoot, &extractError)) {
    QMessageBox::warning(
        this, tr("Import Project ZIP"),
        tr("Could not extract archive.\n\n%1").arg(extractError.trimmed()));
    return;
  }

  const QString bundleRoot = findBundleRootDirectory(
      extractedRoot, QString::fromLatin1(kProjectBundleManifestFile));
  if (bundleRoot.trimmed().isEmpty()) {
    QMessageBox::warning(this, tr("Import Project ZIP"),
                         tr("Bundle manifest was not found in the archive."));
    return;
  }

  QFile manifestFile(QDir(bundleRoot).absoluteFilePath(
      QString::fromLatin1(kProjectBundleManifestFile)));
  if (!manifestFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QMessageBox::warning(this, tr("Import Project ZIP"),
                         tr("Could not open bundle manifest."));
    return;
  }
  const QJsonDocument manifestDoc =
      QJsonDocument::fromJson(manifestFile.readAll());
  if (!manifestDoc.isObject()) {
    QMessageBox::warning(this, tr("Import Project ZIP"),
                         tr("Bundle manifest is not valid JSON."));
    return;
  }
  const QJsonObject manifest = manifestDoc.object();
  const QString format = manifest.value(QStringLiteral("format")).toString().trimmed();
  if (!format.isEmpty() && format != QString::fromLatin1(kProjectBundleFormat)) {
    QMessageBox::warning(this, tr("Import Project ZIP"),
                         tr("Unsupported bundle format: %1").arg(format));
    return;
  }

  const QJsonObject sketchObj = manifest.value(QStringLiteral("sketch")).toObject();
  QString sketchRel = sketchObj.value(QStringLiteral("relative_path")).toString().trimmed();
  if (sketchRel.isEmpty()) {
    sketchRel = QStringLiteral("sketch");
  }
  const QString sketchSourcePath = QDir(bundleRoot).absoluteFilePath(sketchRel);
  if (!QFileInfo(sketchSourcePath).isDir()) {
    QMessageBox::warning(this, tr("Import Project ZIP"),
                         tr("Sketch folder was not found inside the bundle."));
    return;
  }

  QString sketchName = sketchObj.value(QStringLiteral("name")).toString().trimmed();
  if (sketchName.isEmpty()) {
    sketchName = QFileInfo(sketchSourcePath).fileName().trimmed();
  }
  if (sketchName.isEmpty()) {
    sketchName = QStringLiteral("imported_sketch");
  }

  QString destinationSketchPath =
      QDir(destinationRoot).absoluteFilePath(sketchName);
  if (QFileInfo(destinationSketchPath).exists()) {
    int suffix = 1;
    QString candidate;
    do {
      candidate = QDir(destinationRoot).absoluteFilePath(
          QStringLiteral("%1_imported_%2").arg(sketchName).arg(suffix));
      ++suffix;
    } while (QFileInfo(candidate).exists());
    destinationSketchPath = candidate;
  }

  QString copyError;
  if (!copyDirectoryRecursivelyMerged(sketchSourcePath, destinationSketchPath,
                                      &copyError)) {
    QMessageBox::warning(
        this, tr("Import Project ZIP"),
        tr("Could not copy sketch files.\n\n%1").arg(copyError));
    return;
  }

  QStringList notes;
  auto appendNote = [&notes](const QString& text) {
    const QString trimmed = text.trimmed();
    if (!trimmed.isEmpty()) {
      notes << trimmed;
    }
  };

  QString arduinoDataDir = defaultArduinoDataDirPath();
  const QString cliPath =
      arduinoCli_ ? arduinoCli_->arduinoCliPath().trimmed() : QString{};
  if (!cliPath.isEmpty() && arduinoCli_) {
    const ArduinoCliDirectories dirs =
        readArduinoCliDirectories(cliPath, arduinoCli_->withGlobalFlags({}));
    if (!dirs.dataDir.trimmed().isEmpty()) {
      arduinoDataDir = dirs.dataDir.trimmed();
    }
  }

  const QString bundledArduinoDataPath =
      QDir(bundleRoot).absoluteFilePath(QStringLiteral("arduino-data"));
  if (QFileInfo(bundledArduinoDataPath).isDir()) {
    if (!copyDirectoryRecursivelyMerged(bundledArduinoDataPath, arduinoDataDir,
                                        &copyError)) {
      appendNote(
          tr("Could not restore bundled board core data: %1")
              .arg(copyError));
    }
  }

  const QString bundledLibrariesPath =
      QDir(bundleRoot).absoluteFilePath(QStringLiteral("libraries"));
  if (QFileInfo(bundledLibrariesPath).isDir()) {
    const QString targetLibrariesPath =
        QDir(destinationSketchPath).absoluteFilePath(QStringLiteral("libraries"));
    if (!copyDirectoryRecursivelyMerged(bundledLibrariesPath,
                                        targetLibrariesPath,
                                        &copyError)) {
      appendNote(
          tr("Could not restore bundled libraries: %1")
              .arg(copyError));
    } else if (output_) {
      output_->appendLine(
          tr("[Project Import] Restored sketch-local libraries: %1")
              .arg(targetLibrariesPath));
    }
  }

  const QString bundledBuildPath =
      QDir(bundleRoot).absoluteFilePath(QStringLiteral("build-artifacts"));
  if (QFileInfo(bundledBuildPath).isDir()) {
    const QString targetBuildPath = QDir(destinationSketchPath).absoluteFilePath(
        QStringLiteral(".rewritto/build-artifacts"));
    if (!copyDirectoryRecursivelyMerged(bundledBuildPath, targetBuildPath,
                                        &copyError)) {
      appendNote(
          tr("Could not restore bundled build artifacts: %1")
              .arg(copyError));
    }
  }

  auto jsonArrayToStringList = [](const QJsonValue& value) {
    QStringList out;
    const QJsonArray array = value.toArray();
    out.reserve(array.size());
    for (const QJsonValue& item : array) {
      const QString text = item.toString().trimmed();
      if (!text.isEmpty()) {
        out << text;
      }
    }
    return normalizeStringList(out);
  };

  const QStringList additionalUrls =
      jsonArrayToStringList(manifest.value(QStringLiteral("additional_urls")));
  if (!additionalUrls.isEmpty()) {
    QString mergeError;
    if (!mergeAdditionalBoardUrlsIntoPreferences(additionalUrls, &mergeError,
                                                 nullptr)) {
      appendNote(
          tr("Could not merge additional board manager URLs: %1")
              .arg(mergeError));
    }
  }

  const QJsonObject boardObj = manifest.value(QStringLiteral("board")).toObject();
  const QString importedFqbn = boardObj.value(QStringLiteral("fqbn")).toString().trimmed();
  const QString importedProgrammer =
      boardObj.value(QStringLiteral("programmer")).toString().trimmed();

  const bool opened = openSketchFolderInUi(destinationSketchPath);
  if (!opened) {
    appendNote(tr("Sketch was restored but could not be opened automatically."));
  } else if (!importedFqbn.isEmpty()) {
    QSettings settings;
    settings.beginGroup(kSettingsGroup);
    settings.setValue(kFqbnKey, importedFqbn);
    settings.endGroup();
    storeFqbnForCurrentSketch(importedFqbn);
    if (boardCombo_) {
      const int index = boardCombo_->findData(importedFqbn);
      if (index >= 0) {
        boardCombo_->setCurrentIndex(index);
      }
    }
  }
  if (!importedProgrammer.isEmpty()) {
    setProgrammer(importedProgrammer);
  }

  if (boardsManager_) {
    boardsManager_->refresh();
  }
  if (libraryManager_) {
    libraryManager_->refresh();
  }
  refreshInstalledBoards();
  refreshConnectedPorts();

  notes = normalizeStringList(notes);
  if (output_) {
    output_->appendLine(
        tr("[Project Import] Bundle imported to: %1").arg(destinationSketchPath));
    for (const QString& note : notes) {
      output_->appendLine(tr("[Project Import] Note: %1").arg(note));
    }
  }

  if (notes.isEmpty()) {
    QMessageBox::information(
        this, tr("Import Project ZIP"),
        tr("Project bundle imported successfully.\n\n%1")
            .arg(destinationSketchPath));
  } else {
    QMessageBox::information(
        this, tr("Import Project ZIP"),
        tr("Project bundle imported with notes.\n\n%1\n\nNotes:\n- %2")
            .arg(destinationSketchPath, notes.join(QStringLiteral("\n- "))));
  }
}

void MainWindow::showQuickOpen() {
  if (!editor_) return;

  QStringList recent = recentSketches();
  if (recent.isEmpty()) {
    QMessageBox::information(this, tr("No Recent Sketches"),
                             tr("No recent sketches available. Open a sketch first."));
    return;
  }

  auto* dialog = new QuickPickDialog(this);
  dialog->setPlaceholderText(tr("Search recent sketches..."));

  QVector<QuickPickDialog::Item> items;
  for (const QString& sketchPath : recent) {
    const QString name = QFileInfo(sketchPath).completeBaseName();
    QuickPickDialog::Item item;
    item.label = name;
    item.detail = sketchPath;
    item.data = sketchPath;
    items.append(item);
  }

  dialog->setItems(items);

  if (dialog->exec() == QDialog::Accepted) {
    const QVariant data = dialog->selectedData();
    const QString folder = data.toString().trimmed();
    if (!folder.isEmpty()) {
      (void)openSketchFolderInUi(folder);
    }
  }
  dialog->deleteLater();
}

void MainWindow::showCommandPalette() {
  auto* dialog = new QuickPickDialog(this);
  dialog->setPlaceholderText(tr("Type a command..."));

  QVector<QuickPickDialog::Item> items;
  auto addItem = [&items](const QString& id,
                          const QString& label,
                          const QString& detail) {
    QuickPickDialog::Item item;
    item.label = label;
    item.detail = detail;
    item.data = id;
    items.append(item);
  };

  addItem(QStringLiteral("newSketch"), tr("New Sketch"),
          tr("Create a new sketch"));
  addItem(QStringLiteral("openSketch"), tr("Open Sketch"),
          tr("Open an existing .ino sketch"));
  addItem(QStringLiteral("openSketchFolder"), tr("Open Sketch Folder"),
          tr("Open a sketch folder"));
  addItem(QStringLiteral("exportProjectZip"), tr("Export Project ZIP"),
          tr("Export sketch, dependencies, and build artifacts"));
  addItem(QStringLiteral("importProjectZip"), tr("Import Project ZIP"),
          tr("Import sketch and dependencies from a project bundle"));
  addItem(QStringLiteral("verifySketch"), tr("Verify"),
          tr("Compile current sketch"));
  addItem(QStringLiteral("uploadSketch"), tr("Verify and Upload"),
          tr("Compile and upload current sketch"));
  addItem(QStringLiteral("toggleOptimizeDebug"),
          tr("Toggle Optimize for Debugging"),
          tr("Enable/disable compile debug optimization profile"));
  addItem(QStringLiteral("selectBoard"), tr("Select Board"),
          tr("Pick a board for current sketch"));
  addItem(QStringLiteral("boardSetupWizard"), tr("Board Setup Wizard"),
          tr("Configure board cores and URLs"));
  addItem(QStringLiteral("openBoardsManager"), tr("Boards Manager"),
          tr("Open board manager panel"));
  addItem(QStringLiteral("openLibraryManager"), tr("Library Manager"),
          tr("Open library manager panel"));
  addItem(QStringLiteral("exportSetupProfile"), tr("Export Setup Profile"),
          tr("Export boards/libs/preferences into a profile file"));
  addItem(QStringLiteral("importSetupProfile"), tr("Import Setup Profile"),
          tr("Import and apply boards/libs/preferences from a profile file"));
  addItem(QStringLiteral("generateProjectLockfile"), tr("Generate Project Lockfile"),
          tr("Write rewritto.lock for the current sketch"));
  addItem(QStringLiteral("bootstrapProjectLockfile"),
          tr("Bootstrap Project from Lockfile"),
          tr("Install board/library dependencies from rewritto.lock"));
  addItem(QStringLiteral("environmentDoctor"), tr("Environment Doctor"),
          tr("Run diagnostics and optional one-click fixes"));
  addItem(QStringLiteral("toggleSerialMonitor"), tr("Toggle Serial Monitor"),
          tr("Show/hide serial monitor"));
  addItem(QStringLiteral("toggleSerialPlotter"), tr("Toggle Serial Plotter"),
          tr("Show/hide serial plotter"));
  addItem(QStringLiteral("openPreferences"), tr("Preferences"),
          tr("Open IDE settings"));
  addItem(QStringLiteral("requestCompletion"), tr("Trigger Completion"),
          tr("Ask language server for completion items"));
  addItem(QStringLiteral("showHover"), tr("Show Hover"),
          tr("Show hover information at cursor"));
  addItem(QStringLiteral("goToDefinition"), tr("Go to Definition"),
          tr("Navigate to symbol definition"));
  addItem(QStringLiteral("findReferences"), tr("Find References"),
          tr("List symbol references across workspace"));
  addItem(QStringLiteral("renameSymbol"), tr("Rename Symbol"),
          tr("Rename symbol and apply edits"));
  addItem(QStringLiteral("codeActions"), tr("Code Actions"),
          tr("Show available quick fixes and refactors"));
  addItem(QStringLiteral("organizeImports"), tr("Organize Imports"),
          tr("Run organize imports code action"));
  addItem(QStringLiteral("formatDocument"), tr("Format Document"),
          tr("Format current file via language server"));
  addItem(QStringLiteral("configureMcp"), tr("Configure MCP"),
          tr("Set MCP server command"));
  addItem(QStringLiteral("startMcp"), tr("Start MCP Server"),
          tr("Start configured MCP server process"));
  addItem(QStringLiteral("stopMcp"), tr("Stop MCP Server"),
          tr("Stop running MCP server process"));
  addItem(QStringLiteral("restartMcp"), tr("Restart MCP Server"),
          tr("Restart MCP server process"));
  addItem(QStringLiteral("showMcpContext"), tr("Show MCP Context Toolbar"),
          tr("Switch context toolbar to MCP mode"));

  dialog->setItems(items);

  if (dialog->exec() == QDialog::Accepted) {
    const QString commandId = dialog->selectedData().toString().trimmed();
    if (commandId == QStringLiteral("newSketch")) {
      newSketch();
    } else if (commandId == QStringLiteral("openSketch")) {
      openSketch();
    } else if (commandId == QStringLiteral("openSketchFolder")) {
      openSketchFolder();
    } else if (commandId == QStringLiteral("exportProjectZip")) {
      exportProjectZip();
    } else if (commandId == QStringLiteral("importProjectZip")) {
      importProjectZip();
    } else if (commandId == QStringLiteral("verifySketch")) {
      verifySketch();
    } else if (commandId == QStringLiteral("uploadSketch")) {
      uploadSketch();
    } else if (commandId == QStringLiteral("toggleOptimizeDebug")) {
      if (actionOptimizeForDebug_) {
        actionOptimizeForDebug_->toggle();
      }
    } else if (commandId == QStringLiteral("selectBoard")) {
      showSelectBoardDialog();
    } else if (commandId == QStringLiteral("boardSetupWizard")) {
      runBoardSetupWizard();
    } else if (commandId == QStringLiteral("openBoardsManager")) {
      if (actionBoardsManager_) {
        actionBoardsManager_->trigger();
      }
    } else if (commandId == QStringLiteral("openLibraryManager")) {
      if (actionLibraryManager_) {
        actionLibraryManager_->trigger();
      }
    } else if (commandId == QStringLiteral("exportSetupProfile")) {
      exportSetupProfile();
    } else if (commandId == QStringLiteral("importSetupProfile")) {
      importSetupProfile();
    } else if (commandId == QStringLiteral("generateProjectLockfile")) {
      generateProjectLockfile();
    } else if (commandId == QStringLiteral("bootstrapProjectLockfile")) {
      bootstrapProjectLockfile();
    } else if (commandId == QStringLiteral("environmentDoctor")) {
      runEnvironmentDoctor();
    } else if (commandId == QStringLiteral("toggleSerialMonitor")) {
      if (actionSerialMonitor_) {
        actionSerialMonitor_->trigger();
      }
    } else if (commandId == QStringLiteral("toggleSerialPlotter")) {
      if (actionSerialPlotter_) {
        actionSerialPlotter_->trigger();
      }
    } else if (commandId == QStringLiteral("openPreferences")) {
      if (actionPreferences_) {
        actionPreferences_->trigger();
      }
    } else if (commandId == QStringLiteral("requestCompletion")) {
      requestCompletion();
    } else if (commandId == QStringLiteral("showHover")) {
      showHover();
    } else if (commandId == QStringLiteral("goToDefinition")) {
      goToDefinition();
    } else if (commandId == QStringLiteral("findReferences")) {
      findReferences();
    } else if (commandId == QStringLiteral("renameSymbol")) {
      renameSymbol();
    } else if (commandId == QStringLiteral("codeActions")) {
      showCodeActions();
    } else if (commandId == QStringLiteral("organizeImports")) {
      showCodeActions({QStringLiteral("source.organizeImports")});
    } else if (commandId == QStringLiteral("formatDocument")) {
      formatDocument();
    } else if (commandId == QStringLiteral("configureMcp")) {
      configureMcpServer();
    } else if (commandId == QStringLiteral("startMcp")) {
      startMcpServer();
    } else if (commandId == QStringLiteral("stopMcp")) {
      stopMcpServer();
    } else if (commandId == QStringLiteral("restartMcp")) {
      restartMcpServer();
    } else if (commandId == QStringLiteral("showMcpContext")) {
      setContextToolbarMode(ContextToolbarMode::Mcp);
      if (actionToggleFontToolBar_ && !actionToggleFontToolBar_->isChecked()) {
        actionToggleFontToolBar_->setChecked(true);
      } else if (fontToolBar_) {
        fontToolBar_->show();
      }
    }
  }
  dialog->deleteLater();
}

void MainWindow::showFindReplaceDialog() {
  if (!findReplaceDialog_) {
    findReplaceDialog_ = new FindReplaceDialog(editor_, this);
  }
  findReplaceDialog_->show();
  findReplaceDialog_->raise();
  findReplaceDialog_->activateWindow();
}

void MainWindow::showFindInFilesDialog() {
  if (!searchDock_) return;
  searchDock_->show();
  searchDock_->raise();
  if (searchTabs_) {
    searchTabs_->setCurrentWidget(findInFiles_);
  }
}

void MainWindow::showReplaceInFilesDialog() {
  if (!searchDock_) return;
  searchDock_->show();
  searchDock_->raise();
  if (searchTabs_) {
    searchTabs_->setCurrentWidget(replaceInFiles_);
  }
}

void MainWindow::goToLine() {
  bool ok = false;
  int line = QInputDialog::getInt(this, tr("Go to Line"),
                                   tr("Line number:"), 1, 1, 1000000, 1, &ok);
  if (ok && editor_) {
    editor_->goToLine(line);
  }
}

void MainWindow::handleQuickFix(const QString& filePath,
                                int line,
                                int column,
                                const QString& fixType) {
  if (!editor_) {
    return;
  }

  const QString normalizedFix = fixType.trimmed();
  if (normalizedFix.isEmpty()) {
    return;
  }

  const QString absPath =
      normalizeDiagnosticPath(filePath, line, currentSketchFolderPath());
  if (absPath.trimmed().isEmpty() || line <= 0) {
    showToast(tr("Quick fix requires a valid file location."));
    return;
  }

  if (!editor_->openLocation(absPath, line, column)) {
    showToast(tr("Could not open file for quick fix."));
    return;
  }

  auto* plain = qobject_cast<QPlainTextEdit*>(editor_->currentEditorWidget());
  if (!plain || !plain->document()) {
    showToast(tr("No editable document available for quick fix."));
    return;
  }

  QTextDocument* doc = plain->document();
  const QTextBlock block = doc->findBlockByNumber(qMax(0, line - 1));
  if (!block.isValid()) {
    showToast(tr("Quick fix location is out of range."));
    return;
  }

  const QString lineText = block.text();
  const int maxBlockColumn = qMax(0, block.length() - 1);
  const int safeColumn = qBound(0, column > 0 ? column - 1 : 0, maxBlockColumn);

  if (normalizedFix == QStringLiteral("insert_semicolon")) {
    int insertOffset = safeColumn;
    if (column <= 0) {
      insertOffset = lineText.size();
      while (insertOffset > 0 &&
             (lineText.at(insertOffset - 1) == QLatin1Char(' ') ||
              lineText.at(insertOffset - 1) == QLatin1Char('\t'))) {
        --insertOffset;
      }
    }

    const int insertPos = block.position() + insertOffset;
    if (insertPos > 0 && doc->characterAt(insertPos - 1) == QLatin1Char(';')) {
      showToast(tr("Line already ends with a semicolon."));
      return;
    }

    QTextCursor cursor(doc);
    cursor.setPosition(insertPos);
    cursor.insertText(QStringLiteral(";"));
    plain->setFocus(Qt::OtherFocusReason);
    showToast(tr("Inserted semicolon quick fix."));
    return;
  }

  if (normalizedFix == QStringLiteral("insert_brace")) {
    QTextCursor cursor(doc);
    cursor.beginEditBlock();

    const QString trimmed = lineText.trimmed();
    if (!trimmed.contains(QLatin1Char('{')) && trimmed.endsWith(QLatin1Char(')'))) {
      int insertOffset = lineText.size();
      while (insertOffset > 0 &&
             (lineText.at(insertOffset - 1) == QLatin1Char(' ') ||
              lineText.at(insertOffset - 1) == QLatin1Char('\t'))) {
        --insertOffset;
      }
      cursor.setPosition(block.position() + insertOffset);
      cursor.insertText(QStringLiteral(" {"));
    } else {
      QString indent;
      int i = 0;
      while (i < lineText.size() &&
             (lineText.at(i) == QLatin1Char(' ') || lineText.at(i) == QLatin1Char('\t'))) {
        indent += lineText.at(i);
        ++i;
      }
      const int insertPos = block.position() + qMax(0, block.length() - 1);
      cursor.setPosition(insertPos);
      cursor.insertText(QStringLiteral("\n%1}").arg(indent));
    }

    cursor.endEditBlock();
    plain->setFocus(Qt::OtherFocusReason);
    showToast(tr("Inserted brace quick fix."));
    return;
  }

  if (normalizedFix == QStringLiteral("mark_unused")) {
    const QString identifier = identifierNearColumn(lineText, column);
    if (identifier.isEmpty()) {
      showToast(tr("Could not detect variable name for quick fix."));
      return;
    }
    if (identifier.startsWith(QLatin1Char('_'))) {
      showToast(tr("Variable is already marked as unused."));
      return;
    }

    const QRegularExpression tokenExpr(
        QStringLiteral("\\b%1\\b").arg(QRegularExpression::escape(identifier)));
    QRegularExpressionMatchIterator it = tokenExpr.globalMatch(lineText);
    int bestStart = -1;
    int bestLength = 0;
    int bestDistance = std::numeric_limits<int>::max();
    while (it.hasNext()) {
      const QRegularExpressionMatch m = it.next();
      const int start = m.capturedStart();
      if (start < 0) {
        continue;
      }
      const int distance = std::abs(start - safeColumn);
      if (distance < bestDistance) {
        bestDistance = distance;
        bestStart = start;
        bestLength = m.capturedLength();
      }
    }

    if (bestStart < 0 || bestLength <= 0) {
      showToast(tr("Could not apply unused-variable quick fix."));
      return;
    }

    QTextCursor cursor(doc);
    cursor.setPosition(block.position() + bestStart);
    cursor.setPosition(block.position() + bestStart + bestLength, QTextCursor::KeepAnchor);
    cursor.insertText(QStringLiteral("_%1").arg(identifier));
    plain->setFocus(Qt::OtherFocusReason);
    showToast(tr("Marked variable as unused."));
    return;
  }

  if (normalizedFix == QStringLiteral("remove_variable")) {
    QTextCursor cursor(block);
    cursor.beginEditBlock();
    cursor.select(QTextCursor::LineUnderCursor);
    cursor.removeSelectedText();
    if (cursor.position() < doc->characterCount() - 1 &&
        doc->characterAt(cursor.position()) == QChar::ParagraphSeparator) {
      cursor.deleteChar();
    }
    cursor.endEditBlock();
    plain->setFocus(Qt::OtherFocusReason);
    showToast(tr("Removed variable line."));
    return;
  }

  if (normalizedFix == QStringLiteral("create_declaration")) {
    const QString symbol = identifierNearColumn(lineText, column);
    if (symbol.isEmpty()) {
      showToast(tr("Could not infer declaration name."));
      return;
    }

    const QRegularExpression existing(
        QStringLiteral("(^|\\n)\\s*(class|struct)\\s+%1\\s*;")
            .arg(QRegularExpression::escape(symbol)),
        QRegularExpression::MultilineOption);
    if (existing.match(doc->toPlainText()).hasMatch()) {
      showToast(tr("Forward declaration already exists."));
      return;
    }

    QTextCursor cursor(doc);
    cursor.setPosition(declarationInsertPosition(doc));
    if (cursor.position() > 0 &&
        doc->characterAt(cursor.position() - 1) != QLatin1Char('\n') &&
        doc->characterAt(cursor.position() - 1) != QChar::ParagraphSeparator) {
      cursor.insertText(QStringLiteral("\n"));
    }
    cursor.insertText(QStringLiteral("class %1;\n").arg(symbol));
    plain->setFocus(Qt::OtherFocusReason);
    showToast(tr("Inserted forward declaration."));
    return;
  }

  if (normalizedFix == QStringLiteral("add_include_guard")) {
    const QString text = doc->toPlainText();
    static const QRegularExpression pragmaOnceExpr(
        QStringLiteral("^\\s*#\\s*pragma\\s+once\\b"),
        QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression includeGuardExpr(
        QStringLiteral("^\\s*#\\s*ifndef\\s+\\w+\\s*\\n\\s*#\\s*define\\s+\\w+"),
        QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption);

    if (pragmaOnceExpr.match(text).hasMatch() ||
        includeGuardExpr.match(text).hasMatch()) {
      showToast(tr("Include guard already exists."));
      return;
    }

    const QString guard = includeGuardSymbolFromPath(absPath);
    QString body = text;
    if (!body.endsWith(QLatin1Char('\n'))) {
      body += QLatin1Char('\n');
    }
    const QString wrapped =
        QStringLiteral("#ifndef %1\n#define %1\n\n").arg(guard) +
        body +
        QStringLiteral("\n#endif // %1\n").arg(guard);

    QTextCursor cursor(doc);
    cursor.beginEditBlock();
    cursor.select(QTextCursor::Document);
    cursor.insertText(wrapped);
    cursor.endEditBlock();
    plain->setFocus(Qt::OtherFocusReason);
    showToast(tr("Added include guard."));
    return;
  }

  if (normalizedFix == QStringLiteral("add_prototype")) {
    const QString symbol = identifierNearColumn(lineText, column);
    if (symbol.isEmpty()) {
      showToast(tr("Could not infer function name for prototype."));
      return;
    }

    const QRegularExpression existing(
        QStringLiteral("^\\s*[A-Za-z_][A-Za-z0-9_:<>,\\s*&]*\\b%1\\s*\\([^\\n]*\\)\\s*[;{]")
            .arg(QRegularExpression::escape(symbol)),
        QRegularExpression::MultilineOption);
    if (existing.match(doc->toPlainText()).hasMatch()) {
      showToast(tr("Function declaration already exists."));
      return;
    }

    QTextCursor cursor(doc);
    cursor.setPosition(declarationInsertPosition(doc));
    if (cursor.position() > 0 &&
        doc->characterAt(cursor.position() - 1) != QLatin1Char('\n') &&
        doc->characterAt(cursor.position() - 1) != QChar::ParagraphSeparator) {
      cursor.insertText(QStringLiteral("\n"));
    }
    cursor.insertText(QStringLiteral("void %1();\n").arg(symbol));
    plain->setFocus(Qt::OtherFocusReason);
    showToast(tr("Inserted function prototype (update signature if needed)."));
    return;
  }

  showToast(tr("Quick fix type is not implemented yet."));
}

void MainWindow::showGoToSymbol() {
  // Show quick pick dialog with symbols from current file
  if (!lsp_ || !lsp_->isReady() || !editor_) return;

  const QString currentPath = editor_->currentFilePath();
  if (currentPath.isEmpty()) return;

  lsp_->request("textDocument/documentSymbol",
    QJsonObject{{"textDocument", QJsonObject{{"uri", toFileUri(currentPath)}}}},
    [this, currentPath](const QJsonValue& result, const QJsonObject&) {
      auto* dialog = new QuickPickDialog(this);
      dialog->setPlaceholderText(tr("Go to symbol in current file..."));

      QVector<QuickPickDialog::Item> items;

      if (result.isArray()) {
        const QJsonArray symbols = result.toArray();
        for (const QJsonValue& v : symbols) {
          const QJsonObject obj = v.toObject();
          const QString name = obj.value("name").toString();
          const QString kind = obj.value("kind").toString();
          const QJsonObject range = obj.value("range").toObject();
          const QJsonObject start = range.value("start").toObject();

          QuickPickDialog::Item item;
          item.label = name;
          item.detail = kind;
          item.data = QList<QVariant>{start.value("line").toInt(), start.value("character").toInt()};
          items.append(item);
        }
      }

      dialog->setItems(items);

      if (dialog->exec() == QDialog::Accepted) {
        const QVariant data = dialog->selectedData();
        if (data.isValid() && data.typeId() == QMetaType::QVariantList) {
          const QList<QVariant> list = data.toList();
          if (list.size() >= 2) {
            const int line = list[0].toInt();
            const int column = list[1].toInt();
            editor_->openLocation(currentPath, line + 1, column + 1);
          }
        }
      }
      dialog->deleteLater();
    });
}

void MainWindow::requestCompletion() {
  if (!lsp_ || !lsp_->isReady() || !editor_) {
    showToast(tr("Language server is not ready."));
    return;
  }

  auto* plain = qobject_cast<QPlainTextEdit*>(editor_->currentEditorWidget());
  const QString filePath = editor_->currentFilePath().trimmed();
  if (!plain || filePath.isEmpty()) {
    return;
  }

  const QTextCursor cursor = plain->textCursor();
  const QTextBlock block = cursor.block();
  const int line = qMax(0, block.blockNumber());
  const int character = qMax(0, cursor.position() - block.position());
  const QString uri = toFileUri(filePath);

  const QJsonObject params{
      {QStringLiteral("textDocument"),
       QJsonObject{{QStringLiteral("uri"), uri}}},
      {QStringLiteral("position"),
       QJsonObject{{QStringLiteral("line"), line},
                   {QStringLiteral("character"), character}}},
  };

  lsp_->request(
      QStringLiteral("textDocument/completion"), params,
      [this, uri](const QJsonValue& result, const QJsonObject& error) {
        if (!error.isEmpty()) {
          showToast(tr("Completion request failed."));
          return;
        }

        QJsonArray completionItems;
        if (result.isArray()) {
          completionItems = result.toArray();
        } else if (result.isObject()) {
          completionItems =
              result.toObject().value(QStringLiteral("items")).toArray();
        }

        if (completionItems.isEmpty()) {
          showToast(tr("No completions available."));
          return;
        }

        auto* dialog = new QuickPickDialog(this);
        dialog->setPlaceholderText(tr("Choose completion..."));
        QVector<QuickPickDialog::Item> items;
        items.reserve(qMin(400, completionItems.size()));
        for (const QJsonValue& value : completionItems) {
          if (!value.isObject()) {
            continue;
          }
          const QJsonObject obj = value.toObject();
          const QString label = obj.value(QStringLiteral("label")).toString().trimmed();
          if (label.isEmpty()) {
            continue;
          }
          const QString detail = obj.value(QStringLiteral("detail")).toString().trimmed();
          QuickPickDialog::Item item;
          item.label = label;
          item.detail = detail;
          item.data = obj;
          items.push_back(item);
          if (items.size() >= 400) {
            break;
          }
        }

        dialog->setItems(items);
        if (dialog->exec() != QDialog::Accepted) {
          dialog->deleteLater();
          return;
        }

        const QJsonObject selected = dialog->selectedData().toJsonObject();
        dialog->deleteLater();
        if (selected.isEmpty() || !editor_) {
          return;
        }

        auto* plainEditor = qobject_cast<QPlainTextEdit*>(editor_->currentEditorWidget());
        if (!plainEditor || !plainEditor->document()) {
          return;
        }
        QTextDocument* doc = plainEditor->document();
        QTextCursor textCursor = plainEditor->textCursor();

        int startPos = textCursor.position();
        int endPos = textCursor.position();
        QString newText = selected.value(QStringLiteral("insertText")).toString();
        if (newText.isEmpty()) {
          newText = selected.value(QStringLiteral("label")).toString();
        }

        QJsonObject textEditObj = selected.value(QStringLiteral("textEdit")).toObject();
        if (!textEditObj.isEmpty()) {
          QJsonObject rangeObj = textEditObj.value(QStringLiteral("range")).toObject();
          if (rangeObj.isEmpty()) {
            rangeObj = textEditObj.value(QStringLiteral("replace")).toObject();
          }
          if (!rangeObj.isEmpty() &&
              lspRangeToDocumentOffsets(doc, rangeObj, &startPos, &endPos)) {
            // keep resolved range
          } else {
            startPos = textCursor.position();
            endPos = textCursor.position();
          }
          const QString fromEdit = textEditObj.value(QStringLiteral("newText")).toString();
          if (!fromEdit.isEmpty()) {
            newText = fromEdit;
          }
        } else if (!textCursor.hasSelection()) {
          QTextCursor word = textCursor;
          word.select(QTextCursor::WordUnderCursor);
          if (!word.selectedText().trimmed().isEmpty()) {
            startPos = word.selectionStart();
            endPos = word.selectionEnd();
          }
        } else {
          startPos = textCursor.selectionStart();
          endPos = textCursor.selectionEnd();
        }

        const int insertTextFormat =
            selected.value(QStringLiteral("insertTextFormat")).toInt(1);
        if (insertTextFormat == 2) {
          if (auto* codeEditor = qobject_cast<CodeEditor*>(plainEditor)) {
            codeEditor->insertSnippet(startPos, endPos, newText);
          } else {
            QTextCursor c(doc);
            c.setPosition(startPos);
            c.setPosition(endPos, QTextCursor::KeepAnchor);
            c.insertText(newText);
          }
        } else {
          QTextCursor c(doc);
          c.setPosition(startPos);
          c.setPosition(endPos, QTextCursor::KeepAnchor);
          c.insertText(newText);
        }

        const QJsonArray additional =
            selected.value(QStringLiteral("additionalTextEdits")).toArray();
        if (!additional.isEmpty()) {
          QJsonObject ws;
          ws.insert(QStringLiteral("changes"),
                    QJsonObject{{uri, additional}});
          (void)applyWorkspaceEdit(ws);
        }
      });
}

void MainWindow::showHover() {
  if (!lsp_ || !lsp_->isReady() || !editor_) {
    showToast(tr("Language server is not ready."));
    return;
  }

  auto* plain = qobject_cast<QPlainTextEdit*>(editor_->currentEditorWidget());
  const QString filePath = editor_->currentFilePath().trimmed();
  if (!plain || filePath.isEmpty()) {
    return;
  }

  const QTextCursor cursor = plain->textCursor();
  const QTextBlock block = cursor.block();
  const int line = qMax(0, block.blockNumber());
  const int character = qMax(0, cursor.position() - block.position());

  const QJsonObject params{
      {QStringLiteral("textDocument"),
       QJsonObject{{QStringLiteral("uri"), toFileUri(filePath)}}},
      {QStringLiteral("position"),
       QJsonObject{{QStringLiteral("line"), line},
                   {QStringLiteral("character"), character}}},
  };

  lsp_->request(
      QStringLiteral("textDocument/hover"), params,
      [this, plain](const QJsonValue& result, const QJsonObject& error) {
        if (!error.isEmpty() || !result.isObject()) {
          showToast(tr("No hover information available."));
          return;
        }
        const QJsonObject hoverObj = result.toObject();
        const QString text =
            hoverContentsToText(hoverObj.value(QStringLiteral("contents")))
                .trimmed();
        if (text.isEmpty()) {
          showToast(tr("No hover information available."));
          return;
        }
        const QString shown = text.size() > 1200 ? text.left(1200) : text;
        QToolTip::showText(plain->mapToGlobal(plain->cursorRect().bottomRight()),
                           shown, plain);
      });
}

void MainWindow::goToDefinition() {
  if (!lsp_ || !lsp_->isReady() || !editor_) {
    showToast(tr("Language server is not ready."));
    return;
  }

  auto* plain = qobject_cast<QPlainTextEdit*>(editor_->currentEditorWidget());
  const QString filePath = editor_->currentFilePath().trimmed();
  if (!plain || filePath.isEmpty()) {
    return;
  }

  const QTextCursor cursor = plain->textCursor();
  const QTextBlock block = cursor.block();
  const int line = qMax(0, block.blockNumber());
  const int character = qMax(0, cursor.position() - block.position());

  const QJsonObject params{
      {QStringLiteral("textDocument"),
       QJsonObject{{QStringLiteral("uri"), toFileUri(filePath)}}},
      {QStringLiteral("position"),
       QJsonObject{{QStringLiteral("line"), line},
                   {QStringLiteral("character"), character}}},
  };

  lsp_->request(
      QStringLiteral("textDocument/definition"), params,
      [this](const QJsonValue& result, const QJsonObject& error) {
        if (!editor_ || !error.isEmpty()) {
          showToast(tr("Definition lookup failed."));
          return;
        }

        auto parseLocation = [](const QJsonObject& obj, QString* outPath,
                                int* outLine, int* outColumn) {
          if (!outPath || !outLine || !outColumn) {
            return false;
          }
          QString uri = obj.value(QStringLiteral("uri")).toString().trimmed();
          if (uri.isEmpty()) {
            uri = obj.value(QStringLiteral("targetUri")).toString().trimmed();
          }
          QJsonObject range = obj.value(QStringLiteral("range")).toObject();
          if (range.isEmpty()) {
            range = obj.value(QStringLiteral("targetSelectionRange")).toObject();
          }
          if (range.isEmpty()) {
            range = obj.value(QStringLiteral("targetRange")).toObject();
          }
          const QJsonObject start = range.value(QStringLiteral("start")).toObject();
          const QString path = pathFromUriOrPath(uri);
          if (path.isEmpty() || start.isEmpty()) {
            return false;
          }
          *outPath = path;
          *outLine = start.value(QStringLiteral("line")).toInt() + 1;
          *outColumn = start.value(QStringLiteral("character")).toInt() + 1;
          return true;
        };

        QJsonObject locationObj;
        if (result.isArray()) {
          const QJsonArray arr = result.toArray();
          if (!arr.isEmpty() && arr.first().isObject()) {
            locationObj = arr.first().toObject();
          }
        } else if (result.isObject()) {
          locationObj = result.toObject();
        }

        QString targetPath;
        int targetLine = 0;
        int targetColumn = 0;
        if (!parseLocation(locationObj, &targetPath, &targetLine, &targetColumn)) {
          showToast(tr("Definition not found."));
          return;
        }

        if (!editor_->openLocation(targetPath, targetLine, targetColumn)) {
          showToast(tr("Could not open definition location."));
        }
      });
}

void MainWindow::findReferences() {
  if (!lsp_ || !lsp_->isReady() || !editor_) {
    showToast(tr("Language server is not ready."));
    return;
  }

  auto* plain = qobject_cast<QPlainTextEdit*>(editor_->currentEditorWidget());
  const QString filePath = editor_->currentFilePath().trimmed();
  if (!plain || filePath.isEmpty()) {
    return;
  }

  const QTextCursor cursor = plain->textCursor();
  const QTextBlock block = cursor.block();
  const int line = qMax(0, block.blockNumber());
  const int character = qMax(0, cursor.position() - block.position());

  const QJsonObject params{
      {QStringLiteral("textDocument"),
       QJsonObject{{QStringLiteral("uri"), toFileUri(filePath)}}},
      {QStringLiteral("position"),
       QJsonObject{{QStringLiteral("line"), line},
                   {QStringLiteral("character"), character}}},
      {QStringLiteral("context"),
       QJsonObject{{QStringLiteral("includeDeclaration"), true}}},
  };

  lsp_->request(
      QStringLiteral("textDocument/references"), params,
      [this](const QJsonValue& result, const QJsonObject& error) {
        if (!editor_ || !error.isEmpty() || !result.isArray()) {
          showToast(tr("No references found."));
          return;
        }
        const QJsonArray refs = result.toArray();
        if (refs.isEmpty()) {
          showToast(tr("No references found."));
          return;
        }

        auto* dialog = new QuickPickDialog(this);
        dialog->setPlaceholderText(tr("Select reference..."));

        QVector<QuickPickDialog::Item> items;
        items.reserve(qMin(500, refs.size()));
        for (const QJsonValue& value : refs) {
          if (!value.isObject()) {
            continue;
          }
          const QJsonObject location = value.toObject();
          const QString path = pathFromUriOrPath(
              location.value(QStringLiteral("uri")).toString());
          const QJsonObject range = location.value(QStringLiteral("range")).toObject();
          const QJsonObject start = range.value(QStringLiteral("start")).toObject();
          if (path.isEmpty() || start.isEmpty()) {
            continue;
          }

          const int row = start.value(QStringLiteral("line")).toInt() + 1;
          const int col = start.value(QStringLiteral("character")).toInt() + 1;

          QuickPickDialog::Item item;
          item.label = QFileInfo(path).fileName();
          item.detail = QStringLiteral("%1:%2:%3").arg(path).arg(row).arg(col);
          item.data = QVariantList{path, row, col};
          items.push_back(item);
          if (items.size() >= 500) {
            break;
          }
        }

        dialog->setItems(items);
        if (dialog->exec() == QDialog::Accepted) {
          const QVariantList picked = dialog->selectedData().toList();
          if (picked.size() >= 3) {
            (void)editor_->openLocation(picked[0].toString(), picked[1].toInt(),
                                        picked[2].toInt());
          }
        }
        dialog->deleteLater();
      });
}

void MainWindow::renameSymbol() {
  if (!lsp_ || !lsp_->isReady() || !editor_) {
    showToast(tr("Language server is not ready."));
    return;
  }

  auto* plain = qobject_cast<QPlainTextEdit*>(editor_->currentEditorWidget());
  const QString filePath = editor_->currentFilePath().trimmed();
  if (!plain || filePath.isEmpty()) {
    return;
  }

  QTextCursor cursor = plain->textCursor();
  QString oldName = cursor.selectedText().trimmed();
  if (oldName.isEmpty()) {
    QTextCursor word = cursor;
    word.select(QTextCursor::WordUnderCursor);
    oldName = word.selectedText().trimmed();
  }

  bool ok = false;
  const QString newName = QInputDialog::getText(
      this, tr("Rename Symbol"), tr("New symbol name:"),
      QLineEdit::Normal, oldName, &ok).trimmed();
  if (!ok || newName.isEmpty() || newName == oldName) {
    return;
  }

  const QTextBlock block = cursor.block();
  const int line = qMax(0, block.blockNumber());
  const int character = qMax(0, cursor.position() - block.position());

  const QJsonObject params{
      {QStringLiteral("textDocument"),
       QJsonObject{{QStringLiteral("uri"), toFileUri(filePath)}}},
      {QStringLiteral("position"),
       QJsonObject{{QStringLiteral("line"), line},
                   {QStringLiteral("character"), character}}},
      {QStringLiteral("newName"), newName},
  };

  lsp_->request(
      QStringLiteral("textDocument/rename"), params,
      [this](const QJsonValue& result, const QJsonObject& error) {
        if (!error.isEmpty() || !result.isObject()) {
          showToast(tr("Rename failed."));
          return;
        }
        if (applyWorkspaceEdit(result.toObject())) {
          showToast(tr("Rename applied."));
        } else {
          showToast(tr("Rename produced edits but could not be applied."));
        }
      });
}

void MainWindow::showCodeActions(QStringList onlyKinds) {
  if (!lsp_ || !lsp_->isReady() || !editor_) {
    showToast(tr("Language server is not ready."));
    return;
  }

  auto* plain = qobject_cast<QPlainTextEdit*>(editor_->currentEditorWidget());
  const QString filePath = editor_->currentFilePath().trimmed();
  if (!plain || filePath.isEmpty()) {
    return;
  }

  QTextCursor cursor = plain->textCursor();
  int startPos = cursor.selectionStart();
  int endPos = cursor.selectionEnd();
  if (!cursor.hasSelection()) {
    startPos = cursor.position();
    endPos = cursor.position();
  }

  auto lineCharAt = [plain](int pos) {
    const QTextDocument* doc = plain->document();
    const QTextBlock block = doc->findBlock(pos);
    const int line = block.isValid() ? block.blockNumber() : 0;
    const int character =
        block.isValid() ? qMax(0, pos - block.position()) : 0;
    return QPair<int, int>(line, character);
  };
  const QPair<int, int> start = lineCharAt(startPos);
  const QPair<int, int> end = lineCharAt(endPos);

  const QString normalizedPath = QFileInfo(filePath).absoluteFilePath();
  QJsonObject context{
      {QStringLiteral("diagnostics"),
       lspDiagnosticsByFilePath_.value(normalizedPath)},
  };
  onlyKinds.removeAll(QString{});
  if (!onlyKinds.isEmpty()) {
    QJsonArray only;
    for (const QString& kind : onlyKinds) {
      only.push_back(kind);
    }
    context.insert(QStringLiteral("only"), only);
  }

  const QJsonObject params{
      {QStringLiteral("textDocument"),
       QJsonObject{{QStringLiteral("uri"), toFileUri(filePath)}}},
      {QStringLiteral("range"),
       QJsonObject{
           {QStringLiteral("start"),
            QJsonObject{{QStringLiteral("line"), start.first},
                        {QStringLiteral("character"), start.second}}},
           {QStringLiteral("end"),
            QJsonObject{{QStringLiteral("line"), end.first},
                        {QStringLiteral("character"), end.second}}},
       }},
      {QStringLiteral("context"), context},
  };

  lsp_->request(
      QStringLiteral("textDocument/codeAction"), params,
      [this](const QJsonValue& result, const QJsonObject& error) {
        if (!lsp_ || !error.isEmpty() || !result.isArray()) {
          showToast(tr("No code actions available."));
          return;
        }
        const QJsonArray actions = result.toArray();
        if (actions.isEmpty()) {
          showToast(tr("No code actions available."));
          return;
        }

        auto* dialog = new QuickPickDialog(this);
        dialog->setPlaceholderText(tr("Select code action..."));
        QVector<QuickPickDialog::Item> items;
        items.reserve(actions.size());
        for (const QJsonValue& value : actions) {
          if (!value.isObject()) {
            continue;
          }
          const QJsonObject action = value.toObject();
          QString title = action.value(QStringLiteral("title")).toString().trimmed();
          if (title.isEmpty()) {
            const QJsonObject cmd = action.value(QStringLiteral("command")).toObject();
            title = cmd.value(QStringLiteral("title")).toString().trimmed();
          }
          if (title.isEmpty()) {
            title = tr("Unnamed action");
          }

          QuickPickDialog::Item item;
          item.label = title;
          item.detail = action.value(QStringLiteral("kind")).toString().trimmed();
          item.data = action;
          items.push_back(item);
        }

        dialog->setItems(items);
        if (dialog->exec() != QDialog::Accepted) {
          dialog->deleteLater();
          return;
        }
        const QJsonObject actionObj = dialog->selectedData().toJsonObject();
        dialog->deleteLater();
        if (actionObj.isEmpty()) {
          return;
        }

        const LspCodeActionExecution execution =
            lspPlanCodeActionExecution(actionObj);
        if (!execution.workspaceEdit.isEmpty() &&
            !applyWorkspaceEdit(execution.workspaceEdit)) {
          showToast(tr("Failed to apply code action edits."));
          return;
        }

        if (!execution.executeCommandParams.isEmpty()) {
          lsp_->request(
              QStringLiteral("workspace/executeCommand"),
              execution.executeCommandParams,
              [this](const QJsonValue& commandResult, const QJsonObject& commandError) {
                if (!commandError.isEmpty()) {
                  showToast(tr("Code action command failed."));
                  return;
                }
                if (commandResult.isObject()) {
                  const QJsonObject resultObj = commandResult.toObject();
                  if (resultObj.contains(QStringLiteral("changes")) ||
                      resultObj.contains(QStringLiteral("documentChanges"))) {
                    (void)applyWorkspaceEdit(resultObj);
                  }
                }
                showToast(tr("Code action applied."));
              });
          return;
        }

        showToast(tr("Code action applied."));
      });
}

void MainWindow::formatDocument() {
  if (!lsp_ || !lsp_->isReady() || !editor_) {
    showToast(tr("Language server is not ready."));
    return;
  }

  const QString filePath = editor_->currentFilePath().trimmed();
  if (filePath.isEmpty()) {
    return;
  }

  const QJsonObject params{
      {QStringLiteral("textDocument"),
       QJsonObject{{QStringLiteral("uri"), toFileUri(filePath)}}},
      {QStringLiteral("options"),
       QJsonObject{{QStringLiteral("tabSize"), editor_->tabSize()},
                   {QStringLiteral("insertSpaces"), editor_->insertSpaces()}}},
  };

  lsp_->request(
      QStringLiteral("textDocument/formatting"), params,
      [this, filePath](const QJsonValue& result, const QJsonObject& error) {
        if (!error.isEmpty() || !result.isArray()) {
          showToast(tr("Formatting failed."));
          return;
        }
        const QJsonArray edits = result.toArray();
        if (edits.isEmpty()) {
          showToast(tr("No formatting changes needed."));
          return;
        }

        QJsonObject workspaceEdit;
        workspaceEdit.insert(
            QStringLiteral("changes"),
            QJsonObject{{toFileUri(filePath), edits}});
        if (applyWorkspaceEdit(workspaceEdit)) {
          showToast(tr("Document formatted."));
        } else {
          showToast(tr("Failed to apply formatting edits."));
        }
      });
}

bool MainWindow::applyWorkspaceEdit(const QJsonObject& workspaceEdit) {
  if (workspaceEdit.isEmpty()) {
    return true;
  }

  auto applyEditsToDocument = [](QTextDocument* doc, const QJsonArray& edits,
                                 QString* outError) {
    if (!doc) {
      if (outError) {
        *outError = QStringLiteral("Missing document.");
      }
      return false;
    }

    struct ResolvedEdit final {
      int start = 0;
      int end = 0;
      QString newText;
    };

    QVector<ResolvedEdit> resolved;
    resolved.reserve(edits.size());
    for (const QJsonValue& value : edits) {
      if (!value.isObject()) {
        continue;
      }
      const QJsonObject editObj = value.toObject();
      QJsonObject rangeObj = editObj.value(QStringLiteral("range")).toObject();
      if (rangeObj.isEmpty()) {
        rangeObj = editObj.value(QStringLiteral("replace")).toObject();
      }
      if (rangeObj.isEmpty()) {
        rangeObj = editObj.value(QStringLiteral("insert")).toObject();
      }
      if (rangeObj.isEmpty()) {
        if (outError) {
          *outError = QStringLiteral("Invalid text edit range.");
        }
        return false;
      }

      int startPos = 0;
      int endPos = 0;
      if (!lspRangeToDocumentOffsets(doc, rangeObj, &startPos, &endPos)) {
        if (outError) {
          *outError = QStringLiteral("Could not resolve text edit range.");
        }
        return false;
      }
      ResolvedEdit edit;
      edit.start = startPos;
      edit.end = endPos;
      edit.newText = editObj.value(QStringLiteral("newText")).toString();
      resolved.push_back(edit);
    }

    std::sort(resolved.begin(), resolved.end(),
              [](const ResolvedEdit& a, const ResolvedEdit& b) {
                if (a.start != b.start) {
                  return a.start > b.start;
                }
                return a.end > b.end;
              });

    QTextCursor cursor(doc);
    cursor.beginEditBlock();
    for (const ResolvedEdit& edit : resolved) {
      cursor.setPosition(edit.start);
      cursor.setPosition(edit.end, QTextCursor::KeepAnchor);
      cursor.insertText(edit.newText);
    }
    cursor.endEditBlock();
    return true;
  };

  auto applyEditsToPath = [this, &applyEditsToDocument](const QString& filePath,
                                                        const QJsonArray& edits,
                                                        QString* outError) {
    if (filePath.trimmed().isEmpty()) {
      if (outError) {
        *outError = QStringLiteral("Workspace edit path is empty.");
      }
      return false;
    }

    const QString absPath = QFileInfo(filePath).absoluteFilePath();
    if (editor_) {
      if (auto* openEditor = qobject_cast<QPlainTextEdit*>(
              editor_->editorWidgetForFile(absPath))) {
        return applyEditsToDocument(openEditor->document(), edits, outError);
      }
    }

    QString text;
    QFile in(absPath);
    if (in.exists()) {
      if (!in.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (outError) {
          *outError = QStringLiteral("Could not open %1 for reading.").arg(absPath);
        }
        return false;
      }
      text = QString::fromUtf8(in.readAll());
      in.close();
    }

    QTextDocument doc;
    doc.setPlainText(text);
    if (!applyEditsToDocument(&doc, edits, outError)) {
      return false;
    }

    QDir().mkpath(QFileInfo(absPath).absolutePath());
    QSaveFile out(absPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
      if (outError) {
        *outError = QStringLiteral("Could not open %1 for writing.").arg(absPath);
      }
      return false;
    }
    const QByteArray bytes = doc.toPlainText().toUtf8();
    if (out.write(bytes) != bytes.size() || !out.commit()) {
      if (outError) {
        *outError = QStringLiteral("Failed writing %1.").arg(absPath);
      }
      return false;
    }
    return true;
  };

  QString error;
  bool touched = false;

  auto applyChangesObject = [&](const QJsonObject& changesObj) {
    const QStringList keys = changesObj.keys();
    for (const QString& key : keys) {
      const QString path = pathFromUriOrPath(key);
      const QJsonArray edits = changesObj.value(key).toArray();
      if (!applyEditsToPath(path, edits, &error)) {
        return false;
      }
      touched = true;
    }
    return true;
  };

  if (workspaceEdit.contains(QStringLiteral("changes"))) {
    if (!applyChangesObject(
            workspaceEdit.value(QStringLiteral("changes")).toObject())) {
      if (output_) {
        output_->appendLine(tr("[LSP] Workspace edit failed: %1").arg(error));
      }
      return false;
    }
  }

  if (workspaceEdit.contains(QStringLiteral("documentChanges"))) {
    const QJsonArray docChanges =
        workspaceEdit.value(QStringLiteral("documentChanges")).toArray();
    for (const QJsonValue& value : docChanges) {
      if (!value.isObject()) {
        continue;
      }
      const QJsonObject entry = value.toObject();

      const QString kind = entry.value(QStringLiteral("kind")).toString();
      if (kind == QStringLiteral("create")) {
        const QString path = pathFromUriOrPath(
            entry.value(QStringLiteral("uri")).toString());
        if (!path.trimmed().isEmpty()) {
          QDir().mkpath(QFileInfo(path).absolutePath());
          if (!QFileInfo::exists(path)) {
            QFile f(path);
            if (f.open(QIODevice::WriteOnly)) {
              f.close();
              touched = true;
            }
          }
        }
        continue;
      }
      if (kind == QStringLiteral("rename")) {
        const QString oldPath = pathFromUriOrPath(
            entry.value(QStringLiteral("oldUri")).toString());
        const QString newPath = pathFromUriOrPath(
            entry.value(QStringLiteral("newUri")).toString());
        if (!oldPath.isEmpty() && !newPath.isEmpty() && oldPath != newPath) {
          QDir().mkpath(QFileInfo(newPath).absolutePath());
          if (QFile::exists(oldPath) && QFile::rename(oldPath, newPath)) {
            touched = true;
          }
        }
        continue;
      }
      if (kind == QStringLiteral("delete")) {
        const QString path = pathFromUriOrPath(
            entry.value(QStringLiteral("uri")).toString());
        if (!path.isEmpty() && QFile::exists(path) && QFile::remove(path)) {
          touched = true;
        }
        continue;
      }

      const QJsonObject docObj = entry.value(QStringLiteral("textDocument")).toObject();
      const QString path =
          pathFromUriOrPath(docObj.value(QStringLiteral("uri")).toString());
      const QJsonArray edits = entry.value(QStringLiteral("edits")).toArray();
      if (!applyEditsToPath(path, edits, &error)) {
        if (output_) {
          output_->appendLine(tr("[LSP] Workspace edit failed: %1").arg(error));
        }
        return false;
      }
      touched = true;
    }
  }

  if (touched) {
    scheduleOutlineRefresh();
  }
  return true;
}

// === Sketch Menu Actions ===
void MainWindow::verifySketch() {
  const QString sketchFolder = currentSketchFolderPath();
  if (sketchFolder.isEmpty()) {
    QMessageBox::warning(this, tr("No Sketch Open"),
                         tr("Please open or create a sketch first."));
    return;
  }

  if (editor_) {
    editor_->saveAll();
  }

  if (currentFqbn().isEmpty()) {
    QMessageBox::warning(this, tr("No Board Selected"),
                         tr("Please select a board first."));
    return;
  }

  if (!arduinoCli_) return;

  lastCliJobKind_ = CliJobKind::Compile;
  output_->clear();
  if (outputDock_) {
    outputDock_->show();
    outputDock_->raise();
  }
  output_->appendHtml(QString("<b>%1</b>").arg(tr("Compiling sketch...")));
  updateStopActionState();

  // Get compiler settings
  QSettings settings;
  settings.beginGroup("Preferences");
  const bool verbose = settings.value("verboseCompile", false).toBool();
  const QString warningsLevel = settings.value("compilerWarnings", "none").toString();
  settings.endGroup();

  QStringList args = {"compile", "--fqbn", currentFqbn(), "--warnings", warningsLevel};
  if (verbose) {
    args << "--verbose";
  }
  if (actionOptimizeForDebug_ && actionOptimizeForDebug_->isChecked()) {
    args << "--optimize-for-debug";
  }

  QDir buildDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/rewritto/build");
  buildDir.mkpath(buildDir.absolutePath());
  args << "--build-path" << buildDir.absolutePath();
  args << sketchFolder;

  arduinoCli_->run(args);
}

void MainWindow::fastUploadSketch() {
  const QString sketchFolder = currentSketchFolderPath();
  if (sketchFolder.isEmpty()) {
    QMessageBox::warning(this, tr("No Sketch Open"),
                         tr("Please open or create a sketch first."));
    return;
  }

  QString uploadReason;
  if (!canUploadWithoutCompile(&uploadReason)) {
    QMessageBox::information(
        this, tr("Upload Requires Verify"),
        uploadReason.isEmpty()
            ? tr("Verify the sketch before uploading without compile.")
            : uploadReason + QStringLiteral("\n\n") +
                  tr("Run Verify or Verify and Upload first."));
    updateUploadActionStates();
    return;
  }

  const QString fqbn = currentFqbn().trimmed();
  if (fqbn.isEmpty()) {
    QMessageBox::warning(this, tr("No Board Selected"),
                         tr("Please select a board first."));
    return;
  }

  const QString selectedPort = currentPort().trimmed();
  const bool allowMissingPort = isPortOptionalForFqbn(fqbn);
  if (selectedPort.isEmpty() && !allowMissingPort) {
    QMessageBox::warning(this, tr("No Port Selected"),
                         tr("Please select a port first."));
    return;
  }
  if (!selectedPort.isEmpty() && currentPortIsMissing()) {
    QMessageBox::warning(this, tr("Port Not Available"),
                         tr("The selected port is not available. Please select a connected port."));
    return;
  }

  if (!arduinoCli_) return;

  lastCliJobKind_ = CliJobKind::Upload;
  output_->clear();
  if (outputDock_) {
    outputDock_->show();
    outputDock_->raise();
  }
  output_->appendHtml(QString("<b>%1</b>").arg(tr("Uploading prebuilt binary...")));
  updateStopActionState();

  QSettings settings;
  settings.beginGroup("Preferences");
  const bool verboseUpload = settings.value("verboseUpload", false).toBool();
  settings.endGroup();

  clearPendingUploadFlow();
  pendingUploadFlow_.sketchFolder = sketchFolder;
  pendingUploadFlow_.buildPath = lastSuccessfulCompile_.buildPath;
  pendingUploadFlow_.fqbn = fqbn;
  pendingUploadFlow_.port = selectedPort;
  pendingUploadFlow_.protocol =
      selectedPort.isEmpty() ? QString{} : currentPortProtocol();
  pendingUploadFlow_.verboseUpload = verboseUpload;
  pendingUploadFlow_.useInputDir = true;
  pendingUploadFlow_.allowMissingPort = allowMissingPort;
  pendingUploadFlow_.uf2FallbackAttempted = false;
  pendingUploadFlow_.finalJobKind = CliJobKind::Upload;

  if (allowMissingPort && selectedPort.isEmpty()) {
    output_->appendLine(
        tr("No serial port selected. Trying board-specific upload flow."));
  }

  if (!startUploadFromPendingFlow()) {
    output_->appendHtml(QString("<span style=\"color:#d32f2f;\"><b>%1</b></span>")
                            .arg(tr("Upload failed: could not start upload step.")));
    clearPendingUploadFlow();
    updateStopActionState();
  }
}

void MainWindow::uploadSketch() {
  const QString sketchFolder = currentSketchFolderPath();
  if (sketchFolder.isEmpty()) {
    QMessageBox::warning(this, tr("No Sketch Open"),
                         tr("Please open or create a sketch first."));
    return;
  }

  if (editor_) {
    editor_->saveAll();
  }

  const QString fqbn = currentFqbn().trimmed();
  if (fqbn.isEmpty()) {
    QMessageBox::warning(this, tr("No Board Selected"),
                         tr("Please select a board first."));
    return;
  }

  const QString selectedPort = currentPort().trimmed();
  const bool allowMissingPort = isPortOptionalForFqbn(fqbn);
  if (selectedPort.isEmpty() && !allowMissingPort) {
    QMessageBox::warning(this, tr("No Port Selected"),
                         tr("Please select a port first."));
    return;
  }
  if (!selectedPort.isEmpty() && currentPortIsMissing()) {
    QMessageBox::warning(this, tr("Port Not Available"),
                         tr("The selected port is not available. Please select a connected port."));
    return;
  }

  // First compile, then upload
  lastCliJobKind_ = CliJobKind::UploadCompile;
  output_->clear();
  if (outputDock_) {
    outputDock_->show();
    outputDock_->raise();
  }
  output_->appendHtml(QString("<b>%1</b>").arg(tr("Compiling sketch for upload...")));
  updateStopActionState();

  QSettings settings;
  settings.beginGroup("Preferences");
  const bool verbose = settings.value("verboseCompile", false).toBool();
  const QString warningsLevel = settings.value("compilerWarnings", "none").toString();
  const bool verboseUpload = settings.value("verboseUpload", false).toBool();
	  settings.endGroup();

	  QStringList args = {"compile", "--fqbn", fqbn, "--warnings", warningsLevel};
	  if (verbose) {
	    args << "--verbose";
	  }
  if (actionOptimizeForDebug_ && actionOptimizeForDebug_->isChecked()) {
    args << "--optimize-for-debug";
  }

  QDir buildDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/rewritto/upload");
  buildDir.mkpath(buildDir.absolutePath());
  args << "--build-path" << buildDir.absolutePath();
  args << sketchFolder;

	  // Store pending upload info
	  pendingUploadFlow_.sketchFolder = sketchFolder;
	  pendingUploadFlow_.buildPath = buildDir.absolutePath();
	  pendingUploadFlow_.fqbn = fqbn;
	  pendingUploadFlow_.port = selectedPort;
	  pendingUploadFlow_.protocol =
	      selectedPort.isEmpty() ? QString{} : currentPortProtocol();
	  pendingUploadFlow_.verboseCompile = verbose;
	  pendingUploadFlow_.verboseUpload = verboseUpload;
	  pendingUploadFlow_.useInputDir = true;
	  pendingUploadFlow_.allowMissingPort = allowMissingPort;
	  pendingUploadFlow_.uf2FallbackAttempted = false;
	  pendingUploadFlow_.warnings = warningsLevel;
	  pendingUploadFlow_.finalJobKind = CliJobKind::Upload;

  if (allowMissingPort && selectedPort.isEmpty()) {
    output_->appendLine(
        tr("No serial port selected. Will attempt UF2-based upload after compile."));
  }

	  arduinoCli_->run(args);
}

void MainWindow::stopOperation() {
  if (arduinoCli_ && arduinoCli_->isRunning()) {
    cliCancelRequested_ = true;
    arduinoCli_->stop();
    output_->appendLine(tr("Cancelled."));
  }
}

void MainWindow::uploadUsingProgrammer() {
  const QString sketchFolder = currentSketchFolderPath();
  if (sketchFolder.isEmpty()) {
    QMessageBox::warning(this, tr("No Sketch Open"),
                         tr("Please open or create a sketch first."));
    return;
  }

  if (editor_) {
    editor_->saveAll();
  }

  const QString fqbn = currentFqbn();
  if (fqbn.isEmpty()) {
    QMessageBox::warning(this, tr("No Board Selected"),
                         tr("Please select a board first."));
    return;
  }

  const QString programmer = currentProgrammer();
  if (programmer.isEmpty()) {
    QMessageBox::warning(this, tr("No Programmer Selected"),
                         tr("Please select a programmer from Tools > Programmer first."));
    return;
  }

  if (!arduinoCli_) {
    return;
  }

  const QString port = currentPort().trimmed();
  const bool portOk = !port.isEmpty() && !currentPortIsMissing();

  // First compile, then upload (using programmer).
  lastCliJobKind_ = CliJobKind::UploadCompile;
  output_->clear();
  if (outputDock_) {
    outputDock_->show();
    outputDock_->raise();
  }
  output_->appendHtml(
      QString("<b>%1</b>").arg(tr("Compiling sketch for upload using programmer...")));
  updateStopActionState();

  QSettings settings;
  settings.beginGroup("Preferences");
  const bool verboseCompile = settings.value("verboseCompile", false).toBool();
  const QString warningsLevel =
      settings.value("compilerWarnings", "none").toString();
  const bool verboseUpload = settings.value("verboseUpload", false).toBool();
  settings.endGroup();

  QStringList args = {"compile", "--fqbn", fqbn, "--warnings", warningsLevel};
  if (verboseCompile) {
    args << "--verbose";
  }
  if (actionOptimizeForDebug_ && actionOptimizeForDebug_->isChecked()) {
    args << "--optimize-for-debug";
  }

  QDir buildDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation) +
                "/rewritto/upload-programmer");
  buildDir.mkpath(buildDir.absolutePath());
  args << "--build-path" << buildDir.absolutePath();
  args << sketchFolder;

  pendingUploadFlow_.sketchFolder = sketchFolder;
  pendingUploadFlow_.buildPath = buildDir.absolutePath();
  pendingUploadFlow_.fqbn = fqbn;
  pendingUploadFlow_.port = portOk ? port : QString{};
  pendingUploadFlow_.protocol = portOk ? currentPortProtocol() : QString{};
  pendingUploadFlow_.programmer = programmer;
  pendingUploadFlow_.verboseCompile = verboseCompile;
  pendingUploadFlow_.verboseUpload = verboseUpload;
  pendingUploadFlow_.useInputDir = true;
  pendingUploadFlow_.allowMissingPort = false;
  pendingUploadFlow_.uf2FallbackAttempted = false;
  pendingUploadFlow_.warnings = warningsLevel;
  pendingUploadFlow_.finalJobKind = CliJobKind::UploadUsingProgrammer;

  arduinoCli_->run(args);
}

void MainWindow::exportCompiledBinary() {
  const QString sketchFolder = currentSketchFolderPath();
  if (sketchFolder.isEmpty()) {
    QMessageBox::warning(this, tr("No Sketch Open"),
                         tr("Please open or create a sketch first."));
    return;
  }

  // Compile first to generate binary
  verifySketch();

  // After compile succeeds, export binary
  connect(arduinoCli_, &ArduinoCli::finished, this,
          [this, sketchFolder](int exitCode, QProcess::ExitStatus) {
    if (exitCode == 0) {
      const QString buildPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                                + "/rewritto/build";
      const QString sketchName = QFileInfo(sketchFolder).fileName();

      QDir dir(buildPath);
      const QFileInfoList artifacts =
          dir.entryInfoList(QStringList{"*.uf2", "*.bin", "*.hex", "*.elf"},
                            QDir::Files, QDir::Name | QDir::IgnoreCase);

      QString chosenArtifact;
      auto pickBySuffix = [&](const QString& suffix) {
        for (const QFileInfo& fi : artifacts) {
          if (!fi.isFile()) continue;
          if (!fi.fileName().toLower().endsWith(suffix)) continue;
          if (!sketchName.isEmpty() &&
              !fi.fileName().toLower().startsWith(sketchName.toLower())) {
            continue;
          }
          chosenArtifact = fi.absoluteFilePath();
          return;
        }
      };

      pickBySuffix(".uf2");
      if (chosenArtifact.isEmpty()) pickBySuffix(".bin");
      if (chosenArtifact.isEmpty()) pickBySuffix(".hex");
      if (chosenArtifact.isEmpty()) pickBySuffix(".elf");

      // Fallback: just pick the first artifact we can find.
      if (chosenArtifact.isEmpty() && !artifacts.isEmpty()) {
        chosenArtifact = artifacts.first().absoluteFilePath();
      }

      if (!chosenArtifact.isEmpty() && QFileInfo(chosenArtifact).isFile()) {
        const QString suggestedName = QFileInfo(chosenArtifact).fileName();
        const QString savePath = QFileDialog::getSaveFileName(
            this, tr("Export Compiled Binary"),
            QDir::homePath() + "/" + suggestedName,
            tr("Binary Files (*.uf2 *.bin *.hex *.elf);;All Files (*)"));

        if (!savePath.isEmpty()) {
          if (QFile::exists(savePath)) {
            QFile::remove(savePath);
          }
          QFile::copy(chosenArtifact, savePath);
          QMessageBox::information(this, tr("Export Complete"),
                                   tr("Binary exported to: %1").arg(savePath));
        }
      } else {
        QMessageBox::warning(this, tr("Export Failed"),
                             tr("Compiled binary not found. Compilation may have failed."));
      }
    }
    // Disconnect to avoid multiple calls
    disconnect(arduinoCli_, &ArduinoCli::finished, this, nullptr);
  }, Qt::SingleShotConnection);
}

void MainWindow::showSketchFolder() {
  const QString sketchFolder = currentSketchFolderPath();
  if (sketchFolder.isEmpty()) {
    QMessageBox::warning(this, tr("No Sketch Open"),
                         tr("Please open or create a sketch first."));
    return;
  }

  QDesktopServices::openUrl(QUrl::fromLocalFile(sketchFolder));
}

void MainWindow::renameSketch() {
  const QString oldDir = currentSketchFolderPath();
  if (oldDir.isEmpty()) {
    QMessageBox::warning(this, tr("No Sketch Open"),
                         tr("Please open or create a sketch first."));
    return;
  }

  const QString oldName = QFileInfo(oldDir).fileName();

  bool ok = false;
  QString newName = QInputDialog::getText(this, tr("Rename Sketch"),
                                            tr("Sketch name:"),
                                            QLineEdit::Normal,
                                            oldName, &ok);
  newName = newName.trimmed();
  if (!ok || newName.isEmpty() || newName == oldName) {
    return;
  }
  if (newName.contains('/') || newName.contains('\\')) {
    QMessageBox::warning(this, tr("Invalid Sketch Name"),
                         tr("Sketch name must not contain path separators."));
    return;
  }

  const QString parentDir = QFileInfo(oldDir).absolutePath();
  const QString newDir = QDir(parentDir).absoluteFilePath(newName);
  if (QFileInfo::exists(newDir)) {
    QMessageBox::warning(this, tr("Rename Failed"),
                         tr("A folder named '%1' already exists.").arg(newName));
    return;
  }

  const bool wasPinned = pinnedSketches().contains(oldDir);

  // Rename the folder first.
  if (!QDir().rename(oldDir, newDir)) {
    QMessageBox::warning(this, tr("Rename Failed"),
                         tr("Could not rename sketch folder."));
    return;
  }

  // Then rename the primary .ino to match the new folder name.
  const QString oldPrimary = QDir(newDir).absoluteFilePath(oldName + ".ino");
  const QString newPrimary = QDir(newDir).absoluteFilePath(newName + ".ino");
  if (QFileInfo(oldPrimary).isFile() && oldPrimary != newPrimary) {
    if (QFile::exists(newPrimary)) {
      QFile::remove(newPrimary);
    }
    if (!QFile::rename(oldPrimary, newPrimary)) {
      // Roll back folder rename to avoid breaking the sketch structure.
      (void)QDir().rename(newDir, oldDir);
      QMessageBox::warning(this, tr("Rename Failed"),
                           tr("Could not rename the primary .ino file."));
      return;
    }
  }

  // Update pinned + recent sketches (folder paths).
  {
    QStringList recent = recentSketches();
    recent.removeAll(oldDir);
    setRecentSketches(recent);
  }
  if (wasPinned) {
    QStringList pinned = pinnedSketches();
    pinned.removeAll(oldDir);
    pinned.removeAll(newDir);
    pinned.append(newDir);
    setPinnedSketches(pinned);
  }

  (void)openSketchFolderInUi(newDir);

  QMessageBox::information(this, tr("Sketch Renamed"),
                           tr("Sketch renamed to: %1").arg(newName));
}

void MainWindow::addFileToSketch() {
  const QString sketchDir = currentSketchFolderPath();
  if (sketchDir.isEmpty()) {
    QMessageBox::warning(this, tr("No Sketch Open"),
                         tr("Please open or create a sketch first."));
    return;
  }

  QString fileName = QInputDialog::getText(this, tr("Add File to Sketch"),
                                           tr("File name (e.g., header.h):"));
  fileName = fileName.trimmed();
  if (fileName.isEmpty()) {
    return;
  }
  if (fileName.contains('/') || fileName.contains('\\')) {
    QMessageBox::warning(this, tr("Invalid File Name"),
                         tr("File name must not contain path separators."));
    return;
  }

  const QString newFilePath = QDir(sketchDir).absoluteFilePath(fileName);

  QFile file(newFilePath);
  if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    file.close();
    if (editor_) {
      editor_->openFile(newFilePath);
      updateWelcomeVisibility();
    }
  } else {
    QMessageBox::warning(this, tr("Create File Failed"),
                         tr("Could not create file: %1").arg(newFilePath));
  }
}

void MainWindow::addZipLibrary() {
  const QString filePath = QFileDialog::getOpenFileName(
      this,
      tr("Add .ZIP Library"),
      QDir::homePath(),
      tr("ZIP Files (*.zip);;All Files (*)"));

  if (!filePath.isEmpty()) {
    output_->clear();
    output_->appendLine(tr("Installing library from: %1").arg(filePath));

    if (arduinoCli_) {
      lastCliJobKind_ = CliJobKind::LibraryInstall;
      updateStopActionState();
      arduinoCli_->run({"lib", "install", filePath});
    }
  }
}

void MainWindow::exportSetupProfile() {
  QSettings settings;
  settings.beginGroup(QStringLiteral("Preferences"));
  QStringList additionalUrls;
  if (settings.contains(QStringLiteral("additionalUrls"))) {
    additionalUrls = settings.value(QStringLiteral("additionalUrls")).toStringList();
  }
  settings.endGroup();

  const ArduinoCliConfigSnapshot snapshot = readArduinoCliConfigSnapshot(
      arduinoCli_ ? arduinoCli_->arduinoCliConfigPath() : QString{});
  if (additionalUrls.isEmpty()) {
    additionalUrls = snapshot.additionalUrls;
  }
  additionalUrls = normalizeStringList(additionalUrls);

  QVector<InstalledCoreSnapshot> cores;
  QVector<InstalledLibrarySnapshot> libraries;

  if (arduinoCli_ && !arduinoCli_->arduinoCliPath().trimmed().isEmpty()) {
    const QString cliPath = arduinoCli_->arduinoCliPath().trimmed();

    const CommandResult coreListResult =
        runCommandBlocking(cliPath,
                           arduinoCli_->withGlobalFlags({QStringLiteral("core"),
                                                         QStringLiteral("list"),
                                                         QStringLiteral("--json")}),
                           {}, {}, 120000);
    if (commandSucceeded(coreListResult)) {
      cores = parseInstalledCoresFromJson(coreListResult.stdoutText.toUtf8());
    } else if (output_) {
      output_->appendLine(
          tr("[Profile Export] Could not read installed cores: %1")
              .arg(commandErrorSummary(coreListResult)));
    }

    const CommandResult libListResult =
        runCommandBlocking(cliPath,
                           arduinoCli_->withGlobalFlags({QStringLiteral("lib"),
                                                         QStringLiteral("list"),
                                                         QStringLiteral("--json")}),
                           {}, {}, 120000);
    if (commandSucceeded(libListResult)) {
      libraries =
          parseInstalledLibrariesFromJson(libListResult.stdoutText.toUtf8());
    } else if (output_) {
      output_->appendLine(
          tr("[Profile Export] Could not read installed libraries: %1")
              .arg(commandErrorSummary(libListResult)));
    }
  }

  QJsonObject prefs;
  settings.beginGroup(QStringLiteral("Preferences"));
  const QStringList prefKeys = {
      QStringLiteral("theme"),          QStringLiteral("locale"),
      QStringLiteral("uiScale"),        QStringLiteral("sketchbookDir"),
      QStringLiteral("tabSize"),        QStringLiteral("insertSpaces"),
      QStringLiteral("showIndentGuides"), QStringLiteral("showWhitespace"),
      QStringLiteral("defaultLineEnding"),
      QStringLiteral("trimTrailingWhitespace"),
      QStringLiteral("autosaveEnabled"), QStringLiteral("autosaveInterval"),
      QStringLiteral("compilerWarnings"),
      QStringLiteral("verboseCompile"), QStringLiteral("verboseUpload"),
      QStringLiteral("proxyType"),      QStringLiteral("proxyHost"),
      QStringLiteral("proxyPort"),      QStringLiteral("proxyUsername"),
      QStringLiteral("noProxyHosts"),   QStringLiteral("checkIndexesOnStartup"),
  };
  for (const QString& key : prefKeys) {
    if (settings.contains(key)) {
      prefs.insert(key, QJsonValue::fromVariant(settings.value(key)));
    }
  }
  settings.endGroup();

  QJsonArray additionalUrlsJson;
  for (const QString& url : additionalUrls) {
    additionalUrlsJson.append(url);
  }
  prefs.insert(QStringLiteral("additionalUrls"), additionalUrlsJson);

  QJsonObject root;
  root.insert(QStringLiteral("format"), QString::fromLatin1(kSetupProfileFormat));
  root.insert(QStringLiteral("version"), kSetupProfileVersion);
  root.insert(QStringLiteral("generatedAt"),
              QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
  root.insert(QStringLiteral("ideVersion"), QStringLiteral(REWRITTO_IDE_VERSION));
  root.insert(QStringLiteral("notes"),
              tr("Proxy password is intentionally not exported."));
  root.insert(QStringLiteral("preferences"), prefs);

  QJsonObject selected;
  selected.insert(QStringLiteral("fqbn"), currentFqbn());
  selected.insert(QStringLiteral("port"), currentPort());
  selected.insert(QStringLiteral("programmer"), currentProgrammer());
  root.insert(QStringLiteral("selected"), selected);

  QJsonArray coresJson;
  for (const InstalledCoreSnapshot& core : cores) {
    QJsonObject item;
    item.insert(QStringLiteral("id"), core.id);
    item.insert(QStringLiteral("installed_version"), core.installedVersion);
    item.insert(QStringLiteral("latest_version"), core.latestVersion);
    item.insert(QStringLiteral("name"), core.name);
    coresJson.append(item);
  }
  root.insert(QStringLiteral("installed_cores"), coresJson);

  QJsonArray libsJson;
  for (const InstalledLibrarySnapshot& library : libraries) {
    QJsonObject item;
    item.insert(QStringLiteral("name"), library.name);
    item.insert(QStringLiteral("version"), library.version);
    item.insert(QStringLiteral("location"), library.location);
    QJsonArray includes;
    for (const QString& include : library.providesIncludes) {
      includes.append(include);
    }
    item.insert(QStringLiteral("provides_includes"), includes);
    libsJson.append(item);
  }
  root.insert(QStringLiteral("installed_libraries"), libsJson);

  QString defaultPath = QDir(QDir::homePath())
                            .absoluteFilePath(QStringLiteral("rewritto-setup-profile-%1.json")
                                                  .arg(QDateTime::currentDateTime().toString(
                                                      QStringLiteral("yyyyMMdd-HHmmss"))));
  const QString targetPath = QFileDialog::getSaveFileName(
      this, tr("Export Setup Profile"), defaultPath,
      tr("Rewritto Setup Profile (*.json);;JSON Files (*.json);;All Files (*)"));
  if (targetPath.trimmed().isEmpty()) {
    return;
  }

  QSaveFile save(targetPath);
  if (!save.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QMessageBox::warning(this, tr("Export Setup Profile"),
                         tr("Could not write profile file."));
    return;
  }
  const QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Indented);
  if (save.write(payload) != payload.size() || !save.commit()) {
    QMessageBox::warning(this, tr("Export Setup Profile"),
                         tr("Failed to save profile file."));
    return;
  }

  showToast(tr("Setup profile exported"));
  if (output_) {
    output_->appendLine(tr("[Setup Profile] Exported to: %1").arg(targetPath));
  }
}

void MainWindow::importSetupProfile() {
  const QString profilePath = QFileDialog::getOpenFileName(
      this, tr("Import Setup Profile"), QDir::homePath(),
      tr("Rewritto Setup Profile (*.json);;JSON Files (*.json);;All Files (*)"));
  if (profilePath.trimmed().isEmpty()) {
    return;
  }

  QFile file(profilePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QMessageBox::warning(this, tr("Import Setup Profile"),
                         tr("Could not open profile file."));
    return;
  }
  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  if (!doc.isObject()) {
    QMessageBox::warning(this, tr("Import Setup Profile"),
                         tr("Profile file is not valid JSON."));
    return;
  }
  const QJsonObject root = doc.object();
  const QString format = root.value(QStringLiteral("format")).toString().trimmed();
  if (!format.isEmpty() && format != QString::fromLatin1(kSetupProfileFormat)) {
    QMessageBox::warning(this, tr("Import Setup Profile"),
                         tr("Unsupported profile format: %1").arg(format));
    return;
  }

  auto jsonArrayToStringList = [](const QJsonValue& value) {
    QStringList out;
    const QJsonArray array = value.toArray();
    out.reserve(array.size());
    for (const QJsonValue& item : array) {
      const QString text = item.toString().trimmed();
      if (!text.isEmpty()) {
        out << text;
      }
    }
    return normalizeStringList(out);
  };

  const QJsonObject prefs = root.value(QStringLiteral("preferences")).toObject();
  QStringList additionalUrls = jsonArrayToStringList(prefs.value(QStringLiteral("additionalUrls")));

  struct CoreInstallSpec final {
    QString id;
    QString version;
  };
  QVector<CoreInstallSpec> coresToInstall;
  const QJsonArray coresArray = root.value(QStringLiteral("installed_cores")).toArray();
  coresToInstall.reserve(coresArray.size());
  for (const QJsonValue& value : coresArray) {
    if (!value.isObject()) {
      continue;
    }
    const QJsonObject obj = value.toObject();
    const QString id = obj.value(QStringLiteral("id")).toString().trimmed();
    if (id.isEmpty()) {
      continue;
    }
    coresToInstall.push_back(
        {id, obj.value(QStringLiteral("installed_version")).toString().trimmed()});
  }

  struct LibraryInstallSpec final {
    QString name;
    QString version;
  };
  QVector<LibraryInstallSpec> librariesToInstall;
  const QJsonArray libsArray =
      root.value(QStringLiteral("installed_libraries")).toArray();
  librariesToInstall.reserve(libsArray.size());
  for (const QJsonValue& value : libsArray) {
    if (!value.isObject()) {
      continue;
    }
    const QJsonObject obj = value.toObject();
    const QString name = obj.value(QStringLiteral("name")).toString().trimmed();
    if (name.isEmpty()) {
      continue;
    }
    librariesToInstall.push_back(
        {name, obj.value(QStringLiteral("version")).toString().trimmed()});
  }

  const QString summary = tr("Apply setup profile?\n\n%1 board URL(s)\n%2 core(s)\n%3 library(s)")
                              .arg(additionalUrls.size())
                              .arg(coresToInstall.size())
                              .arg(librariesToInstall.size());
  if (QMessageBox::question(this, tr("Import Setup Profile"), summary,
                            QMessageBox::Yes | QMessageBox::No,
                            QMessageBox::Yes) != QMessageBox::Yes) {
    return;
  }

  QSettings settings;
  settings.beginGroup(QStringLiteral("Preferences"));
  const QStringList prefKeys = {
      QStringLiteral("theme"),       QStringLiteral("locale"),
      QStringLiteral("uiScale"),     QStringLiteral("sketchbookDir"),
      QStringLiteral("tabSize"),     QStringLiteral("insertSpaces"),
      QStringLiteral("showIndentGuides"), QStringLiteral("showWhitespace"),
      QStringLiteral("defaultLineEnding"),
      QStringLiteral("trimTrailingWhitespace"),
      QStringLiteral("autosaveEnabled"), QStringLiteral("autosaveInterval"),
      QStringLiteral("compilerWarnings"),
      QStringLiteral("verboseCompile"), QStringLiteral("verboseUpload"),
      QStringLiteral("proxyType"),   QStringLiteral("proxyHost"),
      QStringLiteral("proxyPort"),   QStringLiteral("proxyUsername"),
      QStringLiteral("noProxyHosts"), QStringLiteral("checkIndexesOnStartup"),
  };
  for (const QString& key : prefKeys) {
    if (prefs.contains(key)) {
      settings.setValue(key, prefs.value(key).toVariant());
    }
  }
  settings.endGroup();

  QString mergeError;
  QStringList mergedUrls;
  if (!additionalUrls.isEmpty() &&
      !mergeAdditionalBoardUrlsIntoPreferences(additionalUrls, &mergeError,
                                               &mergedUrls)) {
    QMessageBox::warning(this, tr("Import Setup Profile"),
                         tr("Could not merge board manager URLs.\n\n%1")
                             .arg(mergeError));
    return;
  }
  if (!mergedUrls.isEmpty()) {
    settings.beginGroup(QStringLiteral("Preferences"));
    settings.setValue(QStringLiteral("additionalUrls"), mergedUrls);
    settings.endGroup();
  }

  bool installNow = false;
  if ((!coresToInstall.isEmpty() || !librariesToInstall.isEmpty()) &&
      arduinoCli_ && !arduinoCli_->arduinoCliPath().trimmed().isEmpty()) {
    installNow = QMessageBox::question(
                     this, tr("Install Components"),
                     tr("Install cores and libraries from this profile now?"),
                     QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) ==
                 QMessageBox::Yes;
  }

  QStringList failures;
  if (installNow) {
    const QString cliPath = arduinoCli_->arduinoCliPath().trimmed();
    if (output_) {
      output_->appendLine(
          tr("[Setup Profile] Updating indexes before installation..."));
    }
    const CommandResult coreIndexResult = runCommandBlocking(
        cliPath,
        arduinoCli_->withGlobalFlags({QStringLiteral("core"),
                                      QStringLiteral("update-index")}),
        {}, {}, 600000);
    if (!commandSucceeded(coreIndexResult)) {
      failures << tr("core update-index: %1")
                      .arg(commandErrorSummary(coreIndexResult));
    }
    const CommandResult libIndexResult = runCommandBlocking(
        cliPath,
        arduinoCli_->withGlobalFlags({QStringLiteral("lib"),
                                      QStringLiteral("update-index")}),
        {}, {}, 600000);
    if (!commandSucceeded(libIndexResult)) {
      failures << tr("lib update-index: %1")
                      .arg(commandErrorSummary(libIndexResult));
    }

    for (const CoreInstallSpec& core : coresToInstall) {
      const QString spec = core.version.isEmpty()
                               ? core.id
                               : QStringLiteral("%1@%2").arg(core.id, core.version);
      if (output_) {
        output_->appendLine(
            tr("[Setup Profile] Installing core %1 ...").arg(spec));
      }
      const CommandResult installResult = runCommandBlocking(
          cliPath,
          arduinoCli_->withGlobalFlags({QStringLiteral("core"),
                                        QStringLiteral("install"), spec}),
          {}, {}, 900000);
      if (!commandSucceeded(installResult)) {
        failures << tr("core install %1: %2")
                        .arg(spec, commandErrorSummary(installResult));
      }
    }

    for (const LibraryInstallSpec& lib : librariesToInstall) {
      const QString spec = lib.version.isEmpty()
                               ? lib.name
                               : QStringLiteral("%1@%2").arg(lib.name, lib.version);
      if (output_) {
        output_->appendLine(
            tr("[Setup Profile] Installing library %1 ...").arg(spec));
      }
      const CommandResult installResult = runCommandBlocking(
          cliPath,
          arduinoCli_->withGlobalFlags({QStringLiteral("lib"),
                                        QStringLiteral("install"), spec}),
          {}, {}, 900000);
      if (!commandSucceeded(installResult)) {
        failures << tr("lib install %1: %2")
                        .arg(spec, commandErrorSummary(installResult));
      }
    }
  }

  settings.beginGroup(QStringLiteral("Preferences"));
  const QString appliedTheme =
      settings.value(QStringLiteral("theme"), QStringLiteral("system")).toString();
  const double appliedScale = settings.value(QStringLiteral("uiScale"), 1.0).toDouble();
  const bool insertSpaces = settings.value(QStringLiteral("insertSpaces"), true).toBool();
  const int tabSize = settings.value(QStringLiteral("tabSize"), 2).toInt();
  const bool showIndentGuides =
      settings.value(QStringLiteral("showIndentGuides"), true).toBool();
  const bool showWhitespace =
      settings.value(QStringLiteral("showWhitespace"), false).toBool();
  const bool autosaveEnabled =
      settings.value(QStringLiteral("autosaveEnabled"), false).toBool();
  const int autosaveInterval =
      settings.value(QStringLiteral("autosaveInterval"), 30).toInt();
  const QString fontFamily =
      settings.value(QStringLiteral("editorFontFamily")).toString().trimmed();
  const int fontSize = settings.value(QStringLiteral("editorFontSize"), 0).toInt();
  settings.endGroup();

  UiScaleManager::apply(appliedScale);
  ThemeManager::apply(appliedTheme);
  rebuildContextToolbar();
  if (editor_) {
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    if (!fontFamily.isEmpty()) {
      font.setFamily(fontFamily);
    }
    if (fontSize > 0) {
      font.setPointSize(fontSize);
    }
    editor_->setTheme(isThemeDark(appliedTheme));
    editor_->setEditorFont(font);
    editor_->setEditorSettings(tabSize, insertSpaces);
    editor_->setShowIndentGuides(showIndentGuides);
    editor_->setShowWhitespace(showWhitespace);
    editor_->setAutosaveEnabled(autosaveEnabled);
    editor_->setAutosaveIntervalSeconds(autosaveInterval);
  }
  updateSketchbookView();

  if (boardsManager_) {
    boardsManager_->refresh();
  }
  if (libraryManager_) {
    libraryManager_->refresh();
  }
  refreshInstalledBoards();
  refreshConnectedPorts();

  if (failures.isEmpty()) {
    showToast(tr("Setup profile imported"));
    QMessageBox::information(
        this, tr("Import Setup Profile"),
        tr("Profile imported successfully.\n\nSome settings may require restart."));
  } else {
    QMessageBox::warning(
        this, tr("Import Setup Profile"),
        tr("Profile imported with errors:\n\n%1").arg(failures.join(QStringLiteral("\n"))));
  }
}

void MainWindow::generateProjectLockfile() {
  const QString sketchFolder = currentSketchFolderPath();
  if (sketchFolder.trimmed().isEmpty()) {
    QMessageBox::warning(this, tr("Generate Project Lockfile"),
                         tr("Open a sketch first."));
    return;
  }

  QStringList additionalUrls;
  {
    QSettings settings;
    settings.beginGroup(QStringLiteral("Preferences"));
    additionalUrls = settings.value(QStringLiteral("additionalUrls")).toStringList();
    settings.endGroup();
  }
  if (additionalUrls.isEmpty()) {
    const ArduinoCliConfigSnapshot snapshot = readArduinoCliConfigSnapshot(
        arduinoCli_ ? arduinoCli_->arduinoCliConfigPath() : QString{});
    additionalUrls = snapshot.additionalUrls;
  }
  additionalUrls = normalizeStringList(additionalUrls);

  QVector<InstalledCoreSnapshot> installedCores;
  QVector<InstalledLibrarySnapshot> installedLibraries;
  if (arduinoCli_ && !arduinoCli_->arduinoCliPath().trimmed().isEmpty()) {
    const QString cliPath = arduinoCli_->arduinoCliPath().trimmed();
    const CommandResult coreListResult = runCommandBlocking(
        cliPath,
        arduinoCli_->withGlobalFlags({QStringLiteral("core"),
                                      QStringLiteral("list"),
                                      QStringLiteral("--json")}),
        {}, {}, 120000);
    if (commandSucceeded(coreListResult)) {
      installedCores = parseInstalledCoresFromJson(coreListResult.stdoutText.toUtf8());
    }
    const CommandResult libListResult = runCommandBlocking(
        cliPath,
        arduinoCli_->withGlobalFlags({QStringLiteral("lib"),
                                      QStringLiteral("list"),
                                      QStringLiteral("--json")}),
        {}, {}, 120000);
    if (commandSucceeded(libListResult)) {
      installedLibraries =
          parseInstalledLibrariesFromJson(libListResult.stdoutText.toUtf8());
    }
  }

  QString selectedCoreId;
  const QString fqbn = currentFqbn().trimmed();
  const QStringList fqbnParts = fqbn.split(QLatin1Char(':'), Qt::SkipEmptyParts);
  if (fqbnParts.size() >= 2) {
    selectedCoreId = fqbnParts.at(0).trimmed() + QStringLiteral(":") +
                     fqbnParts.at(1).trimmed();
  }

  QVector<InstalledCoreSnapshot> lockCores;
  if (!selectedCoreId.isEmpty()) {
    for (const InstalledCoreSnapshot& core : installedCores) {
      if (core.id == selectedCoreId) {
        lockCores.push_back(core);
        break;
      }
    }
  }
  if (lockCores.isEmpty()) {
    lockCores = installedCores;
  }

  const QSet<QString> usedHeaders = collectSketchIncludeHeaders(sketchFolder);
  QVector<InstalledLibrarySnapshot> lockLibraries;
  for (const InstalledLibrarySnapshot& library : installedLibraries) {
    bool matches = false;
    for (const QString& include : library.providesIncludes) {
      if (usedHeaders.contains(include) ||
          usedHeaders.contains(QFileInfo(include).fileName())) {
        matches = true;
        break;
      }
    }
    if (!matches && !library.name.isEmpty()) {
      for (const QString& header : usedHeaders) {
        if (header.startsWith(library.name, Qt::CaseInsensitive)) {
          matches = true;
          break;
        }
      }
    }
    if (matches) {
      lockLibraries.push_back(library);
    }
  }
  if (lockLibraries.isEmpty()) {
    lockLibraries = installedLibraries;
  }

  QJsonObject root;
  root.insert(QStringLiteral("format"), QString::fromLatin1(kProjectLockFormat));
  root.insert(QStringLiteral("version"), kProjectLockVersion);
  root.insert(QStringLiteral("generatedAt"),
              QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

  QJsonObject sketchObj;
  sketchObj.insert(QStringLiteral("name"), QFileInfo(sketchFolder).fileName());
  sketchObj.insert(QStringLiteral("path"), sketchFolder);
  root.insert(QStringLiteral("sketch"), sketchObj);

  QJsonObject boardObj;
  boardObj.insert(QStringLiteral("fqbn"), fqbn);
  boardObj.insert(QStringLiteral("programmer"), currentProgrammer());
  root.insert(QStringLiteral("board"), boardObj);

  QJsonArray urlArray;
  for (const QString& url : additionalUrls) {
    urlArray.append(url);
  }
  root.insert(QStringLiteral("additional_urls"), urlArray);

  QJsonArray coreArray;
  for (const InstalledCoreSnapshot& core : lockCores) {
    QJsonObject item;
    item.insert(QStringLiteral("id"), core.id);
    item.insert(QStringLiteral("version"), core.installedVersion);
    coreArray.append(item);
  }
  root.insert(QStringLiteral("cores"), coreArray);

  QJsonArray libraryArray;
  for (const InstalledLibrarySnapshot& library : lockLibraries) {
    QJsonObject item;
    item.insert(QStringLiteral("name"), library.name);
    item.insert(QStringLiteral("version"), library.version);
    QJsonArray includes;
    for (const QString& include : library.providesIncludes) {
      includes.append(include);
    }
    item.insert(QStringLiteral("includes"), includes);
    libraryArray.append(item);
  }
  root.insert(QStringLiteral("libraries"), libraryArray);

  const QString lockPath =
      QDir(sketchFolder).absoluteFilePath(QStringLiteral("rewritto.lock"));
  QSaveFile save(lockPath);
  if (!save.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QMessageBox::warning(this, tr("Generate Project Lockfile"),
                         tr("Could not write rewritto.lock."));
    return;
  }
  const QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Indented);
  if (save.write(payload) != payload.size() || !save.commit()) {
    QMessageBox::warning(this, tr("Generate Project Lockfile"),
                         tr("Failed to save rewritto.lock."));
    return;
  }

  showToast(tr("Project lockfile generated"));
  if (output_) {
    output_->appendLine(tr("[Lockfile] Generated: %1").arg(lockPath));
  }
}

void MainWindow::bootstrapProjectLockfile() {
  const QString sketchFolder = currentSketchFolderPath();
  if (sketchFolder.trimmed().isEmpty()) {
    QMessageBox::warning(this, tr("Bootstrap Project"),
                         tr("Open a sketch first."));
    return;
  }

  const QString lockPath =
      QDir(sketchFolder).absoluteFilePath(QStringLiteral("rewritto.lock"));
  QFile file(lockPath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QMessageBox::warning(this, tr("Bootstrap Project"),
                         tr("rewritto.lock not found in current sketch folder."));
    return;
  }
  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  if (!doc.isObject()) {
    QMessageBox::warning(this, tr("Bootstrap Project"),
                         tr("rewritto.lock is not valid JSON."));
    return;
  }
  const QJsonObject root = doc.object();
  const QString format = root.value(QStringLiteral("format")).toString().trimmed();
  if (!format.isEmpty() && format != QString::fromLatin1(kProjectLockFormat)) {
    QMessageBox::warning(this, tr("Bootstrap Project"),
                         tr("Unsupported lockfile format: %1").arg(format));
    return;
  }

  auto jsonArrayToStringList = [](const QJsonValue& value) {
    QStringList out;
    const QJsonArray array = value.toArray();
    out.reserve(array.size());
    for (const QJsonValue& item : array) {
      const QString text = item.toString().trimmed();
      if (!text.isEmpty()) {
        out << text;
      }
    }
    return normalizeStringList(out);
  };

  const QStringList additionalUrls =
      jsonArrayToStringList(root.value(QStringLiteral("additional_urls")));

  struct InstallSpec final {
    QString name;
    QString version;
  };
  QVector<InstallSpec> coreSpecs;
  const QJsonArray coreArray = root.value(QStringLiteral("cores")).toArray();
  coreSpecs.reserve(coreArray.size());
  for (const QJsonValue& value : coreArray) {
    if (!value.isObject()) {
      continue;
    }
    const QJsonObject obj = value.toObject();
    const QString id = obj.value(QStringLiteral("id")).toString().trimmed();
    if (!id.isEmpty()) {
      coreSpecs.push_back({id, obj.value(QStringLiteral("version")).toString().trimmed()});
    }
  }

  QVector<InstallSpec> librarySpecs;
  const QJsonArray libraryArray = root.value(QStringLiteral("libraries")).toArray();
  librarySpecs.reserve(libraryArray.size());
  for (const QJsonValue& value : libraryArray) {
    if (!value.isObject()) {
      continue;
    }
    const QJsonObject obj = value.toObject();
    const QString name = obj.value(QStringLiteral("name")).toString().trimmed();
    if (!name.isEmpty()) {
      librarySpecs.push_back(
          {name, obj.value(QStringLiteral("version")).toString().trimmed()});
    }
  }

  const QString fqbn =
      root.value(QStringLiteral("board")).toObject().value(QStringLiteral("fqbn")).toString().trimmed();

  if (QMessageBox::question(
          this, tr("Bootstrap Project"),
          tr("Apply rewritto.lock now?\n\n%1 board URL(s)\n%2 core(s)\n%3 library(s)")
              .arg(additionalUrls.size())
              .arg(coreSpecs.size())
              .arg(librarySpecs.size()),
          QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) != QMessageBox::Yes) {
    return;
  }

  QString mergeError;
  if (!additionalUrls.isEmpty() &&
      !mergeAdditionalBoardUrlsIntoPreferences(additionalUrls, &mergeError, nullptr)) {
    QMessageBox::warning(this, tr("Bootstrap Project"),
                         tr("Could not merge board manager URLs.\n\n%1")
                             .arg(mergeError));
    return;
  }

  if (!arduinoCli_ || arduinoCli_->arduinoCliPath().trimmed().isEmpty()) {
    QMessageBox::warning(this, tr("Bootstrap Project"),
                         tr("Arduino CLI is unavailable."));
    return;
  }
  const QString cliPath = arduinoCli_->arduinoCliPath().trimmed();

  QStringList failures;
  if (output_) {
    output_->appendLine(tr("[Lockfile] Updating indexes..."));
  }
  const CommandResult coreIndexResult = runCommandBlocking(
      cliPath,
      arduinoCli_->withGlobalFlags({QStringLiteral("core"),
                                    QStringLiteral("update-index")}),
      {}, {}, 600000);
  if (!commandSucceeded(coreIndexResult)) {
    failures << tr("core update-index: %1")
                    .arg(commandErrorSummary(coreIndexResult));
  }

  const CommandResult libIndexResult = runCommandBlocking(
      cliPath,
      arduinoCli_->withGlobalFlags({QStringLiteral("lib"),
                                    QStringLiteral("update-index")}),
      {}, {}, 600000);
  if (!commandSucceeded(libIndexResult)) {
    failures << tr("lib update-index: %1")
                    .arg(commandErrorSummary(libIndexResult));
  }

  for (const InstallSpec& core : coreSpecs) {
    const QString spec = core.version.isEmpty()
                             ? core.name
                             : QStringLiteral("%1@%2").arg(core.name, core.version);
    if (output_) {
      output_->appendLine(tr("[Lockfile] Installing core %1 ...").arg(spec));
    }
    const CommandResult installResult = runCommandBlocking(
        cliPath,
        arduinoCli_->withGlobalFlags({QStringLiteral("core"),
                                      QStringLiteral("install"), spec}),
        {}, {}, 900000);
    if (!commandSucceeded(installResult)) {
      failures << tr("core install %1: %2")
                      .arg(spec, commandErrorSummary(installResult));
    }
  }

  for (const InstallSpec& lib : librarySpecs) {
    const QString spec = lib.version.isEmpty()
                             ? lib.name
                             : QStringLiteral("%1@%2").arg(lib.name, lib.version);
    if (output_) {
      output_->appendLine(tr("[Lockfile] Installing library %1 ...").arg(spec));
    }
    const CommandResult installResult = runCommandBlocking(
        cliPath,
        arduinoCli_->withGlobalFlags({QStringLiteral("lib"),
                                      QStringLiteral("install"), spec}),
        {}, {}, 900000);
    if (!commandSucceeded(installResult)) {
      failures << tr("lib install %1: %2")
                      .arg(spec, commandErrorSummary(installResult));
    }
  }

  if (!fqbn.isEmpty()) {
    QSettings settings;
    settings.beginGroup(kSettingsGroup);
    settings.setValue(kFqbnKey, fqbn);
    settings.endGroup();
    storeFqbnForCurrentSketch(fqbn);
  }

  if (boardsManager_) {
    boardsManager_->refresh();
  }
  if (libraryManager_) {
    libraryManager_->refresh();
  }
  refreshInstalledBoards();
  refreshConnectedPorts();

  if (failures.isEmpty()) {
    showToast(tr("Project bootstrap completed"));
    QMessageBox::information(this, tr("Bootstrap Project"),
                             tr("Project dependencies were installed."));
  } else {
    QMessageBox::warning(
        this, tr("Bootstrap Project"),
        tr("Bootstrap completed with errors:\n\n%1")
            .arg(failures.join(QStringLiteral("\n"))));
  }
}

void MainWindow::runEnvironmentDoctor() {
  const QString cliPath =
      arduinoCli_ ? arduinoCli_->arduinoCliPath().trimmed() : QString{};
  const QString configPath =
      arduinoCli_ ? arduinoCli_->arduinoCliConfigPath().trimmed() : QString{};

  QSettings settings;
  settings.beginGroup(QStringLiteral("Preferences"));
  QString sketchbookDir = settings.value(QStringLiteral("sketchbookDir")).toString().trimmed();
  QStringList additionalUrls = settings.value(QStringLiteral("additionalUrls")).toStringList();
  settings.endGroup();
  if (sketchbookDir.isEmpty()) {
    sketchbookDir = defaultSketchbookDir();
  }
  if (additionalUrls.isEmpty()) {
    const ArduinoCliConfigSnapshot snapshot = readArduinoCliConfigSnapshot(configPath);
    additionalUrls = snapshot.additionalUrls;
  }
  additionalUrls = normalizeStringList(additionalUrls);

  bool fixSketchbookDir = false;
  bool fixSeedUrls = false;
  bool fixWriteConfig = false;
  bool fixCoreIndex = false;
  bool fixLibIndex = false;
  bool fixBoardSetupWizard = false;

  QStringList lines;
  int errorCount = 0;
  int warningCount = 0;

  if (cliPath.isEmpty() || !QFileInfo::exists(cliPath)) {
    lines << tr("[ERROR] Arduino CLI executable not found.");
    ++errorCount;
  } else {
    lines << tr("[OK] Arduino CLI: %1").arg(cliPath);
  }

  if (configPath.isEmpty()) {
    lines << tr("[WARN] Arduino CLI config path is empty.");
    ++warningCount;
    fixWriteConfig = !cliPath.isEmpty();
  } else if (!QFileInfo::exists(configPath)) {
    lines << tr("[WARN] Arduino CLI config missing: %1").arg(configPath);
    ++warningCount;
    fixWriteConfig = !cliPath.isEmpty();
  } else {
    lines << tr("[OK] Arduino CLI config: %1").arg(configPath);
  }

  if (sketchbookDir.isEmpty()) {
    lines << tr("[WARN] Sketchbook folder is not configured.");
    ++warningCount;
    fixSketchbookDir = true;
  } else if (!QFileInfo(sketchbookDir).isDir()) {
    lines << tr("[WARN] Sketchbook folder missing: %1").arg(sketchbookDir);
    ++warningCount;
    fixSketchbookDir = true;
  } else {
    lines << tr("[OK] Sketchbook folder: %1").arg(sketchbookDir);
  }

  if (additionalUrls.isEmpty()) {
    lines << tr("[WARN] Additional board URLs are empty.");
    ++warningCount;
    fixSeedUrls = true;
  } else {
    lines << tr("[OK] Additional board URLs: %1").arg(additionalUrls.size());
  }

  int installedCoreCount = -1;
  int installedLibCount = -1;
  if (!cliPath.isEmpty() && arduinoCli_) {
    const CommandResult coreListResult =
        runCommandBlocking(cliPath,
                           arduinoCli_->withGlobalFlags({QStringLiteral("core"),
                                                         QStringLiteral("list"),
                                                         QStringLiteral("--json")}),
                           {}, {}, 120000);
    if (commandSucceeded(coreListResult)) {
      installedCoreCount =
          parseInstalledCoresFromJson(coreListResult.stdoutText.toUtf8()).size();
      lines << tr("[OK] Installed cores: %1").arg(installedCoreCount);
      if (installedCoreCount == 0) {
        ++warningCount;
        lines << tr("[WARN] No board cores installed.");
        fixBoardSetupWizard = true;
      }
    } else {
      ++errorCount;
      lines << tr("[ERROR] Failed to list installed cores: %1")
                   .arg(commandErrorSummary(coreListResult));
      fixCoreIndex = true;
    }

    const CommandResult libListResult =
        runCommandBlocking(cliPath,
                           arduinoCli_->withGlobalFlags({QStringLiteral("lib"),
                                                         QStringLiteral("list"),
                                                         QStringLiteral("--json")}),
                           {}, {}, 120000);
    if (commandSucceeded(libListResult)) {
      installedLibCount =
          parseInstalledLibrariesFromJson(libListResult.stdoutText.toUtf8()).size();
      lines << tr("[OK] Installed libraries: %1").arg(installedLibCount);
    } else {
      ++errorCount;
      lines << tr("[ERROR] Failed to list installed libraries: %1")
                   .arg(commandErrorSummary(libListResult));
      fixLibIndex = true;
    }
  }

  QString summaryText;
  if (errorCount == 0 && warningCount == 0) {
    summaryText = tr("No issues found. Environment looks healthy.");
  } else {
    summaryText = tr("Environment Doctor found %1 error(s) and %2 warning(s).")
                      .arg(errorCount)
                      .arg(warningCount);
  }

  QMessageBox box(this);
  box.setWindowTitle(tr("Environment Doctor"));
  box.setIcon(errorCount > 0 ? QMessageBox::Warning : QMessageBox::Information);
  box.setText(summaryText);
  box.setDetailedText(lines.join(QStringLiteral("\n")));
  QPushButton* fixButton = nullptr;
  const bool canFix = fixSketchbookDir || fixSeedUrls || fixWriteConfig ||
                      fixCoreIndex || fixLibIndex || fixBoardSetupWizard;
  if (canFix) {
    fixButton = box.addButton(tr("Run Recommended Fixes"), QMessageBox::AcceptRole);
  }
  box.addButton(QMessageBox::Close);
  box.exec();

  if (!fixButton || box.clickedButton() != fixButton) {
    return;
  }

  QStringList fixResults;
  if (fixSketchbookDir) {
    if (QDir().mkpath(sketchbookDir)) {
      fixResults << tr("[OK] Created sketchbook folder.");
    } else {
      fixResults << tr("[WARN] Could not create sketchbook folder.");
    }
  }

  QStringList mergedUrls = additionalUrls;
  if (fixSeedUrls) {
    QString mergeError;
    QStringList seeded = loadSeededAdditionalBoardsUrls();
    if (mergeAdditionalBoardUrlsIntoPreferences(seeded, &mergeError, &mergedUrls)) {
      fixResults << tr("[OK] Seeded additional board URLs.");
    } else {
      fixResults << tr("[WARN] Could not seed board URLs: %1").arg(mergeError);
    }
  }

  if (fixWriteConfig && arduinoCli_) {
    QString configError;
    if (updateArduinoCliConfig(arduinoCli_->arduinoCliConfigPath(), sketchbookDir,
                               mergedUrls, &configError)) {
      fixResults << tr("[OK] Updated Arduino CLI config.");
    } else {
      fixResults << tr("[WARN] Could not update Arduino CLI config: %1")
                        .arg(configError);
    }
  }

  if ((fixCoreIndex || fixLibIndex) && arduinoCli_ &&
      !arduinoCli_->arduinoCliPath().trimmed().isEmpty()) {
    const QString path = arduinoCli_->arduinoCliPath().trimmed();
    if (fixCoreIndex) {
      const CommandResult result = runCommandBlocking(
          path,
          arduinoCli_->withGlobalFlags({QStringLiteral("core"),
                                        QStringLiteral("update-index")}),
          {}, {}, 600000);
      if (commandSucceeded(result)) {
        fixResults << tr("[OK] Updated boards index.");
      } else {
        fixResults << tr("[WARN] Boards index update failed: %1")
                          .arg(commandErrorSummary(result));
      }
    }
    if (fixLibIndex) {
      const CommandResult result = runCommandBlocking(
          path,
          arduinoCli_->withGlobalFlags({QStringLiteral("lib"),
                                        QStringLiteral("update-index")}),
          {}, {}, 600000);
      if (commandSucceeded(result)) {
        fixResults << tr("[OK] Updated libraries index.");
      } else {
        fixResults << tr("[WARN] Libraries index update failed: %1")
                          .arg(commandErrorSummary(result));
      }
    }
  }

  if (fixBoardSetupWizard) {
    runBoardSetupWizard();
    fixResults << tr("[OK] Board Setup Wizard opened.");
  }

  if (boardsManager_) {
    boardsManager_->refresh();
  }
  if (libraryManager_) {
    libraryManager_->refresh();
  }
  refreshInstalledBoards();
  refreshConnectedPorts();
  updateSketchbookView();

  QMessageBox::information(this, tr("Environment Doctor"),
                           fixResults.isEmpty()
                               ? tr("No automatic fixes were applied.")
                               : fixResults.join(QStringLiteral("\n")));
}

// === Tools Menu Actions ===
void MainWindow::toggleSerialMonitor() {
  if (!serialDock_) return;

  if (serialDock_->isVisible()) {
    serialDock_->hide();
    actionSerialMonitor_->setChecked(false);
  } else {
    serialDock_->show();
    serialDock_->raise();
    actionSerialMonitor_->setChecked(true);

    // Make sure serial plotter is unchecked when opening monitor
    if (serialPlotterDock_ && serialPlotterDock_->isVisible()) {
      serialPlotterDock_->hide();
      actionSerialPlotter_->setChecked(false);
    }
  }
}

void MainWindow::toggleSerialPlotter() {
  if (!serialPlotterDock_) return;

  if (serialPlotterDock_->isVisible()) {
    serialPlotterDock_->hide();
    actionSerialPlotter_->setChecked(false);
  } else {
    serialPlotterDock_->show();
    serialPlotterDock_->raise();
    actionSerialPlotter_->setChecked(true);

    // Make sure serial monitor is unchecked when opening plotter
    if (serialDock_ && serialDock_->isVisible()) {
      serialDock_->hide();
      actionSerialMonitor_->setChecked(false);
    }
  }
}

void MainWindow::getBoardInfo() {
  const QString selectedPort = currentPort().trimmed();
  if (selectedPort.isEmpty()) {
    QMessageBox::warning(this, tr("No Port Selected"),
                         tr("Please select a port first."));
    return;
  }
  if (currentPortIsMissing()) {
    QMessageBox::warning(this, tr("Port Not Available"),
                         tr("The selected port is not available. Please select a connected port."));
    return;
  }

  if (!arduinoCli_) return;

  auto* p = new QProcess(this);
  connect(p, &QProcess::finished, this,
          [this, p, selectedPort](int exitCode, QProcess::ExitStatus) {
            const QString stderrText =
                QString::fromUtf8(p->readAllStandardError()).trimmed();
            const QByteArray stdoutData = p->readAllStandardOutput();
            p->deleteLater();

            if (exitCode != 0) {
              QMessageBox::warning(
                  this, tr("Board Info"),
                  tr("Could not retrieve board information.\n\n%1")
                      .arg(stderrText.isEmpty() ? tr("arduino-cli failed.") : stderrText));
              return;
            }

            const QJsonDocument doc = QJsonDocument::fromJson(stdoutData);
            QJsonArray ports;
            if (doc.isObject()) {
              ports = doc.object().value("detected_ports").toArray();
            } else if (doc.isArray()) {
              ports = doc.array();
            }

            QJsonObject match;
            for (const QJsonValue& value : ports) {
              const QJsonObject obj = value.toObject();
              const QJsonObject portObj = obj.value("port").toObject();
              const QString address = portObj.value("address").toString().trimmed();
              if (!address.isEmpty() && address == selectedPort) {
                match = obj;
                break;
              }
            }

            if (match.isEmpty()) {
              QMessageBox::information(
                  this, tr("Board Info"),
                  tr("No detailed board metadata is currently available for %1.")
                      .arg(selectedPort));
              return;
            }

            const QJsonObject portObj = match.value("port").toObject();
            const QString address = portObj.value("address").toString().trimmed();
            const QString protocol =
                portObj.value("protocol_label").toString().trimmed().isEmpty()
                    ? portObj.value("protocol").toString().trimmed()
                    : portObj.value("protocol_label").toString().trimmed();

            const QJsonArray boards = match.value("boards").toArray();
            QString boardName;
            QString boardFqbn;
            if (!boards.isEmpty()) {
              const QJsonObject board = boards.first().toObject();
              boardName = board.value("name").toString().trimmed();
              boardFqbn = board.value("fqbn").toString().trimmed();
            }

            if (boardName.isEmpty() && boardCombo_) {
              boardName = boardCombo_->currentText().trimmed();
            }
            if (boardFqbn.isEmpty()) {
              boardFqbn = currentFqbn().trimmed();
            }

            QStringList idProps;
            const QJsonObject idObj =
                match.value("matching_identification_properties").toObject();
            for (auto it = idObj.constBegin(); it != idObj.constEnd(); ++it) {
              const QString key = it.key().trimmed();
              const QString val = it.value().toString().trimmed();
              if (!key.isEmpty() && !val.isEmpty()) {
                idProps << QStringLiteral("%1: %2")
                               .arg(key.toHtmlEscaped(), val.toHtmlEscaped());
              }
            }
            idProps.sort(Qt::CaseInsensitive);

            QString details =
                tr("<b>Port:</b> %1<br><b>Protocol:</b> %2<br>"
                   "<b>Board:</b> %3<br><b>FQBN:</b> %4")
                    .arg(address.toHtmlEscaped().isEmpty() ? tr("(unknown)") : address.toHtmlEscaped(),
                         protocol.toHtmlEscaped().isEmpty() ? tr("(unknown)") : protocol.toHtmlEscaped(),
                         boardName.toHtmlEscaped().isEmpty() ? tr("(unknown)") : boardName.toHtmlEscaped(),
                         boardFqbn.toHtmlEscaped().isEmpty() ? tr("(unknown)") : boardFqbn.toHtmlEscaped());

            if (!idProps.isEmpty()) {
              details += tr("<br><br><b>Identification Properties</b><br>%1")
                             .arg(idProps.join(QStringLiteral("<br>")));
            }

            QMessageBox box(this);
            box.setWindowTitle(tr("Board Info"));
            box.setIcon(QMessageBox::Information);
            box.setTextFormat(Qt::RichText);
            box.setText(details);
            box.exec();
          });

  const QStringList args =
      arduinoCli_->withGlobalFlags({"board", "list", "--format", "json"});
  p->start(arduinoCli_->arduinoCliPath(), args);
}

void MainWindow::burnBootloader() {
  const QString fqbn = currentFqbn();
  if (fqbn.isEmpty()) {
    QMessageBox::warning(this, tr("No Board Selected"),
                         tr("Please select a board first."));
    return;
  }

  const QString programmer = currentProgrammer();
  if (programmer.isEmpty()) {
    QMessageBox::warning(this, tr("No Programmer Selected"),
                         tr("Please select a programmer from Tools > Programmer first."));
    return;
  }

  const QString port = currentPort().trimmed();
  const bool portOk = !port.isEmpty() && !currentPortIsMissing();

  QMessageBox::StandardButton reply = QMessageBox::question(
      this,
      tr("Burn Bootloader"),
      tr("Are you sure you want to burn the bootloader?\n\nThis will overwrite the bootloader on the board."),
      QMessageBox::Yes | QMessageBox::No,
      QMessageBox::No);

  if (reply == QMessageBox::Yes) {
    if (!arduinoCli_) return;

    lastCliJobKind_ = CliJobKind::BurnBootloader;
    output_->clear();
    if (outputDock_) {
      outputDock_->show();
      outputDock_->raise();
    }
    output_->appendLine(tr("Burning bootloader..."));
    updateStopActionState();

    QSettings settings;
    settings.beginGroup("Preferences");
    const bool verbose = settings.value("verboseUpload", false).toBool();
    settings.endGroup();

    QStringList args = {"burn-bootloader", "--fqbn", fqbn, "--programmer",
                        programmer};
    if (portOk) {
      args << "--port" << port;
      const QString protocol = currentPortProtocol();
      if (!protocol.isEmpty()) {
        args << "--protocol" << protocol;
      }
    }
    if (verbose) {
      args << "--verbose";
    }
    arduinoCli_->run(args);
  }
}

void MainWindow::loginToGithub() {
  const QString ghPath = findExecutable(QStringLiteral("gh"));
  if (ghPath.isEmpty()) {
    QMessageBox::information(
        this,
        tr("GitHub CLI Not Found"),
        tr("Install GitHub CLI (`gh`) to log in from Rewritto IDE.\n\n"
           "Ubuntu/Debian: sudo apt install gh\n\n"
           "After installing, use the Login action again."));
    return;
  }

  const CommandResult authStatus = runCommandBlocking(
      ghPath,
      {QStringLiteral("auth"), QStringLiteral("status"), QStringLiteral("-h"),
       QStringLiteral("github.com")},
      {}, {}, 15000);
  if (authStatus.started && !authStatus.timedOut &&
      authStatus.exitStatus == QProcess::NormalExit &&
      authStatus.exitCode == 0) {
    showToast(tr("GitHub already authenticated."));
    return;
  }

  bool ok = false;
  const QString token = QInputDialog::getText(
      this,
      tr("GitHub Login"),
      tr("Paste a GitHub personal access token (scope: repo):"),
      QLineEdit::Password,
      QString{},
      &ok).trimmed();
  if (!ok) {
    return;
  }
  if (token.isEmpty()) {
    showToast(tr("Login cancelled."));
    return;
  }

  const CommandResult loginResult = runCommandBlocking(
      ghPath,
      {QStringLiteral("auth"), QStringLiteral("login"),
       QStringLiteral("--hostname"), QStringLiteral("github.com"),
       QStringLiteral("--git-protocol"), QStringLiteral("https"),
       QStringLiteral("--with-token")},
      {}, token.toUtf8() + '\n', 60000);

  if (loginResult.started && !loginResult.timedOut &&
      loginResult.exitStatus == QProcess::NormalExit &&
      loginResult.exitCode == 0) {
    runCommandBlocking(
        ghPath,
        {QStringLiteral("auth"), QStringLiteral("setup-git")},
        {}, {}, 20000);
    showToast(tr("GitHub login successful."));
    return;
  }

  QMessageBox::warning(
      this,
      tr("GitHub Login Failed"),
      tr("Login failed.\n\n%1\n\n"
         "Generate a token at https://github.com/settings/tokens")
          .arg(commandErrorSummary(loginResult)));
}

void MainWindow::initGitRepositoryForCurrentSketch() {
  const QString sketchFolder = currentSketchFolderPath().trimmed();
  if (sketchFolder.isEmpty()) {
    showToast(tr("Open a sketch first."));
    return;
  }
  if (!QDir(sketchFolder).exists()) {
    showToast(tr("Sketch folder is not available."));
    return;
  }

  const QString gitPath = findExecutable(QStringLiteral("git"));
  if (gitPath.isEmpty()) {
    QMessageBox::information(
        this,
        tr("Git Not Found"),
        tr("Install Git to use repository actions.\n\nUbuntu/Debian: sudo apt install git"));
    return;
  }

  if (isGitRepository(gitPath, sketchFolder)) {
    showToast(tr("Git repository already initialized."));
    return;
  }

  CommandResult initResult = runCommandBlocking(
      gitPath,
      {QStringLiteral("init"), QStringLiteral("-b"), QStringLiteral("main")},
      sketchFolder, {}, 20000);
  if (!(initResult.started && !initResult.timedOut &&
        initResult.exitStatus == QProcess::NormalExit &&
        initResult.exitCode == 0)) {
    initResult = runCommandBlocking(
        gitPath, {QStringLiteral("init")}, sketchFolder, {}, 20000);
    if (!(initResult.started && !initResult.timedOut &&
          initResult.exitStatus == QProcess::NormalExit &&
          initResult.exitCode == 0)) {
      QMessageBox::warning(
          this,
          tr("Git Init Failed"),
          tr("Could not initialize the repository.\n\n%1")
              .arg(commandErrorSummary(initResult)));
      return;
    }
    runCommandBlocking(
        gitPath,
        {QStringLiteral("branch"), QStringLiteral("-M"), QStringLiteral("main")},
        sketchFolder, {}, 10000);
  }

  if (output_) {
    const QString details = initResult.stdoutText.trimmed();
    if (!details.isEmpty()) {
      output_->appendLine(details);
    }
  }
  showToast(tr("Git repository initialized."));
}

void MainWindow::commitCurrentSketchToGit() {
  const QString sketchFolder = currentSketchFolderPath().trimmed();
  if (sketchFolder.isEmpty()) {
    showToast(tr("Open a sketch first."));
    return;
  }
  if (!QDir(sketchFolder).exists()) {
    showToast(tr("Sketch folder is not available."));
    return;
  }

  const QString gitPath = findExecutable(QStringLiteral("git"));
  if (gitPath.isEmpty()) {
    QMessageBox::information(
        this,
        tr("Git Not Found"),
        tr("Install Git to use repository actions.\n\nUbuntu/Debian: sudo apt install git"));
    return;
  }

  if (!isGitRepository(gitPath, sketchFolder)) {
    const auto reply = QMessageBox::question(
        this,
        tr("Initialize Repository"),
        tr("This sketch is not a Git repository yet. Initialize it now?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);
    if (reply != QMessageBox::Yes) {
      return;
    }
    initGitRepositoryForCurrentSketch();
    if (!isGitRepository(gitPath, sketchFolder)) {
      return;
    }
  }

  const CommandResult addResult = runCommandBlocking(
      gitPath, {QStringLiteral("add"), QStringLiteral("-A")}, sketchFolder, {}, 30000);
  if (!(addResult.started && !addResult.timedOut &&
        addResult.exitStatus == QProcess::NormalExit &&
        addResult.exitCode == 0)) {
    QMessageBox::warning(
        this,
        tr("Git Commit Failed"),
        tr("Staging changes failed.\n\n%1")
            .arg(commandErrorSummary(addResult)));
    return;
  }

  const CommandResult stagedResult = runCommandBlocking(
      gitPath,
      {QStringLiteral("diff"), QStringLiteral("--cached"), QStringLiteral("--name-only")},
      sketchFolder, {}, 20000);
  if (!(stagedResult.started && !stagedResult.timedOut &&
        stagedResult.exitStatus == QProcess::NormalExit &&
        stagedResult.exitCode == 0)) {
    QMessageBox::warning(
        this,
        tr("Git Commit Failed"),
        tr("Could not inspect staged changes.\n\n%1")
            .arg(commandErrorSummary(stagedResult)));
    return;
  }
  if (stagedResult.stdoutText.trimmed().isEmpty()) {
    showToast(tr("No changes to commit."));
    return;
  }

  bool ok = false;
  const QString defaultMessage = tr("Update sketch (%1)")
                                     .arg(QDateTime::currentDateTime().toString(Qt::ISODate));
  const QString commitMessage = QInputDialog::getText(
      this,
      tr("Commit Message"),
      tr("Commit message:"),
      QLineEdit::Normal,
      defaultMessage,
      &ok).trimmed();
  if (!ok || commitMessage.isEmpty()) {
    return;
  }

  const CommandResult commitResult = runCommandBlocking(
      gitPath, {QStringLiteral("commit"), QStringLiteral("-m"), commitMessage},
      sketchFolder, {}, 40000);
  if (!(commitResult.started && !commitResult.timedOut &&
        commitResult.exitStatus == QProcess::NormalExit &&
        commitResult.exitCode == 0)) {
    QString error = commandErrorSummary(commitResult);
    if (error.contains(QStringLiteral("Please tell me who you are"), Qt::CaseInsensitive) ||
        error.contains(QStringLiteral("unable to auto-detect email address"), Qt::CaseInsensitive)) {
      error += tr("\n\nSet your identity first:\n"
                  "git config --global user.name \"Your Name\"\n"
                  "git config --global user.email \"you@example.com\"");
    }
    QMessageBox::warning(
        this,
        tr("Git Commit Failed"),
        tr("Commit failed.\n\n%1").arg(error));
    return;
  }

  if (output_) {
    const QString details = commitResult.stdoutText.trimmed();
    if (!details.isEmpty()) {
      output_->appendLine(details);
    }
  }
  showToast(tr("Commit created."));
}

void MainWindow::pushCurrentSketchToRemote() {
  const QString sketchFolder = currentSketchFolderPath().trimmed();
  if (sketchFolder.isEmpty()) {
    showToast(tr("Open a sketch first."));
    return;
  }
  if (!QDir(sketchFolder).exists()) {
    showToast(tr("Sketch folder is not available."));
    return;
  }

  const QString gitPath = findExecutable(QStringLiteral("git"));
  if (gitPath.isEmpty()) {
    QMessageBox::information(
        this,
        tr("Git Not Found"),
        tr("Install Git to use repository actions.\n\nUbuntu/Debian: sudo apt install git"));
    return;
  }

  if (!isGitRepository(gitPath, sketchFolder)) {
    const auto reply = QMessageBox::question(
        this,
        tr("Initialize Repository"),
        tr("This sketch is not a Git repository yet. Initialize it now?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);
    if (reply != QMessageBox::Yes) {
      return;
    }
    initGitRepositoryForCurrentSketch();
    if (!isGitRepository(gitPath, sketchFolder)) {
      return;
    }
  }

  CommandResult headResult = runCommandBlocking(
      gitPath,
      {QStringLiteral("rev-parse"), QStringLiteral("--verify"), QStringLiteral("HEAD")},
      sketchFolder, {}, 10000);
  if (!(headResult.started && !headResult.timedOut &&
        headResult.exitStatus == QProcess::NormalExit &&
        headResult.exitCode == 0)) {
    const auto reply = QMessageBox::question(
        this,
        tr("No Commits Yet"),
        tr("This repository has no commits yet. Create a commit now?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);
    if (reply != QMessageBox::Yes) {
      return;
    }
    commitCurrentSketchToGit();
    headResult = runCommandBlocking(
        gitPath,
        {QStringLiteral("rev-parse"), QStringLiteral("--verify"), QStringLiteral("HEAD")},
        sketchFolder, {}, 10000);
    if (!(headResult.started && !headResult.timedOut &&
          headResult.exitStatus == QProcess::NormalExit &&
          headResult.exitCode == 0)) {
      return;
    }
  }

  CommandResult remoteResult = runCommandBlocking(
      gitPath,
      {QStringLiteral("remote"), QStringLiteral("get-url"), QStringLiteral("origin")},
      sketchFolder, {}, 10000);
  QString remoteUrl = remoteResult.stdoutText.trimmed();
  if (!(remoteResult.started && !remoteResult.timedOut &&
        remoteResult.exitStatus == QProcess::NormalExit &&
        remoteResult.exitCode == 0) ||
      remoteUrl.isEmpty()) {
    bool ok = false;
    const QString defaultUrl = tr("https://github.com/<username>/%1.git")
                                   .arg(QFileInfo(sketchFolder).fileName());
    const QString userUrl = QInputDialog::getText(
        this,
        tr("GitHub Remote URL"),
        tr("Enter the GitHub repository URL for `origin`:"),
        QLineEdit::Normal,
        defaultUrl,
        &ok).trimmed();
    if (!ok || userUrl.isEmpty()) {
      return;
    }

    const CommandResult addRemoteResult = runCommandBlocking(
        gitPath,
        {QStringLiteral("remote"), QStringLiteral("add"), QStringLiteral("origin"), userUrl},
        sketchFolder, {}, 10000);
    if (!(addRemoteResult.started && !addRemoteResult.timedOut &&
          addRemoteResult.exitStatus == QProcess::NormalExit &&
          addRemoteResult.exitCode == 0)) {
      QMessageBox::warning(
          this,
          tr("Remote Setup Failed"),
          tr("Could not add remote `origin`.\n\n%1")
              .arg(commandErrorSummary(addRemoteResult)));
      return;
    }
    remoteUrl = userUrl;
  }

  CommandResult branchResult = runCommandBlocking(
      gitPath, {QStringLiteral("branch"), QStringLiteral("--show-current")},
      sketchFolder, {}, 10000);
  QString branch = branchResult.stdoutText.trimmed();
  if (branch.isEmpty()) {
    branch = QStringLiteral("main");
    runCommandBlocking(
        gitPath,
        {QStringLiteral("branch"), QStringLiteral("-M"), branch},
        sketchFolder, {}, 10000);
  }

  const CommandResult pushResult = runCommandBlocking(
      gitPath,
      {QStringLiteral("push"), QStringLiteral("-u"), QStringLiteral("origin"), branch},
      sketchFolder, {}, 120000);
  if (!(pushResult.started && !pushResult.timedOut &&
        pushResult.exitStatus == QProcess::NormalExit &&
        pushResult.exitCode == 0)) {
    const QString details = commandErrorSummary(pushResult);
    QMessageBox::warning(
        this,
        tr("Push Failed"),
        tr("Could not push to GitHub.\n\n%1\n\n"
           "If this is an authentication issue, use the GitHub Login action first.")
            .arg(details));
    return;
  }

  if (output_) {
    const QString details = pushResult.stdoutText.trimmed();
    if (!details.isEmpty()) {
      output_->appendLine(details);
    }
  }

  const QString repoUrl = githubBrowseUrlForRemote(remoteUrl);
  if (!repoUrl.isEmpty()) {
    showToastWithAction(
        tr("Push completed."),
        tr("Open Repo"),
        [repoUrl] { QDesktopServices::openUrl(QUrl(repoUrl)); });
  } else {
    showToast(tr("Push completed."));
  }
}

// === Help Menu Actions ===
void MainWindow::showAbout() {
  const QString appVersion =
      QCoreApplication::applicationVersion().trimmed().isEmpty()
          ? QStringLiteral(REWRITTO_IDE_VERSION)
          : QCoreApplication::applicationVersion().trimmed();
  QMessageBox::about(
      this,
      tr("About Rewritto-ide"),
      tr("<h3>Rewritto-ide</h3>"
          "<p>A native Qt application for embedded development workflows.</p>"
          "<p><b>Version:</b> %1</p>"
          "<p><b>Qt Version:</b> %2</p>"
          "<p>This project is built with native Qt Widgets for performance, "
          "desktop integration, and a focused toolchain workflow.</p>"
          "<p>License: AGPL-3.0</p>"
          "<p>Project homepage: https://github.com/lolren/rewritto-ide</p>"
          ).arg(appVersion, qVersion()));
}

void MainWindow::updateWindowTitleForFile(const QString& filePath) {
  if (!filePath.isEmpty()) {
    const QFileInfo info(filePath);
    const QString title = QString("%1 - Rewritto-ide")
        .arg(info.completeBaseName());
    setWindowTitle(title);
  } else {
    setWindowTitle("Rewritto-ide");
  }
}

// === Additional Tools Menu Actions ===
void MainWindow::autoFormatSketch() {
  if (!editor_) return;

  auto* codeEditor = qobject_cast<CodeEditor*>(editor_->currentEditorWidget());
  if (!codeEditor || !codeEditor->document()) {
    return;
  }

  QTextDocument* document = codeEditor->document();
  QTextCursor cursor(document);
  cursor.beginEditBlock();

  bool changed = false;
  for (QTextBlock block = document->begin(); block.isValid();
       block = block.next()) {
    const QString original = block.text();
    int keep = original.size();
    while (keep > 0) {
      const QChar c = original.at(keep - 1);
      if (c == QLatin1Char(' ') || c == QLatin1Char('\t')) {
        --keep;
      } else {
        break;
      }
    }
    if (keep == original.size()) {
      continue;
    }
    const QString trimmedRight = original.left(keep);
    cursor.setPosition(block.position());
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    cursor.insertText(trimmedRight);
    changed = true;
  }

  QString allText = document->toPlainText();
  if (!allText.isEmpty() && !allText.endsWith(QLatin1Char('\n'))) {
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(QStringLiteral("\n"));
    changed = true;
  }

  cursor.endEditBlock();
  if (changed) {
    document->setModified(true);
    showToast(tr("Auto format applied"));
    if (output_) {
      output_->appendLine(tr("Auto format applied."));
    }
  } else {
    showToast(tr("No formatting changes needed"));
  }
}

void MainWindow::archiveSketch() {
  const QString sketchDir = currentSketchFolderPath();
  if (sketchDir.isEmpty()) {
    QMessageBox::warning(this, tr("No Sketch Open"),
                         tr("Please open or create a sketch first."));
    return;
  }

  const QFileInfo info(sketchDir);
  const QString sketchName = info.fileName().trimmed().isEmpty()
                                 ? QStringLiteral("sketch")
                                 : info.fileName().trimmed();

  const QString zipPath = QFileDialog::getSaveFileName(
      this,
      tr("Archive Sketch"),
      QDir::homePath() + "/" + sketchName + ".zip",
      tr("ZIP Files (*.zip);;All Files (*)"));

  if (!zipPath.isEmpty()) {
    // Create zip archive
    QString archiveError;
    if (!createZipArchive(sketchDir, zipPath, &archiveError)) {
      QMessageBox::warning(this, tr("Archive Failed"),
                           tr("Could not create zip archive.\n\n%1")
                               .arg(archiveError.trimmed().isEmpty()
                                        ? tr("No compatible archive tool found.")
                                        : archiveError.trimmed()));
    } else {
      QMessageBox::information(this, tr("Archive Complete"),
                               tr("Sketch archived to: %1").arg(zipPath));
    }
  }
}

void MainWindow::showWiFiFirmwareUpdater() {
  const QString port = currentPort().trimmed();
  if (port.isEmpty()) {
    QMessageBox::warning(this, tr("No Port Selected"),
                         tr("Please select a port first."));
    return;
  }
  if (currentPortIsMissing()) {
    QMessageBox::warning(this, tr("Port Not Available"),
                         tr("The selected port is not available. Please select a connected port."));
    return;
  }

  const QString boardName =
      boardCombo_ ? boardCombo_->currentText().trimmed() : QString{};
  QMessageBox box(this);
  box.setWindowTitle(tr("WiFi Firmware Updater"));
  box.setIcon(QMessageBox::Information);
  box.setTextFormat(Qt::RichText);
  box.setText(
      tr("<b>Firmware Updater</b><br><br>"
         "Selected board: %1<br>"
         "Selected port: %2<br><br>"
         "Use the official updater guide for WiFiNINA / WiFi101 modules.")
          .arg(boardName.toHtmlEscaped().isEmpty() ? tr("(unknown)") : boardName.toHtmlEscaped(),
               port.toHtmlEscaped()));
  QPushButton* openGuide =
      box.addButton(tr("Open Guide"), QMessageBox::AcceptRole);
  box.addButton(QMessageBox::Close);
  box.exec();
  if (box.clickedButton() == openGuide) {
    QDesktopServices::openUrl(
        QUrl(QStringLiteral("https://support.arduino.cc/hc/en-us/articles/4403365234322")));
  }
}

void MainWindow::uploadSslRootCertificates() {
  const QString port = currentPort().trimmed();
  if (port.isEmpty()) {
    QMessageBox::warning(this, tr("No Port Selected"),
                         tr("Please select a port first."));
    return;
  }
  if (currentPortIsMissing()) {
    QMessageBox::warning(this, tr("Port Not Available"),
                         tr("The selected port is not available. Please select a connected port."));
    return;
  }

  const QString boardName =
      boardCombo_ ? boardCombo_->currentText().trimmed() : QString{};
  QMessageBox box(this);
  box.setWindowTitle(tr("Upload SSL Root Certificates"));
  box.setIcon(QMessageBox::Information);
  box.setTextFormat(Qt::RichText);
  box.setText(
      tr("<b>Upload SSL Root Certificates</b><br><br>"
         "Selected board: %1<br>"
         "Selected port: %2<br><br>"
         "Use the official Arduino workflow to download and upload root certificates.")
          .arg(boardName.toHtmlEscaped().isEmpty() ? tr("(unknown)") : boardName.toHtmlEscaped(),
               port.toHtmlEscaped()));
  QPushButton* openGuide =
      box.addButton(tr("Open Guide"), QMessageBox::AcceptRole);
  box.addButton(QMessageBox::Close);
  box.exec();
  if (box.clickedButton() == openGuide) {
    QDesktopServices::openUrl(
        QUrl(QStringLiteral("https://www.arduino.cc/en/Tutorial/WiFiNINAFirmwareUpdater/")));
  }
}

void MainWindow::setProgrammer(const QString& programmer) {
  const QString trimmed = programmer.trimmed();

  QSettings settings;
  settings.beginGroup(kSettingsGroup);
  if (trimmed.isEmpty()) {
    settings.remove(kProgrammerKey);
  } else {
    settings.setValue(kProgrammerKey, trimmed);
  }
  settings.endGroup();

  QAction* matchedAction = nullptr;
  auto matches = [&trimmed](QAction* a) {
    return a && a->data().toString().trimmed() == trimmed;
  };
  if (!trimmed.isEmpty()) {
    if (matches(actionProgrammerAVRISP_)) matchedAction = actionProgrammerAVRISP_;
    else if (matches(actionProgrammerUSBasp_)) matchedAction = actionProgrammerUSBasp_;
    else if (matches(actionProgrammerArduinoISP_)) matchedAction = actionProgrammerArduinoISP_;
    else if (matches(actionProgrammerUSBTinyISP_)) matchedAction = actionProgrammerUSBTinyISP_;
  }

  if (matchedAction) {
    matchedAction->setChecked(true);
    showToast(tr("Programmer: %1").arg(matchedAction->text()));
  } else if (!trimmed.isEmpty()) {
    showToast(tr("Programmer: %1").arg(trimmed));
  } else {
    showToast(tr("Programmer cleared"));
  }
}

// === Debug Actions ===
void MainWindow::startDebugging() {
  if (!editor_ || editor_->currentFilePath().isEmpty()) {
    QMessageBox::warning(this, tr("No Sketch Open"),
                         tr("Please open a sketch first."));
    return;
  }

  if (currentFqbn().isEmpty()) {
    QMessageBox::warning(this, tr("No Board Selected"),
                         tr("Please select a board first."));
    return;
  }

  showToast(tr("Debugging will be implemented in a future version."));
}

void MainWindow::debugStepOver() {
  showToast(tr("Debugging not yet available."));
}

void MainWindow::debugStepInto() {
  showToast(tr("Debugging not yet available."));
}

void MainWindow::debugStepOut() {
  showToast(tr("Debugging not yet available."));
}

void MainWindow::debugContinue() {
  showToast(tr("Debugging not yet available."));
}

void MainWindow::stopDebugging() {
  showToast(tr("Debugging not yet available."));
}

// === Helper Functions ===
bool MainWindow::createZipArchive(const QString& sourceDir,
                                  const QString& zipPath,
                                  QString* outError,
                                  std::function<void(const QString& status)>
                                      progressCallback) {
  const QFileInfo sourceInfo(sourceDir);
  if (!sourceInfo.exists() || !sourceInfo.isDir()) {
    if (outError) {
      *outError = tr("Source folder does not exist.");
    }
    return false;
  }
  const QFileInfo targetInfo(zipPath);
  if (targetInfo.absolutePath().trimmed().isEmpty()) {
    if (outError) {
      *outError = tr("Destination path is invalid.");
    }
    return false;
  }
  if (!QDir().mkpath(targetInfo.absolutePath())) {
    if (outError) {
      *outError = tr("Could not create destination folder.");
    }
    return false;
  }
  (void)QFile::remove(zipPath);

  const QString workingDir = sourceInfo.absoluteDir().absolutePath();
  const QString sourceName = sourceInfo.fileName();
  const int timeoutMs = 3600000;  // allow large project bundles

  struct Attempt final {
    QString program;
    QStringList args;
    QString label;
  };
  QVector<Attempt> attempts;

  const QString zipExe = QStandardPaths::findExecutable(QStringLiteral("zip"));
  if (!zipExe.isEmpty()) {
    attempts.push_back(
        {zipExe, {QStringLiteral("-rq"), zipPath, sourceName}, QStringLiteral("zip")});
  }

  const QString bsdtarExe =
      QStandardPaths::findExecutable(QStringLiteral("bsdtar"));
  if (!bsdtarExe.isEmpty()) {
    attempts.push_back({bsdtarExe,
                        {QStringLiteral("-a"), QStringLiteral("-cf"), zipPath, sourceName},
                        QStringLiteral("bsdtar")});
  }

  const QString sevenZipExe = QStandardPaths::findExecutable(QStringLiteral("7z"));
  if (!sevenZipExe.isEmpty()) {
    attempts.push_back({sevenZipExe,
                        {QStringLiteral("a"), QStringLiteral("-tzip"),
                         QStringLiteral("-mx=9"), QStringLiteral("-y"), zipPath,
                         sourceName},
                        QStringLiteral("7z")});
  }

  const QString pythonExe =
      QStandardPaths::findExecutable(QStringLiteral("python3"));
  if (!pythonExe.isEmpty()) {
    const QString script = QStringLiteral(
        "import os,sys,zipfile\n"
        "out_path=sys.argv[1]\n"
        "src_dir=os.path.abspath(sys.argv[2])\n"
        "base=os.path.dirname(src_dir)\n"
        "with zipfile.ZipFile(out_path,'w',compression=zipfile.ZIP_DEFLATED,allowZip64=True) as zf:\n"
        "  for root,dirs,files in os.walk(src_dir):\n"
        "    for name in files:\n"
        "      p=os.path.join(root,name)\n"
        "      arc=os.path.relpath(p,base)\n"
        "      zf.write(p,arc)\n");
    attempts.push_back({pythonExe,
                        {QStringLiteral("-c"), script, zipPath, sourceName},
                        QStringLiteral("python3")});
  }

  QStringList errors;
  for (const Attempt& attempt : attempts) {
    CommandResult result;
    QProcess process(this);
    process.setWorkingDirectory(workingDir);
    process.start(attempt.program, attempt.args);

    if (!process.waitForStarted(5000)) {
      result.stderrText = tr("Failed to start archiver tool.");
      errors << QStringLiteral("%1: %2")
                    .arg(attempt.label, commandErrorSummary(result));
      (void)QFile::remove(zipPath);
      continue;
    }
    result.started = true;

    if (progressCallback) {
      progressCallback(tr("Creating ZIP with %1...").arg(attempt.label));
    }

    QElapsedTimer elapsed;
    elapsed.start();
    QByteArray stdoutData;
    QByteArray stderrData;
    while (process.state() != QProcess::NotRunning) {
      (void)process.waitForFinished(250);
      stdoutData.append(process.readAllStandardOutput());
      stderrData.append(process.readAllStandardError());

      if (elapsed.elapsed() > timeoutMs) {
        result.timedOut = true;
        process.kill();
        process.waitForFinished(1500);
        break;
      }

      if (progressCallback) {
        const qint64 archiveSize = QFileInfo(zipPath).isFile()
                                       ? QFileInfo(zipPath).size()
                                       : 0;
        progressCallback(
            tr("Creating ZIP with %1... %2 written (%3 elapsed)")
                .arg(attempt.label,
                     formatByteSize(archiveSize),
                     formatElapsedTimeMs(elapsed.elapsed())));
      }
    }

    stdoutData.append(process.readAllStandardOutput());
    stderrData.append(process.readAllStandardError());
    result.exitStatus = process.exitStatus();
    result.exitCode = process.exitCode();
    result.stdoutText = QString::fromUtf8(stdoutData);
    result.stderrText = QString::fromUtf8(stderrData);

    if (commandSucceeded(result) && QFileInfo::exists(zipPath)) {
      if (progressCallback) {
        progressCallback(
            tr("ZIP created using %1 (%2).")
                .arg(attempt.label, formatByteSize(QFileInfo(zipPath).size())));
      }
      if (outError) {
        outError->clear();
      }
      return true;
    }
    errors << QStringLiteral("%1: %2")
                  .arg(attempt.label, commandErrorSummary(result));
    (void)QFile::remove(zipPath);
  }

  if (outError) {
    if (errors.isEmpty()) {
      *outError = tr("No compatible archive tool found. Install `zip`, `bsdtar`, `7z`, or `python3`.");
    } else {
      *outError = tr("Archive creation failed.\n%1")
                      .arg(errors.join(QStringLiteral("\n")));
    }
  }
  return false;
}

void MainWindow::showToast(const QString& message, int timeoutMs) {
  const QString trimmed = message.trimmed();
  if (trimmed.isEmpty()) {
    return;
  }

  const QString summary = windowTitle().trimmed().isEmpty()
                              ? QStringLiteral("Rewritto IDE")
                              : windowTitle().trimmed();
  if (sendLinuxDesktopNotification(summary, trimmed, timeoutMs)) {
    return;
  }

  if (toast_) {
    toast_->showToast(trimmed, QString(), std::function<void()>(), timeoutMs);
  }
}

void MainWindow::showToastWithAction(const QString& message,
                                     const QString& actionText,
                                     std::function<void()> action,
                                     int timeoutMs) {
  const QString trimmed = message.trimmed();
  if (trimmed.isEmpty()) {
    return;
  }

  const QString summary = windowTitle().trimmed().isEmpty()
                              ? QStringLiteral("Rewritto IDE")
                              : windowTitle().trimmed();
  if (sendLinuxDesktopNotificationWithAction(this,
                                             summary,
                                             trimmed,
                                             actionText,
                                             action,
                                             timeoutMs)) {
    return;
  }
  if (sendLinuxDesktopNotification(summary, trimmed, timeoutMs)) {
    return;
  }

  if (toast_) {
    toast_->showToast(trimmed, actionText, action, timeoutMs);
  }
}

void MainWindow::focusOutputDock() {
  if (!outputDock_) {
    return;
  }
  outputDock_->show();
  outputDock_->raise();
}

void MainWindow::focusBoardsManagerSearch(const QString& query) {
  if (!boardsManagerDock_ || !boardsManager_) {
    return;
  }
  boardsManagerDock_->show();
  boardsManagerDock_->raise();
  boardsManager_->showSearchFor(query);
}

void MainWindow::focusLibraryManagerSearch(const QString& query) {
  if (!libraryManagerDock_ || !libraryManager_) {
    return;
  }
  libraryManagerDock_->show();
  libraryManagerDock_->raise();
  libraryManager_->showSearchFor(query);
}

// === Sketch and Recent Files Management ===
void MainWindow::setRecentSketches(QStringList items) {
  QSettings settings;
  settings.beginGroup(kSettingsGroup);
  settings.setValue(kRecentSketchesKey, items);
  settings.endGroup();
  updateWelcomePage();
}

void MainWindow::setPinnedSketches(QStringList items) {
  QSettings settings;
  settings.beginGroup(kSettingsGroup);
  settings.setValue(kPinnedSketchesKey, items);
  settings.endGroup();
  updateWelcomePage();
}

void MainWindow::addRecentSketch(const QString& folder) {
  if (folder.trimmed().isEmpty()) return;

  QStringList items = recentSketches();
  items.removeAll(folder);
  items.prepend(folder);

  // Keep only the most recent 20 items
  while (items.size() > 20) {
    items.removeLast();
  }

  setRecentSketches(items);
}

bool MainWindow::isSketchPinned(const QString& folder) const {
  const QString normalized = normalizeSketchFolderPath(folder);
  if (normalized.isEmpty()) {
    return false;
  }

  const QString needle = QDir::cleanPath(normalized);
  const QStringList items = pinnedSketches();
  for (const QString& item : items) {
    if (QDir::cleanPath(item) == needle) {
      return true;
    }
  }
  return false;
}

void MainWindow::setSketchPinned(const QString& folder, bool pinned) {
  QStringList items = pinnedSketches();
  items.removeAll(folder);
  if (pinned) {
    items.append(folder);
  }
  setPinnedSketches(items);
}

void MainWindow::updateBoardPortIndicator() {
  if (!boardPortLabel_) return;

  const QString fqbn = currentFqbn();
  const QString port = currentPort();

  if (fqbn.isEmpty() && port.isEmpty()) {
    boardPortLabel_->setText(tr("Board: (none) | Port: (none)"));
  } else if (fqbn.isEmpty()) {
    boardPortLabel_->setText(tr("Board: (none) | Port: %1").arg(port));
  } else if (port.isEmpty()) {
    boardPortLabel_->setText(tr("Board: %1 | Port: (none)").arg(fqbn));
  } else {
    boardPortLabel_->setText(tr("Board: %1 | Port: %2").arg(fqbn, port));
  }

  if (boardMenu_) {
    const QString boardText =
        fqbn.isEmpty()
            ? tr("(none)")
            : (boardCombo_ ? boardCombo_->currentText().trimmed() : fqbn);
    boardMenu_->setTitle(tr("Board: %1").arg(boardText.isEmpty() ? fqbn : boardText));
  }
  if (portMenu_) {
    QString portText = port;
    if (!port.isEmpty() && portCombo_) {
      const QString t = portCombo_->currentText().trimmed();
      if (!t.isEmpty()) {
        portText = t;
      }
    }
    portMenu_->setTitle(tr("Port: %1").arg(portText.isEmpty() ? tr("(none)") : portText));
  }
}

void MainWindow::restyleContextModeToolBar() {
  if (!contextModeToolBar_) {
    return;
  }

  const QPalette pal = palette();
  const QColor panelColor = pal.color(QPalette::Window);
  const QColor textColor = pal.color(QPalette::WindowText);
  const QColor borderColor = pal.color(QPalette::Mid);
  const bool darkTheme = panelColor.lightnessF() < 0.5;

  const QColor fontsAccent = QColor(QStringLiteral("#38bdf8"));
  const QColor snapshotsAccent = QColor(QStringLiteral("#22c55e"));
  const QColor githubAccent = QColor(QStringLiteral("#6366f1"));
  const QColor mcpAccent = QColor(QStringLiteral("#f59e0b"));

  const QColor barBackground = blendColors(
      panelColor, pal.color(QPalette::Base), darkTheme ? 0.22 : 0.06);
  const QColor barBorder = blendColors(
      borderColor, pal.color(QPalette::Highlight), darkTheme ? 0.35 : 0.18);
  const QColor hoverBackground =
      blendColors(barBackground, textColor, darkTheme ? 0.14 : 0.08);
  const QColor fontsChecked =
      blendColors(barBackground, fontsAccent, darkTheme ? 0.55 : 0.34);
  const QColor snapshotsChecked =
      blendColors(barBackground, snapshotsAccent, darkTheme ? 0.55 : 0.34);
  const QColor githubChecked =
      blendColors(barBackground, githubAccent, darkTheme ? 0.55 : 0.34);
  const QColor mcpChecked =
      blendColors(barBackground, mcpAccent, darkTheme ? 0.55 : 0.34);
  const QColor checkedText = readableForeground(fontsChecked);

  contextModeToolBar_->setStyleSheet(QString(
      "QToolBar#ContextModeBar {"
      "  background-color: %1;"
      "  border: none;"
      "  border-left: 1px solid %2;"
      "  spacing: 6px;"
      "  padding: 8px 3px;"
      "  min-width: 44px;"
      "  max-width: 44px;"
      "}"
      "QToolBar#ContextModeBar QToolButton {"
      "  color: %3;"
      "  border: none;"
      "  border-radius: 8px;"
      "  padding: 8px;"
      "  margin: 1px 0;"
      "}"
      "QToolBar#ContextModeBar QToolButton:hover {"
      "  background-color: %4;"
      "}"
      "QToolBar#ContextModeBar QToolButton#ContextModeFontsButton {"
      "  border-right: 3px solid %5;"
      "}"
      "QToolBar#ContextModeBar QToolButton#ContextModeSnapshotsButton {"
      "  border-right: 3px solid %6;"
      "}"
      "QToolBar#ContextModeBar QToolButton#ContextModeGithubButton {"
      "  border-right: 3px solid %7;"
      "}"
      "QToolBar#ContextModeBar QToolButton#ContextModeMcpButton {"
      "  border-right: 3px solid %8;"
      "}"
      "QToolBar#ContextModeBar QToolButton#ContextModeFontsButton:checked {"
      "  background-color: %9;"
      "  color: %13;"
      "}"
      "QToolBar#ContextModeBar QToolButton#ContextModeSnapshotsButton:checked {"
      "  background-color: %10;"
      "  color: %13;"
      "}"
      "QToolBar#ContextModeBar QToolButton#ContextModeGithubButton:checked {"
      "  background-color: %11;"
      "  color: %13;"
      "}"
      "QToolBar#ContextModeBar QToolButton#ContextModeMcpButton:checked {"
      "  background-color: %12;"
      "  color: %13;"
      "}")
      .arg(colorHex(barBackground), colorHex(barBorder), colorHex(textColor),
           colorHex(hoverBackground), colorHex(fontsAccent), colorHex(snapshotsAccent),
           colorHex(githubAccent), colorHex(mcpAccent), colorHex(fontsChecked),
           colorHex(snapshotsChecked), colorHex(githubChecked), colorHex(mcpChecked),
           colorHex(checkedText)));
}

void MainWindow::syncContextModeSelection(bool contextVisible) {
  if (!contextModeGroup_) {
    return;
  }

  if (!contextVisible) {
    const bool wasExclusive = contextModeGroup_->isExclusive();
    contextModeGroup_->setExclusive(false);
    if (actionContextFontsMode_) actionContextFontsMode_->setChecked(false);
    if (actionContextSnapshotsMode_) actionContextSnapshotsMode_->setChecked(false);
    if (actionContextGithubMode_) actionContextGithubMode_->setChecked(false);
    if (actionContextMcpMode_) actionContextMcpMode_->setChecked(false);
    contextModeGroup_->setExclusive(wasExclusive);
    return;
  }

  const bool hasCheckedAction =
      (actionContextFontsMode_ && actionContextFontsMode_->isChecked()) ||
      (actionContextSnapshotsMode_ && actionContextSnapshotsMode_->isChecked()) ||
      (actionContextGithubMode_ && actionContextGithubMode_->isChecked()) ||
      (actionContextMcpMode_ && actionContextMcpMode_->isChecked());
  if (hasCheckedAction) {
    return;
  }

  QAction* modeAction = nullptr;
  switch (contextToolbarMode_) {
    case ContextToolbarMode::Fonts:
      modeAction = actionContextFontsMode_;
      break;
    case ContextToolbarMode::Snapshots:
      modeAction = actionContextSnapshotsMode_;
      break;
    case ContextToolbarMode::Github:
      modeAction = actionContextGithubMode_;
      break;
    case ContextToolbarMode::Mcp:
      modeAction = actionContextMcpMode_;
      break;
  }
  if (modeAction) {
    const QSignalBlocker blocker(contextModeGroup_);
    modeAction->setChecked(true);
  }
}

void MainWindow::enforceToolbarLayout() {
  if (!buildToolBar_ || !fontToolBar_) {
    return;
  }

  const bool contextVisible =
      actionToggleFontToolBar_ ? actionToggleFontToolBar_->isChecked()
                               : fontToolBar_->isVisible();

  addToolBar(Qt::TopToolBarArea, buildToolBar_);
  addToolBar(Qt::TopToolBarArea, fontToolBar_);
  removeToolBarBreak(fontToolBar_);
  insertToolBarBreak(fontToolBar_);
  buildToolBar_->setVisible(true);

  if (contextModeToolBar_) {
    addToolBar(Qt::RightToolBarArea, contextModeToolBar_);
    contextModeToolBar_->setVisible(true);
  }

  fontToolBar_->setVisible(contextVisible);
}

void MainWindow::setContextToolbarMode(ContextToolbarMode mode) {
  contextToolbarMode_ = mode;
  rebuildContextToolbar();
}

void MainWindow::rebuildContextToolbar() {
  if (!fontToolBar_) {
    return;
  }

  if (actionContextFontsMode_) {
    const QSignalBlocker blocker(actionContextFontsMode_);
    actionContextFontsMode_->setChecked(contextToolbarMode_ == ContextToolbarMode::Fonts);
  }
  if (actionContextSnapshotsMode_) {
    const QSignalBlocker blocker(actionContextSnapshotsMode_);
    actionContextSnapshotsMode_->setChecked(contextToolbarMode_ == ContextToolbarMode::Snapshots);
  }
  if (actionContextGithubMode_) {
    const QSignalBlocker blocker(actionContextGithubMode_);
    actionContextGithubMode_->setChecked(contextToolbarMode_ == ContextToolbarMode::Github);
  }
  if (actionContextMcpMode_) {
    const QSignalBlocker blocker(actionContextMcpMode_);
    actionContextMcpMode_->setChecked(contextToolbarMode_ == ContextToolbarMode::Mcp);
  }

  fontFamilyCombo_ = nullptr;
  fontSizeCombo_ = nullptr;
  mcpStatusLabel_ = nullptr;
  fontToolBar_->clear();
  restyleContextModeToolBar();

  const QPalette pal = palette();
  const QColor panelColor = pal.color(QPalette::Window);
  const QColor textColor = pal.color(QPalette::WindowText);
  const QColor borderBase = pal.color(QPalette::Mid);
  const QColor highlightColor = pal.color(QPalette::Highlight);
  const bool darkTheme = panelColor.lightnessF() < 0.5;

  QColor modeAccent;
  QString title;
  switch (contextToolbarMode_) {
    case ContextToolbarMode::Fonts:
      title = tr("Fonts");
      modeAccent = QColor(QStringLiteral("#38bdf8"));
      break;
    case ContextToolbarMode::Snapshots:
      title = tr("Snapshots");
      modeAccent = QColor(QStringLiteral("#22c55e"));
      break;
    case ContextToolbarMode::Github:
      title = tr("GitHub");
      modeAccent = QColor(QStringLiteral("#6366f1"));
      break;
    case ContextToolbarMode::Mcp:
      title = tr("MCP");
      modeAccent = QColor(QStringLiteral("#f59e0b"));
      break;
  }

  const QColor accentColor = blendColors(modeAccent, highlightColor, 0.18);
  const QColor gradientStartColor =
      blendColors(panelColor, accentColor, darkTheme ? 0.12 : 0.06);
  const QColor gradientEndColor =
      blendColors(panelColor, accentColor, darkTheme ? 0.06 : 0.03);
  const QColor borderColor =
      blendColors(borderBase, accentColor, darkTheme ? 0.44 : 0.22);
  const QColor toolbarTextColor = textColor;
  const QColor themeButtonColor = pal.color(QPalette::Highlight);
  const QColor themeButtonText = pal.color(QPalette::HighlightedText);
  const QColor buttonBackground = themeButtonColor.isValid() ? themeButtonColor : accentColor;
  const QColor buttonTextColor =
      themeButtonText.isValid() ? themeButtonText : readableForeground(buttonBackground);
  const QColor buttonBorder =
      blendColors(borderBase, buttonTextColor, darkTheme ? 0.20 : 0.12);
  const QColor buttonHover =
      blendColors(buttonBackground, panelColor, darkTheme ? 0.14 : 0.10);
  const QColor buttonPressed =
      blendColors(buttonBackground, panelColor, darkTheme ? 0.22 : 0.16);
  const QColor checkedBackground =
      blendColors(buttonBackground, accentColor, darkTheme ? 0.18 : 0.12);
  const QColor checkedTextColor =
      readableForeground(checkedBackground);

  fontToolBar_->setStyleSheet(QString(
      "QToolBar#ContextToolBar {"
      "  background-color: %1;"
      "  background-image: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 %1, stop:1 %2);"
      "  border: none;"
      "  border-bottom: 1px solid %3;"
      "  padding: 4px 10px;"
      "  spacing: 6px;"
      "}"
      "QToolBar#ContextToolBar QLabel#ContextToolbarTitle {"
      "  color: %4;"
      "  font-weight: 700;"
      "  margin-right: 6px;"
      "}"
      "QToolBar#ContextToolBar QToolButton {"
      "  color: %13;"
      "  border: 1px solid %5;"
      "  border-radius: 6px;"
      "  background-color: %6;"
      "  padding: 5px 8px;"
      "}"
      "QToolBar#ContextToolBar QToolButton:hover {"
      "  background-color: %7;"
      "}"
      "QToolBar#ContextToolBar QToolButton:pressed {"
      "  background-color: %8;"
      "}"
      "QToolBar#ContextToolBar QToolButton:checked {"
      "  background-color: %9;"
      "  border-color: %9;"
      "  color: %10;"
      "}"
      "}")
      .arg(colorHex(gradientStartColor), colorHex(gradientEndColor),
           colorHex(borderColor), colorHex(toolbarTextColor), colorHex(buttonBorder),
           colorHex(buttonBackground), colorHex(buttonHover),
           colorHex(buttonPressed), colorHex(checkedBackground),
           colorHex(checkedTextColor), colorHex(buttonTextColor)));

  auto* titleLabel = new QLabel(title, fontToolBar_);
  titleLabel->setObjectName("ContextToolbarTitle");
  fontToolBar_->addWidget(titleLabel);
  fontToolBar_->addSeparator();

  if (contextToolbarMode_ == ContextToolbarMode::Fonts) {
    fontFamilyCombo_ = new QComboBox(fontToolBar_);
    fontFamilyCombo_->setEditable(false);
    fontFamilyCombo_->setInsertPolicy(QComboBox::NoInsert);
    fontFamilyCombo_->setMinimumWidth(200);
    fontFamilyCombo_->setMaximumWidth(280);
    QFontDatabase fontDb;
    fontFamilyCombo_->addItems(fontDb.families());
    fontToolBar_->addWidget(fontFamilyCombo_);

    fontSizeCombo_ = new QComboBox(fontToolBar_);
    fontSizeCombo_->setEditable(false);
    fontSizeCombo_->setInsertPolicy(QComboBox::NoInsert);
    fontSizeCombo_->setMinimumWidth(120);
    fontSizeCombo_->setMaximumWidth(180);
    for (int size : {8, 9, 10, 11, 12, 14, 16, 18, 20, 24, 28, 32, 36, 48, 72}) {
      fontSizeCombo_->addItem(QString::number(size));
    }
    fontToolBar_->addWidget(fontSizeCombo_);

    if (editor_) {
      const QFont currentFont = editor_->editorFont();
      const QString family = currentFont.family().trimmed();
      if (!family.isEmpty()) {
        const int familyIndex =
            fontFamilyCombo_->findText(family, Qt::MatchFixedString);
        if (familyIndex >= 0) {
          fontFamilyCombo_->setCurrentIndex(familyIndex);
        } else {
          fontFamilyCombo_->addItem(family);
          fontFamilyCombo_->setCurrentIndex(fontFamilyCombo_->count() - 1);
        }
      }

      const int pointSize = currentFont.pointSize();
      if (pointSize > 0) {
        const QString sizeText = QString::number(pointSize);
        const int sizeIndex =
            fontSizeCombo_->findText(sizeText, Qt::MatchFixedString);
        if (sizeIndex >= 0) {
          fontSizeCombo_->setCurrentIndex(sizeIndex);
        } else {
          fontSizeCombo_->addItem(sizeText);
          fontSizeCombo_->setCurrentIndex(fontSizeCombo_->count() - 1);
        }
      }

      const QSignalBlocker blocker(actionToggleBold_);
      actionToggleBold_->setChecked(currentFont.bold());
    }

    actionToggleBold_->setIcon(QIcon());
    actionToggleBold_->setIconText(tr("B"));
    actionToggleBold_->setToolTip(tr("Bold"));
    fontToolBar_->addAction(actionToggleBold_);

    connect(fontFamilyCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { updateFontFromToolbar(); });
    connect(fontSizeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { updateFontFromToolbar(); });
    connect(actionToggleBold_, &QAction::triggered, this,
            &MainWindow::updateFontFromToolbar, Qt::UniqueConnection);
  } else if (contextToolbarMode_ == ContextToolbarMode::Snapshots) {
    fontToolBar_->addAction(actionSnapshotCapture_);
    fontToolBar_->addAction(actionSnapshotCompare_);
    fontToolBar_->addAction(actionSnapshotGallery_);
  } else if (contextToolbarMode_ == ContextToolbarMode::Github) {
    fontToolBar_->addAction(actionGithubLogin_);
    fontToolBar_->addAction(actionGitInitRepo_);
    fontToolBar_->addAction(actionGitCommit_);
    fontToolBar_->addAction(actionGitPush_);
  } else if (contextToolbarMode_ == ContextToolbarMode::Mcp) {
    fontToolBar_->addAction(actionMcpConfigure_);
    fontToolBar_->addAction(actionMcpStart_);
    fontToolBar_->addAction(actionMcpStop_);
    fontToolBar_->addAction(actionMcpRestart_);
    fontToolBar_->addAction(actionMcpAutostart_);
    fontToolBar_->addSeparator();
    mcpStatusLabel_ = new QLabel(fontToolBar_);
    mcpStatusLabel_->setObjectName("ContextMcpStatusLabel");
    fontToolBar_->addWidget(mcpStatusLabel_);
    updateMcpUiState();
  }

  QWidget* spacer = new QWidget(fontToolBar_);
  spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  fontToolBar_->addWidget(spacer);
}

void MainWindow::configureMcpServer() {
  QDialog dialog(this);
  dialog.setWindowTitle(tr("MCP Server Settings"));
  dialog.setMinimumWidth(620);

  auto* commandEdit = new QLineEdit(&dialog);
  commandEdit->setClearButtonEnabled(true);
  commandEdit->setText(mcpServerCommand_);
  commandEdit->setPlaceholderText(
      tr("e.g. npx -y @modelcontextprotocol/server-filesystem %1")
          .arg(QDir::homePath()));

  auto* autoStartCheck =
      new QCheckBox(tr("Start MCP server automatically on launch"), &dialog);
  autoStartCheck->setChecked(actionMcpAutostart_ &&
                             actionMcpAutostart_->isChecked());

  auto* hintLabel = new QLabel(
      tr("Use the exact command used to launch your MCP stdio server."),
      &dialog);
  hintLabel->setWordWrap(true);
  hintLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

  auto* form = new QFormLayout();
  form->addRow(tr("Command:"), commandEdit);

  auto* buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  auto* layout = new QVBoxLayout(&dialog);
  layout->addLayout(form);
  layout->addWidget(autoStartCheck);
  layout->addWidget(hintLabel);
  layout->addStretch(1);
  layout->addWidget(buttons);

  if (dialog.exec() != QDialog::Accepted) {
    return;
  }

  const QString oldCommand = mcpServerCommand_;
  mcpServerCommand_ = commandEdit->text().trimmed();
  const bool autoStart = autoStartCheck->isChecked();

  QSettings settings;
  settings.beginGroup(kSettingsGroup);
  if (mcpServerCommand_.isEmpty()) {
    settings.remove(kMcpServerCommandKey);
  } else {
    settings.setValue(kMcpServerCommandKey, mcpServerCommand_);
  }
  settings.setValue(kMcpAutoStartKey, autoStart);
  settings.endGroup();

  if (actionMcpAutostart_) {
    const QSignalBlocker blocker(actionMcpAutostart_);
    actionMcpAutostart_->setChecked(autoStart);
  }

  updateMcpUiState();
  if (mcpServerCommand_.isEmpty()) {
    showToast(tr("MCP command cleared"));
  } else if (oldCommand != mcpServerCommand_ &&
             mcpServerProcess_ &&
             mcpServerProcess_->state() != QProcess::NotRunning) {
    showToast(tr("MCP command updated. Restart server to apply."));
  } else {
    showToast(tr("MCP settings updated"));
  }
}

void MainWindow::startMcpServer() {
  if (mcpServerProcess_ &&
      mcpServerProcess_->state() != QProcess::NotRunning) {
    updateMcpUiState();
    return;
  }

  const QString command = mcpServerCommand_.trimmed();
  if (command.isEmpty()) {
    showToastWithAction(
        tr("MCP command is not configured."),
        tr("Configure"),
        [this] { configureMcpServer(); },
        6500);
    updateMcpUiState();
    return;
  }

  QStringList parts = QProcess::splitCommand(command);
  if (parts.isEmpty()) {
    showToast(tr("Invalid MCP command"));
    updateMcpUiState();
    return;
  }

  const QString program = parts.takeFirst();
  if (!mcpServerProcess_) {
    mcpServerProcess_ = new QProcess(this);
    mcpServerProcess_->setProcessChannelMode(QProcess::SeparateChannels);

    connect(mcpServerProcess_, &QProcess::started, this, [this] {
      if (output_) {
        output_->appendLine(tr("[MCP] Server started (pid %1)")
                                .arg(QString::number(
                                    static_cast<qulonglong>(
                                        mcpServerProcess_->processId()))));
      }
      updateMcpUiState();
    });

    connect(mcpServerProcess_, &QProcess::readyReadStandardOutput, this, [this] {
      if (!mcpServerProcess_ || !output_) {
        return;
      }
      const QString text =
          QString::fromUtf8(mcpServerProcess_->readAllStandardOutput())
              .replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
      const QStringList lines = text.split(QLatin1Char('\n'));
      for (const QString& line : lines) {
        if (!line.trimmed().isEmpty()) {
          output_->appendLine(tr("[MCP] %1").arg(line.trimmed()));
        }
      }
    });

    connect(mcpServerProcess_, &QProcess::readyReadStandardError, this, [this] {
      if (!mcpServerProcess_ || !output_) {
        return;
      }
      const QString text =
          QString::fromUtf8(mcpServerProcess_->readAllStandardError())
              .replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
      const QStringList lines = text.split(QLatin1Char('\n'));
      for (const QString& line : lines) {
        if (!line.trimmed().isEmpty()) {
          output_->appendLine(tr("[MCP] %1").arg(line.trimmed()));
        }
      }
    });

    connect(mcpServerProcess_,
            &QProcess::errorOccurred,
            this,
            [this](QProcess::ProcessError err) {
              if (!mcpStopRequested_) {
                showToast(tr("MCP server error (%1)")
                              .arg(QString::number(static_cast<int>(err))));
              }
              updateMcpUiState();
            });

    connect(
        mcpServerProcess_,
        QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        this,
        [this](int exitCode, QProcess::ExitStatus exitStatus) {
          if (output_) {
            output_->appendLine(
                tr("[MCP] Server stopped (exit code %1, %2)")
                    .arg(exitCode)
                    .arg(exitStatus == QProcess::NormalExit ? tr("normal exit")
                                                            : tr("crashed")));
          }
          if (!mcpStopRequested_) {
            showToast(tr("MCP server stopped"));
          }
          mcpStopRequested_ = false;
          updateMcpUiState();
        });
  }

  mcpStopRequested_ = false;
  if (output_) {
    output_->appendLine(tr("[MCP] Starting: %1").arg(command));
  }
  mcpServerProcess_->start(program, parts);
  updateMcpUiState();
}

void MainWindow::stopMcpServer() {
  if (!mcpServerProcess_ || mcpServerProcess_->state() == QProcess::NotRunning) {
    updateMcpUiState();
    return;
  }

  mcpStopRequested_ = true;
  mcpServerProcess_->terminate();
  if (!mcpServerProcess_->waitForFinished(1200)) {
    mcpServerProcess_->kill();
    (void)mcpServerProcess_->waitForFinished(400);
  }
  updateMcpUiState();
}

void MainWindow::restartMcpServer() {
  if (mcpServerProcess_ && mcpServerProcess_->state() != QProcess::NotRunning) {
    stopMcpServer();
  }
  startMcpServer();
}

void MainWindow::updateMcpUiState() {
  const bool running =
      mcpServerProcess_ && mcpServerProcess_->state() != QProcess::NotRunning;
  const bool hasCommand = !mcpServerCommand_.trimmed().isEmpty();

  if (actionMcpStart_) {
    actionMcpStart_->setEnabled(hasCommand && !running);
  }
  if (actionMcpStop_) {
    actionMcpStop_->setEnabled(running);
  }
  if (actionMcpRestart_) {
    actionMcpRestart_->setEnabled(hasCommand);
  }
  if (actionMcpAutostart_) {
    actionMcpAutostart_->setEnabled(hasCommand || actionMcpAutostart_->isChecked());
  }

  if (!mcpStatusLabel_) {
    return;
  }

  if (!hasCommand) {
    mcpStatusLabel_->setText(tr("Status: not configured"));
    return;
  }
  if (!running) {
    mcpStatusLabel_->setText(tr("Status: stopped"));
    return;
  }

  mcpStatusLabel_->setText(
      tr("Status: running (pid %1)")
          .arg(QString::number(
              static_cast<qulonglong>(mcpServerProcess_->processId()))));
}

QHash<QString, QByteArray> MainWindow::snapshotFileOverridesForSketch(
    const QString& sketchFolder) const {
  QHash<QString, QByteArray> out;
  if (!editor_ || sketchFolder.trimmed().isEmpty()) {
    return out;
  }

  const QString sketchRoot = QDir(sketchFolder).absolutePath();
  if (sketchRoot.isEmpty()) {
    return out;
  }
  const QString sketchPrefix = sketchRoot + QDir::separator();
  QDir sketchDir(sketchRoot);

  for (const QString& filePath : editor_->openedFiles()) {
    const QString absPath = QFileInfo(filePath).absoluteFilePath();
    if (absPath.isEmpty()) {
      continue;
    }
    if (absPath != sketchRoot && !absPath.startsWith(sketchPrefix)) {
      continue;
    }

    QString rel = sketchDir.relativeFilePath(absPath).trimmed();
    rel.replace('\\', '/');
    rel = QDir::cleanPath(rel);
    if (rel.isEmpty() || rel == QStringLiteral(".")) {
      continue;
    }

    const QString text = editor_->textForFile(absPath);
    QByteArray bytes = text.toUtf8();

    // Preserve existing line endings for already-on-disk files.
    QFile diskFile(absPath);
    if (diskFile.open(QIODevice::ReadOnly)) {
      const QByteArray diskBytes = diskFile.readAll();
      if (diskBytes.contains("\r\n")) {
        QString crlfText = text;
        crlfText.replace(QStringLiteral("\n"), QStringLiteral("\r\n"));
        bytes = crlfText.toUtf8();
      }
    }

    out.insert(rel, bytes);
  }
  return out;
}

QHash<QString, QByteArray> MainWindow::currentSketchFilesForCompare(
    const QString& sketchFolder,
    QString* outError) const {
  QHash<QString, QByteArray> out;
  const QString sketchRoot = QDir(sketchFolder).absolutePath();
  if (sketchRoot.trimmed().isEmpty() || !QDir(sketchRoot).exists()) {
    if (outError) {
      *outError = tr("Sketch folder is not available.");
    }
    return out;
  }

  QDir sketchDir(sketchRoot);
  QDirIterator it(sketchRoot,
                  QDir::Files | QDir::Hidden | QDir::NoSymLinks,
                  QDirIterator::Subdirectories);
  while (it.hasNext()) {
    const QString absPath = it.next();
    const QFileInfo info(absPath);
    if (!info.exists() || !info.isFile() || info.isSymLink()) {
      continue;
    }

    const QString rel = normalizeSnapshotRelativePath(sketchDir.relativeFilePath(absPath));
    if (rel.isEmpty() || shouldIgnoreSnapshotComparePath(rel)) {
      continue;
    }

    QFile file(absPath);
    if (!file.open(QIODevice::ReadOnly)) {
      if (outError) {
        *outError = tr("Failed to read '%1'.").arg(rel);
      }
      return {};
    }
    out.insert(rel, file.readAll());
  }

  const QHash<QString, QByteArray> overrides = snapshotFileOverridesForSketch(sketchRoot);
  for (auto it = overrides.constBegin(); it != overrides.constEnd(); ++it) {
    const QString rel = normalizeSnapshotRelativePath(it.key());
    if (rel.isEmpty() || shouldIgnoreSnapshotComparePath(rel)) {
      continue;
    }
    out.insert(rel, it.value());
  }

  if (outError) {
    outError->clear();
  }
  return out;
}

void MainWindow::captureCodeSnapshot(bool promptForComment) {
  const QString sketchFolder = currentSketchFolderPath();
  if (sketchFolder.isEmpty()) {
    showToast(tr("Open a sketch first."));
    return;
  }

  QString comment;
  if (promptForComment) {
    bool ok = false;
    comment = QInputDialog::getMultiLineText(this,
                                             tr("New Snapshot"),
                                             tr("Comment (optional):"),
                                             QString{},
                                             &ok);
    if (!ok) {
      return;
    }
  }

  CodeSnapshotStore::CreateOptions options;
  options.sketchFolder = sketchFolder;
  options.comment = comment;
  options.fileOverrides = snapshotFileOverridesForSketch(sketchFolder);

  QProgressDialog progress(tr("Creating snapshot\u2026"), tr("Cancel"), 0, 0, this);
  progress.setWindowModality(Qt::WindowModal);
  progress.setMinimumDuration(250);
  progress.setAutoClose(true);
  progress.setAutoReset(true);

  auto progressCb = [this, &progress](int done, int total, const QString& rel) -> bool {
    progress.setMaximum(std::max(0, total));
    progress.setValue(std::max(0, done));
    if (!rel.trimmed().isEmpty()) {
      progress.setLabelText(tr("Creating snapshot\u2026\n%1").arg(rel));
    } else {
      progress.setLabelText(tr("Creating snapshot\u2026"));
    }
    QCoreApplication::processEvents();
    return !progress.wasCanceled();
  };

  QString err;
  CodeSnapshotStore::SnapshotMeta meta;
  if (!CodeSnapshotStore::createSnapshot(options, &meta, &err, progressCb)) {
    showToast(err.isEmpty() ? tr("Snapshot failed.") : err);
    return;
  }

  if (!promptForComment && meta.comment.trimmed().isEmpty()) {
    showToastWithAction(
        tr("Snapshot created."),
        tr("Add Comment\u2026"),
        [this, sketchFolder, snapshotId = meta.id] {
          bool ok = false;
          const QString comment = QInputDialog::getMultiLineText(
              this, tr("Snapshot Comment"), tr("Comment:"), QString{}, &ok);
          if (!ok) {
            return;
          }
          QString err;
          if (!CodeSnapshotStore::updateSnapshotComment(sketchFolder, snapshotId, comment, &err)) {
            showToast(err.isEmpty() ? tr("Failed to update comment.") : err);
          }
        },
        6500);
  } else {
    showToast(tr("Snapshot created."));
  }
}

void MainWindow::showCodeSnapshotCompare() {
  const QString sketchFolder = currentSketchFolderPath();
  if (sketchFolder.isEmpty()) {
    showToast(tr("Open a sketch first."));
    return;
  }

  QString listError;
  const QVector<CodeSnapshotStore::SnapshotMeta> snapshots =
      CodeSnapshotStore::listSnapshots(sketchFolder, &listError);
  if (snapshots.isEmpty()) {
    showToast(tr("Create at least one snapshot first."));
    return;
  }

  QString filesError;
  const QHash<QString, QByteArray> currentFiles =
      currentSketchFilesForCompare(sketchFolder, &filesError);
  if (!filesError.trimmed().isEmpty()) {
    showToast(filesError);
    return;
  }

  CodeSnapshotCompareDialog dialog(sketchFolder, currentFiles, this);
  dialog.exec();
}

void MainWindow::showCodeSnapshotsGallery() {
  const QString sketchFolder = currentSketchFolderPath();
  if (sketchFolder.isEmpty()) {
    showToast(tr("Open a sketch first."));
    return;
  }

  CodeSnapshotsDialog dialog(sketchFolder, this);
  connect(&dialog, &CodeSnapshotsDialog::captureRequested, this, [this, &dialog] {
    captureCodeSnapshot(true);
    dialog.reload();
  });
  connect(&dialog, &CodeSnapshotsDialog::restoreRequested, this,
          [this, &dialog](const QString& id) {
            restoreCodeSnapshot(id);
            dialog.reload();
          });
  connect(&dialog, &CodeSnapshotsDialog::editCommentRequested, this,
          [this, &dialog, sketchFolder](const QString& id, const QString& currentComment) {
            bool ok = false;
            const QString nextComment =
                QInputDialog::getMultiLineText(this,
                                               tr("Edit Comment"),
                                               tr("Comment:"),
                                               currentComment,
                                               &ok);
            if (!ok) {
              return;
            }
            QString err;
            if (!CodeSnapshotStore::updateSnapshotComment(sketchFolder, id, nextComment, &err)) {
              showToast(err.isEmpty() ? tr("Failed to update comment.") : err);
              return;
            }
            dialog.reload();
          });
  connect(&dialog, &CodeSnapshotsDialog::deleteRequested, this,
          [this, &dialog, sketchFolder](const QString& id) {
            QMessageBox box(this);
            box.setIcon(QMessageBox::Warning);
            box.setWindowTitle(tr("Delete Snapshot"));
            box.setText(tr("Delete snapshot '%1'?").arg(id));
            box.setInformativeText(tr("This cannot be undone."));
            QPushButton* deleteBtn = box.addButton(tr("Delete"), QMessageBox::AcceptRole);
            box.addButton(QMessageBox::Cancel);
            box.setDefaultButton(QMessageBox::Cancel);
            box.exec();
            if (box.clickedButton() != static_cast<QAbstractButton*>(deleteBtn)) {
              return;
            }
            QString err;
            if (!CodeSnapshotStore::deleteSnapshot(sketchFolder, id, &err)) {
              showToast(err.isEmpty() ? tr("Failed to delete snapshot.") : err);
              return;
            }
            dialog.reload();
          });

  dialog.exec();
}

void MainWindow::restoreCodeSnapshot(const QString& snapshotId) {
  const QString sketchFolder = currentSketchFolderPath();
  if (sketchFolder.isEmpty()) {
    showToast(tr("Open a sketch first."));
    return;
  }
  if (snapshotId.trimmed().isEmpty()) {
    return;
  }

  QString err;
  const auto snapshot = CodeSnapshotStore::readSnapshot(sketchFolder, snapshotId, &err);
  if (!snapshot) {
    showToast(err.isEmpty() ? tr("Snapshot could not be read.") : err);
    return;
  }

  QMessageBox box(this);
  box.setIcon(QMessageBox::Warning);
  box.setWindowTitle(tr("Restore Snapshot"));
  const QString when =
      QLocale().toString(snapshot->meta.createdAtUtc.toLocalTime(), QLocale::ShortFormat);
  box.setText(tr("Restore snapshot from %1?").arg(when));

  QString info;
  const QString comment = snapshot->meta.comment.trimmed();
  if (!comment.isEmpty()) {
    info += tr("Comment: %1").arg(comment) + QStringLiteral("\n\n");
  }
  info += tr("This will overwrite %1 files in the sketch folder.")
              .arg(snapshot->meta.fileCount);
  if (editor_ && editor_->hasUnsavedChanges()) {
    info += tr("\n\nYou have unsaved changes in the editor. A safety snapshot will be created first, but your current unsaved changes will be discarded after restore.");
  } else {
    info += tr("\n\nA safety snapshot will be created first.");
  }
  box.setInformativeText(info);

  QPushButton* restoreBtn = box.addButton(tr("Restore"), QMessageBox::AcceptRole);
  box.addButton(QMessageBox::Cancel);
  box.setDefaultButton(QMessageBox::Cancel);
  box.exec();
  if (box.clickedButton() != static_cast<QAbstractButton*>(restoreBtn)) {
    return;
  }

  CodeSnapshotStore::CreateOptions backupOptions;
  backupOptions.sketchFolder = sketchFolder;
  backupOptions.comment = tr("Auto-backup before restoring %1").arg(snapshotId);
  backupOptions.fileOverrides = snapshotFileOverridesForSketch(sketchFolder);

  CodeSnapshotStore::SnapshotMeta backupMeta;
  {
    QProgressDialog progress(tr("Creating safety snapshot\u2026"), tr("Cancel"), 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(250);
    progress.setAutoClose(true);
    progress.setAutoReset(true);
    auto progressCb = [this, &progress](int done, int total, const QString& rel) -> bool {
      progress.setMaximum(std::max(0, total));
      progress.setValue(std::max(0, done));
      if (!rel.trimmed().isEmpty()) {
        progress.setLabelText(tr("Creating safety snapshot\u2026\n%1").arg(rel));
      } else {
        progress.setLabelText(tr("Creating safety snapshot\u2026"));
      }
      QCoreApplication::processEvents();
      return !progress.wasCanceled();
    };

    if (!CodeSnapshotStore::createSnapshot(backupOptions, &backupMeta, &err, progressCb)) {
      showToast(err.isEmpty() ? tr("Restore aborted.") : err);
      return;
    }
  }

  struct DiskEventSuppressionGuard final {
    EditorWidget* editor = nullptr;
    explicit DiskEventSuppressionGuard(EditorWidget* editor) : editor(editor) {
      if (editor) {
        editor->setSuppressDiskEvents(true);
      }
    }
    ~DiskEventSuppressionGuard() {
      if (editor) {
        editor->setSuppressDiskEvents(false);
      }
    }
  };
  DiskEventSuppressionGuard suppressGuard(editor_);

  QStringList writtenFiles;
  {
    QProgressDialog progress(tr("Restoring snapshot\u2026"), QString{}, 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(250);
    progress.setAutoClose(true);
    progress.setAutoReset(true);
    auto progressCb = [this, &progress](int done, int total, const QString& rel) -> bool {
      progress.setMaximum(std::max(0, total));
      progress.setValue(std::max(0, done));
      if (!rel.trimmed().isEmpty()) {
        progress.setLabelText(tr("Restoring snapshot\u2026\n%1").arg(rel));
      } else {
        progress.setLabelText(tr("Restoring snapshot\u2026"));
      }
      QCoreApplication::processEvents();
      return true;
    };

    if (!CodeSnapshotStore::restoreSnapshot(sketchFolder, snapshotId, &writtenFiles, &err, progressCb)) {
      showToast(err.isEmpty() ? tr("Restore failed.") : err);
      return;
    }
  }

  if (editor_ && !writtenFiles.isEmpty()) {
    QSet<QString> writtenSet;
    writtenSet.reserve(writtenFiles.size());
    for (const QString& p : writtenFiles) {
      writtenSet.insert(QFileInfo(p).absoluteFilePath());
    }
    for (const QString& openPath : editor_->openedFiles()) {
      const QString abs = QFileInfo(openPath).absoluteFilePath();
      if (!writtenSet.contains(abs)) {
        continue;
      }
      if (auto* w = editor_->editorWidgetForFile(abs)) {
        if (w->document()) {
          w->document()->setModified(false);
        }
      }
      (void)editor_->reloadFileIfUnmodified(abs);
    }
  }

  if (!lastSuccessfulCompile_.sketchFolder.trimmed().isEmpty() &&
      QDir(lastSuccessfulCompile_.sketchFolder).absolutePath() == QDir(sketchFolder).absolutePath()) {
    const QString currentSignature = computeSketchSignature(sketchFolder);
    lastSuccessfulCompile_.sketchChangedSinceCompile =
        currentSignature.isEmpty() ||
        currentSignature != lastSuccessfulCompile_.sketchSignature;
  }
  updateUploadActionStates();

  showToast(tr("Snapshot restored. Safety snapshot: %1").arg(backupMeta.id), 7000);
}

void MainWindow::updateFontFromToolbar() {
  if (!editor_ || !fontFamilyCombo_ || !fontSizeCombo_ || !actionToggleBold_) {
    return;
  }

  QFont font = editor_->editorFont();
  const QString family = fontFamilyCombo_->currentText().trimmed();
  if (!family.isEmpty()) {
    font.setFamily(family);
  }

  const int size = fontSizeCombo_->currentText().toInt();
  if (size > 0) {
    font.setPointSize(size);
  }

  font.setBold(actionToggleBold_->isChecked());
  editor_->setEditorFont(font);
}
