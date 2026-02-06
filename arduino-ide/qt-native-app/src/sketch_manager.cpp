#include "sketch_manager.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

SketchManager::SketchManager(QObject* parent) : QObject(parent) {}

void SketchManager::openSketchFolder(const QString& folder) {
  const QDir dir(folder);
  if (!dir.exists()) {
    return;
  }
  lastSketchPath_ = dir.absolutePath();
  emit sketchFolderChanged(lastSketchPath_);
}

QString SketchManager::lastSketchPath() const {
  return lastSketchPath_;
}

namespace {
QString primaryInoForSketchFolder(const QString& folder) {
  const QDir dir(folder);
  if (!dir.exists()) {
    return {};
  }

  const QString baseName = QFileInfo(folder).fileName();
  if (!baseName.isEmpty()) {
    const QString primary = dir.absoluteFilePath(baseName + ".ino");
    if (QFileInfo(primary).isFile()) {
      return QFileInfo(primary).absoluteFilePath();
    }
  }

  const QStringList inos =
      dir.entryList(QStringList{"*.ino"}, QDir::Files, QDir::Name | QDir::IgnoreCase);
  if (!inos.isEmpty()) {
    return QFileInfo(dir.absoluteFilePath(inos.first())).absoluteFilePath();
  }
  return {};
}

bool copyFolderRecursivelyWithRename(const QString& sourceFolder,
                                    const QString& destinationFolder,
                                    const QString& sourcePrimaryRel,
                                    const QString& destinationPrimaryRel,
                                    QString* outError) {
  const QDir srcDir(sourceFolder);
  const QDir dstDir(destinationFolder);
  if (!srcDir.exists()) {
    if (outError) {
      *outError = QStringLiteral("Source folder does not exist.");
    }
    return false;
  }
  if (!QDir().mkpath(destinationFolder)) {
    if (outError) {
      *outError = QStringLiteral("Could not create destination folder.");
    }
    return false;
  }

  QDirIterator it(sourceFolder,
                  QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden |
                      QDir::System,
                  QDirIterator::Subdirectories);
  while (it.hasNext()) {
    const QString srcPath = it.next();
    const QFileInfo srcInfo(srcPath);

    const QString rel = QDir(sourceFolder).relativeFilePath(srcPath);
    if (rel.trimmed().isEmpty() || rel == QStringLiteral(".")) {
      continue;
    }

    QString dstRel = rel;
    if (!sourcePrimaryRel.isEmpty() && !destinationPrimaryRel.isEmpty() &&
        QDir::cleanPath(rel) == QDir::cleanPath(sourcePrimaryRel)) {
      dstRel = destinationPrimaryRel;
    }

    const QString dstPath = dstDir.absoluteFilePath(dstRel);

    if (srcInfo.isDir()) {
      if (!QDir().mkpath(dstPath)) {
        if (outError) {
          *outError =
              QStringLiteral("Could not create folder: %1").arg(dstRel);
        }
        return false;
      }
      continue;
    }

    if (!srcInfo.isFile() && !srcInfo.isSymLink()) {
      continue;
    }

    QDir().mkpath(QFileInfo(dstPath).absolutePath());
    if (!QFile::copy(srcPath, dstPath)) {
      if (outError) {
        *outError = QStringLiteral("Could not copy '%1'.").arg(rel);
      }
      return false;
    }
  }

  return true;
}
}  // namespace

bool SketchManager::isSketchFolder(const QString& folder) {
  const QDir dir(folder);
  if (!dir.exists()) {
    return false;
  }
  return !primaryInoForSketchFolder(folder).isEmpty();
}

bool SketchManager::cloneSketchFolder(const QString& sourceFolder,
                                      const QString& destinationParentFolder,
                                      const QString& newSketchName,
                                      QString* outNewFolder,
                                      QString* outError) {
  const QDir srcDir(sourceFolder);
  if (!srcDir.exists()) {
    if (outError) {
      *outError = QStringLiteral("Source folder does not exist.");
    }
    return false;
  }

  QString name = newSketchName.trimmed();
  if (name.isEmpty() || name.contains('/') || name.contains('\\')) {
    if (outError) {
      *outError = QStringLiteral("Invalid sketch name.");
    }
    return false;
  }

  const QString parent = QDir(destinationParentFolder).absolutePath();
  if (parent.trimmed().isEmpty()) {
    if (outError) {
      *outError = QStringLiteral("Destination folder is empty.");
    }
    return false;
  }
  if (!QDir().mkpath(parent)) {
    if (outError) {
      *outError = QStringLiteral("Could not create destination parent folder.");
    }
    return false;
  }

  const QString destFolder = QDir(parent).absoluteFilePath(name);
  if (QFileInfo::exists(destFolder)) {
    if (outError) {
      *outError = QStringLiteral("Destination already exists.");
    }
    return false;
  }

  const QString srcPrimaryAbs = primaryInoForSketchFolder(sourceFolder);
  if (srcPrimaryAbs.isEmpty()) {
    if (outError) {
      *outError = QStringLiteral("Source folder does not contain an .ino file.");
    }
    return false;
  }

  const QString srcPrimaryRel = QDir(sourceFolder).relativeFilePath(srcPrimaryAbs);
  const QString dstPrimaryRel = name + QStringLiteral(".ino");

  QString error;
  if (!copyFolderRecursivelyWithRename(sourceFolder, destFolder, srcPrimaryRel,
                                      dstPrimaryRel, &error)) {
    (void)QDir(destFolder).removeRecursively();
    if (outError) {
      *outError = error.trimmed().isEmpty() ? QStringLiteral("Copy failed.") : error;
    }
    return false;
  }

  if (outNewFolder) {
    *outNewFolder = QDir(destFolder).absolutePath();
  }
  if (outError) {
    outError->clear();
  }
  return true;
}
