#include "replace_in_files_dialog.h"

#include <QCheckBox>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QThread>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {
QString detectLineEnding(const QByteArray& data) {
  if (data.contains("\r\n")) {
    return QStringLiteral("CRLF");
  }
  if (data.contains('\r')) {
    return QStringLiteral("CR");
  }
  return QStringLiteral("LF");
}

QString normalizeLineEndings(QString text) {
  text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
  text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
  return text;
}

QByteArray applyLineEnding(QString text, const QString& lineEnding) {
  if (lineEnding == QStringLiteral("CRLF")) {
    text.replace(QStringLiteral("\n"), QStringLiteral("\r\n"));
  } else if (lineEnding == QStringLiteral("CR")) {
    text.replace(QStringLiteral("\n"), QStringLiteral("\r"));
  }
  return text.toUtf8();
}

bool isWordChar(QChar c) {
  return c.isLetterOrNumber() || c == QLatin1Char('_');
}

bool isWholeWordAt(const QString& line, int index, int len) {
  if (index < 0 || len <= 0) {
    return false;
  }
  const int before = index - 1;
  const int after = index + len;
  if (before >= 0 && before < line.size() && isWordChar(line.at(before))) {
    return false;
  }
  if (after >= 0 && after < line.size() && isWordChar(line.at(after))) {
    return false;
  }
  return true;
}

QStringList defaultPatterns() {
  return {QStringLiteral("*.ino"), QStringLiteral("*.c"),   QStringLiteral("*.cc"),
          QStringLiteral("*.cpp"), QStringLiteral("*.cxx"), QStringLiteral("*.h"),
          QStringLiteral("*.hh"),  QStringLiteral("*.hpp"), QStringLiteral("*.hxx")};
}
}  // namespace

ReplaceInFilesWorker::ReplaceInFilesWorker(QObject* parent) : QObject(parent) {}

void ReplaceInFilesWorker::cancel() {
  cancelled_.store(true, std::memory_order_relaxed);
}

void ReplaceInFilesWorker::preview(QString rootDir,
                                   QString query,
                                   QStringList patterns,
                                   bool caseSensitive,
                                   bool wholeWord) {
  cancelled_.store(false, std::memory_order_relaxed);

  if (rootDir.trimmed().isEmpty()) {
    emit message("Search root is empty.");
    emit previewFinished(0, 0);
    return;
  }

  if (query.trimmed().isEmpty()) {
    emit message("Search text is empty.");
    emit previewFinished(0, 0);
    return;
  }

  QDir root(rootDir);
  if (!root.exists()) {
    emit message("Search root does not exist.");
    emit previewFinished(0, 0);
    return;
  }

  if (patterns.isEmpty()) {
    patterns = defaultPatterns();
  }

  const Qt::CaseSensitivity cs =
      caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;

  int matches = 0;
  int filesScanned = 0;
  const QSet<QString> excludedDirNames = {
      QStringLiteral(".git"),
      QStringLiteral(".idea"),
      QStringLiteral(".vscode"),
      QStringLiteral(".pio"),
      QStringLiteral("build"),
      QStringLiteral("dist"),
      QStringLiteral("out"),
  };
  auto isExcluded = [&root, &excludedDirNames](const QString& absPath) {
    const QString rel = root.relativeFilePath(absPath);
    if (rel.trimmed().isEmpty()) {
      return false;
    }
    const QStringList segs = rel.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (int i = 0; i + 1 < segs.size(); ++i) {
      if (excludedDirNames.contains(segs[i])) {
        return true;
      }
    }
    return false;
  };

  QDirIterator it(root.absolutePath(), patterns, QDir::Files,
                  QDirIterator::Subdirectories);
  while (it.hasNext()) {
    if (cancelled_.load(std::memory_order_relaxed)) {
      emit message("Search cancelled.");
      break;
    }
    const QString filePath = it.next();
    if (isExcluded(filePath)) {
      continue;
    }
    ++filesScanned;

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
      continue;
    }
    const QByteArray data = f.readAll();
    if (data.size() > 10 * 1024 * 1024) {
      continue;
    }

    const QString text = normalizeLineEndings(QString::fromUtf8(data));
    const QStringList lines = text.split(QLatin1Char('\n'), Qt::KeepEmptyParts);

    for (int lineNo = 0; lineNo < lines.size(); ++lineNo) {
      if (cancelled_.load(std::memory_order_relaxed)) {
        break;
      }
      const QString& line = lines.at(lineNo);

      int from = 0;
      while (true) {
        const int idx = line.indexOf(query, from, cs);
        if (idx < 0) {
          break;
        }
        if (wholeWord && !isWholeWordAt(line, idx, query.size())) {
          from = idx + 1;
          continue;
        }
        ++matches;
        emit matchFound(filePath, lineNo + 1, idx + 1, line);
        from = idx + qMax(1, query.size());
        if (matches >= 200000) {
          emit message("Too many matches; stopping at 200,000.");
          emit previewFinished(matches, filesScanned);
          return;
        }
      }
    }
  }

  emit previewFinished(matches, filesScanned);
}

