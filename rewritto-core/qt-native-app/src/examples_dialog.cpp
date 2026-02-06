#include "examples_dialog.h"

#include <QAbstractItemView>
#include <QBoxLayout>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QThread>
#include <QTreeView>

#include <QPlainTextEdit>
#include <QSplitter>
#include <QTimer>

namespace {
constexpr int kRoleExampleFolder = Qt::UserRole + 501;
constexpr int kRoleExampleIno = Qt::UserRole + 502;

QString safeTempRoot() {
  QString tmp = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
  if (tmp.isEmpty()) {
    tmp = QDir::tempPath();
  }
  return tmp;
}

QString sanitizeName(QString name) {
  name.replace(QRegularExpression(QStringLiteral(R"([^A-Za-z0-9._-]+)")), QStringLiteral("_"));
  name = name.trimmed();
  if (name.isEmpty()) {
    return QStringLiteral("example");
  }
  return name;
}

QStandardItem* ensureChild(QStandardItem* parent, const QString& text) {
  if (!parent) {
    return nullptr;
  }
  for (int i = 0; i < parent->rowCount(); ++i) {
    QStandardItem* child = parent->child(i, 0);
    if (child && child->data(Qt::DisplayRole).toString() == text &&
        !child->data(kRoleExampleFolder).isValid()) {
      return child;
    }
  }
  auto* item = new QStandardItem(text);
  item->setEditable(false);
  parent->appendRow(item);
  return item;
}
}  // namespace

ExamplesDialog::ExamplesDialog(QWidget* parent) : QDialog(parent) {
  setWindowTitle(tr("Examples"));
  resize(1000, 600); // Made wider for preview

  options_ = ExamplesScanner::defaultOptions();
  
  previewTimer_ = new QTimer(this);
  previewTimer_->setSingleShot(true);
  previewTimer_->setInterval(150);

  buildUi();
  wireSignals();
  startScan();
}

ExamplesDialog::~ExamplesDialog() {
  if (scanThread_) {
    scanThread_->requestInterruption();
    scanThread_->quit();
    scanThread_->wait(200);
  }
}

void ExamplesDialog::setScanOptions(ExamplesScanner::Options options) {
  options_ = std::move(options);
}

void ExamplesDialog::setFilterText(QString text) {
  if (!filterEdit_) {
    return;
  }
  filterEdit_->setText(std::move(text));
}

void ExamplesDialog::buildUi() {
  filterEdit_ = new QLineEdit(this);
  filterEdit_->setPlaceholderText(tr("Filter examples…"));

  refreshButton_ = new QPushButton(tr("Refresh"), this);

  model_ = new QStandardItemModel(this);
  model_->setHorizontalHeaderLabels({tr("Examples")});

  proxy_ = new QSortFilterProxyModel(this);
  proxy_->setSourceModel(model_);
  proxy_->setRecursiveFilteringEnabled(true);
  proxy_->setFilterKeyColumn(0);
  proxy_->setFilterCaseSensitivity(Qt::CaseInsensitive);

  tree_ = new QTreeView(this);
  tree_->setModel(proxy_);
  tree_->setHeaderHidden(true);
  tree_->setUniformRowHeights(true);
  tree_->setEditTriggers(QAbstractItemView::NoEditTriggers);

  preview_ = new QPlainTextEdit(this);
  preview_->setReadOnly(true);
  preview_->setPlaceholderText(tr("Select an example to see its code..."));
  QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  font.setPointSize(10);
  preview_->setFont(font);

  QSplitter* splitter = new QSplitter(Qt::Horizontal, this);
  splitter->addWidget(tree_);
  splitter->addWidget(preview_);
  splitter->setStretchFactor(0, 1);
  splitter->setStretchFactor(1, 2);

  openButton_ = new QPushButton(tr("Open"), this);
  openButton_->setDefault(true);
  openButton_->setEnabled(false);

  auto* closeButton = new QPushButton(tr("Close"), this);

  statusLabel_ = new QLabel(tr("Ready"), this);
  statusLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);

  auto* topRow = new QHBoxLayout();
  topRow->addWidget(filterEdit_, 1);
  topRow->addWidget(refreshButton_);

  auto* bottomRow = new QHBoxLayout();
  bottomRow->addWidget(statusLabel_, 1);
  bottomRow->addWidget(openButton_);
  bottomRow->addWidget(closeButton);

  auto* layout = new QVBoxLayout(this);
  layout->addLayout(topRow);
  layout->addWidget(splitter, 1);
  layout->addLayout(bottomRow);

  connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);
}

