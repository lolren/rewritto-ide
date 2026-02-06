#pragma once

#include <QWidget>

#include <QSet>
#include <QTextDocument>
#include <QFont>

#include "code_editor.h"

class QPlainTextEdit;
class QTabWidget;
class QFileSystemWatcher;
class QTimer;

class CppHighlighter;

class EditorWidget final : public QWidget {
  Q_OBJECT

 public:
  explicit EditorWidget(QWidget* parent = nullptr);

  bool openFile(const QString& filePath);
  bool reloadFileIfUnmodified(const QString& filePath);
  bool save();
  bool saveCurrentWithDialog();
  bool saveAs(const QString& filePath);
  bool saveCopyAs(const QString& filePath);
  bool saveAll();
  bool saveAllWithDialog();

  bool closeCurrentTab();
  bool closeAllTabs();
  bool reopenLastClosedTab();

  void setDefaultSaveDirectory(QString directory);
  QString defaultSaveDirectory() const;

  bool hasUnsavedChanges() const;
  QString currentFilePath() const;
  QPlainTextEdit* currentEditorWidget() const;
  QPlainTextEdit* editorWidgetForFile(const QString& filePath) const;
  void setEditorSettings(int tabSize, bool insertSpaces);
  int tabSize() const;
  bool insertSpaces() const;

  void setWordWrapEnabled(bool enabled);
  bool wordWrapEnabled() const;

  void setEditorFont(QFont font);
  QFont editorFont() const;

  void setTheme(bool isDark);

  void setShowIndentGuides(bool enabled);
  bool showIndentGuides() const;
  void setShowWhitespace(bool enabled);
  bool showWhitespace() const;

  void setAutosaveEnabled(bool enabled);
  bool autosaveEnabled() const;
  void setAutosaveIntervalSeconds(int seconds);
  int autosaveIntervalSeconds() const;

  void applyZoomDelta(int deltaSteps);
  void resetZoom();
  int zoomSteps() const;

  bool findNext(const QString& text);
  bool find(const QString& text, QTextDocument::FindFlags flags);
  bool replaceOne(const QString& findText, const QString& replaceText);
  bool replaceOne(const QString& findText,
                  const QString& replaceText,
                  QTextDocument::FindFlags flags);
  int replaceAll(const QString& findText,
                 const QString& replaceText,
                 QTextDocument::FindFlags flags);

  bool openLocation(const QString& filePath, int line, int column);
  bool goToLine(int line, int column = 1);

  void setDiagnostics(const QString& filePath, const QVector<CodeEditor::Diagnostic>& diagnostics);
  void clearDiagnostics(const QString& filePath);
  void clearAllDiagnostics();

  QVector<QString> openedFiles() const;
  QString textForFile(const QString& filePath) const;

 signals:
  void documentOpened(QString filePath, QString text);
  void documentChanged(QString filePath, QString text);
  void documentClosed(QString filePath);
  void currentFileChanged(QString filePath);
  void newTabRequested();
  void breakpointsChanged(QString filePath, QVector<int> lines);
  void autosaveFailed(QString filePath);

 private:
  QTabWidget* tabs_ = nullptr;
  int tabSize_ = 2;
  bool insertSpaces_ = true;
  bool wordWrapEnabled_ = false;
  bool showIndentGuides_ = true;
  bool showWhitespace_ = false;
  bool autosaveEnabled_ = false;
  int autosaveIntervalSec_ = 30;
  QTimer* autosaveTimer_ = nullptr;
  QSet<QString> autosaveErrorNotified_;
  int zoomSteps_ = 0;
  QFont editorFont_;
  QString defaultSaveDir_;
  QFileSystemWatcher* fileWatcher_ = nullptr;
  QTimer* fileChangedTimer_ = nullptr;
  QSet<QString> pendingFileChanges_;
  QStringList closedFileStack_;

  QPlainTextEdit* currentEditor() const;
  QPlainTextEdit* editorForFile(const QString& filePath) const;
  QString filePathFor(QPlainTextEdit* editor) const;
  void setFilePathFor(QPlainTextEdit* editor, const QString& filePath);
  void watchFilePath(const QString& filePath);
  void unwatchFilePath(const QString& filePath);
  void processPendingFileChanges();
  void updateDiskMTimeProperty(QPlainTextEdit* editor, const QString& filePath);
  void wireBreakpointSignals(CodeEditor* editor);
  void updateTabTitle(QPlainTextEdit* editor);
  void applyAppearance(QPlainTextEdit* editor);
  bool closeTabAt(int index);
  void updateAutosaveTimer();
  void maybeAutosave();

  bool saveEditorAs(QPlainTextEdit* editor, const QString& filePath);
  bool promptAndSaveEditor(QPlainTextEdit* editor);
};
