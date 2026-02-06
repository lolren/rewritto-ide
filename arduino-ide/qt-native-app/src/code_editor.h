#pragma once

#include <QPlainTextEdit>
#include <QMap>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextFormat>
#include <QVector>

class QTimer;

class CodeEditor final : public QPlainTextEdit {
  Q_OBJECT

 public:
  explicit CodeEditor(QWidget* parent = nullptr);

  struct Diagnostic final {
    int startLine = 0;       // 0-based
    int startCharacter = 0;  // 0-based (UTF-16 code units)
    int endLine = 0;         // 0-based
    int endCharacter = 0;    // 0-based (UTF-16 code units)
    int severity = 0;        // 1 error, 2 warning, 3 info, 4 hint
  };

  void setEditorSettings(int tabSize, bool insertSpaces);
  int tabSize() const;
  bool insertSpaces() const;

  void setShowIndentGuides(bool enabled);
  bool showIndentGuides() const;
  void setShowWhitespace(bool enabled);
  bool showWhitespace() const;

  void setDiagnostics(const QVector<Diagnostic>& diagnostics);
  void clearDiagnostics();

  void setNavigationLineHighlight(const QTextCursor& cursor);
  void clearNavigationLineHighlight();

  QVector<int> breakpoints() const;        // 1-based lines
  bool hasBreakpoint(int line) const;      // 1-based
  bool breakpointEnabled(int line) const;  // 1-based
  bool hasConditionalBreakpoint(int line) const;  // 1-based
  bool hasLogpoint(int line) const;  // 1-based
  void toggleBreakpoint(int line);         // 1-based
  void setBreakpointEnabled(int line, bool enabled);  // 1-based
  void setConditionalBreakpoint(int line, bool conditional);  // 1-based
  void setLogpoint(int line, bool isLogpoint);  // 1-based
  void setBreakpoints(const QVector<int>& lines);  // 1-based
  void clearAllBreakpoints();

  bool canFold(int line) const;     // 1-based
  bool isFolded(int line) const;    // 1-based
  void toggleFold(int line);        // 1-based
  void unfoldAllFolds();
  void setFoldingEnabled(bool enabled);
  bool foldingEnabled() const;

  bool snippetSessionActive() const;
  void clearSnippetSession();
  bool insertSnippet(int replaceStart, int replaceEnd, const QString& snippetText);

  // Multi-cursor support
  void addCursorBelow();
  void addCursorAbove();
  void addCursorToNextOccurrence();
  void addCursorToAllOccurrences();
  void addCursorToLineEnds();
  void clearAdditionalCursors();
  int cursorCount() const;
  bool hasMultipleCursors() const;

  int lineNumberAreaWidth() const;
  void lineNumberAreaPaintEvent(QPaintEvent* event);

 signals:
  void breakpointsChanged(QVector<int> lines);

 protected:
  void resizeEvent(QResizeEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void paintEvent(QPaintEvent* event) override;

 private:
  class LineNumberArea;
  LineNumberArea* lineNumberArea_ = nullptr;
  QTimer* foldRecomputeTimer_ = nullptr;
  int tabSize_ = 2;
  bool insertSpaces_ = true;
  bool showIndentGuides_ = true;
  bool showWhitespace_ = false;
  bool foldingEnabled_ = true;

  void updateLineNumberAreaWidth(int newBlockCount);
  void updateLineNumberArea(const QRect& rect, int dy);

  static constexpr int kExtraPropertyRole = QTextFormat::UserProperty + 1;
  static constexpr int kFoldIndicatorWidth = 14;

  struct FoldBlockUserData final : public QTextBlockUserData {
    static constexpr int kMagic = 0x56424C4B;  // 'VBLK'
    int magic = kMagic;
    bool folded = false;
  };

  struct SnippetPlaceholder final {
    int index = 0;
    int start = 0;
    int end = 0;
  };

  struct SnippetNavItem final {
    int index = 0;
    int start = 0;
    int end = 0;
  };

  bool snippetActive_ = false;
  bool snippetSettingCursor_ = false;
  int snippetFinalPos_ = 0;
  int snippetCurrent_ = 0;
  QVector<SnippetNavItem> snippetNav_;

  // Multi-cursor state
  struct CursorInfo final {
    int position = 0;
    int anchor = 0;
  };
  QVector<CursorInfo> additionalCursors_;
  bool multiCursorEditing_ = false;

  QMap<int, int> foldEndByStartBlock_;
  QVector<Diagnostic> diagnostics_;

  void updateBracketMatch();
  void updateCurrentLineHighlight();
  void recomputeFoldRegions();
  void applyFoldStates();
  FoldBlockUserData* foldUserDataFor(const QTextBlock& block, bool create) const;
  bool blockIsFolded(const QTextBlock& block) const;

  bool advanceSnippet(bool forward);
  void selectSnippetCurrent();
  void updateSnippetRanges(int pos, int charsRemoved, int charsAdded);

  // Multi-cursor helpers
  void updateMultiCursorRendering();
  void insertTextAtAllCursors(const QString& text);
  void deleteTextAtAllCursors();
  void syncAdditionalCursorsFromSelection();
  QVector<QTextCursor> allTextCursors() const;
  bool handleMultiCursorKeyEvent(QKeyEvent* event);
  QString currentWordUnderCursor() const;
};
