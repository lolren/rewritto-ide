#include <QtTest/QtTest>

#include <QApplication>
#include <QFile>
#include <QPlainTextEdit>
#include <QSettings>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTextCursor>
#include <QTextDocument>

#include "code_editor.h"
#include "cpp_highlighter.h"
#include "editor_widget.h"

namespace {
class SettingsKeyGuard final {
 public:
  explicit SettingsKeyGuard(QString key) : key_(std::move(key)) {
    QSettings settings;
    settings.beginGroup("Preferences");
    hadPrev_ = settings.contains(key_);
    prev_ = settings.value(key_);
    settings.endGroup();
  }

  ~SettingsKeyGuard() {
    QSettings settings;
    settings.beginGroup("Preferences");
    if (hadPrev_) {
      settings.setValue(key_, prev_);
    } else {
      settings.remove(key_);
    }
    settings.endGroup();
  }

 private:
  QString key_;
  bool hadPrev_ = false;
  QVariant prev_;
};
}  // namespace

class TestEditorWidget final : public QObject {
  Q_OBJECT

 private slots:
  void openSaveFindReplaceGoToLine();
  void preservesCrlfOnSaveAs();
  void usesDefaultLineEndingForEmptyFiles();
  void trimsTrailingWhitespaceWhenEnabled();
 void saveAllSavesModifiedFiles();
 void saveCopyAsKeepsOriginalFilePath();
 void autoReloadsWhenFileChangesOnDisk();
 void opensLargeFilesWithoutHighlightingOrFolding();
};

void TestEditorWidget::openSaveFindReplaceGoToLine() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  const QString filePath = dir.filePath("test.ino");
  {
    QFile f(filePath);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write("void setup(){}\nint x = 0;\ninteger i = 1;\nvoid loop(){}\n");
  }

  EditorWidget w;
  QSignalSpy openedSpy(&w, &EditorWidget::documentOpened);

  QVERIFY(w.openFile(filePath));
  QCOMPARE(openedSpy.count(), 1);
  QCOMPARE(w.currentFilePath(), filePath);
  QVERIFY(w.openedFiles().contains(filePath));

  QVERIFY(w.findNext("int x"));
  QVERIFY(w.replaceOne("int x", "int y"));

  const int replacedWholeWords =
      w.replaceAll("int", "short", QTextDocument::FindWholeWords);
  QCOMPARE(replacedWholeWords, 1);

  QVERIFY(w.goToLine(2));
  auto* tabs = w.findChild<QTabWidget*>();
  QVERIFY(tabs);
  auto* editor = qobject_cast<QPlainTextEdit*>(tabs->currentWidget());
  QVERIFY(editor);
  QCOMPARE(editor->textCursor().blockNumber(), 1);

  const QString outPath = dir.filePath("out.ino");
  QVERIFY(w.saveAs(outPath));

  QFile out(outPath);
  QVERIFY(out.open(QIODevice::ReadOnly | QIODevice::Text));
  const QByteArray saved = out.readAll();
  QVERIFY(saved.contains("short y"));
  QVERIFY(saved.contains("integer i"));
}

void TestEditorWidget::preservesCrlfOnSaveAs() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  const QString filePath = dir.filePath("crlf.ino");
  {
    QFile f(filePath);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write("line1\r\nline2\r\n");
  }

  EditorWidget w;
  QVERIFY(w.openFile(filePath));

  const QString outPath = dir.filePath("out_crlf.ino");
  QVERIFY(w.saveAs(outPath));

  QFile out(outPath);
  QVERIFY(out.open(QIODevice::ReadOnly));
  const QByteArray saved = out.readAll();

  QVERIFY(saved.contains("\r\n"));
  for (int i = 0; i < saved.size(); ++i) {
    if (saved.at(i) == '\n') {
      QVERIFY(i > 0);
      QCOMPARE(saved.at(i - 1), '\r');
    }
  }
}

