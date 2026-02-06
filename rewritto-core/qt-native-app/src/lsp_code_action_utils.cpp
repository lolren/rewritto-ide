#include "lsp_code_action_utils.h"

#include <QJsonArray>
#include <QJsonValue>

LspCodeActionExecution lspPlanCodeActionExecution(const QJsonObject& action) {
  LspCodeActionExecution out;

  if (action.contains("edit") && action.value("edit").isObject()) {
    out.workspaceEdit = action.value("edit").toObject();
  }

  QJsonObject commandObj;
  if (action.contains("command")) {
    const QJsonValue cmdVal = action.value("command");
    if (cmdVal.isObject()) {
      commandObj = cmdVal.toObject();
    } else if (cmdVal.isString()) {
      // Command shape: {title, command, arguments?}
      commandObj = action;
    }
  }

  const QString command = commandObj.value("command").toString().trimmed();
  if (!command.isEmpty()) {
    out.executeCommandParams.insert("command", command);
    if (commandObj.contains("arguments") && commandObj.value("arguments").isArray()) {
      const QJsonArray args = commandObj.value("arguments").toArray();
      if (!args.isEmpty()) {
        out.executeCommandParams.insert("arguments", args);
      }
    }
  }

  return out;
}