void ReplaceInFilesWorker::apply(QString rootDir,
                                 QString query,
                                 QString replaceText,
                                 QStringList patterns,
                                 bool caseSensitive,
                                 bool wholeWord) {
  cancelled_.store(false, std::memory_order_relaxed);

  if (rootDir.trimmed().isEmpty()) {
    emit message("Search root is empty.");
    emit applyFinished(0, 0, {});
    return;
  }

  if (query.trimmed().isEmpty()) {
    emit message("Search text is empty.");
    emit applyFinished(0, 0, {});
    return;
  }

  QDir root(rootDir);
  if (!root.exists()) {
    emit message("Search root does not exist.");
    emit applyFinished(0, 0, {});
    return;
  }

  if (patterns.isEmpty()) {
    patterns = defaultPatterns();
  }

  const Qt::CaseSensitivity cs =
      caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;

  int matchesReplaced = 0;
  int filesScanned = 0;
  QStringList modifiedFiles;
  const QSet<QString> excludedDirNames = {
      QStringLiteral(".git"),
      QStringLiteral(".idea"),
      QStringLiteral(".vscode"),
      QStringLiteral(".pio"),
      QStringLiteral("build"),
      QStringLiteral("dist"),
      QStringLiteral("out"),
  };
  auto isExcluded = [&root, &excludedDirNames](const QString& absPath) {
    const QString rel = root.relativeFilePath(absPath);
    if (rel.trimmed().isEmpty()) {
      return false;
    }
    const QStringList segs = rel.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (int i = 0; i + 1 < segs.size(); ++i) {
      if (excludedDirNames.contains(segs[i])) {
        return true;
      }
    }
    return false;
  };

  QDirIterator it(root.absolutePath(), patterns, QDir::Files,
                  QDirIterator::Subdirectories);
  while (it.hasNext()) {
    if (cancelled_.load(std::memory_order_relaxed)) {
      emit message("Replace cancelled.");
      break;
    }
    const QString filePath = it.next();
    if (isExcluded(filePath)) {
      continue;
    }
    ++filesScanned;

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
      continue;
    }
    const QByteArray data = f.readAll();
    if (data.size() > 10 * 1024 * 1024) {
      continue;
    }
    const QString lineEnding = detectLineEnding(data);
    const QString original = normalizeLineEndings(QString::fromUtf8(data));
    const QStringList lines =
        original.split(QLatin1Char('\n'), Qt::KeepEmptyParts);

    bool changed = false;
    QStringList outLines;
    outLines.reserve(lines.size());
    for (const QString& line : lines) {
      if (cancelled_.load(std::memory_order_relaxed)) {
        break;
      }
      int from = 0;
      int localCount = 0;
      QString rebuilt;
      rebuilt.reserve(line.size());
      while (true) {
        const int idx = line.indexOf(query, from, cs);
        if (idx < 0) {
          break;
        }
        if (wholeWord && !isWholeWordAt(line, idx, query.size())) {
          from = idx + 1;
          continue;
        }
        rebuilt += line.mid(from, idx - from);
        rebuilt += replaceText;
        ++localCount;
        ++matchesReplaced;
        from = idx + qMax(1, query.size());
        if (matchesReplaced >= 200000) {
          emit message("Too many replacements; stopping at 200,000.");
          break;
        }
      }
      if (localCount > 0) {
        rebuilt += line.mid(from);
        outLines.push_back(rebuilt);
        changed = true;
      } else {
        outLines.push_back(line);
      }
    }

    if (cancelled_.load(std::memory_order_relaxed)) {
      break;
    }

    if (!changed) {
      continue;
    }

    const QString outText = outLines.join(QLatin1Char('\n'));
    const QByteArray outBytes = applyLineEnding(outText, lineEnding);

    QFile out(filePath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      continue;
    }
    if (out.write(outBytes) != outBytes.size()) {
      continue;
    }
    out.close();
    modifiedFiles.push_back(filePath);
  }

  emit applyFinished(matchesReplaced, filesScanned, modifiedFiles);
}