void TestEditorWidget::usesDefaultLineEndingForEmptyFiles() {
  SettingsKeyGuard keyGuard(QStringLiteral("defaultLineEnding"));

  QSettings settings;
  settings.beginGroup("Preferences");
  settings.setValue("defaultLineEnding", QStringLiteral("CRLF"));
  settings.endGroup();

  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  const QString filePath = dir.filePath("empty.ino");
  {
    QFile f(filePath);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
  }

  EditorWidget w;
  QVERIFY(w.openFile(filePath));

  auto* editor = w.currentEditorWidget();
  QVERIFY(editor);
  editor->setPlainText(QStringLiteral("line1\nline2\n"));

  QVERIFY(w.saveAs(filePath));

  QFile out(filePath);
  QVERIFY(out.open(QIODevice::ReadOnly));
  const QByteArray saved = out.readAll();
  QVERIFY(saved.contains("\r\n"));
  for (int i = 0; i < saved.size(); ++i) {
    if (saved.at(i) == '\n') {
      QVERIFY(i > 0);
      QCOMPARE(saved.at(i - 1), '\r');
    }
  }
}

void TestEditorWidget::trimsTrailingWhitespaceWhenEnabled() {
  SettingsKeyGuard keyGuard(QStringLiteral("trimTrailingWhitespace"));

  QSettings settings;
  settings.beginGroup("Preferences");
  settings.setValue("trimTrailingWhitespace", true);
  settings.endGroup();

  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  const QString filePath = dir.filePath("ws.ino");
  {
    QFile f(filePath);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write("int x = 1;  \nint y = 2;\t\t\n");
  }

  EditorWidget w;
  QVERIFY(w.openFile(filePath));

  const QString outPath = dir.filePath("out_ws.ino");
  QVERIFY(w.saveAs(outPath));

  QFile out(outPath);
  QVERIFY(out.open(QIODevice::ReadOnly | QIODevice::Text));
  const QByteArray saved = out.readAll();
  QCOMPARE(saved, QByteArray("int x = 1;\nint y = 2;\n"));
}

void TestEditorWidget::saveAllSavesModifiedFiles() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  const QString aPath = dir.filePath("a.ino");
  const QString bPath = dir.filePath("b.ino");
  {
    QFile f(aPath);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write("int a = 1;\n");
  }
  {
    QFile f(bPath);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write("int b = 2;\n");
  }

  EditorWidget w;
  QVERIFY(w.openFile(aPath));
  QVERIFY(w.openFile(bPath));

  auto* tabs = w.findChild<QTabWidget*>();
  QVERIFY(tabs);

  // Modify file a.
  {
    const int idx = tabs->indexOf(w.editorWidgetForFile(aPath));
    QVERIFY(idx >= 0);
    tabs->setCurrentIndex(idx);
    auto* editor = qobject_cast<QPlainTextEdit*>(tabs->currentWidget());
    QVERIFY(editor);
    editor->moveCursor(QTextCursor::End);
    editor->insertPlainText("int aa = 11;\n");
    QVERIFY(editor->document()->isModified());
  }

  // Modify file b.
  {
    const int idx = tabs->indexOf(w.editorWidgetForFile(bPath));
    QVERIFY(idx >= 0);
    tabs->setCurrentIndex(idx);
    auto* editor = qobject_cast<QPlainTextEdit*>(tabs->currentWidget());
    QVERIFY(editor);
    editor->moveCursor(QTextCursor::End);
    editor->insertPlainText("int bb = 22;\n");
    QVERIFY(editor->document()->isModified());
  }

  QVERIFY(w.saveAll());

  QFile a(aPath);
  QVERIFY(a.open(QIODevice::ReadOnly | QIODevice::Text));
  const QByteArray aSaved = a.readAll();
  QVERIFY(aSaved.contains("int aa = 11;"));

  QFile b(bPath);
  QVERIFY(b.open(QIODevice::ReadOnly | QIODevice::Text));
  const QByteArray bSaved = b.readAll();
  QVERIFY(bSaved.contains("int bb = 22;"));

  // If an Untitled tab becomes modified, saveAll should fail (it can't pick a path).
  EditorWidget w2;
  auto* tabs2 = w2.findChild<QTabWidget*>();
  QVERIFY(tabs2);
  tabs2->setCurrentIndex(0);
  auto* untitled = qobject_cast<QPlainTextEdit*>(tabs2->currentWidget());
  QVERIFY(untitled);
  untitled->insertPlainText("x");
  QVERIFY(untitled->document()->isModified());
  QVERIFY(!w2.saveAll());
}

