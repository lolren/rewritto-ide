#include "problems_widget.h"

#include <algorithm>
#include <QAction>
#include <QClipboard>
#include <QFileInfo>
#include <QBoxLayout>
#include <QGuiApplication>
#include <QLabel>
#include <QMenu>
#include <QPoint>
#include <QRegularExpression>
#include <QStyle>
#include <QToolBar>

namespace {
constexpr int kRoleFilePath = Qt::UserRole + 1;
constexpr int kRoleLine = Qt::UserRole + 2;
constexpr int kRoleColumn = Qt::UserRole + 3;

QString missingHeaderSearchQuery(const QString& message) {
  const QString text = message.trimmed();
  if (text.isEmpty()) {
    return {};
  }

  static const QRegularExpression headerMissing(
      R"((?:fatal error:\s*)?([A-Za-z0-9_./+\-]+\.(?:h|hpp)):\s*No such file or directory)",
      QRegularExpression::CaseInsensitiveOption);
  const QRegularExpressionMatch m = headerMissing.match(text);
  if (!m.hasMatch()) {
    return {};
  }

  const QString headerPath = m.captured(1).trimmed();
  QString base = QFileInfo(headerPath).fileName().trimmed();
  if (base.endsWith(QStringLiteral(".hpp"), Qt::CaseInsensitive)) {
    base.chop(4);
  } else if (base.endsWith(QStringLiteral(".h"), Qt::CaseInsensitive)) {
    base.chop(2);
  }
  return base.trimmed();
}

QString missingPlatformSearchQuery(const QString& message) {
  const QString text = message.trimmed();
  if (text.isEmpty()) {
    return {};
  }

  static const QRegularExpression platformNotInstalled(
      R"(platform\s+(?:not\s+installed|not\s+found)\s*:\s*([A-Za-z0-9_.+\-]+:[A-Za-z0-9_.+\-]+))",
      QRegularExpression::CaseInsensitiveOption);
  QRegularExpressionMatch m = platformNotInstalled.match(text);
  if (m.hasMatch()) {
    return m.captured(1).trimmed();
  }

  static const QRegularExpression platformIsNotInstalled(
      R"(platform\s+\"?([A-Za-z0-9_.+\-]+:[A-Za-z0-9_.+\-]+)\"?\s+is\s+not\s+installed)",
      QRegularExpression::CaseInsensitiveOption);
  m = platformIsNotInstalled.match(text);
  if (m.hasMatch()) {
    return m.captured(1).trimmed();
  }

  return {};
}

QString missingToolSearchQuery(const QString& message) {
  const QString text = message.trimmed();
  if (text.isEmpty()) {
    return {};
  }

  static const QRegularExpression execNotFound(
      R"(exec:\s*\"([^\"]+)\":\s*executable file not found in\s+\$PATH)",
      QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption);
  static const QRegularExpression forkExecNoSuchFile(
      R"(fork/exec\s+([^\s:]+):\s*no such file or directory)",
      QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption);
  static const QRegularExpression shellNotFound(
      R"((?:^|\n)(?:sh|bash):\s*\d+:\s*([^\s:]+):\s*not found\s*$)",
      QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption);
  static const QRegularExpression commandNotFound(
      R"((?:^|\n)([^\s:]+):\s*command not found\s*$)",
      QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption);

  QRegularExpressionMatch m = execNotFound.match(text);
  if (!m.hasMatch()) {
    m = forkExecNoSuchFile.match(text);
  }
  if (!m.hasMatch()) {
    m = shellNotFound.match(text);
  }
  if (!m.hasMatch()) {
    m = commandNotFound.match(text);
  }
  if (!m.hasMatch()) {
    return {};
  }

  return m.captured(1).trimmed();
}
}  // namespace

ProblemsWidget::SeverityKind ProblemsWidget::kindForSeverityString(const QString& severity) {
  const QString s = severity.trimmed().toLower();
  if (s == QStringLiteral("error") || s == QStringLiteral("fatal") ||
      s == QStringLiteral("fatal error")) {
    return SeverityKind::Error;
  }
  if (s == QStringLiteral("warning")) {
    return SeverityKind::Warning;
  }
  if (s == QStringLiteral("hint")) {
    return SeverityKind::Hint;
  }
  // Treat everything else ("info", "note", etc.) as Info.
  return SeverityKind::Info;
}

