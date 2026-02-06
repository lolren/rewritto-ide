#include <QtTest/QtTest>

#include <QApplication>
#include <QKeyEvent>
#include <QTextOption>

#include "code_editor.h"

class TestCodeEditor final : public QObject {
  Q_OBJECT

 private slots:
  void lineNumberAreaWidthGrowsWithLines();
  void autoIndentOnNewlineAfterBrace();
  void tabInsertsSpaces();
  void shiftTabUnindentsCurrentLine();
  void togglesWhitespaceRendering();
  void highlightsMatchingBrackets();
  void keepsBracketMatchWithDiagnosticsAndNavHighlight();
  void breakpointEnableDisableKeepsLine();
  void foldsAndUnfoldsBraceRegions();
  void insertsAndNavigatesSnippets();
};

void TestCodeEditor::lineNumberAreaWidthGrowsWithLines() {
  CodeEditor editor;
  editor.setPlainText("one\n");
  const int w1 = editor.lineNumberAreaWidth();

  QString many;
  for (int i = 0; i < 200; ++i) {
    many += "x\n";
  }
  editor.setPlainText(many);
  const int w2 = editor.lineNumberAreaWidth();

  QVERIFY(w2 > w1);
}

void TestCodeEditor::autoIndentOnNewlineAfterBrace() {
  CodeEditor editor;
  editor.setPlainText("  if (true) {");
  editor.moveCursor(QTextCursor::End);

  QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
  QApplication::sendEvent(&editor, &enter);

  const QString text = editor.toPlainText();
  QVERIFY(text.contains("\n    "));
}

void TestCodeEditor::tabInsertsSpaces() {
  CodeEditor editor;
  editor.setEditorSettings(4, true);
  editor.setPlainText("");
  editor.moveCursor(QTextCursor::Start);

  QKeyEvent tab(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier);
  QApplication::sendEvent(&editor, &tab);

  QCOMPARE(editor.toPlainText(), QStringLiteral("    "));
}

void TestCodeEditor::shiftTabUnindentsCurrentLine() {
  CodeEditor editor;
  editor.setEditorSettings(4, true);
  editor.setPlainText("    x");
  editor.moveCursor(QTextCursor::Start);

  QKeyEvent backtab(QEvent::KeyPress, Qt::Key_Backtab, Qt::ShiftModifier);
  QApplication::sendEvent(&editor, &backtab);

  QCOMPARE(editor.toPlainText(), QStringLiteral("x"));
}

void TestCodeEditor::togglesWhitespaceRendering() {
  CodeEditor editor;
  QVERIFY(!editor.showWhitespace());
  const auto flags0 = editor.document()->defaultTextOption().flags();
  QVERIFY(!(flags0 & QTextOption::ShowTabsAndSpaces));

  editor.setShowWhitespace(true);
  QVERIFY(editor.showWhitespace());
  const auto flags1 = editor.document()->defaultTextOption().flags();
  QVERIFY(flags1 & QTextOption::ShowTabsAndSpaces);

  editor.setShowWhitespace(false);
  QVERIFY(!editor.showWhitespace());
  const auto flags2 = editor.document()->defaultTextOption().flags();
  QVERIFY(!(flags2 & QTextOption::ShowTabsAndSpaces));
}

void TestCodeEditor::highlightsMatchingBrackets() {
  CodeEditor editor;
  editor.setPlainText("{\n}\n");

  QTextCursor c(editor.document());
  c.setPosition(1);  // just after '{'
  editor.setTextCursor(c);
  QApplication::processEvents();

  const auto selections = editor.extraSelections();
  int bracketSelections = 0;
  int scopeSelections = 0;
  for (const auto& s : selections) {
    const QString role = s.format.property(QTextFormat::UserProperty + 1).toString();
    if (role == "bracket") {
      ++bracketSelections;
    } else if (role == "scope") {
      ++scopeSelections;
    }
  }
  QVERIFY(bracketSelections >= 2);
  QVERIFY(scopeSelections >= 1);
}

