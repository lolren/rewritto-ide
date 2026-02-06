#include "editor_widget.h"

#include "code_editor.h"
#include "cpp_highlighter.h"

#include <algorithm>
#include <utility>
#include <QAbstractButton>
#include <QClipboard>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFontDatabase>
#include <QCryptographicHash>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabBar>
#include <QTabWidget>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextOption>
#include <QTimer>
#include <QToolButton>
#include <QSettings>
#include <QStyle>
#include <QUrl>

namespace {
constexpr auto kPropDiskMTime = "diskMTimeMSecs";
constexpr auto kPropDiskHash = "diskSha1";
constexpr auto kPropSuppressNextDiskEvent = "suppressNextDiskEvent";
constexpr qint64 kLargeFileModeBytes = 512LL * 1024;

QString defaultLineEndingPreference() {
  QSettings settings;
  settings.beginGroup("Preferences");
  QString v = settings.value("defaultLineEnding", QStringLiteral("LF"))
                  .toString()
                  .trimmed()
                  .toUpper();
  settings.endGroup();
  return v == QStringLiteral("CRLF") ? QStringLiteral("CRLF") : QStringLiteral("LF");
}

bool trimTrailingWhitespacePreference() {
  QSettings settings;
  settings.beginGroup("Preferences");
  const bool enabled = settings.value("trimTrailingWhitespace", false).toBool();
  settings.endGroup();
  return enabled;
}

QString detectLineEnding(const QByteArray& data) {
  if (data.isEmpty()) {
    return defaultLineEndingPreference();
  }
  return data.contains("\r\n") ? QStringLiteral("CRLF") : QStringLiteral("LF");
}

QString normalizeLineEndings(QString text) {
  text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
  text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
  return text;
}

QString trimTrailingWhitespace(QString text) {
  QString out;
  out.reserve(text.size());

  int start = 0;
  while (start < text.size()) {
    int end = text.indexOf(QLatin1Char('\n'), start);
    if (end < 0) {
      end = text.size();
    }

    int trimmedEnd = end;
    while (trimmedEnd > start) {
      const QChar ch = text.at(trimmedEnd - 1);
      if (ch != QLatin1Char(' ') && ch != QLatin1Char('\t')) {
        break;
      }
      --trimmedEnd;
    }

    out += text.mid(start, trimmedEnd - start);
    if (end < text.size()) {
      out += QLatin1Char('\n');
    }
    start = end + 1;
  }

  return out;
}

QByteArray applyLineEnding(QString text, const QString& lineEnding) {
  if (lineEnding == QStringLiteral("CRLF")) {
    text.replace(QStringLiteral("\n"), QStringLiteral("\r\n"));
  }
  return text.toUtf8();
}
}  // namespace