ProblemsWidget::ProblemsWidget(QWidget* parent) : QWidget(parent) {
  auto iconFor = [this](const QString& themeName, QStyle::StandardPixmap fallback) {
    QIcon icon = QIcon::fromTheme(themeName);
    if (icon.isNull()) {
      icon = style()->standardIcon(fallback);
    }
    return icon;
  };

  toolBar_ = new QToolBar(this);
  toolBar_->setObjectName("ProblemsToolBar");
  toolBar_->setIconSize(QSize(18, 18));
  toolBar_->setToolButtonStyle(Qt::ToolButtonIconOnly);

  clearAction_ = toolBar_->addAction(
      iconFor("edit-clear", QStyle::SP_DialogResetButton), tr("Clear Problems"));
  copyAction_ =
      toolBar_->addAction(iconFor("edit-copy", QStyle::SP_FileDialogDetailedView),
                          tr("Copy Problems"));

  toolBar_->addSeparator();

  showErrorsAction_ = toolBar_->addAction(
      iconFor("dialog-error", QStyle::SP_MessageBoxCritical), tr("Show Errors"));
  showErrorsAction_->setCheckable(true);
  showErrorsAction_->setChecked(true);
  showErrorsAction_->setToolTip(tr("Show errors"));

  showWarningsAction_ = toolBar_->addAction(
      iconFor("dialog-warning", QStyle::SP_MessageBoxWarning), tr("Show Warnings"));
  showWarningsAction_->setCheckable(true);
  showWarningsAction_->setChecked(true);
  showWarningsAction_->setToolTip(tr("Show warnings"));

  showInfoAction_ = toolBar_->addAction(
      iconFor("dialog-information", QStyle::SP_MessageBoxInformation), tr("Show Info"));
  showInfoAction_->setCheckable(true);
  showInfoAction_->setChecked(true);
  showInfoAction_->setToolTip(tr("Show informational diagnostics"));

  showHintsAction_ = toolBar_->addAction(
      iconFor("dialog-question", QStyle::SP_DialogHelpButton), tr("Show Hints"));
  showHintsAction_->setCheckable(true);
  showHintsAction_->setChecked(true);
  showHintsAction_->setToolTip(tr("Show hints"));

  summaryLabel_ = new QLabel(toolBar_);
  summaryLabel_->setObjectName("ProblemsSummaryLabel");
  summaryLabel_->setText(QStringLiteral("E 0  W 0  I 0  H 0"));
  toolBar_->addWidget(summaryLabel_);

  list_ = new QListWidget(this);
  list_->setUniformItemSizes(true);
  list_->setSelectionMode(QAbstractItemView::SingleSelection);
  list_->setContextMenuPolicy(Qt::CustomContextMenu);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(toolBar_);
  layout->addWidget(list_);

  connect(clearAction_, &QAction::triggered, this, [this] { clearAll(); });
  connect(copyAction_, &QAction::triggered, this, [this] {
    QStringList lines;
    lines.reserve(list_ ? list_->count() : 0);
    if (list_) {
      for (int i = 0; i < list_->count(); ++i) {
        if (auto* item = list_->item(i)) {
          lines << item->text();
        }
      }
    }
    if (auto* cb = QGuiApplication::clipboard()) {
      cb->setText(lines.join(QLatin1Char('\n')));
    }
  });

  auto onFilterChanged = [this](bool) { rebuild(); };
  connect(showErrorsAction_, &QAction::toggled, this, onFilterChanged);
  connect(showWarningsAction_, &QAction::toggled, this, onFilterChanged);
  connect(showInfoAction_, &QAction::toggled, this, onFilterChanged);
  connect(showHintsAction_, &QAction::toggled, this, onFilterChanged);

  connect(list_, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) {
    if (!item) {
      return;
    }
    const QString filePath = item->data(kRoleFilePath).toString();
    const int line = item->data(kRoleLine).toInt();
    const int column = item->data(kRoleColumn).toInt();
    if (!filePath.isEmpty() && line > 0) {
      emit openLocationRequested(filePath, line, column);
    }
  });

  connect(list_, &QListWidget::customContextMenuRequested, this,
          [this](const QPoint& pos) {
            if (!list_) {
              return;
            }
            QListWidgetItem* item = list_->itemAt(pos);
            if (!item) {
              return;
            }

            const QString filePath = item->data(kRoleFilePath).toString();
            const int line = item->data(kRoleLine).toInt();
            const int column = item->data(kRoleColumn).toInt();
            const QString message = item->toolTip().trimmed();

            QMenu menu(this);

            if (!filePath.isEmpty() && line > 0) {
              QAction* open = menu.addAction(tr("Open"));
              connect(open, &QAction::triggered, this,
                      [this, filePath, line, column] {
                        emit openLocationRequested(filePath, line, column);
                      });
              menu.addSeparator();
            }

            QAction* copy = menu.addAction(tr("Copy Message"));
            connect(copy, &QAction::triggered, this, [message] {
              if (auto* cb = QGuiApplication::clipboard()) {
                cb->setText(message);
              }
            });

            const QString libQuery = missingHeaderSearchQuery(message);
            const QString boardsQuery = missingPlatformSearchQuery(message);
            const QString toolQuery = missingToolSearchQuery(message);
            const QString boardsSearchQuery =
                !boardsQuery.isEmpty() ? boardsQuery : toolQuery;
            if (!libQuery.isEmpty() || !boardsSearchQuery.isEmpty()) {
              menu.addSeparator();
              if (!libQuery.isEmpty()) {
                QAction* searchLib = menu.addAction(
                    tr("Search Library Manager for \"%1\"").arg(libQuery));
                connect(searchLib, &QAction::triggered, this,
                        [this, libQuery] { emit searchLibrariesRequested(libQuery); });
              }
              if (!boardsSearchQuery.isEmpty()) {
                QAction* searchBoards = menu.addAction(
                    tr("Search Boards Manager for \"%1\"").arg(boardsSearchQuery));
                connect(searchBoards, &QAction::triggered, this,
                        [this, boardsSearchQuery] {
                  emit searchBoardsRequested(boardsSearchQuery);
                });
              }
            }

            // Additional quick fixes for common errors
            bool hasQuickFixes = !libQuery.isEmpty() || !boardsSearchQuery.isEmpty();

            // Detect semicolon errors: "expected ';' before 'something'"
            if (message.contains(QRegularExpression("expected.*;.*before"))) {
              menu.addSeparator();
              hasQuickFixes = true;
              QAction* insertSemicolon = menu.addAction(
                  tr("Quick Fix: Insert Semicolon"));
              connect(insertSemicolon, &QAction::triggered, this, [this, filePath, line, column] {
                emit quickFixRequested(filePath, line, column, "insert_semicolon");
              });
            }

            // Detect missing brace errors
            if (message.contains(QRegularExpression("expected.*\\}.*before|expected.*\\{.*before"))) {
              menu.addSeparator();
              hasQuickFixes = true;
              QAction* insertBrace = menu.addAction(
                  tr("Quick Fix: Insert Matching Brace"));
              connect(insertBrace, &QAction::triggered, this, [this, filePath, line, column] {
                emit quickFixRequested(filePath, line, column, "insert_brace");
              });
            }

            // Detect unused variable warnings
            if (message.contains(QRegularExpression("unused variable|variable.*unused|-Wunused"))) {
              menu.addSeparator();
              hasQuickFixes = true;
              QAction* markUnused = menu.addAction(
                  tr("Quick Fix: Mark as Unused (prefix with _)"));
              connect(markUnused, &QAction::triggered, this, [this, filePath, line, column] {
                emit quickFixRequested(filePath, line, column, "mark_unused");
              });
              QAction* removeVar = menu.addAction(
                  tr("Quick Fix: Remove Variable"));
              connect(removeVar, &QAction::triggered, this, [this, filePath, line, column] {
                emit quickFixRequested(filePath, line, column, "remove_variable");
              });
            }

            // Detect deprecated function warnings
            if (message.contains(QRegularExpression("deprecated|'[^']+' is deprecated"))) {
              menu.addSeparator();
              hasQuickFixes = true;
              QAction* showDocs = menu.addAction(
                  tr("Quick Fix: Show Alternative Documentation"));
              connect(showDocs, &QAction::triggered, this, [this, message] {
                emit showDocsRequested(message);
              });
            }

            // Detect type/class not declared errors
            if (message.contains(QRegularExpression("'[^']+' was not declared|not declared in this scope|has not been declared"))) {
              menu.addSeparator();
              hasQuickFixes = true;
              QAction* createDeclaration = menu.addAction(
                  tr("Quick Fix: Create Forward Declaration"));
              connect(createDeclaration, &QAction::triggered, this, [this, filePath, line, column] {
                emit quickFixRequested(filePath, line, column, "create_declaration");
              });
            }

            // Detect missing include guard
            if (message.contains(QRegularExpression("#ifndef.*not found|multiple definition|redefinition"))) {
              menu.addSeparator();
              hasQuickFixes = true;
              QAction* addIncludeGuard = menu.addAction(
                  tr("Quick Fix: Add Include Guard"));
              connect(addIncludeGuard, &QAction::triggered, this, [this, filePath, line, column] {
                emit quickFixRequested(filePath, line, column, "add_include_guard");
              });
            }

            // Detect missing prototype errors
            if (message.contains(QRegularExpression("prototype|no matching function|candidate function not viable"))) {
              menu.addSeparator();
              hasQuickFixes = true;
              QAction* addPrototype = menu.addAction(
                  tr("Quick Fix: Add Function Prototype"));
              connect(addPrototype, &QAction::triggered, this, [this, filePath, line, column] {
                emit quickFixRequested(filePath, line, column, "add_prototype");
              });
            }

            // Show "No Quick Fixes Available" if none found
            if (!hasQuickFixes) {
              menu.addSeparator();
              QAction* noFix = menu.addAction(tr("No Quick Fixes Available"));
              noFix->setEnabled(false);
            }

            menu.exec(list_->viewport()->mapToGlobal(pos));
          });
}

