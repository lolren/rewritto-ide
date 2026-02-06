#include "find_in_files_dialog.h"

#include <QCheckBox>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QThread>
#include <QTreeWidget>
#include <QVBoxLayout>

FindInFilesWorker::FindInFilesWorker(QObject* parent) : QObject(parent) {}

void FindInFilesWorker::cancel() {
  cancelled_.store(true, std::memory_order_relaxed);
}

void FindInFilesWorker::run(QString rootDir,
                            QString query,
                            QStringList patterns,
                            QStringList excludePatterns,
                            bool caseSensitive) {
  cancelled_.store(false, std::memory_order_relaxed);

  if (rootDir.trimmed().isEmpty()) {
    emit message("Search root is empty.");
    emit finished(0, 0);
    return;
  }

  if (query.isEmpty()) {
    emit message("Search text is empty.");
    emit finished(0, 0);
    return;
  }

  QDir root(rootDir);
  if (!root.exists()) {
    emit message("Search root does not exist.");
    emit finished(0, 0);
    return;
  }

  if (patterns.isEmpty()) {
    patterns = {QStringLiteral("*.ino"), QStringLiteral("*.c"),   QStringLiteral("*.cc"),
                QStringLiteral("*.cpp"), QStringLiteral("*.cxx"), QStringLiteral("*.h"),
                QStringLiteral("*.hh"),  QStringLiteral("*.hpp"), QStringLiteral("*.hxx")};
  }

  for (QString& p : patterns) {
    p = p.trimmed();
  }
  patterns.removeAll(QString{});
  patterns.removeDuplicates();

  for (QString& p : excludePatterns) {
    p = p.trimmed();
  }
  excludePatterns.removeAll(QString{});
  excludePatterns.removeDuplicates();

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

  auto matchesAny = [](const QString& relPath,
                       const QString& fileName,
                       const QStringList& pats) -> bool {
    for (QString p : pats) {
      p = p.trimmed();
      if (p.isEmpty()) {
        continue;
      }
      if (p.contains('/') || p.contains('\\')) {
        if (QDir::match(p, relPath)) {
          return true;
        }
        continue;
      }
      if (QDir::match(p, fileName)) {
        return true;
      }
    }
    return false;
  };

  QDirIterator it(root.absolutePath(), QDir::Files, QDirIterator::Subdirectories);

  while (it.hasNext()) {
    if (cancelled_.load(std::memory_order_relaxed)) {
      emit message("Search cancelled.");
      break;
    }
    const QString filePath = it.next();
    if (isExcluded(filePath)) {
      continue;
    }

    const QString relPath = root.relativeFilePath(filePath);
    const QString fileName = QFileInfo(filePath).fileName();
    if (!matchesAny(relPath, fileName, patterns)) {
      continue;
    }
    if (matchesAny(relPath, fileName, excludePatterns)) {
      continue;
    }
    ++filesScanned;

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
      continue;
    }

    int lineNo = 0;
    QByteArray lineBytes;
    while (!f.atEnd()) {
      if (cancelled_.load(std::memory_order_relaxed)) {
        break;
      }
      lineBytes = f.readLine();
      ++lineNo;

      QString line = QString::fromUtf8(lineBytes);
      if (line.endsWith('\n')) {
        line.chop(1);
      }
      if (line.endsWith('\r')) {
        line.chop(1);
      }

      int from = 0;
      while (true) {
        const int idx = line.indexOf(query, from, cs);
        if (idx < 0) {
          break;
        }
        ++matches;
        emit matchFound(filePath, lineNo, idx + 1, line);
        from = idx + qMax(1, query.size());
        if (matches >= 10000) {
          emit message("Too many matches; stopping at 10,000.");
          emit finished(matches, filesScanned);
          return;
        }
      }
    }
  }

  emit finished(matches, filesScanned);
}