void TestEditorWidget::saveCopyAsKeepsOriginalFilePath() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  const QString inPath = dir.filePath("in.ino");
  {
    QFile f(inPath);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write("int x = 1;\n");
  }

  EditorWidget w;
  QVERIFY(w.openFile(inPath));
  QCOMPARE(w.currentFilePath(), inPath);

  auto* tabs = w.findChild<QTabWidget*>();
  QVERIFY(tabs);
  auto* editor = qobject_cast<QPlainTextEdit*>(tabs->currentWidget());
  QVERIFY(editor);
  editor->moveCursor(QTextCursor::End);
  editor->insertPlainText("int y = 2;\n");
  QVERIFY(editor->document()->isModified());

  const QString copyPath = dir.filePath("copy.ino");
  QVERIFY(w.saveCopyAs(copyPath));

  // Saving a copy must not change the current file path nor clear the modified flag.
  QCOMPARE(w.currentFilePath(), inPath);
  QVERIFY(editor->document()->isModified());

  QFile out(copyPath);
  QVERIFY(out.open(QIODevice::ReadOnly | QIODevice::Text));
  const QByteArray savedCopy = out.readAll();
  QVERIFY(savedCopy.contains("int y = 2;"));

  QFile in(inPath);
  QVERIFY(in.open(QIODevice::ReadOnly | QIODevice::Text));
  const QByteArray originalOnDisk = in.readAll();
  QVERIFY(!originalOnDisk.contains("int y = 2;"));
}

void TestEditorWidget::autoReloadsWhenFileChangesOnDisk() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  const QString filePath = dir.filePath("reload.ino");
  {
    QFile f(filePath);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write("int a = 1;\n");
  }

  EditorWidget w;
  QVERIFY(w.openFile(filePath));

  auto* editor = w.currentEditorWidget();
  QVERIFY(editor);
  QCOMPARE(editor->toPlainText(), QStringLiteral("int a = 1;\n"));
  QVERIFY(!editor->document()->isModified());

  // Change the file on disk while the document is unmodified; the editor should auto-reload.
  {
    QFile f(filePath);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text));
    f.write("int a = 2;\n");
  }

  QTest::qWait(800);
  QCOMPARE(editor->toPlainText(), QStringLiteral("int a = 2;\n"));
  QVERIFY(!editor->document()->isModified());
}

void TestEditorWidget::opensLargeFilesWithoutHighlightingOrFolding() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  const QString largePath = dir.filePath("large.cpp");
  {
    QByteArray data;
    constexpr int kLargeBytes = 700 * 1024;
    data.reserve(kLargeBytes);
    data.append("void foo() {\n  int x = 0;\n}\n");
    while (data.size() < kLargeBytes) {
      data.append("x\n");
    }

    QFile f(largePath);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(f.write(data), data.size());
  }

  const QString smallPath = dir.filePath("small.cpp");
  {
    QFile f(smallPath);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write("void foo() {\n  int x = 0;\n}\n");
  }

  EditorWidget w;
  QVERIFY(w.openFile(largePath));

  auto* largeEditor = qobject_cast<CodeEditor*>(w.editorWidgetForFile(largePath));
  QVERIFY(largeEditor);
  QVERIFY(!largeEditor->foldingEnabled());
  QVERIFY(!largeEditor->canFold(1));
  QVERIFY(largeEditor->document()->findChildren<CppHighlighter*>().isEmpty());

  QVERIFY(w.openFile(smallPath));
  auto* smallEditor = qobject_cast<CodeEditor*>(w.editorWidgetForFile(smallPath));
  QVERIFY(smallEditor);
  QVERIFY(smallEditor->foldingEnabled());
  QVERIFY(smallEditor->canFold(1));
  QVERIFY(!smallEditor->document()->findChildren<CppHighlighter*>().isEmpty());
}

int main(int argc, char** argv) {
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QTemporaryDir settingsDir;
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());
  QApplication::setOrganizationName("RewrittoIdeTests");
  QApplication::setApplicationName("test_editor_widget");
  QApplication app(argc, argv);
  TestEditorWidget tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "test_editor_widget.moc"