EditorWidget::EditorWidget(QWidget* parent) : QWidget(parent) {
  defaultSaveDir_ = QDir::homePath();
  editorFont_ = QFontDatabase::systemFont(QFontDatabase::FixedFont);

  fileWatcher_ = new QFileSystemWatcher(this);
  fileChangedTimer_ = new QTimer(this);
  fileChangedTimer_->setSingleShot(true);
  fileChangedTimer_->setInterval(250);
  connect(fileWatcher_, &QFileSystemWatcher::fileChanged, this,
          [this](const QString& path) {
            if (suppressDiskEvents_) {
              return;
            }
            if (path.trimmed().isEmpty()) {
              return;
            }
            pendingFileChanges_.insert(QFileInfo(path).absoluteFilePath());
            if (fileChangedTimer_) {
              fileChangedTimer_->start();
            }
          });
  connect(fileChangedTimer_, &QTimer::timeout, this,
          [this] { processPendingFileChanges(); });

  autosaveTimer_ = new QTimer(this);
  autosaveTimer_->setSingleShot(false);
  connect(autosaveTimer_, &QTimer::timeout, this, [this] { maybeAutosave(); });
  updateAutosaveTimer();

  tabs_ = new QTabWidget(this);
  tabs_->setObjectName("EditorTabs");
  tabs_->setTabsClosable(true);
  tabs_->setMovable(true);

  {
    auto* newTabButton = new QToolButton(tabs_);
    newTabButton->setObjectName("NewTabButton");
    newTabButton->setToolTip(tr("New Tab"));
    newTabButton->setAutoRaise(true);
    QIcon icon = QIcon::fromTheme("tab-new");
    if (icon.isNull()) {
      icon = QIcon::fromTheme("list-add");
    }
    if (icon.isNull()) {
      icon = style()->standardIcon(QStyle::SP_FileDialogNewFolder);
    }
    newTabButton->setIcon(icon);
    newTabButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    tabs_->setCornerWidget(newTabButton, Qt::TopRightCorner);
    connect(newTabButton, &QToolButton::clicked, this,
            [this] { emit newTabRequested(); });
  }

  auto* initial = new CodeEditor(tabs_);
  initial->setEditorSettings(tabSize_, insertSpaces_);
  (void)new CppHighlighter(initial->document());
  applyAppearance(initial);
  setFilePathFor(initial, {});
  initial->setProperty("lineEnding", defaultLineEndingPreference());
  initial->setProperty(kPropDiskMTime, 0LL);
  initial->setProperty(kPropDiskHash, QByteArray{});
  initial->setProperty(kPropSuppressNextDiskEvent, false);
  wireBreakpointSignals(initial);
  tabs_->addTab(initial, "Untitled");

  connect(tabs_, &QTabWidget::currentChanged, this, [this](int index) {
    auto* editor = qobject_cast<QPlainTextEdit*>(tabs_->widget(index));
    emit currentFileChanged(editor ? filePathFor(editor) : QString{});
  });

  connect(tabs_, &QTabWidget::tabCloseRequested, this, [this](int index) {
    (void)closeTabAt(index);
  });

  if (auto* bar = tabs_->tabBar()) {
    bar->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(bar, &QWidget::customContextMenuRequested, this, [this, bar](const QPoint& pos) {
      const int index = bar->tabAt(pos);
      if (index < 0 || index >= tabs_->count()) {
        return;
      }

      auto* editor = qobject_cast<QPlainTextEdit*>(tabs_->widget(index));
      const QString filePath = filePathFor(editor);

      QMenu menu(this);
      QAction* closeTabAction = menu.addAction(tr("Close Tab"));
      QAction* closeOtherTabsAction = menu.addAction(tr("Close Other Tabs"));
      QAction* closeAllTabsAction = menu.addAction(tr("Close All Tabs"));
      QAction* reopenClosedTabAction = menu.addAction(tr("Reopen Closed Tab"));
      menu.addSeparator();
      QAction* copyPath = menu.addAction(tr("Copy Path"));
      QAction* reveal = menu.addAction(tr("Show in File Manager"));

      const bool hasPath = !filePath.trimmed().isEmpty();
      copyPath->setEnabled(hasPath);
      reveal->setEnabled(hasPath);
      reopenClosedTabAction->setEnabled(!closedFileStack_.isEmpty());

      QAction* chosen = menu.exec(bar->mapToGlobal(pos));
      if (!chosen) {
        return;
      }

      if (chosen == closeTabAction) {
        (void)closeTabAt(index);
        return;
      }
      if (chosen == closeOtherTabsAction) {
        for (int i = tabs_->count() - 1; i >= 0; --i) {
          if (i == index) {
            continue;
          }
          if (!closeTabAt(i)) {
            return;
          }
        }
        return;
      }
      if (chosen == closeAllTabsAction) {
        (void)this->closeAllTabs();
        return;
      }
      if (chosen == reopenClosedTabAction) {
        (void)reopenLastClosedTab();
        return;
      }
      if (chosen == copyPath) {
        if (auto* cb = QGuiApplication::clipboard()) {
          cb->setText(filePath);
        }
        return;
      }
      if (chosen == reveal) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(filePath).absolutePath()));
        return;
      }
    });
  }

  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(tabs_);
}

void EditorWidget::setSuppressDiskEvents(bool suppress) {
  if (suppressDiskEvents_ == suppress) {
    return;
  }
  suppressDiskEvents_ = suppress;
  if (suppressDiskEvents_) {
    pendingFileChanges_.clear();
    if (fileChangedTimer_) {
      fileChangedTimer_->stop();
    }
  }
}

void EditorWidget::watchFilePath(const QString& filePath) {
  if (!fileWatcher_) {
    return;
  }
  const QString abs = QFileInfo(filePath).absoluteFilePath();
  if (abs.trimmed().isEmpty()) {
    return;
  }
  if (!QFileInfo(abs).exists()) {
    return;
  }
  const QStringList watched = fileWatcher_->files();
  if (!watched.contains(abs)) {
    fileWatcher_->addPath(abs);
  }
}

void EditorWidget::unwatchFilePath(const QString& filePath) {
  if (!fileWatcher_) {
    return;
  }
  const QString abs = QFileInfo(filePath).absoluteFilePath();
  if (abs.trimmed().isEmpty()) {
    return;
  }
  const QStringList watched = fileWatcher_->files();
  if (watched.contains(abs)) {
    fileWatcher_->removePath(abs);
  }
}

void EditorWidget::updateDiskMTimeProperty(QPlainTextEdit* editor, const QString& filePath) {
  if (!editor) {
    return;
  }
  const QFileInfo info(filePath);
  const qint64 mtime = info.exists() ? info.lastModified().toMSecsSinceEpoch() : 0LL;
  editor->setProperty(kPropDiskMTime, mtime);
}