ReplaceInFilesDialog::ReplaceInFilesDialog(QString rootDir, QWidget* parent)
    : QWidget(parent), rootDir_(std::move(rootDir)) {
  setWindowTitle(tr("Replace in Files"));

  queryEdit_ = new QLineEdit(this);
  queryEdit_->setPlaceholderText(tr("Find"));

  replaceEdit_ = new QLineEdit(this);
  replaceEdit_->setPlaceholderText(tr("Replace"));

  patternsEdit_ = new QLineEdit(this);
  patternsEdit_->setPlaceholderText(tr("File patterns (e.g. *.ino;*.cpp;*.h)"));
  patternsEdit_->setText("*.ino;*.cpp;*.h;*.hpp;*.c;*.cc;*.cxx;*.hh;*.hxx");

  caseSensitive_ = new QCheckBox(tr("Case sensitive"), this);
  caseSensitive_->setChecked(false);

  wholeWord_ = new QCheckBox(tr("Whole words"), this);
  wholeWord_->setChecked(false);

  findButton_ = new QPushButton(tr("Find"), this);
  replaceAllButton_ = new QPushButton(tr("Replace All"), this);
  cancelButton_ = new QPushButton(tr("Cancel"), this);
  cancelButton_->setEnabled(false);

  statusLabel_ = new QLabel(this);
  statusLabel_->setText(tr("Root: %1").arg(rootDir_));

  results_ = new QTreeWidget(this);
  results_->setColumnCount(4);
  results_->setHeaderLabels({tr("File"), tr("Line"), tr("Col"), tr("Text")});
  results_->setRootIsDecorated(false);
  results_->setUniformRowHeights(true);

  auto* topRow = new QHBoxLayout();
  topRow->addWidget(new QLabel(tr("Find:"), this));
  topRow->addWidget(queryEdit_, 1);
  topRow->addWidget(new QLabel(tr("Replace:"), this));
  topRow->addWidget(replaceEdit_, 1);
  topRow->addWidget(findButton_);
  topRow->addWidget(replaceAllButton_);
  topRow->addWidget(cancelButton_);

  auto* patternsRow = new QHBoxLayout();
  patternsRow->addWidget(new QLabel(tr("Files:"), this));
  patternsRow->addWidget(patternsEdit_, 1);
  patternsRow->addWidget(caseSensitive_);
  patternsRow->addWidget(wholeWord_);

  auto* layout = new QVBoxLayout(this);
  layout->addLayout(topRow);
  layout->addLayout(patternsRow);
  layout->addWidget(statusLabel_);
  layout->addWidget(results_, 1);

  connect(findButton_, &QPushButton::clicked, this, [this] { startPreview(); });
  connect(replaceAllButton_, &QPushButton::clicked, this,
          [this] { startReplaceAll(); });
  connect(cancelButton_, &QPushButton::clicked, this, [this] {
    if (worker_) {
      worker_->cancel();
    }
  });
  connect(queryEdit_, &QLineEdit::returnPressed, this,
          [this] { startPreview(); });

  connect(results_, &QTreeWidget::itemActivated, this,
          [this](QTreeWidgetItem* item, int) {
            if (!item) {
              return;
            }
            const QString filePath = item->data(0, Qt::UserRole).toString();
            const int line = item->data(1, Qt::UserRole).toInt();
            const int col = item->data(2, Qt::UserRole).toInt();
            if (!filePath.isEmpty() && line > 0) {
              emit openLocation(filePath, line, col);
            }
          });

  workerThread_ = new QThread(this);
  worker_ = new ReplaceInFilesWorker();
  worker_->moveToThread(workerThread_);
  connect(workerThread_, &QThread::finished, worker_, &QObject::deleteLater);

  connect(worker_, &ReplaceInFilesWorker::matchFound, this,
          [this](const QString& filePath, int line, int column,
                 const QString& preview) { addResult(filePath, line, column, preview); });
  connect(worker_, &ReplaceInFilesWorker::message, this,
          [this](const QString& text) { statusLabel_->setText(text); });
  connect(worker_, &ReplaceInFilesWorker::previewFinished, this,
          [this](int matches, int filesScanned) {
            lastMatches_ = matches;
            lastFilesScanned_ = filesScanned;
            statusLabel_->setText(tr("Found %1 matches in %2 files.")
                                      .arg(matches)
                                      .arg(filesScanned));
            stopWork();
          });
  connect(worker_, &ReplaceInFilesWorker::applyFinished, this,
          [this](int matchesReplaced, int filesScanned, const QStringList& modifiedFiles) {
            statusLabel_->setText(tr("Replaced %1 occurrences in %2 files.")
                                      .arg(matchesReplaced)
                                      .arg(modifiedFiles.size()));
            if (!modifiedFiles.isEmpty()) {
              emit filesModified(modifiedFiles);
            }
            lastMatches_ = 0;
            lastFilesScanned_ = filesScanned;
            stopWork();
          });

  workerThread_->start();
}