void TestCodeEditor::keepsBracketMatchWithDiagnosticsAndNavHighlight() {
  CodeEditor editor;
  editor.setPlainText("{\n}\n");

  QTextCursor c(editor.document());
  c.setPosition(1);  // just after '{'
  editor.setTextCursor(c);
  QApplication::processEvents();

  CodeEditor::Diagnostic d;
  d.startLine = 0;
  d.startCharacter = 0;
  d.endLine = 0;
  d.endCharacter = 1;
  d.severity = 1;
  editor.setDiagnostics({d});
  editor.setNavigationLineHighlight(editor.textCursor());

  const auto selections = editor.extraSelections();
  int bracketSelections = 0;
  int diagnosticSelections = 0;
  int navSelections = 0;
  for (const auto& s : selections) {
    const QString role = s.format.property(QTextFormat::UserProperty + 1).toString();
    if (role == "bracket") {
      ++bracketSelections;
    } else if (role == "diagnostic") {
      ++diagnosticSelections;
      QCOMPARE(s.format.underlineStyle(), QTextCharFormat::WaveUnderline);
    } else if (role == "navline") {
      ++navSelections;
    }
  }

  QVERIFY(bracketSelections >= 2);
  QCOMPARE(diagnosticSelections, 1);
  QCOMPARE(navSelections, 1);
}

void TestCodeEditor::breakpointEnableDisableKeepsLine() {
  CodeEditor editor;
  editor.setPlainText("a\nb\nc\n");

  editor.setBreakpoints({2});
  QVERIFY(editor.hasBreakpoint(2));
  QVERIFY(editor.breakpointEnabled(2));
  QCOMPARE(editor.breakpoints(), QVector<int>({2}));

  editor.setBreakpointEnabled(2, false);
  QVERIFY(editor.hasBreakpoint(2));
  QVERIFY(!editor.breakpointEnabled(2));
  QCOMPARE(editor.breakpoints(), QVector<int>({2}));

  editor.setBreakpointEnabled(2, true);
  QVERIFY(editor.breakpointEnabled(2));
  QCOMPARE(editor.breakpoints(), QVector<int>({2}));
}

void TestCodeEditor::foldsAndUnfoldsBraceRegions() {
  CodeEditor editor;
  editor.setPlainText(
      "void foo() {\n"
      "  int x = 0;\n"
      "  if (x) {\n"
      "    x++;\n"
      "  }\n"
      "  x++;\n"
      "}\n"
      "int after = 1;\n");

  QVERIFY(editor.canFold(1));
  QVERIFY(!editor.isFolded(1));

  int visibleBefore = 0;
  for (QTextBlock b = editor.document()->firstBlock(); b.isValid(); b = b.next()) {
    if (b.isVisible()) {
      ++visibleBefore;
    }
  }

  editor.toggleFold(1);
  QVERIFY(editor.isFolded(1));

  int visibleAfterFold = 0;
  for (QTextBlock b = editor.document()->firstBlock(); b.isValid(); b = b.next()) {
    if (b.isVisible()) {
      ++visibleAfterFold;
    }
  }
  QVERIFY(visibleAfterFold < visibleBefore);

  editor.toggleFold(1);
  QVERIFY(!editor.isFolded(1));

  int visibleAfterUnfold = 0;
  for (QTextBlock b = editor.document()->firstBlock(); b.isValid(); b = b.next()) {
    if (b.isVisible()) {
      ++visibleAfterUnfold;
    }
  }
  QCOMPARE(visibleAfterUnfold, visibleBefore);
}

void TestCodeEditor::insertsAndNavigatesSnippets() {
  CodeEditor editor;
  editor.setPlainText("");
  editor.show();
  editor.setFocus();
  QApplication::processEvents();

  QVERIFY(editor.insertSnippet(0, 0, "foo(${1:first}, ${2:second})$0"));
  QCOMPARE(editor.toPlainText(), QStringLiteral("foo(first, second)"));
  QVERIFY(editor.snippetSessionActive());

  QTextCursor c = editor.textCursor();
  QVERIFY(c.hasSelection());
  QCOMPARE(c.selectedText(), QStringLiteral("first"));

  QTest::keyClicks(&editor, "longer");
  QCOMPARE(editor.toPlainText(), QStringLiteral("foo(longer, second)"));

  QKeyEvent tab(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier);
  QApplication::sendEvent(&editor, &tab);
  QApplication::processEvents();

  c = editor.textCursor();
  QVERIFY(c.hasSelection());
  QCOMPARE(c.selectedText(), QStringLiteral("second"));

  QTest::keyClicks(&editor, "x");
  QCOMPARE(editor.toPlainText(), QStringLiteral("foo(longer, x)"));

  QApplication::sendEvent(&editor, &tab);  // finish snippet ($0)
  QApplication::processEvents();

  QVERIFY(!editor.snippetSessionActive());
  QCOMPARE(editor.textCursor().position(), editor.toPlainText().size());

  QApplication::sendEvent(&editor, &tab);  // normal tab inserts spaces
  QCOMPARE(editor.toPlainText(), QStringLiteral("foo(longer, x)  "));
}

int main(int argc, char** argv) {
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);
  TestCodeEditor tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "test_code_editor.moc"