void ExamplesDialog::wireSignals() {
  connect(refreshButton_, &QPushButton::clicked, this, [this] { startScan(); });
  connect(filterEdit_, &QLineEdit::textChanged, this, [this](const QString& text) {
    if (text.trimmed().isEmpty()) {
      proxy_->setFilterRegularExpression(QRegularExpression());
    } else {
      proxy_->setFilterRegularExpression(
          QRegularExpression(QRegularExpression::escape(text),
                             QRegularExpression::CaseInsensitiveOption));
    }
    tree_->expandAll();
  });

  connect(tree_, &QTreeView::doubleClicked, this,
          [this](const QModelIndex&) { openSelectedExample(); });
  connect(openButton_, &QPushButton::clicked, this,
          [this] { openSelectedExample(); });

  connect(tree_->selectionModel(), &QItemSelectionModel::currentChanged, this,
          [this](const QModelIndex&, const QModelIndex&) {
            QString folder;
            QString ino;
            bool isExample = currentSelectionToExample(&folder, &ino);
            openButton_->setEnabled(isExample);
            if (isExample) {
              statusLabel_->setText(folder);
              previewTimer_->start(); // Debounce the preview update
            } else {
              statusLabel_->setText(tr("Ready"));
              preview_->clear();
            }
          });

  connect(previewTimer_, &QTimer::timeout, this, &ExamplesDialog::updatePreview);
}

void ExamplesDialog::updatePreview() {
    QString folder;
    QString ino;
    if (currentSelectionToExample(&folder, &ino)) {
        QFile file(ino);
        if (file.open(QFile::ReadOnly | QFile::Text)) {
            // Read first 10KB for preview to keep it fast
            preview_->setPlainText(QString::fromUtf8(file.read(10240)));
        } else {
            preview_->setPlainText(tr("Could not read example code."));
        }
    }
}

void ExamplesDialog::startScan() {
  if (scanThread_) {
    return;
  }

  model_->clear();
  model_->setHorizontalHeaderLabels({tr("Examples")});
  openButton_->setEnabled(false);
  tree_->setEnabled(false);
  refreshButton_->setEnabled(false);
  filterEdit_->setEnabled(false);
  statusLabel_->setText(tr("Scanning examples…"));

  const ExamplesScanner::Options options = options_;

  QPointer<ExamplesDialog> self(this);
  scanThread_ = QThread::create([options, self] {
    const QVector<ExampleSketch> result = ExamplesScanner::scan(options);
    if (!self) {
      return;
    }
    QMetaObject::invokeMethod(
        self.data(), [self, result] {
          if (!self) {
            return;
          }
          self->populate(result);
        }, Qt::QueuedConnection);
  });

  connect(scanThread_, &QThread::finished, this, [this] {
    scanThread_->deleteLater();
    scanThread_ = nullptr;
    tree_->setEnabled(true);
    refreshButton_->setEnabled(true);
    filterEdit_->setEnabled(true);
  });
  scanThread_->start();
}

void ExamplesDialog::populate(const QVector<ExampleSketch>& examples) {
  model_->clear();
  model_->setHorizontalHeaderLabels({tr("Examples")});

  QStandardItem* root = model_->invisibleRootItem();
  for (const ExampleSketch& ex : examples) {
    if (ex.menuPath.isEmpty() || ex.folderPath.isEmpty() || ex.inoPath.isEmpty()) {
      continue;
    }

    QStandardItem* parent = root;
    for (int i = 0; i < ex.menuPath.size(); ++i) {
      const QString seg = ex.menuPath.at(i);
      if (seg.trimmed().isEmpty()) {
        continue;
      }
      const bool isLeaf = (i == ex.menuPath.size() - 1);
      if (!isLeaf) {
        parent = ensureChild(parent, seg);
        continue;
      }

      auto* leaf = new QStandardItem(seg);
      leaf->setEditable(false);
      leaf->setData(ex.folderPath, kRoleExampleFolder);
      leaf->setData(ex.inoPath, kRoleExampleIno);
      parent->appendRow(leaf);
    }
  }

  tree_->expandToDepth(2);
  statusLabel_->setText(tr("Found %1 examples").arg(examples.size()));
}