void EditorWidget::processPendingFileChanges() {
  if (pendingFileChanges_.isEmpty()) {
    return;
  }

  const QSet<QString> paths = std::exchange(pendingFileChanges_, {});
  for (const QString& path : paths) {
    if (path.trimmed().isEmpty()) {
      continue;
    }
    auto* editor = editorForFile(path);
    if (!editor || !editor->document()) {
      continue;
    }

    const QFileInfo info(path);

    if (!info.exists()) {
      QMessageBox::warning(this, tr("File Deleted"),
                           tr("The file '%1' was deleted from disk.").arg(path));
      editor->setProperty(kPropDiskMTime, 0LL);
      editor->setProperty(kPropDiskHash, QByteArray{});
      continue;
    }

    const qint64 newMTime = info.lastModified().toMSecsSinceEpoch();
    QByteArray newHash;
    {
      QFile f(path);
      if (f.open(QIODevice::ReadOnly)) {
        newHash = QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha1);
      }
    }
    const QByteArray oldHash = editor->property(kPropDiskHash).toByteArray();
    if (!oldHash.isEmpty() && oldHash == newHash) {
      editor->setProperty(kPropSuppressNextDiskEvent, false);
      updateDiskMTimeProperty(editor, path);
      watchFilePath(path);
      continue;
    }
    editor->setProperty(kPropSuppressNextDiskEvent, false);

    const bool modified = editor->document()->isModified();
    if (!modified) {
      if (reloadFileIfUnmodified(path)) {
        updateDiskMTimeProperty(editor, path);
      }
      watchFilePath(path);
      continue;
    }

    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(tr("File Changed on Disk"));
    box.setText(tr("The file '%1' has changed on disk.").arg(QFileInfo(path).fileName()));
    box.setInformativeText(
        tr("You have unsaved changes. Reloading will discard them."));

    QPushButton* reloadBtn = box.addButton(tr("Reload"), QMessageBox::AcceptRole);
    QPushButton* keepBtn = box.addButton(tr("Keep My Changes"), QMessageBox::RejectRole);
    box.setDefaultButton(keepBtn);

    box.exec();
    if (box.clickedButton() == static_cast<QAbstractButton*>(reloadBtn)) {
      editor->document()->setModified(false);
      if (reloadFileIfUnmodified(path)) {
        updateDiskMTimeProperty(editor, path);
      } else {
        updateDiskMTimeProperty(editor, path);
      }
    } else {
      editor->setProperty(kPropDiskMTime, newMTime);
      editor->setProperty(kPropDiskHash, newHash);
    }

    watchFilePath(path);
  }
}

bool EditorWidget::openFile(const QString& filePath) {
  const QString absPath = QDir::cleanPath(QFileInfo(filePath).absoluteFilePath());
  closedFileStack_.removeAll(absPath);
  for (int i = 0; i < tabs_->count(); ++i) {
    auto* editor = qobject_cast<QPlainTextEdit*>(tabs_->widget(i));
    if (editor) {
      const QString existing = filePathFor(editor);
      if (!existing.isEmpty() && QDir::cleanPath(QFileInfo(existing).absoluteFilePath()) == absPath) {
        tabs_->setCurrentIndex(i);
        return true;
      }
    }
  }

  for (int i = 0; i < tabs_->count(); ++i) {
    auto* current = qobject_cast<QPlainTextEdit*>(tabs_->widget(i));
    if (current && filePathFor(current).isEmpty() &&
        current->document() && !current->document()->isModified()) {
      tabs_->removeTab(i);
      current->deleteLater();
      --i;  // Adjust index after removal
    }
  }

  const QFileInfo absInfo(absPath);
  const bool largeFileMode = absInfo.size() >= kLargeFileModeBytes;

  QFile file(absPath);
  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }

  const QByteArray data = file.readAll();
  const QByteArray diskHash = QCryptographicHash::hash(data, QCryptographicHash::Sha1);
  const QString lineEnding = detectLineEnding(data);
  const QString contents = normalizeLineEndings(QString::fromUtf8(data));

  auto* editor = new CodeEditor(tabs_);
  editor->setEditorSettings(tabSize_, insertSpaces_);
  editor->setFoldingEnabled(!largeFileMode);
  editor->setPlainText(contents);
  editor->document()->setModified(false);
  if (!largeFileMode) {
    (void)new CppHighlighter(editor->document());
  }
  applyAppearance(editor);
  setFilePathFor(editor, absPath);
  editor->setProperty("lineEnding", lineEnding);
  editor->setProperty(kPropSuppressNextDiskEvent, false);
  updateDiskMTimeProperty(editor, absPath);
  editor->setProperty(kPropDiskHash, diskHash);
  wireBreakpointSignals(editor);

  connect(editor->document(), &QTextDocument::modificationChanged, this,
          [this, editor] { updateTabTitle(editor); });

  auto* timer = new QTimer(editor);
  timer->setSingleShot(true);
  timer->setInterval(250);
  connect(editor, &QPlainTextEdit::textChanged, this, [timer] {
    timer->start();
  });
  connect(timer, &QTimer::timeout, this, [this, editor] {
    const QString path = filePathFor(editor);
    if (!path.isEmpty()) {
      emit documentChanged(path, editor->toPlainText());
    }
  });

  const int index = tabs_->addTab(editor, QFileInfo(absPath).fileName());
  if (largeFileMode) {
    tabs_->setTabToolTip(
        index,
        tr("Large File Mode: syntax highlighting and folding are disabled for performance."));
  }
  tabs_->setCurrentIndex(index);
  updateTabTitle(editor);

  watchFilePath(absPath);

  emit documentOpened(absPath, editor->toPlainText());
  return true;
}

void EditorWidget::wireBreakpointSignals(CodeEditor* editor) {
  if (!editor) {
    return;
  }
  connect(editor, &CodeEditor::breakpointsChanged, this,
          [this, editor](const QVector<int>& lines) {
            const QString filePath = filePathFor(editor);
            if (!filePath.trimmed().isEmpty()) {
              emit breakpointsChanged(filePath, lines);
            }
          });
}