FindInFilesDialog::FindInFilesDialog(QString rootDir, QWidget* parent)
    : QWidget(parent), rootDir_(std::move(rootDir)) {
  setWindowTitle(tr("Find in Files"));

  queryEdit_ = new QLineEdit(this);
  queryEdit_->setPlaceholderText(tr("Search text"));

  patternsEdit_ = new QLineEdit(this);
  patternsEdit_->setPlaceholderText(tr("File patterns (e.g. *.ino;*.cpp;*.h)"));
  patternsEdit_->setText("*.ino;*.cpp;*.h;*.hpp;*.c;*.cc;*.cxx;*.hh;*.hxx");

  excludeEdit_ = new QLineEdit(this);
  excludeEdit_->setPlaceholderText(
      tr("Exclude patterns (e.g. build/*;*.o;*.a)"));

  caseSensitive_ = new QCheckBox(tr("Case sensitive"), this);
  caseSensitive_->setChecked(false);

  findButton_ = new QPushButton(tr("Find"), this);
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
  topRow->addWidget(findButton_);
  topRow->addWidget(cancelButton_);

  auto* patternsRow = new QHBoxLayout();
  patternsRow->addWidget(new QLabel(tr("Files:"), this));
  patternsRow->addWidget(patternsEdit_, 1);
  patternsRow->addWidget(caseSensitive_);

  auto* excludeRow = new QHBoxLayout();
  excludeRow->addWidget(new QLabel(tr("Exclude:"), this));
  excludeRow->addWidget(excludeEdit_, 1);

  auto* layout = new QVBoxLayout(this);
  layout->addLayout(topRow);
  layout->addLayout(patternsRow);
  layout->addLayout(excludeRow);
  layout->addWidget(statusLabel_);
  layout->addWidget(results_, 1);

  connect(findButton_, &QPushButton::clicked, this, [this] { startSearch(); });
  connect(cancelButton_, &QPushButton::clicked, this, [this] {
    if (worker_) {
      worker_->cancel();
    }
  });
  connect(queryEdit_, &QLineEdit::returnPressed, this, [this] { startSearch(); });

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

  // Thread + worker
  workerThread_ = new QThread(this);
  worker_ = new FindInFilesWorker();
  worker_->moveToThread(workerThread_);
  connect(workerThread_, &QThread::finished, worker_, &QObject::deleteLater);

  connect(worker_, &FindInFilesWorker::matchFound, this,
          [this](const QString& filePath, int line, int column, const QString& preview) {
            addResult(filePath, line, column, preview);
          });
  connect(worker_, &FindInFilesWorker::message, this,
          [this](const QString& text) { statusLabel_->setText(text); });
  connect(worker_, &FindInFilesWorker::finished, this,
          [this](int matches, int filesScanned) {
            matches_ = matches;
            filesScanned_ = filesScanned;
            statusLabel_->setText(
                tr("Done. %1 matches in %2 files.").arg(matches_).arg(filesScanned_));
            stopSearch();
          });

  workerThread_->start();
}

FindInFilesDialog::~FindInFilesDialog() {
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

void FindInFilesDialog::setRootDir(QString rootDir) {
  rootDir_ = std::move(rootDir);
  if (!statusLabel_) {
    return;
  }
  if (rootDir_.trimmed().isEmpty()) {
    statusLabel_->setText(tr("Open a sketch folder first."));
  } else {
    statusLabel_->setText(tr("Root: %1").arg(rootDir_));
  }
}

void FindInFilesDialog::setQueryText(QString text) {
  if (queryEdit_) {
    queryEdit_->setText(std::move(text));
  }
}

void FindInFilesDialog::focusQuery() {
  if (!queryEdit_) {
    return;
  }
  queryEdit_->setFocus(Qt::ShortcutFocusReason);
  queryEdit_->selectAll();
}

QStringList FindInFilesDialog::parsePatterns() const {
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

QStringList FindInFilesDialog::parseExcludePatterns() const {
  QString text = excludeEdit_ ? excludeEdit_->text().trimmed() : QString{};
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

void FindInFilesDialog::addResult(QString filePath, int line, int column, QString preview) {
  if (!results_) {
    return;
  }
  const QString rel =
      QDir(rootDir_).relativeFilePath(filePath);
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

void FindInFilesDialog::startSearch() {
  if (!worker_) {
    return;
  }
  const QString query = queryEdit_ ? queryEdit_->text() : QString{};
  if (query.trimmed().isEmpty()) {
    statusLabel_->setText("Enter text to search.");
    return;
  }

  results_->clear();
  matches_ = 0;
  filesScanned_ = 0;

  findButton_->setEnabled(false);
  cancelButton_->setEnabled(true);
  statusLabel_->setText(tr("Searching\u2026"));

  const QStringList patterns = parsePatterns();
  const QStringList excludePatterns = parseExcludePatterns();

  const bool caseSensitive = caseSensitive_ ? caseSensitive_->isChecked() : false;

  QMetaObject::invokeMethod(worker_, "run", Qt::QueuedConnection,
                            Q_ARG(QString, rootDir_),
                            Q_ARG(QString, query),
                            Q_ARG(QStringList, patterns),
                            Q_ARG(QStringList, excludePatterns),
                            Q_ARG(bool, caseSensitive));
}

void FindInFilesDialog::stopSearch() {
  findButton_->setEnabled(true);
  cancelButton_->setEnabled(false);
}