ReplaceInFilesDialog::~ReplaceInFilesDialog() {
  if (worker_) {
    worker_->cancel();
  }
  if (workerThread_) {
    workerThread_->quit();
    if (!workerThread_->wait(5000)) {
      workerThread_->terminate();
      workerThread_->wait(2000);
    }
  }
}

void ReplaceInFilesDialog::setRootDir(QString rootDir) {
  rootDir_ = std::move(rootDir);
  if (statusLabel_) {
    if (rootDir_.trimmed().isEmpty()) {
      statusLabel_->setText(tr("Open a sketch folder first."));
    } else {
      statusLabel_->setText(tr("Root: %1").arg(rootDir_));
    }
  }
}

void ReplaceInFilesDialog::setQueryText(QString text) {
  if (queryEdit_) {
    queryEdit_->setText(std::move(text));
  }
}

void ReplaceInFilesDialog::setReplaceText(QString text) {
  if (replaceEdit_) {
    replaceEdit_->setText(std::move(text));
  }
}

void ReplaceInFilesDialog::focusQuery() {
  if (!queryEdit_) {
    return;
  }
  queryEdit_->setFocus(Qt::ShortcutFocusReason);
  queryEdit_->selectAll();
}

QStringList ReplaceInFilesDialog::parsePatterns() const {
  QString text = patternsEdit_ ? patternsEdit_->text().trimmed() : QString{};
  if (text.isEmpty()) {
    return {};
  }
  text.replace(',', ';');
  const QStringList parts = text.split(';', Qt::SkipEmptyParts);
  QStringList patterns;
  patterns.reserve(parts.size());
  for (QString p : parts) {
    p = p.trimmed();
    if (!p.isEmpty()) {
      patterns.push_back(p);
    }
  }
  patterns.removeDuplicates();
  return patterns;
}