bool EditorWidget::reloadFileIfUnmodified(const QString& filePath) {
  if (filePath.trimmed().isEmpty()) {
    return false;
  }
  const QString absPath = QFileInfo(filePath).absoluteFilePath();
  auto* editor = editorForFile(absPath);
  if (!editor || !editor->document()) {
    return false;
  }
  if (editor->document()->isModified()) {
    return false;
  }

  QFile file(absPath);
  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }
  const QByteArray data = file.readAll();
  const QByteArray diskHash = QCryptographicHash::hash(data, QCryptographicHash::Sha1);
  const QString lineEnding = detectLineEnding(data);
  const QString contents = normalizeLineEndings(QString::fromUtf8(data));

  const int cursorPos = editor->textCursor().position();

  editor->setPlainText(contents);
  editor->setProperty("lineEnding", lineEnding);
  editor->document()->setModified(false);
  editor->setProperty(kPropSuppressNextDiskEvent, false);
  updateDiskMTimeProperty(editor, absPath);
  editor->setProperty(kPropDiskHash, diskHash);

  QTextCursor cursor(editor->document());
  const int maxPos = std::max(0, editor->document()->characterCount() - 1);
  cursor.setPosition(std::clamp(cursorPos, 0, maxPos));
  editor->setTextCursor(cursor);

  updateTabTitle(editor);
  return true;
}

bool EditorWidget::save() {
  const QString path = currentFilePath();
  if (path.isEmpty()) {
    return false;
  }
  return saveAs(path);
}

bool EditorWidget::saveCurrentWithDialog() {
  auto* editor = currentEditor();
  if (!editor) {
    return false;
  }
  return promptAndSaveEditor(editor);
}

bool EditorWidget::saveAs(const QString& filePath) {
  auto* editor = currentEditor();
  if (!editor) {
    return false;
  }
  return saveEditorAs(editor, filePath);
}

bool EditorWidget::saveCopyAs(const QString& filePath) {
  auto* editor = currentEditor();
  if (!editor || filePath.trimmed().isEmpty()) {
    return false;
  }

  QDir().mkpath(QFileInfo(filePath).absolutePath());

  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return false;
  }

  const QString lineEnding = editor->property("lineEnding").toString();
  QString text = editor->toPlainText();
  if (trimTrailingWhitespacePreference()) {
    text = trimTrailingWhitespace(std::move(text));
  }
  const QByteArray data = applyLineEnding(std::move(text), lineEnding);
  const QByteArray diskHash = QCryptographicHash::hash(data, QCryptographicHash::Sha1);
  if (file.write(data) != data.size()) {
    return false;
  }
  file.close();
  return true;
}

bool EditorWidget::saveAll() {
  if (!tabs_) {
    return false;
  }
  const int originalIndex = tabs_->currentIndex();

  for (int i = 0; i < tabs_->count(); ++i) {
    auto* editor = qobject_cast<QPlainTextEdit*>(tabs_->widget(i));
    if (!editor || !editor->document() || !editor->document()->isModified()) {
      continue;
    }
    const QString path = filePathFor(editor);
    if (path.isEmpty()) {
      if (originalIndex >= 0) {
        tabs_->setCurrentIndex(originalIndex);
      }
      return false;
    }
    if (!saveEditorAs(editor, path)) {
      if (originalIndex >= 0) {
        tabs_->setCurrentIndex(originalIndex);
      }
      return false;
    }
  }

  if (originalIndex >= 0) {
    tabs_->setCurrentIndex(originalIndex);
  }
  return true;
}

bool EditorWidget::saveAllWithDialog() {
  if (!tabs_) {
    return false;
  }
  const int originalIndex = tabs_->currentIndex();

  for (int i = 0; i < tabs_->count(); ++i) {
    auto* editor = qobject_cast<QPlainTextEdit*>(tabs_->widget(i));
    if (!editor || !editor->document() || !editor->document()->isModified()) {
      continue;
    }
    if (!promptAndSaveEditor(editor)) {
      if (originalIndex >= 0) {
        tabs_->setCurrentIndex(originalIndex);
      }
      return false;
    }
  }

  if (originalIndex >= 0) {
    tabs_->setCurrentIndex(originalIndex);
  }
  return true;
}

void EditorWidget::setDefaultSaveDirectory(QString directory) {
  directory = directory.trimmed();
  if (!directory.isEmpty()) {
    defaultSaveDir_ = std::move(directory);
  }
}

QString EditorWidget::defaultSaveDirectory() const {
  return defaultSaveDir_;
}

bool EditorWidget::hasUnsavedChanges() const {
  for (int i = 0; i < tabs_->count(); ++i) {
    auto* editor = qobject_cast<QPlainTextEdit*>(tabs_->widget(i));
    if (editor && editor->document()->isModified()) {
      return true;
    }
  }
  return false;
}

QString EditorWidget::currentFilePath() const {
  auto* editor = currentEditor();
  return editor ? filePathFor(editor) : QString{};
}

QPlainTextEdit* EditorWidget::currentEditorWidget() const {
  return currentEditor();
}

QPlainTextEdit* EditorWidget::editorWidgetForFile(const QString& filePath) const {
  return editorForFile(filePath);
}

