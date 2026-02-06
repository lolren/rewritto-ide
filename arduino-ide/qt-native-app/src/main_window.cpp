#include "main_window.h"

#include "arduino_cli.h"
#include "board_selector_dialog.h"
#include "boards_manager_dialog.h"
#include "build_output_parser.h"
#include "code_editor.h"
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
#include <functional>
#include <memory>
#include <QAbstractButton>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDockWidget>
#include <QCompleter>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
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
#include <QLineEdit>
#include <QMap>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QInputDialog>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
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
static constexpr auto kOpenFilesKey = "openFiles";
static constexpr auto kActiveFileKey = "activeFile";
static constexpr auto kEditorViewStatesKey = "editorViewStates";
static constexpr auto kBreakpointsKey = "breakpoints";
static constexpr auto kDebugWatchesKey = "debugWatches";
static constexpr auto kStateVersionKey = "stateVersion";
static constexpr auto kSketchBoardSelectionsKey = "sketchBoardSelections";
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
  const QString preferred = home + QStringLiteral("/Rewritto");
  const QString previous = home + QStringLiteral("/BlingBlink");
  const QString legacy = home + QStringLiteral("/Arduino");
  if (QDir(legacy).exists() && !QDir(preferred).exists() && !QDir(previous).exists()) {
    return legacy;
  }
  if (QDir(previous).exists() && !QDir(preferred).exists()) {
    return previous;
  }
  return preferred;
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

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setWindowTitle("Rewritto Ide");
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

  // Background index updates (boards + libraries) when stale.
  QTimer::singleShot(1000, this, [this] {
    if (boardsManager_) {
      boardsManager_->refresh();
    }
    if (libraryManager_) {
      libraryManager_->refresh();
    }
  });

  statusBar()->showMessage("Ready");
}

