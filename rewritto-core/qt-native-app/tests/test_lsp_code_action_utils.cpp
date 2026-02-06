#include <QtTest/QtTest>

#include <QJsonArray>
#include <QJsonObject>

#include "lsp_code_action_utils.h"

class TestLspCodeActionUtils final : public QObject {
  Q_OBJECT

 private slots:
  void extractsCommandFromCommandObject();
  void extractsCommandFromCodeActionCommand();
  void extractsWorkspaceEditFromCodeAction();
  void ignoresEmptyCommand();
};

void TestLspCodeActionUtils::extractsCommandFromCommandObject() {
  const QJsonObject cmd{
      {"title", "Organize Imports"},
      {"command", "clangd.applyTweak"},
      {"arguments", QJsonArray{QJsonValue(1), QJsonValue(2)}},
  };

  const LspCodeActionExecution exec = lspPlanCodeActionExecution(cmd);
  QVERIFY(exec.workspaceEdit.isEmpty());
  QCOMPARE(exec.executeCommandParams.value("command").toString(),
           QStringLiteral("clangd.applyTweak"));
  QCOMPARE(exec.executeCommandParams.value("arguments").toArray().size(), 2);
}

void TestLspCodeActionUtils::extractsCommandFromCodeActionCommand() {
  const QJsonObject action{
      {"title", "Fix something"},
      {"kind", "quickfix"},
      {"command",
       QJsonObject{
           {"title", "run"},
           {"command", "clangd.applyFix"},
           {"arguments", QJsonArray{QJsonValue("x")}},
       }},
  };

  const LspCodeActionExecution exec = lspPlanCodeActionExecution(action);
  QVERIFY(exec.workspaceEdit.isEmpty());
  QCOMPARE(exec.executeCommandParams.value("command").toString(),
           QStringLiteral("clangd.applyFix"));
  QCOMPARE(exec.executeCommandParams.value("arguments").toArray().size(), 1);
}

void TestLspCodeActionUtils::extractsWorkspaceEditFromCodeAction() {
  const QJsonObject edit{
      {"changes",
       QJsonObject{
           {"file:///tmp/a.cpp",
            QJsonArray{QJsonObject{
                {"range",
                 QJsonObject{
                     {"start", QJsonObject{{"line", 0}, {"character", 0}}},
                     {"end", QJsonObject{{"line", 0}, {"character", 0}}},
                 }},
                {"newText", "hello"},
            }}},
       }},
  };

  const QJsonObject action{
      {"title", "Apply edit"},
      {"edit", edit},
  };

  const LspCodeActionExecution exec = lspPlanCodeActionExecution(action);
  QVERIFY(!exec.workspaceEdit.isEmpty());
  QVERIFY(exec.executeCommandParams.isEmpty());
  QVERIFY(exec.workspaceEdit.contains("changes"));
}

void TestLspCodeActionUtils::ignoresEmptyCommand() {
  const QJsonObject action{
      {"title", "No-op"},
      {"command", ""},
  };

  const LspCodeActionExecution exec = lspPlanCodeActionExecution(action);
  QVERIFY(exec.workspaceEdit.isEmpty());
  QVERIFY(exec.executeCommandParams.isEmpty());
}

QTEST_MAIN(TestLspCodeActionUtils)
#include "test_lsp_code_action_utils.moc"