void EditorWidget::setEditorSettings(int tabSize, bool insertSpaces) {
  tabSize_ = std::clamp(tabSize, 1, 8);
  insertSpaces_ = insertSpaces;

  for (int i = 0; i < tabs_->count(); ++i) {
    if (auto* e = qobject_cast<CodeEditor*>(tabs_->widget(i))) {
      e->setEditorSettings(tabSize_, insertSpaces_);
    } else if (auto* pe = qobject_cast<QPlainTextEdit*>(tabs_->widget(i))) {
      pe->setTabStopDistance(
          tabSize_ * pe->fontMetrics().horizontalAdvance(' '));
    }
  }
}

void EditorWidget::setWordWrapEnabled(bool enabled) {
  wordWrapEnabled_ = enabled;
  if (!tabs_) {
    return;
  }
  const auto mode = wordWrapEnabled_ ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap;
  for (int i = 0; i < tabs_->count(); ++i) {
    if (auto* pe = qobject_cast<QPlainTextEdit*>(tabs_->widget(i))) {
      pe->setLineWrapMode(mode);
    }
  }
}

bool EditorWidget::wordWrapEnabled() const {
  return wordWrapEnabled_;
}

void EditorWidget::setEditorFont(QFont font) {
  if (font.family().trimmed().isEmpty()) {
    return;
  }
  editorFont_ = std::move(font);
  if (!tabs_) {
    return;
  }
  for (int i = 0; i < tabs_->count(); ++i) {
    auto* pe = qobject_cast<QPlainTextEdit*>(tabs_->widget(i));
    if (!pe) {
      continue;
    }
    pe->setFont(editorFont_);
    if (zoomSteps_ > 0) {
      pe->zoomIn(zoomSteps_);
    } else if (zoomSteps_ < 0) {
      pe->zoomOut(-zoomSteps_);
    }
    if (auto* e = qobject_cast<CodeEditor*>(pe)) {
      e->setEditorSettings(tabSize_, insertSpaces_);
    } else {
      pe->setTabStopDistance(
          tabSize_ * pe->fontMetrics().horizontalAdvance(' '));
    }
  }
}

QFont EditorWidget::editorFont() const {
  return editorFont_;
}

void EditorWidget::setTheme(bool isDark) {
  if (!tabs_) {
    return;
  }
  for (int i = 0; i < tabs_->count(); ++i) {
    if (auto* pe = qobject_cast<QPlainTextEdit*>(tabs_->widget(i))) {
      if (auto* doc = pe->document()) {
        if (auto* highlighter = doc->findChild<CppHighlighter*>()) {
          highlighter->setTheme(isDark);
        }
      }
    }
  }
}

void EditorWidget::setShowIndentGuides(bool enabled) {
  showIndentGuides_ = enabled;
  if (!tabs_) {
    return;
  }
  for (int i = 0; i < tabs_->count(); ++i) {
    if (auto* e = qobject_cast<CodeEditor*>(tabs_->widget(i))) {
      e->setShowIndentGuides(showIndentGuides_);
    }
  }
}

bool EditorWidget::showIndentGuides() const {
  return showIndentGuides_;
}

void EditorWidget::setShowWhitespace(bool enabled) {
  showWhitespace_ = enabled;
  if (!tabs_) {
    return;
  }
  for (int i = 0; i < tabs_->count(); ++i) {
    if (auto* e = qobject_cast<CodeEditor*>(tabs_->widget(i))) {
      e->setShowWhitespace(showWhitespace_);
    } else if (auto* pe = qobject_cast<QPlainTextEdit*>(tabs_->widget(i))) {
      if (auto* doc = pe->document()) {
        QTextOption opt = doc->defaultTextOption();
        QTextOption::Flags flags = opt.flags();
        if (showWhitespace_) {
          flags |= QTextOption::ShowTabsAndSpaces;
        } else {
          flags &= ~QTextOption::ShowTabsAndSpaces;
        }
        opt.setFlags(flags);
        doc->setDefaultTextOption(opt);
      }
      pe->viewport()->update();
    }
  }
}

bool EditorWidget::showWhitespace() const {
  return showWhitespace_;
}

void EditorWidget::setAutosaveEnabled(bool enabled) {
  autosaveEnabled_ = enabled;
  updateAutosaveTimer();
}

bool EditorWidget::autosaveEnabled() const {
  return autosaveEnabled_;
}

void EditorWidget::setAutosaveIntervalSeconds(int seconds) {
  autosaveIntervalSec_ = std::clamp(seconds, 5, 600);
  updateAutosaveTimer();
}

int EditorWidget::autosaveIntervalSeconds() const {
  return autosaveIntervalSec_;
}

void EditorWidget::applyZoomDelta(int deltaSteps) {
  if (!tabs_ || deltaSteps == 0) {
    return;
  }
  const int next = std::clamp(zoomSteps_ + deltaSteps, -10, 20);
  const int appliedDelta = next - zoomSteps_;
  if (appliedDelta == 0) {
    return;
  }
  zoomSteps_ = next;

  for (int i = 0; i < tabs_->count(); ++i) {
    if (auto* pe = qobject_cast<QPlainTextEdit*>(tabs_->widget(i))) {
      if (appliedDelta > 0) {
        pe->zoomIn(appliedDelta);
      } else {
        pe->zoomOut(-appliedDelta);
      }
      if (auto* e = qobject_cast<CodeEditor*>(pe)) {
        e->setEditorSettings(tabSize_, insertSpaces_);
      } else {
        pe->setTabStopDistance(
            tabSize_ * pe->fontMetrics().horizontalAdvance(' '));
      }
    }
  }
}