bool ExamplesDialog::currentSelectionToExample(QString* folderPath, QString* inoPath) const {
  if (folderPath) {
    folderPath->clear();
  }
  if (inoPath) {
    inoPath->clear();
  }

  const QModelIndex current = tree_->currentIndex();
  if (!current.isValid()) {
    return false;
  }

  const QModelIndex src = proxy_->mapToSource(current);
  if (!src.isValid()) {
    return false;
  }

  const QString folder = model_->data(src, kRoleExampleFolder).toString();
  const QString ino = model_->data(src, kRoleExampleIno).toString();
  if (folder.isEmpty() || ino.isEmpty()) {
    return false;
  }

  if (folderPath) {
    *folderPath = folder;
  }
  if (inoPath) {
    *inoPath = ino;
  }
  return true;
}

void ExamplesDialog::openSelectedExample() {
  QString srcFolder;
  QString srcIno;
  if (!currentSelectionToExample(&srcFolder, &srcIno)) {
    return;
  }

  const QString exampleName = sanitizeName(QFileInfo(srcFolder).fileName());
  const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
	  const QString folderName = QStringLiteral("%1_%2").arg(exampleName, stamp);
	  const QString dstFolder =
	      QDir(safeTempRoot())
	          .absoluteFilePath(QStringLiteral("rewritto-ide/examples/%1")
	                                .arg(folderName));

  QString error;
  if (!copyDirectoryRecursively(srcFolder, dstFolder, &error)) {
    QMessageBox::warning(this, tr("Open Example Failed"),
                         error.isEmpty() ? tr("Could not copy example.") : error);
    return;
  }

  const QString relIno = QDir(srcFolder).relativeFilePath(srcIno);
  QString dstIno = QDir(dstFolder).absoluteFilePath(relIno);

  // arduino-cli requires the main .ino file to match the folder name.
  // Rename the copied main file to folderName.ino
  const QString finalIno = QDir(dstFolder).absoluteFilePath(folderName + ".ino");
  if (dstIno != finalIno) {
    if (QFile::exists(finalIno)) {
      QFile::remove(finalIno);
    }
    if (QFile::rename(dstIno, finalIno)) {
      dstIno = finalIno;
    }
  }

  emit openExampleRequested(dstFolder, dstIno);
  accept();
}

bool ExamplesDialog::copyDirectoryRecursively(const QString& srcDir,
                                             const QString& dstDir,
                                             QString* errorMessage) {
  const QDir src(srcDir);
  if (!src.exists()) {
    if (errorMessage) {
      *errorMessage = tr("Source folder does not exist.");
    }
    return false;
  }

  if (!QDir().mkpath(dstDir)) {
    if (errorMessage) {
      *errorMessage = tr("Could not create destination folder.");
    }
    return false;
  }

  const QDir srcRoot(srcDir);
  QDirIterator it(srcDir, QDir::NoDotAndDotDot | QDir::AllEntries,
                  QDirIterator::Subdirectories);
  while (it.hasNext()) {
    const QString srcPath = it.next();
    const QFileInfo fi(srcPath);
    const QString rel = srcRoot.relativeFilePath(srcPath);
    const QString dstPath = QDir(dstDir).absoluteFilePath(rel);

    if (fi.isDir()) {
      if (!QDir().mkpath(dstPath)) {
        if (errorMessage) {
          *errorMessage = tr("Could not create folder: %1").arg(dstPath);
        }
        return false;
      }
      continue;
    }

    if (fi.isFile() || fi.isSymLink()) {
      QDir().mkpath(QFileInfo(dstPath).absolutePath());
      if (QFile::exists(dstPath)) {
        QFile::remove(dstPath);
      }
      if (!QFile::copy(srcPath, dstPath)) {
        if (errorMessage) {
          *errorMessage = tr("Failed to copy file: %1").arg(rel);
        }
        return false;
      }
    }
  }

  return true;
}
