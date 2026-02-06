#include "output_widget.h"

#include <QAction>
#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QGuiApplication>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QStandardPaths>
#include <QStyle>
#include <QTextCursor>
#include <QToolBar>
#include <QVBoxLayout>

OutputWidget::OutputWidget(QWidget* parent) : QWidget(parent) {
  auto iconFor = [this](const QString& themeName, QStyle::StandardPixmap fallback) {
    QIcon icon = QIcon::fromTheme(themeName);
    if (icon.isNull()) {
      icon = style()->standardIcon(fallback);
    }
    return icon;
  };

  auto* toolBar = new QToolBar(this);
  toolBar->setObjectName("OutputToolBar");
  toolBar->setIconSize(QSize(18, 18));
  toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);

  QAction* clearAction = toolBar->addAction(
      iconFor("edit-clear", QStyle::SP_DialogResetButton), tr("Clear Output"));
  QAction* copyAction = toolBar->addAction(
      iconFor("edit-copy", QStyle::SP_FileDialogDetailedView), tr("Copy Output"));
  QAction* saveAction = toolBar->addAction(
      iconFor("document-save", QStyle::SP_DialogSaveButton), tr("Save Output\u2026"));

  output_ = new QPlainTextEdit(this);
  output_->setReadOnly(true);
  output_->setObjectName("OutputTextEdit");
  // Prevent the output view from growing without bound (large builds can produce
  // tens of thousands of lines and freeze the UI).
  output_->setMaximumBlockCount(20000);

  connect(clearAction, &QAction::triggered, this, [this] { clear(); });
  connect(copyAction, &QAction::triggered, this, [this] {
    if (auto* cb = QGuiApplication::clipboard()) {
      cb->setText(output_ ? output_->toPlainText() : QString{});
    }
  });
  connect(saveAction, &QAction::triggered, this, [this] {
    const QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString baseDir = docs.isEmpty() ? QDir::homePath() : docs;
	    const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
	    const QString suggested =
	        QDir(baseDir).absoluteFilePath(QStringLiteral("rewritto-ide-output-%1.txt").arg(stamp));
	    const QString path = QFileDialog::getSaveFileName(
	        this, tr("Save Output"), suggested, tr("Text Files (*.txt);;All Files (*)"));
    if (path.trimmed().isEmpty()) {
      return;
    }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
      QMessageBox::warning(this, tr("Save Failed"), tr("Could not write file."));
      return;
    }
    const QString text = output_ ? output_->toPlainText() : QString{};
    const QByteArray bytes = text.toUtf8();
    if (f.write(bytes) != bytes.size()) {
      QMessageBox::warning(this, tr("Save Failed"), tr("Could not write file."));
      return;
    }
  });

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(toolBar);
  layout->addWidget(output_);
}

void OutputWidget::appendText(const QString& text) {
  output_->moveCursor(QTextCursor::End);
  output_->insertPlainText(text);
  output_->moveCursor(QTextCursor::End);
}

void OutputWidget::appendHtml(const QString& html) {
  output_->moveCursor(QTextCursor::End);
  output_->appendHtml(html);
  output_->moveCursor(QTextCursor::End);
}

void OutputWidget::appendLine(const QString& line) {
  output_->appendPlainText(line);
}

void OutputWidget::clear() {
  output_->clear();
}