void EditorWidget::resetZoom() {
  if (zoomSteps_ == 0) {
    return;
  }
  applyZoomDelta(-zoomSteps_);
}

int EditorWidget::zoomSteps() const {
  return zoomSteps_;
}

void EditorWidget::applyAppearance(QPlainTextEdit* editor) {
  if (!editor) {
    return;
  }
  editor->setFont(editorFont_);
  editor->setLineWrapMode(wordWrapEnabled_ ? QPlainTextEdit::WidgetWidth
                                          : QPlainTextEdit::NoWrap);
  if (zoomSteps_ > 0) {
    editor->zoomIn(zoomSteps_);
  } else if (zoomSteps_ < 0) {
    editor->zoomOut(-zoomSteps_);
  }
  if (auto* e = qobject_cast<CodeEditor*>(editor)) {
    e->setEditorSettings(tabSize_, insertSpaces_);
    e->setShowIndentGuides(showIndentGuides_);
    e->setShowWhitespace(showWhitespace_);
  } else {
    editor->setTabStopDistance(
        tabSize_ * editor->fontMetrics().horizontalAdvance(' '));
  }
}

int EditorWidget::tabSize() const {
  return tabSize_;
}

bool EditorWidget::insertSpaces() const {
  return insertSpaces_;
}

bool EditorWidget::findNext(const QString& text) {
  return find(text, {});
}

bool EditorWidget::find(const QString& text, QTextDocument::FindFlags flags) {
  if (text.isEmpty()) {
    return false;
  }
  auto* editor = currentEditor();
  if (!editor) {
    return false;
  }
  if (editor->find(text, flags)) {
    return true;
  }

  // Wrap-around
  QTextCursor c = editor->textCursor();
  if (flags & QTextDocument::FindBackward) {
    c.movePosition(QTextCursor::End);
  } else {
    c.movePosition(QTextCursor::Start);
  }
  editor->setTextCursor(c);
  return editor->find(text, flags);
}

bool EditorWidget::replaceOne(const QString& findText, const QString& replaceText) {
  return replaceOne(findText, replaceText, {});
}

bool EditorWidget::replaceOne(const QString& findText,
                              const QString& replaceText,
                              QTextDocument::FindFlags flags) {
  if (findText.isEmpty()) {
    return false;
  }
  auto* editor = currentEditor();
  if (!editor) {
    return false;
  }

  auto selectionMatches = [&] {
    QTextCursor cursor = editor->textCursor();
    if (!cursor.hasSelection()) {
      return false;
    }
    const QString selected = cursor.selectedText();
    if (flags & QTextDocument::FindCaseSensitively) {
      return selected == findText;
    }
    return selected.compare(findText, Qt::CaseInsensitive) == 0;
  };

  if (!selectionMatches()) {
    if (!find(findText, flags)) {
      return false;
    }
  }

  QTextCursor cursor = editor->textCursor();
  if (!cursor.hasSelection()) {
    return false;
  }
  cursor.insertText(replaceText);
  return true;
}

int EditorWidget::replaceAll(const QString& findText,
                             const QString& replaceText,
                             QTextDocument::FindFlags flags) {
  if (findText.isEmpty()) {
    return 0;
  }
  auto* editor = currentEditor();
  if (!editor || !editor->document()) {
    return 0;
  }

  QTextDocument::FindFlags searchFlags = flags;
  searchFlags &= ~QTextDocument::FindBackward;

  const QTextCursor original = editor->textCursor();

  QTextCursor edit(editor->document());
  edit.beginEditBlock();

  int count = 0;
  QTextCursor found = editor->document()->find(findText, 0, searchFlags);
  while (!found.isNull()) {
    found.insertText(replaceText);
    ++count;
    const int from = found.position();
    found = editor->document()->find(findText, from, searchFlags);
    if (count > 200000) {
      break;
    }
  }

  edit.endEditBlock();
  editor->setTextCursor(original);
  return count;
}

QPlainTextEdit* EditorWidget::currentEditor() const {
  return qobject_cast<QPlainTextEdit*>(tabs_->currentWidget());
}

QPlainTextEdit* EditorWidget::editorForFile(const QString& filePath) const {
  for (int i = 0; i < tabs_->count(); ++i) {
    auto* editor = qobject_cast<QPlainTextEdit*>(tabs_->widget(i));
    if (editor && filePathFor(editor) == filePath) {
      return editor;
    }
  }
  return nullptr;
}

QString EditorWidget::filePathFor(QPlainTextEdit* editor) const {
  return editor ? editor->property("filePath").toString() : QString{};
}

void EditorWidget::setFilePathFor(QPlainTextEdit* editor, const QString& filePath) {
  if (editor) {
    editor->setProperty("filePath", filePath);
  }
}

void EditorWidget::updateTabTitle(QPlainTextEdit* editor) {
  const int index = tabs_->indexOf(editor);
  if (index < 0) {
    return;
  }
  const QString path = filePathFor(editor);
  const QString base =
      path.isEmpty() ? QStringLiteral("Untitled") : QFileInfo(path).fileName();
  const QString title = editor->document()->isModified() ? base + "*" : base;
  tabs_->setTabText(index, title);
  if (!path.isEmpty()) {
    tabs_->setTabToolTip(index, path);
  }
}