MainWindow::~MainWindow() {
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

  actionShowSketchFolder_ = new QAction(tr("Show Sketch Folder"), this);

  actionRenameSketch_ = new QAction(tr("Rename Sketch\u2026"), this);

  actionAddFileToSketch_ = new QAction(tr("Add File to Sketch\u2026"), this);

  actionAddZipLibrary_ = new QAction(tr("Add .ZIP Library\u2026"), this);

  actionManageLibraries_ = new QAction(tr("Manage Libraries\u2026"), this);
  actionManageLibraries_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_I));

  actionRefreshBoards_ = new QAction(tr("Refresh Boards"), this);
  actionRefreshPorts_ = new QAction(tr("Refresh Ports"), this);
  actionSelectBoard_ = new QAction(tr("Select Board\u2026"), this);

  actionBoardsManager_ = new QAction(tr("Boards Manager\u2026"), this);
  actionBoardsManager_->setCheckable(true);
  actionLibraryManager_ = new QAction(tr("Library Manager\u2026"), this);
  actionLibraryManager_->setCheckable(true);

  actionToggleFontToolBar_ = new QAction(tr("Context Toolbar"), this);
  actionToggleFontToolBar_->setCheckable(true);
  actionToggleFontToolBar_->setChecked(true);

  actionToggleBold_ = new QAction(tr("Bold"), this);
  actionToggleBold_->setCheckable(true);
  actionToggleBold_->setIcon(QIcon::fromTheme("format-text-bold"));

  actionContextBuildMode_ = new QAction(tr("Build"), this);
  actionContextBuildMode_->setCheckable(true);
  actionContextFontsMode_ = new QAction(tr("Fonts"), this);
  actionContextFontsMode_->setCheckable(true);
  actionContextSnapshotsMode_ = new QAction(tr("Snapshots"), this);
  actionContextSnapshotsMode_->setCheckable(true);

  actionSnapshotCapture_ = new QAction(tr("Capture"), this);
  actionSnapshotCompare_ = new QAction(tr("Compare"), this);
  actionSnapshotGallery_ = new QAction(tr("Gallery"), this);

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

  actionGettingStarted_ = new QAction(tr("Getting Started"), this);
  actionReference_ = new QAction(tr("Reference"), this);
  actionTroubleshooting_ = new QAction(tr("Troubleshooting"), this);
  actionArduinoWebsite_ = new QAction(tr("Website"), this);

  // Additional Tools menu actions for Arduino IDE 2.x parity
  actionAutoFormat_ = new QAction(tr("Auto Format"), this);
  actionAutoFormat_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
  actionAutoFormat_->setStatusTip(tr("Automatically format the current sketch code"));

  actionArchiveSketch_ = new QAction(tr("Archive Sketch"), this);
  actionArchiveSketch_->setStatusTip(tr("Create a zip archive of the current sketch"));

  actionWiFiFirmwareUpdater_ = new QAction(tr("Firmware Updater"), this);
  actionWiFiFirmwareUpdater_->setStatusTip(tr("Update the firmware on compatible boards"));

  actionUploadSSL_ = new QAction(tr("Upload SSL Root Certificates"), this);
  actionUploadSSL_->setStatusTip(tr("Upload SSL Root Certificates to the board"));

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
  sketchMenu->addSeparator();
  sketchMenu->addAction(actionShowSketchFolder_);
  sketchMenu->addSeparator();
  includeLibraryMenu_ = sketchMenu->addMenu(tr("Include Library"));

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
  helpMenu->addAction(actionGettingStarted_);
  helpMenu->addAction(actionReference_);
  helpMenu->addAction(actionTroubleshooting_);
  helpMenu->addAction(actionArduinoWebsite_);
  helpMenu->addSeparator();
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
    if (editor_) {
      editor_->saveCurrentWithDialog();
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

  connect(actionRefreshBoards_, &QAction::triggered, this, [this] {
    refreshInstalledBoards();
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

  connect(actionGettingStarted_, &QAction::triggered, this, [this] {
    QDesktopServices::openUrl(QUrl("https://docs.arduino.cc/"));
  });

  connect(actionReference_, &QAction::triggered, this, [this] {
    QDesktopServices::openUrl(QUrl("https://www.arduino.cc/reference/en/"));
  });

  connect(actionTroubleshooting_, &QAction::triggered, this, [this] {
    QDesktopServices::openUrl(QUrl("https://support.arduino.cc/hc/en-us"));
  });

  connect(actionArduinoWebsite_, &QAction::triggered, this, [this] {
    QDesktopServices::openUrl(QUrl("https://www.arduino.cc/"));
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

  // --- Header ToolBar (Arduino IDE 2.x style) ---
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
  QToolButton* boardSearchButton = new QToolButton(boardSelectorContainer);
  boardSearchButton->setObjectName("BoardSearchButton");
  boardSearchButton->setAutoRaise(true);
  boardSearchButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogContentsView));
  boardSearchButton->setToolTip(tr("Search boards"));
  connect(boardSearchButton, &QToolButton::clicked, this,
          [this] { showSelectBoardDialog(); });

  boardLayout->addWidget(boardSearchButton);
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
            refreshBoardOptions();
            updateUploadActionStates();
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

  actionContextBuildMode_->setIcon(themedModeIcon("applications-engineering", QStyle::SP_ComputerIcon));
  actionContextBuildMode_->setToolTip(tr("Build Tools"));
  actionContextFontsMode_->setIcon(themedModeIcon("preferences-desktop-font", QStyle::SP_FileDialogDetailedView));
  actionContextFontsMode_->setToolTip(tr("Font Controls"));
  actionContextSnapshotsMode_->setIcon(themedModeIcon("camera-photo", QStyle::SP_DialogSaveButton));
  actionContextSnapshotsMode_->setToolTip(tr("Snapshots"));

  actionSnapshotCapture_->setIcon(themedModeIcon("camera-photo", QStyle::SP_DialogSaveButton));
  actionSnapshotCapture_->setToolTip(tr("Capture editor snapshot"));
  actionSnapshotCompare_->setIcon(themedModeIcon("view-sort-descending", QStyle::SP_FileDialogListView));
  actionSnapshotCompare_->setToolTip(tr("Compare snapshots"));
  actionSnapshotGallery_->setIcon(themedModeIcon("folder-pictures", QStyle::SP_DirIcon));
  actionSnapshotGallery_->setToolTip(tr("Open snapshot gallery"));
  connect(actionSnapshotCapture_, &QAction::triggered, this,
          [this] { showToast(tr("Snapshot capture is not implemented yet.")); });
  connect(actionSnapshotCompare_, &QAction::triggered, this,
          [this] { showToast(tr("Snapshot compare is not implemented yet.")); });
  connect(actionSnapshotGallery_, &QAction::triggered, this,
          [this] { showToast(tr("Snapshot gallery is not implemented yet.")); });

  contextModeToolBar_->addAction(actionContextBuildMode_);
  contextModeToolBar_->addAction(actionContextFontsMode_);
  contextModeToolBar_->addAction(actionContextSnapshotsMode_);

  if (QWidget* widget = contextModeToolBar_->widgetForAction(actionContextBuildMode_)) {
    widget->setObjectName("ContextModeBuildButton");
  }
  if (QWidget* widget = contextModeToolBar_->widgetForAction(actionContextFontsMode_)) {
    widget->setObjectName("ContextModeFontsButton");
  }
  if (QWidget* widget = contextModeToolBar_->widgetForAction(actionContextSnapshotsMode_)) {
    widget->setObjectName("ContextModeSnapshotsButton");
  }

  restyleContextModeToolBar();

  contextModeGroup_ = new QActionGroup(this);
  contextModeGroup_->setExclusive(true);
  contextModeGroup_->addAction(actionContextBuildMode_);
  contextModeGroup_->addAction(actionContextFontsMode_);
  contextModeGroup_->addAction(actionContextSnapshotsMode_);
  connect(contextModeGroup_, &QActionGroup::triggered, this,
          [this](QAction* action) {
            if (action == actionContextBuildMode_) {
              setContextToolbarMode(ContextToolbarMode::Build);
            } else if (action == actionContextFontsMode_) {
              setContextToolbarMode(ContextToolbarMode::Fonts);
            } else if (action == actionContextSnapshotsMode_) {
              setContextToolbarMode(ContextToolbarMode::Snapshots);
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
  sketchbookToggle->setChecked(true);
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
  auto loadSketchbookDir = [] {
    QSettings settings;
    settings.beginGroup("Preferences");
    QString dir = settings.value("sketchbookDir").toString();
    settings.endGroup();
    if (dir.trimmed().isEmpty()) {
      dir = defaultSketchbookDir();
    }
    return QDir(dir).absolutePath();
  };

  if (boardsManager_) {
    connect(boardsManager_, &BoardsManagerDialog::platformsChanged, this,
            [this] { refreshInstalledBoards(); });
    connect(boardsManager_, &BoardsManagerDialog::busyChanged, this,
            [this](bool) { updateStopActionState(); });
  }
  if (libraryManager_) {
    connect(libraryManager_, &LibraryManagerDialog::librariesChanged, this,
            [this] { clearIncludeLibraryMenuActions(); });
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
    }
  });
  connect(arduinoCli_, &ArduinoCli::diagnosticFound, this,
          [this](const QString& filePath, int line, int column,
                 const QString& severity, const QString& message) {
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

            compilerDiagnostics_[filePath].push_back(d);
            if (editor_) {
              editor_->setDiagnostics(filePath, compilerDiagnostics_[filePath]);
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
  });

  connect(editor_, &EditorWidget::documentOpened, this,
          [this](const QString& path, const QString&) {
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
          });

  connect(editor_, &EditorWidget::documentChanged, this,
          [this](const QString& path, const QString&) {
            markSketchAsChanged(path);
            updateUploadActionStates();
          });

  connect(editor_, &EditorWidget::documentClosed, this, [this](const QString& path) {
    // Document closed - could trigger re-analysis
    updateUploadActionStates();
  });

  connect(editor_, &EditorWidget::breakpointsChanged, this, [this](const QString& path, const QVector<int>& lines) {
    // Breakpoints changed - sync with debugger if needed
  });
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

void MainWindow::closeEvent(QCloseEvent* event) {
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

      if (!arr.isEmpty()) {
        auto* proxy = static_cast<BoardFilterProxyModel*>(boardCombo_->model());
        auto* sourceModel = qobject_cast<QStandardItemModel*>(proxy->sourceModel());
        sourceModel->clear();
        
        // Add placeholder
        auto* placeholder = new QStandardItem(tr("Select Board..."));
        placeholder->setData(QString(), Qt::UserRole);
        sourceModel->appendRow(placeholder);

        // Collect unique boards
        QMap<QString, QString> uniqueBoards; // name -> fqbn
        for (const QJsonValue& v : arr) {
          const QJsonObject obj = v.toObject();
          const QString name = obj.value("name").toString();
          const QString fqbn = obj.value("fqbn").toString();
          if (!name.isEmpty() && !fqbn.isEmpty()) {
            if (!uniqueBoards.contains(name)) {
              uniqueBoards[name] = fqbn;
            }
          }
        }
        
        if (uniqueBoards.isEmpty()) {
            output_->appendLine(tr("Warning: No boards found. Please install platforms via Boards Manager."));
        }

        // Add boards to source model
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
            proxy->sort(0, Qt::AscendingOrder); // Trigger sort
        }

        // Restore selection index
        if (!savedFqbn.isEmpty()) {
          const int index = boardCombo_->findData(savedFqbn);
          if (index >= 0) {
            boardCombo_->setCurrentIndex(index);
          }
        }
      }
    }
    p->deleteLater();
  });

  const QStringList args =
      arduinoCli_->withGlobalFlags({"board", "listall", "--format", "json"});
  p->start(arduinoCli_->arduinoCliPath(), args);
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

        const QSignalBlocker blockPortSignals(portCombo_);
        portCombo_->clear();
        portCombo_->addItem(tr("Select Port..."), QString{});
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
    portsWatchProcess_->kill();
    portsWatchProcess_->waitForFinished();
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
  for (QAction* a : includeLibraryMenuActions_) a->deleteLater();
  includeLibraryMenuActions_.clear();
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
void MainWindow::refreshOutline() {
  if (!lsp_ || !lsp_->isReady() || !editor_) return;
  const QString path = editor_->currentFilePath();
  if (path.isEmpty()) return;

  lsp_->request("textDocument/documentSymbol", 
    QJsonObject{{"textDocument", QJsonObject{{"uri", toFileUri(path)}}}},
    [this](const QJsonValue& result, const QJsonObject&) {
      if (outlineModel_) {
        outlineModel_->clear();
        // Parsing logic for symbols would go here for full parity
      }
    });
}
void MainWindow::restartLanguageServer() {
  stopLanguageServer();
  if (lsp_ && !arduinoCli_->isRunning()) {
    lsp_->start(arduinoCli_->arduinoCliPath(), {"daemon", "--format", "json"}, arduinoCli_->arduinoCliConfigPath());
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

  // Show a dialog that lists all available boards and allows selection
  auto* dialog = new BoardSelectorDialog(this);
  dialog->setWindowTitle(tr("Select Board"));
  dialog->setMinimumSize(700, 500);

  // Fetch all available boards
  QProcess* p = new QProcess(this);
  connect(p, &QProcess::finished, this, [this, dialog, p](int exitCode, QProcess::ExitStatus) {
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
  });

  const QStringList args =
      arduinoCli_->withGlobalFlags({"board", "listall", "--format", "json"});
  p->start(arduinoCli_->arduinoCliPath(), args);
}

// === File Menu Actions ===
void MainWindow::newSketch() {
  const QString sketchbookDir = defaultSketchbookDir();
  QDir().mkpath(sketchbookDir);

  // Generate unique sketch name
  int counter = 1;
  QString sketchName;
  QString sketchPath;
  do {
    sketchName = counter == 1 ? tr("sketch") : tr("sketch_%1").arg(counter);
    sketchPath = sketchbookDir + "/" + sketchName;
    ++counter;
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
      "  This is a sample sketch provided with the Rewritto Ide.\n"
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

// === Help Menu Actions ===
void MainWindow::showAbout() {
  const QString appVersion =
      QCoreApplication::applicationVersion().trimmed().isEmpty()
          ? QStringLiteral("0.2.0")
          : QCoreApplication::applicationVersion().trimmed();
  QMessageBox::about(
      this,
      tr("About Rewritto Ide"),
      tr("<h3>Rewritto Ide (Qt Native)</h3>"
          "<p>A native Qt port of the Arduino IDE 2.x</p>"
          "<p><b>Version:</b> %1</p>"
          "<p><b>Qt Version:</b> %2</p>"
          "<p>This is a modern, Qt-based implementation of the Arduino IDE with "
          "feature parity to Arduino IDE 2.x, built with native Qt Widgets.</p>"
          "<p>License: AGPL-3.0</p>"
          "<p>Based on the original Arduino IDE 2.x (Eclipse Theia based)</p>"
          ).arg(appVersion, qVersion()));
}

void MainWindow::updateWindowTitleForFile(const QString& filePath) {
  if (!filePath.isEmpty()) {
    const QFileInfo info(filePath);
    const QString title = QString("%1 - Rewritto Ide")
        .arg(info.completeBaseName());
    setWindowTitle(title);
  } else {
    setWindowTitle("Rewritto Ide");
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
    if (!createZipArchive(sketchDir, zipPath)) {
      QMessageBox::warning(this, tr("Archive Failed"),
                           tr("Could not create zip archive.\n\nPlease ensure the `zip` command is installed."));
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
bool MainWindow::createZipArchive(const QString& sourceDir, const QString& zipPath) {
  const QFileInfo sourceInfo(sourceDir);
  if (!sourceInfo.exists() || !sourceInfo.isDir()) {
    return false;
  }
  const QFileInfo targetInfo(zipPath);
  if (targetInfo.absolutePath().trimmed().isEmpty()) {
    return false;
  }
  QDir().mkpath(targetInfo.absolutePath());
  QFile::remove(zipPath);

  QProcess zipProcess(this);
  zipProcess.setWorkingDirectory(sourceInfo.absoluteDir().absolutePath());
  zipProcess.start("zip", {"-rq", zipPath, sourceInfo.fileName()});
  if (!zipProcess.waitForStarted(1000)) {
    return false;
  }
  if (!zipProcess.waitForFinished(30000)) {
    zipProcess.kill();
    return false;
  }
  return zipProcess.exitCode() == 0 && QFileInfo::exists(zipPath);
}

void MainWindow::showToast(const QString& message, int timeoutMs) {
  if (toast_) {
    toast_->showToast(message, QString(), std::function<void()>(), timeoutMs);
  }
}

void MainWindow::showToastWithAction(const QString& message,
                                     const QString& actionText,
                                     std::function<void()> action,
                                     int timeoutMs) {
  if (toast_) {
    toast_->showToast(message, actionText, action, timeoutMs);
  }
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

  const QColor buildAccent = QColor(QStringLiteral("#f59e0b"));
  const QColor fontsAccent = QColor(QStringLiteral("#38bdf8"));
  const QColor snapshotsAccent = QColor(QStringLiteral("#22c55e"));

  const QColor barBackground = blendColors(
      panelColor, pal.color(QPalette::Base), darkTheme ? 0.22 : 0.06);
  const QColor barBorder = blendColors(
      borderColor, pal.color(QPalette::Highlight), darkTheme ? 0.35 : 0.18);
  const QColor hoverBackground =
      blendColors(barBackground, textColor, darkTheme ? 0.14 : 0.08);
  const QColor buildChecked =
      blendColors(barBackground, buildAccent, darkTheme ? 0.55 : 0.34);
  const QColor fontsChecked =
      blendColors(barBackground, fontsAccent, darkTheme ? 0.55 : 0.34);
  const QColor snapshotsChecked =
      blendColors(barBackground, snapshotsAccent, darkTheme ? 0.55 : 0.34);
  const QColor checkedText = readableForeground(buildChecked);

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
      "QToolBar#ContextModeBar QToolButton#ContextModeBuildButton {"
      "  border-right: 3px solid %5;"
      "}"
      "QToolBar#ContextModeBar QToolButton#ContextModeFontsButton {"
      "  border-right: 3px solid %6;"
      "}"
      "QToolBar#ContextModeBar QToolButton#ContextModeSnapshotsButton {"
      "  border-right: 3px solid %7;"
      "}"
      "QToolBar#ContextModeBar QToolButton#ContextModeBuildButton:checked {"
      "  background-color: %8;"
      "  color: %11;"
      "}"
      "QToolBar#ContextModeBar QToolButton#ContextModeFontsButton:checked {"
      "  background-color: %9;"
      "  color: %11;"
      "}"
      "QToolBar#ContextModeBar QToolButton#ContextModeSnapshotsButton:checked {"
      "  background-color: %10;"
      "  color: %11;"
      "}")
      .arg(colorHex(barBackground), colorHex(barBorder), colorHex(textColor),
           colorHex(hoverBackground), colorHex(buildAccent), colorHex(fontsAccent),
           colorHex(snapshotsAccent), colorHex(buildChecked), colorHex(fontsChecked),
           colorHex(snapshotsChecked), colorHex(checkedText)));
}

void MainWindow::syncContextModeSelection(bool contextVisible) {
  if (!contextModeGroup_) {
    return;
  }

  if (!contextVisible) {
    const bool wasExclusive = contextModeGroup_->isExclusive();
    contextModeGroup_->setExclusive(false);
    if (actionContextBuildMode_) actionContextBuildMode_->setChecked(false);
    if (actionContextFontsMode_) actionContextFontsMode_->setChecked(false);
    if (actionContextSnapshotsMode_) actionContextSnapshotsMode_->setChecked(false);
    contextModeGroup_->setExclusive(wasExclusive);
    return;
  }

  const bool hasCheckedAction =
      (actionContextBuildMode_ && actionContextBuildMode_->isChecked()) ||
      (actionContextFontsMode_ && actionContextFontsMode_->isChecked()) ||
      (actionContextSnapshotsMode_ && actionContextSnapshotsMode_->isChecked());
  if (hasCheckedAction) {
    return;
  }

  QAction* modeAction = nullptr;
  switch (contextToolbarMode_) {
    case ContextToolbarMode::Build:
      modeAction = actionContextBuildMode_;
      break;
    case ContextToolbarMode::Fonts:
      modeAction = actionContextFontsMode_;
      break;
    case ContextToolbarMode::Snapshots:
      modeAction = actionContextSnapshotsMode_;
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

  if (actionContextBuildMode_) {
    const QSignalBlocker blocker(actionContextBuildMode_);
    actionContextBuildMode_->setChecked(contextToolbarMode_ == ContextToolbarMode::Build);
  }
  if (actionContextFontsMode_) {
    const QSignalBlocker blocker(actionContextFontsMode_);
    actionContextFontsMode_->setChecked(contextToolbarMode_ == ContextToolbarMode::Fonts);
  }
  if (actionContextSnapshotsMode_) {
    const QSignalBlocker blocker(actionContextSnapshotsMode_);
    actionContextSnapshotsMode_->setChecked(contextToolbarMode_ == ContextToolbarMode::Snapshots);
  }

  fontFamilyCombo_ = nullptr;
  fontSizeCombo_ = nullptr;
  fontToolBar_->clear();
  restyleContextModeToolBar();

  const QPalette pal = palette();
  const QColor panelColor = pal.color(QPalette::Window);
  const QColor baseColor = pal.color(QPalette::Base);
  const QColor textColor = pal.color(QPalette::WindowText);
  const QColor borderBase = pal.color(QPalette::Mid);
  const QColor highlightColor = pal.color(QPalette::Highlight);
  const bool darkTheme = panelColor.lightnessF() < 0.5;

  QColor modeAccent;
  QString title;
  switch (contextToolbarMode_) {
    case ContextToolbarMode::Build:
      title = tr("Build");
      modeAccent = QColor(QStringLiteral("#f59e0b"));
      break;
    case ContextToolbarMode::Fonts:
      title = tr("Fonts");
      modeAccent = QColor(QStringLiteral("#38bdf8"));
      break;
    case ContextToolbarMode::Snapshots:
      title = tr("Snapshots");
      modeAccent = QColor(QStringLiteral("#22c55e"));
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
  const QColor buttonBackground = pal.color(QPalette::Button);
  const QColor buttonTextColor = pal.color(QPalette::ButtonText);
  const QColor buttonBorder =
      blendColors(borderBase, buttonTextColor, darkTheme ? 0.20 : 0.12);
  const QColor buttonHover =
      blendColors(buttonBackground, highlightColor, darkTheme ? 0.20 : 0.12);
  const QColor buttonPressed =
      blendColors(buttonBackground, highlightColor, darkTheme ? 0.30 : 0.20);
  const QColor checkedBackground = pal.color(QPalette::Highlight);
  const QColor checkedTextColor = pal.color(QPalette::HighlightedText);
  const QColor comboBackground = baseColor;
  const QColor comboBorder = borderBase;

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
      "  color: %14;"
      "  border: 1px solid %6;"
      "  border-radius: 6px;"
      "  background-color: %7;"
      "  padding: 5px 8px;"
      "}"
      "QToolBar#ContextToolBar QToolButton:hover {"
      "  background-color: %8;"
      "}"
      "QToolBar#ContextToolBar QToolButton:pressed {"
      "  background-color: %9;"
      "}"
      "QToolBar#ContextToolBar QToolButton:checked {"
      "  background-color: %10;"
      "  border-color: %10;"
      "  color: %11;"
      "}"
      "QToolBar#ContextToolBar QComboBox {"
      "  color: %14;"
      "  border: 1px solid %12;"
      "  border-radius: 6px;"
      "  background-color: %13;"
      "  padding: 3px 22px 3px 8px;"
      "  min-height: 22px;"
      "}"
      "QToolBar#ContextToolBar QComboBox::drop-down {"
      "  border: none;"
      "  width: 20px;"
      "}")
      .arg(colorHex(gradientStartColor), colorHex(gradientEndColor),
           colorHex(borderColor), colorHex(toolbarTextColor), colorHex(accentColor),
           colorHex(buttonBorder), colorHex(buttonBackground), colorHex(buttonHover),
           colorHex(buttonPressed), colorHex(checkedBackground),
           colorHex(checkedTextColor), colorHex(comboBorder),
           colorHex(comboBackground), colorHex(buttonTextColor)));

  auto* titleLabel = new QLabel(title, fontToolBar_);
  titleLabel->setObjectName("ContextToolbarTitle");
  fontToolBar_->addWidget(titleLabel);
  fontToolBar_->addSeparator();

  if (contextToolbarMode_ == ContextToolbarMode::Build) {
    if (actionRefreshBoards_ && actionRefreshBoards_->icon().isNull()) {
      actionRefreshBoards_->setIcon(
          QIcon::fromTheme("view-refresh", style()->standardIcon(QStyle::SP_BrowserReload)));
    }
    if (actionRefreshPorts_ && actionRefreshPorts_->icon().isNull()) {
      actionRefreshPorts_->setIcon(
          QIcon::fromTheme("view-refresh", style()->standardIcon(QStyle::SP_BrowserReload)));
    }

    fontToolBar_->addAction(actionVerify_);
    fontToolBar_->addAction(actionUpload_);
    fontToolBar_->addAction(actionJustUpload_);
    fontToolBar_->addAction(actionStop_);
    fontToolBar_->addSeparator();
    fontToolBar_->addAction(actionRefreshBoards_);
    fontToolBar_->addAction(actionRefreshPorts_);
  } else if (contextToolbarMode_ == ContextToolbarMode::Fonts) {
    fontFamilyCombo_ = new QComboBox(fontToolBar_);
    fontFamilyCombo_->setEditable(true);
    fontFamilyCombo_->setMinimumWidth(180);
    QFontDatabase fontDb;
    fontFamilyCombo_->addItems(fontDb.families());
    fontToolBar_->addWidget(fontFamilyCombo_);

    fontSizeCombo_ = new QComboBox(fontToolBar_);
    fontSizeCombo_->setEditable(true);
    fontSizeCombo_->setMinimumWidth(64);
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
          fontFamilyCombo_->setEditText(family);
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
          fontSizeCombo_->setEditText(sizeText);
        }
      }

      const QSignalBlocker blocker(actionToggleBold_);
      actionToggleBold_->setChecked(currentFont.bold());
    }

    actionToggleBold_->setIcon(
        QIcon::fromTheme("format-text-bold",
                         style()->standardIcon(QStyle::SP_TitleBarShadeButton)));
    fontToolBar_->addAction(actionToggleBold_);

    connect(fontFamilyCombo_, &QComboBox::editTextChanged, this,
            [this](const QString&) { updateFontFromToolbar(); });
    connect(fontFamilyCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { updateFontFromToolbar(); });
    connect(fontSizeCombo_, &QComboBox::editTextChanged, this,
            [this](const QString&) { updateFontFromToolbar(); });
    connect(fontSizeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { updateFontFromToolbar(); });
    connect(actionToggleBold_, &QAction::triggered, this,
            &MainWindow::updateFontFromToolbar, Qt::UniqueConnection);
  } else if (contextToolbarMode_ == ContextToolbarMode::Snapshots) {
    fontToolBar_->addAction(actionSnapshotCapture_);
    fontToolBar_->addAction(actionSnapshotCompare_);
    fontToolBar_->addAction(actionSnapshotGallery_);
  }

  QWidget* spacer = new QWidget(fontToolBar_);
  spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  fontToolBar_->addWidget(spacer);
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
