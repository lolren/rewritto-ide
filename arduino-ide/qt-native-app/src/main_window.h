#pragma once

#include <QByteArray>
#include <QMainWindow>
#include <QHash>
#include <QJsonArray>
#include <QMap>
#include <QMetaObject>
#include <QSet>
#include <QStringList>
#include <QVector>
#include <QTreeWidget>

#include <functional>

#include "code_editor.h"
#include "mi_parser.h"

class QAction;
class QFileSystemModel;
class QDockWidget;
class QMenu;
class QCompleter;
class QStandardItemModel;
class QPlainTextEdit;
class QTabWidget;
class QTreeView;
class QToolBar;
class QComboBox;
class QProgressBar;
class QProcess;
class QTimer;
class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;
class QStackedWidget;
class QTemporaryDir;

class ArduinoCli;
class EditorWidget;
class WelcomeWidget;
class LspClient;
class OutputWidget;
class ProblemsWidget;
class SerialMonitorWidget;
class SerialPlotterWidget;
class SerialPort;
class SketchManager;
class ToastWidget;
class ExamplesDialog;
class FindReplaceDialog;
class FindInFilesDialog;
class BoardsManagerDialog;
class LibraryManagerDialog;
class ReplaceInFilesDialog;

class MainWindow final : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;

 public slots:
  void openPaths(const QStringList& paths);

  void toggleFavorite(const QString& fqbn);
  bool isFavorite(const QString& fqbn) const;

 private:
  enum class CliJobKind {
    None,
    IndexUpdate,
    Compile,
    UploadCompile,
    Upload,
    UploadUsingProgrammer,
    BurnBootloader,
    DebugCheck,
    LibraryInstall,
  };

  enum class SerialClientKind {
    None,
    Monitor,
    Plotter,
  };

  SketchManager* sketchManager_ = nullptr;
  ArduinoCli* arduinoCli_ = nullptr;

  QFileSystemModel* fileModel_ = nullptr;
  QTreeView* fileTree_ = nullptr;
  QDockWidget* fileDock_ = nullptr;
  QTabWidget* sketchbookTabs_ = nullptr;
  QFileSystemModel* sketchbookModel_ = nullptr;
  QTreeView* sketchbookTree_ = nullptr;
  QDockWidget* boardsManagerDock_ = nullptr;
  BoardsManagerDialog* boardsManager_ = nullptr;
  QDockWidget* libraryManagerDock_ = nullptr;
  LibraryManagerDialog* libraryManager_ = nullptr;
  QDockWidget* searchDock_ = nullptr;
  QTabWidget* searchTabs_ = nullptr;
  FindInFilesDialog* findInFiles_ = nullptr;
  ReplaceInFilesDialog* replaceInFiles_ = nullptr;

  QStackedWidget* centralStack_ = nullptr;
  WelcomeWidget* welcome_ = nullptr;
  EditorWidget* editor_ = nullptr;
  ToastWidget* toast_ = nullptr;
  OutputWidget* output_ = nullptr;
  QDockWidget* outputDock_ = nullptr;
  ProblemsWidget* problems_ = nullptr;
  QDockWidget* problemsDock_ = nullptr;
  QDockWidget* debugDock_ = nullptr;

  QDockWidget* outlineDock_ = nullptr;
  QTreeView* outlineTree_ = nullptr;
  QStandardItemModel* outlineModel_ = nullptr;
  QTimer* outlineRefreshTimer_ = nullptr;

  SerialPort* serialPort_ = nullptr;
  SerialMonitorWidget* serialMonitor_ = nullptr;
  QDockWidget* serialDock_ = nullptr;
  SerialPlotterWidget* serialPlotter_ = nullptr;
  QDockWidget* serialPlotterDock_ = nullptr;

  QToolBar* buildToolBar_ = nullptr;
  QToolBar* sideBarToolBar_ = nullptr;
  QToolBar* fontToolBar_ = nullptr;
  QComboBox* boardCombo_ = nullptr;
  QComboBox* portCombo_ = nullptr;
  QComboBox* fontFamilyCombo_ = nullptr;
  QComboBox* fontSizeCombo_ = nullptr;
  QAction* actionToggleBold_ = nullptr;
  QAction* actionToggleFontToolBar_ = nullptr;
  QAction* actionRefreshBoards_ = nullptr;
  QAction* actionRefreshPorts_ = nullptr;
  QAction* actionSelectBoard_ = nullptr;
  QProcess* boardsRefreshProcess_ = nullptr;
  QProcess* portsRefreshProcess_ = nullptr;
  bool portsRefreshQueued_ = false;
  QProcess* portsWatchProcess_ = nullptr;
  QByteArray portsWatchBuffer_;
  QTimer* portsWatchDebounceTimer_ = nullptr;
  QTimer* portsAutoRefreshTimer_ = nullptr;
  bool portsWatchHadError_ = false;
  QProgressBar* cliBusy_ = nullptr;
  QLabel* cliBusyLabel_ = nullptr;
  QLabel* boardPortLabel_ = nullptr;
  QLabel* buildSummaryLabel_ = nullptr;
  QLabel* cursorPosLabel_ = nullptr;
  QMetaObject::Connection cursorPosConn_;

  LspClient* lsp_ = nullptr;
  QTimer* lspRestartTimer_ = nullptr;

  QAction* actionOpenSketch_ = nullptr;
  QAction* actionOpenSketchFolder_ = nullptr;
  QAction* actionNewSketch_ = nullptr;
  QAction* actionNewTab_ = nullptr;
  QAction* actionCloseTab_ = nullptr;
  QAction* actionCloseAllTabs_ = nullptr;
  QAction* actionReopenClosedTab_ = nullptr;
  QAction* actionSave_ = nullptr;
  QAction* actionSaveAs_ = nullptr;
  QAction* actionSaveCopyAs_ = nullptr;
  QAction* actionSaveAll_ = nullptr;
  QAction* actionExamples_ = nullptr;
  QAction* actionPreferences_ = nullptr;
  QAction* actionPinSketch_ = nullptr;
  QAction* actionQuickOpen_ = nullptr;
  QAction* actionCommandPalette_ = nullptr;
  QAction* actionQuit_ = nullptr;
  QAction* actionUndo_ = nullptr;
  QAction* actionRedo_ = nullptr;
  QAction* actionCut_ = nullptr;
  QAction* actionCopy_ = nullptr;
  QAction* actionPaste_ = nullptr;
  QAction* actionSelectAll_ = nullptr;
  QAction* actionToggleComment_ = nullptr;
  QAction* actionIncreaseIndent_ = nullptr;
  QAction* actionDecreaseIndent_ = nullptr;
  QAction* actionFind_ = nullptr;
  QAction* actionFindNext_ = nullptr;
  QAction* actionFindPrevious_ = nullptr;
  QAction* actionReplace_ = nullptr;
  QAction* actionFindInFiles_ = nullptr;
  QAction* actionReplaceInFiles_ = nullptr;
  QAction* actionSidebarSearch_ = nullptr;
  QAction* actionGoToLine_ = nullptr;
  QAction* actionGoToSymbol_ = nullptr;
  QAction* actionCompletion_ = nullptr;
  QAction* actionShowHover_ = nullptr;
  QAction* actionGoToDefinition_ = nullptr;
  QAction* actionFindReferences_ = nullptr;
  QAction* actionRenameSymbol_ = nullptr;
  QAction* actionCodeActions_ = nullptr;
  QAction* actionOrganizeImports_ = nullptr;
  QAction* actionFormatDocument_ = nullptr;
  QAction* actionZoomIn_ = nullptr;
  QAction* actionZoomOut_ = nullptr;
  QAction* actionZoomReset_ = nullptr;
  QAction* actionFullScreen_ = nullptr;
  QAction* actionWordWrap_ = nullptr;
  QAction* actionLineEndingLF_ = nullptr;
  QAction* actionLineEndingCRLF_ = nullptr;
  QAction* actionResetLayout_ = nullptr;
  QAction* actionVerify_ = nullptr;
  QAction* actionUpload_ = nullptr;
  QAction* actionJustUpload_ = nullptr;
  QAction* actionStop_ = nullptr;
  QAction* actionUploadUsingProgrammer_ = nullptr;
  QAction* actionExportCompiledBinary_ = nullptr;
  QAction* actionOptimizeForDebug_ = nullptr;
  QAction* actionShowSketchFolder_ = nullptr;
  QAction* actionRenameSketch_ = nullptr;
  QAction* actionAddFileToSketch_ = nullptr;
  QAction* actionAddZipLibrary_ = nullptr;
  QAction* actionManageLibraries_ = nullptr;
  QAction* actionBoardsManager_ = nullptr;
  QAction* actionLibraryManager_ = nullptr;
  QAction* actionSerialMonitor_ = nullptr;
  QAction* actionSerialPlotter_ = nullptr;
  QAction* actionBurnBootloader_ = nullptr;
  QAction* actionGetBoardInfo_ = nullptr;
  QAction* actionAbout_ = nullptr;
  QAction* actionGettingStarted_ = nullptr;
  QAction* actionReference_ = nullptr;
  QAction* actionTroubleshooting_ = nullptr;
  QAction* actionArduinoWebsite_ = nullptr;

  // Additional Tools menu actions
  QAction* actionAutoFormat_ = nullptr;
  QAction* actionArchiveSketch_ = nullptr;
  QAction* actionWiFiFirmwareUpdater_ = nullptr;
  QAction* actionUploadSSL_ = nullptr;
  QAction* actionProgrammerAVRISP_ = nullptr;
  QAction* actionProgrammerUSBasp_ = nullptr;
  QAction* actionProgrammerArduinoISP_ = nullptr;
  QAction* actionProgrammerUSBTinyISP_ = nullptr;

  // Debug actions
  QAction* actionStartDebugging_ = nullptr;
  QAction* actionStepOver_ = nullptr;
  QAction* actionStepInto_ = nullptr;
  QAction* actionStepOut_ = nullptr;
  QAction* actionContinue_ = nullptr;
  QAction* actionStopDebugging_ = nullptr;

  QMenu* recentSketchesMenu_ = nullptr;
  QMenu* examplesMenu_ = nullptr;
  QMenu* viewMenu_ = nullptr;
  QMenu* toolsMenu_ = nullptr;
  QMenu* toolbarsMenu_ = nullptr;
  QMenu* includeLibraryMenu_ = nullptr;
  QAction* toolsMenuSeparator_ = nullptr;
  QMenu* boardMenu_ = nullptr;
  QMenu* portMenu_ = nullptr;
  QAction* portMenuAction_ = nullptr;
  QMenu* programmerMenu_ = nullptr;
  QAction* programmerMenuAction_ = nullptr;
  QVector<QAction*> boardOptionMenuActions_;
  QProcess* boardDetailsProcess_ = nullptr;
  QTimer* boardOptionsRefreshTimer_ = nullptr;
  QVector<QAction*> includeLibraryMenuActions_;
  QProcess* includeLibraryProcess_ = nullptr;

  // Required tools for the current board (from board details)
  struct RequiredTool final {
    QString name;
    QString packager;
    QString version;
  };
  QVector<RequiredTool> requiredTools_;

  QByteArray defaultDockState_;

  QStandardItemModel* completionModel_ = nullptr;
  QCompleter* completer_ = nullptr;
  FindReplaceDialog* findReplaceDialog_ = nullptr;
  QLineEdit* debugProgrammerEdit_ = nullptr;
  QPushButton* debugCheckButton_ = nullptr;
  QPushButton* debugStartButton_ = nullptr;
  QPushButton* debugStopButton_ = nullptr;
  QPushButton* debugAttachButton_ = nullptr;
  QPushButton* debugDetachButton_ = nullptr;
  QPushButton* debugClearButton_ = nullptr;
  QPushButton* debugContinueButton_ = nullptr;
  QPushButton* debugInterruptButton_ = nullptr;
  QPushButton* debugNextButton_ = nullptr;
  QPushButton* debugStepButton_ = nullptr;
  QPushButton* debugFinishButton_ = nullptr;
  QPushButton* debugSyncBreakpointsButton_ = nullptr;
  QTabWidget* debugInfoTabs_ = nullptr;
  QTreeWidget* debugBreakpointsTree_ = nullptr;
  QTreeWidget* debugThreadsTree_ = nullptr;
  QTreeWidget* debugCallStackTree_ = nullptr;
  QTreeWidget* debugLocalsTree_ = nullptr;
  QTreeWidget* debugWatchesTree_ = nullptr;
  QLineEdit* debugWatchEdit_ = nullptr;
  QPushButton* debugWatchAddButton_ = nullptr;
  QPushButton* debugWatchRemoveButton_ = nullptr;
  QPushButton* debugWatchClearButton_ = nullptr;
  QPlainTextEdit* debugConsole_ = nullptr;
  QLineEdit* debugCommandEdit_ = nullptr;
  QPushButton* debugSendButton_ = nullptr;
  QProcess* debugProcess_ = nullptr;
  struct BreakpointSpec final {
    bool enabled = true;
    QString condition;
    int hitCount = 0;  // break on Nth hit; 0 disables hit-count
    int gdbId = -1;    // GDB breakpoint ID for precise management
    QString logMessage;  // If non-empty, this is a logpoint (print without stopping)
    bool isLogpoint = false;  // true if this breakpoint is a logpoint
  };
  QHash<QString, QMap<int, BreakpointSpec>> breakpointsByFile_;  // abs file -> (line -> spec)
  QHash<int, QPair<QString, int>> breakpointById_;  // gdb id -> (file, line) reverse mapping
  QStringList debugWatchExpressions_;
  QString debugSelectedThreadId_;

  enum class DebugInferiorState {
    Unknown,
    Running,
    Stopped,
  };
  DebugInferiorState debugInferiorState_ = DebugInferiorState::Unknown;
  int debugSelectedFrameLevel_ = 0;

  MiParser debugMiParser_;
  int debugMiNextToken_ = 1;
  QHash<int, std::function<void(const MiParser::Record&)>> debugMiPending_;

  void createActions();
  void createMenus();
  void createLayout();
  void updateFontFromToolbar();
  void wireSignals();
  void rebuildRecentSketchesMenu();
  void rebuildExamplesMenu();
  void updateWelcomePage();
  void updateWelcomeVisibility();
  void migrateSketchListsToFolders();
  QString normalizeSketchFolderPath(const QString& path) const;
  QString currentSketchFolderPath() const;
  bool openSketchFolderInUi(const QString& folder);
  void rebuildIncludeLibraryMenu();
  void showExamplesDialog(QString initialFilter = {});
  void insertLibraryIncludes(const QString& libraryName, const QStringList& includes);
  void clearIncludeLibraryMenuActions();
  QStringList recentSketches() const;
  void setRecentSketches(QStringList items);
  void addRecentSketch(const QString& folder);
  QStringList pinnedSketches() const;
  void setPinnedSketches(QStringList items);
  void setSketchPinned(const QString& folder, bool pinned);
  bool isSketchPinned(const QString& folder) const;
  void refreshInstalledBoards();
  void refreshConnectedPorts();
  void scheduleBoardListRefresh();
  void startPortWatcher();
  void stopPortWatcher();
  void rebuildBoardMenu();
  void rebuildPortMenu();
  void maybeAutoSelectBoardForCurrentPort();
  void updateSketchbookView();
  void scheduleRefreshBoardOptions();
  void refreshBoardOptions();
  void clearBoardOptionMenus();
  void stopRefreshProcesses();
  QString currentFqbn() const;
  QString currentProgrammer() const;
  QString currentPort() const;
  QString currentPortProtocol() const;
  bool currentPortIsMissing() const;
  void updateBoardPortIndicator();
  void clearPendingUploadFlow();
  bool startUploadFromPendingFlow();
  bool hasBackgroundWork() const;
  QString busyStatusText() const;
  void showToast(const QString& message, int timeoutMs = 5000);
  void showToastWithAction(const QString& message,
                           const QString& actionText,
                           std::function<void()> action,
                           int timeoutMs = 5000);
  void focusOutputDock();
  void focusBoardsManagerSearch(const QString& query);
  void focusLibraryManagerSearch(const QString& query);
  void updateStopActionState();
  void showQuickOpen();
  void showCommandPalette();
  void showGoToSymbol();
  void showFindReplaceDialog();
  void showFindInFilesDialog();
  void showReplaceInFilesDialog();
  void showSelectBoardDialog();
  void goToLine();
  void handleQuickFix(const QString& filePath, int line, int column, const QString& fixType);
  void restartLanguageServer();
  void scheduleRestartLanguageServer();
  void stopLanguageServer();
  void applyDebugBreakpoints();
  void rebuildDebugBreakpointsTree();
  void rebuildDebugWatchesTree();
  void stopDebugProcess();
  void clearDebugSessionState();
  void refreshDebugThreads();
  void refreshDebugCallStack();
  void refreshDebugLocals();
  void refreshDebugWatches();
  void selectDebugThread(const QString& threadId);
  void selectDebugFrame(int level, bool openLocation);

  // File menu actions
  void newSketch();
  void openSketch();
  void openSketchFolder();

  // Sketch menu actions
  void verifySketch();
  void uploadSketch();
  void fastUploadSketch();
  void stopOperation();
  void uploadUsingProgrammer();
  void exportCompiledBinary();
  void showSketchFolder();
  void renameSketch();
  void addFileToSketch();
  void addZipLibrary();

  // Tools menu actions
  void toggleSerialMonitor();
  void toggleSerialPlotter();
  void getBoardInfo();
  void burnBootloader();
  void autoFormatSketch();
  void archiveSketch();
  void showWiFiFirmwareUpdater();
  void uploadSslRootCertificates();
  void setProgrammer(const QString& programmer);
  void setBoardOption(const QString& optionId, const QString& valueId);

  // Debug actions
  void startDebugging();
  void debugStepOver();
  void debugStepInto();
  void debugStepOut();
  void debugContinue();
  void stopDebugging();

  // Help menu actions
  void showAbout();

  // Window management
  void updateWindowTitleForFile(const QString& filePath);

  // Attach/detach support
  void attachToRunningProcess();
  void detachFromProcess();

  // Required tools validation
  bool checkRequiredTools(QStringList& missingTools) const;
  void showMissingToolsDialog(const QStringList& missingTools);

  // Variables tree helpers for nested structures
  void onDebugVariableExpanded(QTreeWidgetItem* item);
  void expandDebugVariable(QTreeWidgetItem* item, const QString& varPath);
  void requestVariableChildren(QTreeWidgetItem* item, const QString& varPath);
  QString formatDebugValue(const QString& value, const QString& type);
  bool isExpandableType(const QString& type) const;
  void handleMiRecord(const MiParser::Record& r);
  void sendMiRequest(const QString& cmd,
                     std::function<void(const MiParser::Record&)> onResult);
  void setDebugInferiorState(DebugInferiorState state);
  static QString toFileUri(const QString& filePath);
  static QString findExecutable(const QString& name);

  // Helper functions
  bool createZipArchive(const QString& sourceDir, const QString& zipPath);

  void requestCompletion();
  void showHover();
  void goToDefinition();
  void findReferences();
  void renameSymbol();
  void showCodeActions(QStringList onlyKinds = {});
  void formatDocument();
  bool applyWorkspaceEdit(const QJsonObject& workspaceEdit);
  void scheduleOutlineRefresh();
  void refreshOutline();

  void restoreStateFromSettings();
  void persistStateToSettings();
  void persistBuildFields();

  // Dock state validation helpers
  bool validateDockState(const QByteArray& state) const;
  QByteArray sanitizeDockState(const QByteArray& state) const;
  bool restoreDockStateWithFallback(const QByteArray& state);

 protected:
  void closeEvent(QCloseEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  QString lastFindText_;
  QString lastReplaceText_;
  int lastFindFlags_ = 0;

  QHash<QString, QJsonArray> lspDiagnosticsByFilePath_;
  QHash<QString, QVector<CodeEditor::Diagnostic>> compilerDiagnostics_;

  CliJobKind lastCliJobKind_ = CliJobKind::None;
  bool capturingCliOutput_ = false;
  QString cliOutputCapture_;
  QString cliOutputBuffer_;

  void processOutputChunk(const QString& chunk);

  struct PendingUploadFlow final {
    QString sketchFolder;
    QString buildPath;
    QString fqbn;
    QString port;
    QString protocol;
    QString programmer;
    QString warnings;
    bool verboseCompile = false;
    bool verboseUpload = false;
    bool useInputDir = true;
    CliJobKind finalJobKind = CliJobKind::Upload;
  };
  PendingUploadFlow pendingUploadFlow_;
  QTemporaryDir* uploadBuildDir_ = nullptr;

  QTimer* serialReconnectTimer_ = nullptr;
  SerialClientKind serialDesiredClient_ = SerialClientKind::None;
  QString serialDesiredPort_;
  int serialDesiredBaud_ = 0;
  int serialReconnectAttempt_ = 0;
  bool serialManualDisconnect_ = false;
  bool serialSuppressCloseEvent_ = false;

  bool cliCancelRequested_ = false;
  bool pendingUploadCancelled_ = false;

  QSet<QString> lastDetectedPorts_;
  QSet<QString> favoriteFqbns_;

  void loadFavorites();
  void saveFavorites();
};