bool EditorWidget::closeTabAt(int index) {
  if (!tabs_ || index < 0 || index >= tabs_->count()) {
    return false;
  }

  auto* editor = qobject_cast<QPlainTextEdit*>(tabs_->widget(index));
  if (!editor) {
    tabs_->removeTab(index);
    if (tabs_->count() == 0) {
      auto* untitled = new CodeEditor(tabs_);
      untitled->setEditorSettings(tabSize_, insertSpaces_);
      (void)new CppHighlighter(untitled->document());
      applyAppearance(untitled);
      setFilePathFor(untitled, {});
      untitled->setProperty("lineEnding", defaultLineEndingPreference());
      tabs_->addTab(untitled, "Untitled");
    }
    return true;
  }

  if (editor->document() && editor->document()->isModified()) {
    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(tr("Unsaved Changes"));

    const QString path = filePathFor(editor);
    const QString name = path.isEmpty() ? tr("Untitled") : QFileInfo(path).fileName();
    box.setText(tr("Save changes to '%1'?").arg(name));
    box.setInformativeText(tr("Your changes will be lost if you don't save them."));

    box.setStandardButtons(QMessageBox::Save | QMessageBox::Discard |
                           QMessageBox::Cancel);
    box.setDefaultButton(QMessageBox::Save);

    const int choice = box.exec();
    if (choice == QMessageBox::Cancel) {
      return false;
    }
    if (choice == QMessageBox::Save) {
      if (!promptAndSaveEditor(editor)) {
        return false;
      }
    }
  }

  const QString closingPath = filePathFor(editor);
  if (!closingPath.trimmed().isEmpty()) {
    closedFileStack_.removeAll(closingPath);
    closedFileStack_.push_back(closingPath);
    const int maxClosed = 50;
    while (closedFileStack_.size() > maxClosed) {
      closedFileStack_.pop_front();
    }
  }
  tabs_->removeTab(index);
  editor->deleteLater();
  if (!closingPath.isEmpty()) {
    unwatchFilePath(closingPath);
    emit documentClosed(closingPath);
  }
  if (tabs_->count() == 0) {
    auto* untitled = new CodeEditor(tabs_);
    untitled->setEditorSettings(tabSize_, insertSpaces_);
    (void)new CppHighlighter(untitled->document());
    applyAppearance(untitled);
    setFilePathFor(untitled, {});
    untitled->setProperty("lineEnding", defaultLineEndingPreference());
    tabs_->addTab(untitled, "Untitled");
  }
  return true;
}

bool EditorWidget::saveEditorAs(QPlainTextEdit* editor, const QString& filePath) {
  if (!editor || filePath.trimmed().isEmpty()) {
    return false;
  }

  const QString absPath = QFileInfo(filePath).absoluteFilePath();
  QDir().mkpath(QFileInfo(absPath).absolutePath());

  QFile file(absPath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return false;
  }

  const QString lineEnding = editor->property("lineEnding").toString();
  QString text = editor->toPlainText();
  if (trimTrailingWhitespacePreference()) {
    text = trimTrailingWhitespace(std::move(text));
  }
  const QByteArray data = applyLineEnding(std::move(text), lineEnding);
  const QByteArray diskHash = QCryptographicHash::hash(data, QCryptographicHash::Sha1);
  if (file.write(data) != data.size()) {
    return false;
  }
  file.close();

  const QString oldPath = filePathFor(editor);
  editor->document()->setModified(false);
  setFilePathFor(editor, absPath);
  editor->setProperty(kPropSuppressNextDiskEvent, true);
  updateDiskMTimeProperty(editor, absPath);
  editor->setProperty(kPropDiskHash, diskHash);
  updateTabTitle(editor);

  if (!oldPath.isEmpty() && QFileInfo(oldPath).absoluteFilePath() != absPath) {
    unwatchFilePath(oldPath);
    emit documentClosed(oldPath);
    emit breakpointsChanged(oldPath, {});
  }
  if (QFileInfo(oldPath).absoluteFilePath() != absPath) {
    emit documentOpened(absPath, editor->toPlainText());
    if (auto* ce = qobject_cast<CodeEditor*>(editor)) {
      emit breakpointsChanged(absPath, ce->breakpoints());
    }
  }

  watchFilePath(absPath);
  return true;
}

bool EditorWidget::closeCurrentTab() {
  if (!tabs_) {
    return false;
  }
  const int idx = tabs_->currentIndex();
  if (idx < 0) {
    return false;
  }
  return closeTabAt(idx);
}

bool EditorWidget::closeAllTabs() {
  if (!tabs_) {
    return false;
  }
  for (int i = tabs_->count() - 1; i >= 0; --i) {
    if (!closeTabAt(i)) {
      return false;
    }
  }
  return true;
}

bool EditorWidget::reopenLastClosedTab() {
  while (!closedFileStack_.isEmpty()) {
    const QString filePath = closedFileStack_.takeLast();
    if (filePath.trimmed().isEmpty()) {
      continue;
    }
    if (!QFileInfo(filePath).isFile()) {
      continue;
    }
    if (openFile(filePath)) {
      return true;
    }
  }
  return false;
}

