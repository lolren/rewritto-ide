#pragma once

#include <QHash>
#include <QListWidget>
#include <QWidget>

class QAction;
class QLabel;
class QToolBar;

class ProblemsWidget final : public QWidget {
  Q_OBJECT

 public:
  struct Diagnostic {
    QString filePath;
    int line = 0;    // 1-based
    int column = 0;  // 1-based
    QString severity;
    QString message;
  };

  explicit ProblemsWidget(QWidget* parent = nullptr);

  void clearAll();
  void clearSource(const QString& source);
  void addDiagnostic(const QString& source, const Diagnostic& diag);
  void setDiagnostics(const QString& source,
                      const QString& filePath,
                      const QVector<Diagnostic>& diags);

 signals:
  void openLocationRequested(QString filePath, int line, int column);
  void searchLibrariesRequested(QString query);
  void searchBoardsRequested(QString query);
  void quickFixRequested(QString filePath, int line, int column, QString fixType);
  void showDocsRequested(QString message);

 private:
  enum class SeverityKind {
    Error,
    Warning,
    Info,
    Hint,
  };

  static SeverityKind kindForSeverityString(const QString& severity);

  QToolBar* toolBar_ = nullptr;
  QAction* clearAction_ = nullptr;
  QAction* copyAction_ = nullptr;
  QAction* showErrorsAction_ = nullptr;
  QAction* showWarningsAction_ = nullptr;
  QAction* showInfoAction_ = nullptr;
  QAction* showHintsAction_ = nullptr;
  QLabel* summaryLabel_ = nullptr;

  QListWidget* list_ = nullptr;
  QHash<QString, QHash<QString, QVector<Diagnostic>>> diagsBySourceAndFile_;

  void rebuild();
};
