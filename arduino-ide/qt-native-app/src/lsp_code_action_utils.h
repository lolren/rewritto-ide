#pragma once

#include <QJsonObject>

struct LspCodeActionExecution final {
  QJsonObject workspaceEdit;
  QJsonObject executeCommandParams;
};

LspCodeActionExecution lspPlanCodeActionExecution(const QJsonObject& action);