void EditorWidget::updateAutosaveTimer() {
  if (!autosaveTimer_) {
    return;
  }
  autosaveTimer_->stop();
  if (!autosaveEnabled_) {
    return;
  }
  autosaveTimer_->setInterval(std::clamp(autosaveIntervalSec_, 5, 600) * 1000);
  autosaveTimer_->start();
}

void EditorWidget::maybeAutosave() {
  if (!autosaveEnabled_ || !tabs_) {
    return;
  }

  for (int i = 0; i < tabs_->count(); ++i) {
    auto* editor = qobject_cast<QPlainTextEdit*>(tabs_->widget(i));
    if (!editor || !editor->document() || !editor->document()->isModified()) {
      continue;
    }
    const QString path = filePathFor(editor).trimmed();
    if (path.isEmpty()) {
      // Avoid unexpected Save As prompts during autosave.
      continue;
    }

    const bool ok = saveEditorAs(editor, path);
    if (ok) {
      autosaveErrorNotified_.remove(path);
      continue;
    }

    if (!autosaveErrorNotified_.contains(path)) {
      autosaveErrorNotified_.insert(path);
      emit autosaveFailed(path);
    }
  }
}

bool EditorWidget::promptAndSaveEditor(QPlainTextEdit* editor) {
  if (!editor) {
    return false;
  }

  QString path = filePathFor(editor);
  if (path.trimmed().isEmpty()) {
    QString initialDir = defaultSaveDir_;
    if (initialDir.trimmed().isEmpty()) {
      initialDir = QDir::homePath();
    }
    if (QFileInfo(initialDir).isDir()) {
      initialDir = QDir(initialDir).absoluteFilePath(QStringLiteral("Untitled.ino"));
    }
    const QString chosen = QFileDialog::getSaveFileName(
        this, tr("Save File"), initialDir,
        tr("Rewritto Sketch (*.ino);;C/C++ Files (*.c *.cc *.cpp *.cxx *.h *.hh *.hpp *.hxx);;All Files (*)"));
    if (chosen.isEmpty()) {
      return false;
    }
    path = chosen;
  }

  if (!saveEditorAs(editor, path)) {
    QMessageBox::warning(this, tr("Save Failed"), tr("Could not save file."));
    return false;
  }
  return true;
}

bool EditorWidget::openLocation(const QString& filePath, int line, int column) {
  if (filePath.isEmpty() || line <= 0) {
    return false;
  }

  if (!openFile(filePath)) {
    return false;
  }

  auto* editor = editorForFile(filePath);
  if (!editor) {
    return false;
  }

  auto* doc = editor->document();
  const QTextBlock block = doc->findBlockByNumber(line - 1);
  if (!block.isValid()) {
    return false;
  }

  int pos = block.position();
  if (column > 0) {
    pos += column - 1;
    pos = std::min(pos, block.position() + block.length() - 1);
  }

  QTextCursor cursor(doc);
  cursor.setPosition(pos);
  editor->setTextCursor(cursor);
  editor->centerCursor();

  if (auto* ce = qobject_cast<CodeEditor*>(editor)) {
    ce->setNavigationLineHighlight(cursor);
  }
  return true;
}

bool EditorWidget::goToLine(int line, int column) {
  if (line <= 0) {
    return false;
  }

  auto* editor = currentEditor();
  if (!editor) {
    return false;
  }

  auto* doc = editor->document();
  const QTextBlock block = doc->findBlockByNumber(line - 1);
  if (!block.isValid()) {
    return false;
  }

  int pos = block.position();
  if (column > 0) {
    pos += column - 1;
    pos = std::min(pos, block.position() + block.length() - 1);
  }

  QTextCursor cursor(doc);
  cursor.setPosition(pos);
  editor->setTextCursor(cursor);
  editor->centerCursor();

  if (auto* ce = qobject_cast<CodeEditor*>(editor)) {
    ce->setNavigationLineHighlight(cursor);
  }
  return true;
}

QVector<QString> EditorWidget::openedFiles() const {
  QVector<QString> files;
  for (int i = 0; i < tabs_->count(); ++i) {
    auto* editor = qobject_cast<QPlainTextEdit*>(tabs_->widget(i));
    if (!editor) {
      continue;
    }
    const QString path = filePathFor(editor);
    if (!path.isEmpty()) {
      files.push_back(path);
    }
  }
  return files;
}

QString EditorWidget::textForFile(const QString& filePath) const {
  auto* editor = editorForFile(filePath);
  return editor ? editor->toPlainText() : QString{};
}

void EditorWidget::setDiagnostics(const QString& filePath, const QVector<CodeEditor::Diagnostic>& diagnostics) {
  auto* editor = qobject_cast<CodeEditor*>(editorForFile(filePath));
  if (editor) {
    editor->setDiagnostics(diagnostics);
  }
}

void EditorWidget::clearDiagnostics(const QString& filePath) {
  auto* editor = qobject_cast<CodeEditor*>(editorForFile(filePath));
  if (editor) {
    editor->clearDiagnostics();
  }
}

void EditorWidget::clearAllDiagnostics() {
  for (int i = 0; i < tabs_->count(); ++i) {
    auto* editor = qobject_cast<CodeEditor*>(tabs_->widget(i));
    if (editor) {
      editor->clearDiagnostics();
    }
  }
}
