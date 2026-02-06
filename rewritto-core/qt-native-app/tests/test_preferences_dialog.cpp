#include <QtTest/QtTest>

#include <QApplication>
#include <QDir>
#include <QFontDatabase>
#include <QFontInfo>

#include "preferences_dialog.h"

class TestPreferencesDialog final : public QObject {
  Q_OBJECT

 private slots:
  void roundTripValues();
};

void TestPreferencesDialog::roundTripValues() {
  PreferencesDialog dlg;
  QFont editorFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  editorFont.setPointSize(14);
  dlg.setTheme("dark");
  dlg.setUiScale(1.25);
  dlg.setSketchbookDir(QDir::homePath() + "/Rewritto");
  dlg.setAdditionalUrls(
      {QStringLiteral("https://example.com/package_a.json"),
       QStringLiteral("https://example.com/package_b.json")});
  dlg.setEditorFont(editorFont);
  dlg.setTabSize(4);
  dlg.setInsertSpaces(false);
  dlg.setShowIndentGuides(false);
  dlg.setShowWhitespace(true);
  dlg.setDefaultLineEnding("CRLF");
  dlg.setTrimTrailingWhitespace(true);
  dlg.setAutosaveEnabled(true);
  dlg.setAutosaveIntervalSeconds(42);
  dlg.setWarningsLevel("more");
  dlg.setVerboseCompile(true);
  dlg.setVerboseUpload(true);

  QCOMPARE(dlg.theme(), QString("dark"));
  QCOMPARE(dlg.uiScale(), 1.25);
  QCOMPARE(dlg.sketchbookDir(), QDir::homePath() + "/Rewritto");
  QCOMPARE(dlg.additionalUrls(),
           (QStringList{QStringLiteral("https://example.com/package_a.json"),
                        QStringLiteral("https://example.com/package_b.json")}));
  const QFont roundTripFont = dlg.editorFont();
  QVERIFY(!roundTripFont.family().trimmed().isEmpty());
  QCOMPARE(roundTripFont.pointSize(), 14);
  QCOMPARE(dlg.tabSize(), 4);
  QCOMPARE(dlg.insertSpaces(), false);
  QCOMPARE(dlg.showIndentGuides(), false);
  QCOMPARE(dlg.showWhitespace(), true);
  QCOMPARE(dlg.defaultLineEnding(), QString("CRLF"));
  QCOMPARE(dlg.trimTrailingWhitespace(), true);
  QCOMPARE(dlg.autosaveEnabled(), true);
  QCOMPARE(dlg.autosaveIntervalSeconds(), 42);
  QCOMPARE(dlg.warningsLevel(), QString("more"));
  QCOMPARE(dlg.verboseCompile(), true);
  QCOMPARE(dlg.verboseUpload(), true);
}

int main(int argc, char** argv) {
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);
  TestPreferencesDialog tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "test_preferences_dialog.moc"