void ProblemsWidget::clearAll() {
  diagsBySourceAndFile_.clear();
  rebuild();
}

void ProblemsWidget::clearSource(const QString& source) {
  diagsBySourceAndFile_.remove(source);
  rebuild();
}

void ProblemsWidget::addDiagnostic(const QString& source, const Diagnostic& diag) {
  diagsBySourceAndFile_[source][diag.filePath].push_back(diag);
  rebuild();
}

void ProblemsWidget::setDiagnostics(const QString& source,
                                   const QString& filePath,
                                   const QVector<Diagnostic>& diags) {
  diagsBySourceAndFile_[source][filePath] = diags;
  rebuild();
}

void ProblemsWidget::rebuild() {
  list_->clear();

  struct Counts final {
    int errors = 0;
    int warnings = 0;
    int infos = 0;
    int hints = 0;
  };

  auto addToCounts = [](Counts* c, SeverityKind kind) {
    if (!c) {
      return;
    }
    switch (kind) {
      case SeverityKind::Error:
        ++c->errors;
        break;
      case SeverityKind::Warning:
        ++c->warnings;
        break;
      case SeverityKind::Info:
        ++c->infos;
        break;
      case SeverityKind::Hint:
        ++c->hints;
        break;
    }
  };

  const bool showErrors = !showErrorsAction_ || showErrorsAction_->isChecked();
  const bool showWarnings = !showWarningsAction_ || showWarningsAction_->isChecked();
  const bool showInfos = !showInfoAction_ || showInfoAction_->isChecked();
  const bool showHints = !showHintsAction_ || showHintsAction_->isChecked();
  auto visible = [&](SeverityKind kind) {
    switch (kind) {
      case SeverityKind::Error:
        return showErrors;
      case SeverityKind::Warning:
        return showWarnings;
      case SeverityKind::Info:
        return showInfos;
      case SeverityKind::Hint:
        return showHints;
    }
    return true;
  };

  auto iconForKind = [this](SeverityKind kind) {
    switch (kind) {
      case SeverityKind::Error:
        return style()->standardIcon(QStyle::SP_MessageBoxCritical);
      case SeverityKind::Warning:
        return style()->standardIcon(QStyle::SP_MessageBoxWarning);
      case SeverityKind::Info:
        return style()->standardIcon(QStyle::SP_MessageBoxInformation);
      case SeverityKind::Hint:
        return style()->standardIcon(QStyle::SP_DialogHelpButton);
    }
    return QIcon{};
  };

  Counts all;
  Counts shown;

  auto sources = diagsBySourceAndFile_.keys();
  std::sort(sources.begin(), sources.end());
  for (const QString& source : sources) {
    auto files = diagsBySourceAndFile_.value(source).keys();
    std::sort(files.begin(), files.end());
    for (const QString& file : files) {
      const auto diags = diagsBySourceAndFile_.value(source).value(file);
      for (const Diagnostic& d : diags) {
        const SeverityKind kind = kindForSeverityString(d.severity);
        addToCounts(&all, kind);
        if (!visible(kind)) {
          continue;
        }
        addToCounts(&shown, kind);

        QString msg = d.message;
        msg.replace('\r', ' ');
        msg.replace('\n', ' ');
        msg = msg.simplified();

        QString text;
        if (file.isEmpty()) {
          text = QString("[%1] %2: %3").arg(source, d.severity, msg);
        } else if (d.line <= 0) {
          text = QString("[%1] %2: %3: %4").arg(source, file, d.severity, msg);
        } else {
          const QString loc =
              d.column > 0 ? QString("%1:%2:%3").arg(file).arg(d.line).arg(d.column)
                           : QString("%1:%2").arg(file).arg(d.line);
          text = QString("[%1] %2: %3: %4").arg(source, loc, d.severity, msg);
        }

        auto* item = new QListWidgetItem(text, list_);
        item->setIcon(iconForKind(kind));
        item->setData(kRoleFilePath, d.filePath);
        item->setData(kRoleLine, d.line);
        item->setData(kRoleColumn, d.column);
        item->setToolTip(d.message);
        list_->addItem(item);
      }
    }
  }

  if (summaryLabel_) {
    const QString text =
        QStringLiteral("E %1  W %2  I %3  H %4")
            .arg(all.errors)
            .arg(all.warnings)
            .arg(all.infos)
            .arg(all.hints);
    const QString shownText =
        QStringLiteral("Showing: E %1  W %2  I %3  H %4")
            .arg(shown.errors)
            .arg(shown.warnings)
            .arg(shown.infos)
            .arg(shown.hints);
    summaryLabel_->setText(text);
    summaryLabel_->setToolTip(shownText);
  }
}