void ReplaceInFilesDialog::addResult(QString filePath,
                                     int line,
                                     int column,
                                     QString preview) {
  if (!results_) {
    return;
  }
  const QString rel = QDir(rootDir_).relativeFilePath(filePath);

  auto* item = new QTreeWidgetItem();
  item->setText(0, rel.isEmpty() ? filePath : rel);
  item->setText(1, QString::number(line));
  item->setText(2, QString::number(column));
  item->setText(3, preview);
  item->setData(0, Qt::UserRole, filePath);
  item->setData(1, Qt::UserRole, line);
  item->setData(2, Qt::UserRole, column);
  results_->addTopLevelItem(item);
}

void ReplaceInFilesDialog::setRunning(bool running) {
  running_ = running;
  findButton_->setEnabled(!running_);
  replaceAllButton_->setEnabled(!running_);
  cancelButton_->setEnabled(running_);
  queryEdit_->setEnabled(!running_);
  replaceEdit_->setEnabled(!running_);
  patternsEdit_->setEnabled(!running_);
  caseSensitive_->setEnabled(!running_);
  wholeWord_->setEnabled(!running_);
}

void ReplaceInFilesDialog::startPreview() {
  if (!worker_ || running_) {
    return;
  }
  const QString query = queryEdit_ ? queryEdit_->text().trimmed() : QString{};
  if (query.isEmpty()) {
    statusLabel_->setText(tr("Enter text to search."));
    return;
  }

  results_->clear();
  lastMatches_ = 0;
  lastFilesScanned_ = 0;
  setRunning(true);
  statusLabel_->setText(tr("Searching\u2026"));

  const QStringList patterns = parsePatterns();
  const bool caseSensitive = caseSensitive_ ? caseSensitive_->isChecked() : false;
  const bool wholeWord = wholeWord_ ? wholeWord_->isChecked() : false;

  QMetaObject::invokeMethod(worker_, "preview", Qt::QueuedConnection,
                            Q_ARG(QString, rootDir_),
                            Q_ARG(QString, query),
                            Q_ARG(QStringList, patterns),
                            Q_ARG(bool, caseSensitive),
                            Q_ARG(bool, wholeWord));
}

void ReplaceInFilesDialog::startReplaceAll() {
  if (!worker_ || running_) {
    return;
  }
  const QString query = queryEdit_ ? queryEdit_->text().trimmed() : QString{};
  const QString replaceText = replaceEdit_ ? replaceEdit_->text() : QString{};
  if (query.isEmpty()) {
    statusLabel_->setText(tr("Enter text to search."));
    return;
  }

  QString prompt = tr("Replace all occurrences of '%1'?").arg(query);
  if (lastMatches_ > 0) {
    prompt += tr("\n\nLast search: %1 matches in %2 files scanned.")
                  .arg(lastMatches_)
                  .arg(lastFilesScanned_);
  }
  const auto choice = QMessageBox::question(
      this, tr("Replace in Files"), prompt,
      QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
  if (choice != QMessageBox::Yes) {
    return;
  }

  setRunning(true);
  statusLabel_->setText(tr("Replacing\u2026"));

  const QStringList patterns = parsePatterns();
  const bool caseSensitive = caseSensitive_ ? caseSensitive_->isChecked() : false;
  const bool wholeWord = wholeWord_ ? wholeWord_->isChecked() : false;

  QMetaObject::invokeMethod(worker_, "apply", Qt::QueuedConnection,
                            Q_ARG(QString, rootDir_),
                            Q_ARG(QString, query),
                            Q_ARG(QString, replaceText),
                            Q_ARG(QStringList, patterns),
                            Q_ARG(bool, caseSensitive),
                            Q_ARG(bool, wholeWord));
}

void ReplaceInFilesDialog::stopWork() {
  setRunning(false);
}
