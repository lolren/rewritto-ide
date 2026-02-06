#include "code_snapshot_store.h"

#include <algorithm>
#include <utility>

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QUuid>

namespace {
QString normalizeRelativePath(QString rel) {
  rel = rel.trimmed();
  rel.replace('\\', '/');
  rel = QDir::cleanPath(rel);
  if (rel == QStringLiteral(".")) {
    return {};
  }
  while (rel.startsWith(QStringLiteral("./"))) {
    rel = rel.mid(2);
  }
  return rel;
}

bool isSafeRelativePath(const QString& relPath) {
  const QString rel = normalizeRelativePath(relPath);
  if (rel.isEmpty()) {
    return false;
  }
  if (QDir::isAbsolutePath(rel)) {
    return false;
  }
  if (rel == QStringLiteral("..") || rel.startsWith(QStringLiteral("../")) ||
      rel.contains(QStringLiteral("/../")) || rel.endsWith(QStringLiteral("/.."))) {
    return false;
  }
  return true;
}

bool shouldIgnoreRelativePath(const QString& relPath) {
  const QString rel = normalizeRelativePath(relPath);
  if (rel.isEmpty()) {
    return true;
  }
  if (rel == QStringLiteral(".rewritto") || rel.startsWith(QStringLiteral(".rewritto/"))) {
    return true;
  }
  if (rel == QStringLiteral(".git") || rel.startsWith(QStringLiteral(".git/"))) {
    return true;
  }
  return false;
}

QString metaPathForSnapshot(const QString& snapshotDir) {
  return QDir(snapshotDir).filePath(QStringLiteral("meta.json"));
}

QString filesRootForSnapshot(const QString& snapshotDir) {
  return QDir(snapshotDir).filePath(QStringLiteral("files"));
}

QString newSnapshotId() {
  const QString timestamp =
      QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"));
  const QString uuid =
      QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
  return timestamp + QStringLiteral("_") + uuid;
}

QJsonObject fileToJson(const CodeSnapshotStore::SnapshotFile& f) {
  QJsonObject o;
  o.insert(QStringLiteral("path"), f.relativePath);
  o.insert(QStringLiteral("sizeBytes"), static_cast<double>(f.sizeBytes));
  o.insert(QStringLiteral("sha1"), f.sha1Hex);
  o.insert(QStringLiteral("permissions"), f.permissions);
  return o;
}

std::optional<CodeSnapshotStore::SnapshotFile> fileFromJson(const QJsonObject& o,
                                                            QString* outError) {
  CodeSnapshotStore::SnapshotFile f;
  f.relativePath = normalizeRelativePath(o.value(QStringLiteral("path")).toString());
  if (!isSafeRelativePath(f.relativePath)) {
    if (outError) {
      *outError = QStringLiteral("Invalid snapshot file path.");
    }
    return std::nullopt;
  }
  f.sizeBytes = static_cast<qint64>(o.value(QStringLiteral("sizeBytes")).toDouble());
  f.sha1Hex = o.value(QStringLiteral("sha1")).toString().trimmed();
  f.permissions = o.value(QStringLiteral("permissions")).toInt();
  return f;
}

QJsonObject metaToJson(const CodeSnapshotStore::SnapshotMeta& meta) {
  QJsonObject o;
  o.insert(QStringLiteral("version"), CodeSnapshotStore::kSnapshotVersion);
  o.insert(QStringLiteral("id"), meta.id);
  o.insert(QStringLiteral("createdAtUtc"), meta.createdAtUtc.toUTC().toString(Qt::ISODateWithMs));
  o.insert(QStringLiteral("comment"), meta.comment);
  o.insert(QStringLiteral("fileCount"), meta.fileCount);
  o.insert(QStringLiteral("totalBytes"), static_cast<double>(meta.totalBytes));
  return o;
}

std::optional<CodeSnapshotStore::SnapshotMeta> metaFromJson(const QJsonObject& o,
                                                            QString* outError) {
  const int version = o.value(QStringLiteral("version")).toInt();
  if (version != CodeSnapshotStore::kSnapshotVersion) {
    if (outError) {
      *outError = QStringLiteral("Unsupported snapshot version.");
    }
    return std::nullopt;
  }

  CodeSnapshotStore::SnapshotMeta meta;
  meta.id = o.value(QStringLiteral("id")).toString().trimmed();
  meta.comment = o.value(QStringLiteral("comment")).toString();
  meta.fileCount = o.value(QStringLiteral("fileCount")).toInt();
  meta.totalBytes = static_cast<qint64>(o.value(QStringLiteral("totalBytes")).toDouble());

  const QString created = o.value(QStringLiteral("createdAtUtc")).toString().trimmed();
  meta.createdAtUtc = QDateTime::fromString(created, Qt::ISODateWithMs).toUTC();
  if (!meta.createdAtUtc.isValid()) {
    meta.createdAtUtc = QDateTime::fromString(created, Qt::ISODate).toUTC();
  }
  if (meta.id.isEmpty() || !meta.createdAtUtc.isValid()) {
    if (outError) {
      *outError = QStringLiteral("Invalid snapshot metadata.");
    }
    return std::nullopt;
  }
  return meta;
}

bool writeJsonFile(const QString& path, const QJsonObject& object, QString* outError) {
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    if (outError) {
      *outError = QStringLiteral("Failed to write snapshot metadata.");
    }
    return false;
  }
  const QByteArray data = QJsonDocument(object).toJson(QJsonDocument::Indented);
  if (file.write(data) != data.size()) {
    if (outError) {
      *outError = QStringLiteral("Failed to write snapshot metadata.");
    }
    return false;
  }
  if (!file.commit()) {
    if (outError) {
      *outError = QStringLiteral("Failed to write snapshot metadata.");
    }
    return false;
  }
  return true;
}

QByteArray sha1Hex(const QByteArray& data) {
  return QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex();
}

bool writeBytesToFile(const QString& path,
                      const QByteArray& bytes,
                      int permissions,
                      QString* outError) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QSaveFile out(path);
  if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    if (outError) {
      *outError = QStringLiteral("Failed to write snapshot file.");
    }
    return false;
  }
  if (out.write(bytes) != bytes.size()) {
    if (outError) {
      *outError = QStringLiteral("Failed to write snapshot file.");
    }
    return false;
  }
  if (!out.commit()) {
    if (outError) {
      *outError = QStringLiteral("Failed to write snapshot file.");
    }
    return false;
  }
  if (permissions != 0) {
    (void)QFile::setPermissions(path, static_cast<QFileDevice::Permissions>(permissions));
  }
  return true;
}

QByteArray readFileBytes(const QString& path, QString* outError) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) {
    if (outError) {
      *outError = QStringLiteral("Failed to read file.");
    }
    return {};
  }
  return f.readAll();
}
}  // namespace

QString CodeSnapshotStore::snapshotsRootForSketch(const QString& sketchFolder) {
  return QDir(sketchFolder).filePath(QStringLiteral(".rewritto/snapshots"));
}

QVector<CodeSnapshotStore::SnapshotMeta> CodeSnapshotStore::listSnapshots(
    const QString& sketchFolder,
    QString* outError) {
  QVector<SnapshotMeta> out;
  const QString root = snapshotsRootForSketch(sketchFolder);
  QDir rootDir(root);
  if (!rootDir.exists()) {
    return out;
  }

  const QStringList snapshotDirs =
      rootDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
  for (const QString& id : snapshotDirs) {
    if (id.startsWith(QStringLiteral(".tmp-"))) {
      continue;
    }
    const QString snapshotDir = rootDir.filePath(id);
    QFile metaFile(metaPathForSnapshot(snapshotDir));
    if (!metaFile.open(QIODevice::ReadOnly)) {
      continue;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(metaFile.readAll());
    if (!doc.isObject()) {
      continue;
    }
    QString err;
    const auto meta = metaFromJson(doc.object(), &err);
    if (!meta) {
      continue;
    }
    out.push_back(*meta);
  }

  std::sort(out.begin(), out.end(), [](const SnapshotMeta& a, const SnapshotMeta& b) {
    return a.createdAtUtc > b.createdAtUtc;
  });

  Q_UNUSED(outError);
  return out;
}

std::optional<CodeSnapshotStore::Snapshot> CodeSnapshotStore::readSnapshot(
    const QString& sketchFolder,
    const QString& id,
    QString* outError) {
  const QString snapshotDir =
      QDir(snapshotsRootForSketch(sketchFolder)).filePath(id);
  QFile metaFile(metaPathForSnapshot(snapshotDir));
  if (!metaFile.open(QIODevice::ReadOnly)) {
    if (outError) {
      *outError = QStringLiteral("Snapshot metadata could not be read.");
    }
    return std::nullopt;
  }
  const QJsonDocument doc = QJsonDocument::fromJson(metaFile.readAll());
  if (!doc.isObject()) {
    if (outError) {
      *outError = QStringLiteral("Snapshot metadata is invalid.");
    }
    return std::nullopt;
  }
  QString err;
  const QJsonObject obj = doc.object();
  auto meta = metaFromJson(obj, &err);
  if (!meta) {
    if (outError) {
      *outError = err.isEmpty() ? QStringLiteral("Snapshot metadata is invalid.") : err;
    }
    return std::nullopt;
  }

  const QJsonArray filesArray = obj.value(QStringLiteral("files")).toArray();
  QVector<SnapshotFile> files;
  files.reserve(filesArray.size());
  for (const QJsonValue& v : filesArray) {
    if (!v.isObject()) {
      continue;
    }
    QString fileErr;
    const auto file = fileFromJson(v.toObject(), &fileErr);
    if (!file) {
      continue;
    }
    files.push_back(*file);
  }

  Snapshot snapshot;
  snapshot.meta = *meta;
  snapshot.files = std::move(files);
  snapshot.meta.fileCount = snapshot.files.size();
  if (snapshot.meta.totalBytes <= 0) {
    qint64 total = 0;
    for (const auto& f : snapshot.files) {
      total += std::max<qint64>(0, f.sizeBytes);
    }
    snapshot.meta.totalBytes = total;
  }
  return snapshot;
}

bool CodeSnapshotStore::createSnapshot(const CreateOptions& options,
                                       SnapshotMeta* outSnapshot,
                                       QString* outError,
                                       ProgressCallback progress) {
  const QString sketchFolder = QDir(options.sketchFolder).absolutePath();
  if (sketchFolder.trimmed().isEmpty() || !QDir(sketchFolder).exists()) {
    if (outError) {
      *outError = QStringLiteral("Sketch folder is not available.");
    }
    return false;
  }

  const QString root = snapshotsRootForSketch(sketchFolder);
  if (!QDir().mkpath(root)) {
    if (outError) {
      *outError = QStringLiteral("Failed to create snapshots folder.");
    }
    return false;
  }

  QDir rootDir(root);
  const QString id = newSnapshotId();
  const QString tmpName =
      QStringLiteral(".tmp-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
  const QString tmpDirPath = rootDir.filePath(tmpName);
  const QString finalDirPath = rootDir.filePath(id);
  if (QDir(finalDirPath).exists()) {
    if (outError) {
      *outError = QStringLiteral("Snapshot already exists.");
    }
    return false;
  }

  if (!QDir().mkpath(filesRootForSnapshot(tmpDirPath))) {
    if (outError) {
      *outError = QStringLiteral("Failed to create snapshot folder.");
    }
    return false;
  }

  QDir sketchDir(sketchFolder);
  QSet<QString> relPathsSet;
  {
    QDirIterator it(sketchFolder,
                    QDir::Files | QDir::Hidden | QDir::NoSymLinks,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
      const QString absPath = it.next();
      const QFileInfo info(absPath);
      if (!info.exists() || !info.isFile() || info.isSymLink()) {
        continue;
      }
      QString rel = normalizeRelativePath(sketchDir.relativeFilePath(absPath));
      if (!isSafeRelativePath(rel) || shouldIgnoreRelativePath(rel)) {
        continue;
      }
      relPathsSet.insert(rel);
    }
  }
  for (auto it = options.fileOverrides.constBegin(); it != options.fileOverrides.constEnd(); ++it) {
    const QString rel = normalizeRelativePath(it.key());
    if (!isSafeRelativePath(rel) || shouldIgnoreRelativePath(rel)) {
      continue;
    }
    relPathsSet.insert(rel);
  }

  QStringList relPaths = relPathsSet.values();
  std::sort(relPaths.begin(), relPaths.end());

  QVector<SnapshotFile> files;
  files.reserve(relPaths.size());
  qint64 totalBytes = 0;

  const QString snapshotFilesRoot = filesRootForSnapshot(tmpDirPath);

  for (int i = 0; i < relPaths.size(); ++i) {
    const QString rel = relPaths.at(i);
    if (progress && !progress(i, relPaths.size(), rel)) {
      QDir(tmpDirPath).removeRecursively();
      if (outError) {
        *outError = QStringLiteral("Snapshot creation cancelled.");
      }
      return false;
    }

    const QString absSourcePath = sketchDir.filePath(rel);
    QByteArray bytes;
    QString err;
    if (options.fileOverrides.contains(rel)) {
      bytes = options.fileOverrides.value(rel);
    } else {
      bytes = readFileBytes(absSourcePath, &err);
      if (!err.isEmpty() && bytes.isEmpty()) {
        QDir(tmpDirPath).removeRecursively();
        if (outError) {
          *outError = QStringLiteral("Failed to read '%1'.").arg(rel);
        }
        return false;
      }
    }

    int perms = 0;
    const QFileInfo sourceInfo(absSourcePath);
    if (sourceInfo.exists()) {
      perms = static_cast<int>(sourceInfo.permissions());
    }

    const QString destPath = QDir(snapshotFilesRoot).filePath(rel);
    if (!writeBytesToFile(destPath, bytes, perms, &err)) {
      QDir(tmpDirPath).removeRecursively();
      if (outError) {
        *outError = QStringLiteral("Failed to write '%1'.").arg(rel);
      }
      return false;
    }

    SnapshotFile f;
    f.relativePath = rel;
    f.sizeBytes = bytes.size();
    f.sha1Hex = QString::fromUtf8(sha1Hex(bytes));
    f.permissions = perms;
    files.push_back(std::move(f));
    totalBytes += bytes.size();
  }
  if (progress) {
    (void)progress(relPaths.size(), relPaths.size(), {});
  }

  SnapshotMeta meta;
  meta.id = id;
  meta.createdAtUtc = QDateTime::currentDateTimeUtc();
  meta.comment = options.comment;
  meta.fileCount = files.size();
  meta.totalBytes = totalBytes;

  QJsonObject metaObj = metaToJson(meta);
  QJsonArray filesArray;
  for (const auto& f : files) {
    filesArray.append(fileToJson(f));
  }
  metaObj.insert(QStringLiteral("files"), filesArray);

  QString metaErr;
  if (!writeJsonFile(metaPathForSnapshot(tmpDirPath), metaObj, &metaErr)) {
    QDir(tmpDirPath).removeRecursively();
    if (outError) {
      *outError = metaErr;
    }
    return false;
  }

  if (!rootDir.rename(tmpName, id)) {
    QDir(tmpDirPath).removeRecursively();
    if (outError) {
      *outError = QStringLiteral("Failed to finalize snapshot.");
    }
    return false;
  }

  if (outSnapshot) {
    *outSnapshot = meta;
  }
  return true;
}

bool CodeSnapshotStore::updateSnapshotComment(const QString& sketchFolder,
                                              const QString& id,
                                              const QString& comment,
                                              QString* outError) {
  const QString snapshotDir =
      QDir(snapshotsRootForSketch(sketchFolder)).filePath(id);
  QFile metaFile(metaPathForSnapshot(snapshotDir));
  if (!metaFile.open(QIODevice::ReadOnly)) {
    if (outError) {
      *outError = QStringLiteral("Snapshot metadata could not be read.");
    }
    return false;
  }
  const QJsonDocument doc = QJsonDocument::fromJson(metaFile.readAll());
  if (!doc.isObject()) {
    if (outError) {
      *outError = QStringLiteral("Snapshot metadata is invalid.");
    }
    return false;
  }
  QJsonObject obj = doc.object();
  obj.insert(QStringLiteral("comment"), comment);

  QString err;
  if (!writeJsonFile(metaPathForSnapshot(snapshotDir), obj, &err)) {
    if (outError) {
      *outError = err;
    }
    return false;
  }
  return true;
}

bool CodeSnapshotStore::deleteSnapshot(const QString& sketchFolder,
                                       const QString& id,
                                       QString* outError) {
  const QString snapshotDir =
      QDir(snapshotsRootForSketch(sketchFolder)).filePath(id);
  QDir dir(snapshotDir);
  if (!dir.exists()) {
    return true;
  }
  if (!dir.removeRecursively()) {
    if (outError) {
      *outError = QStringLiteral("Failed to delete snapshot.");
    }
    return false;
  }
  return true;
}

bool CodeSnapshotStore::restoreSnapshot(const QString& sketchFolder,
                                        const QString& id,
                                        QStringList* outWrittenFiles,
                                        QString* outError,
                                        ProgressCallback progress) {
  QString err;
  const auto snapshot = readSnapshot(sketchFolder, id, &err);
  if (!snapshot) {
    if (outError) {
      *outError = err.isEmpty() ? QStringLiteral("Snapshot could not be read.") : err;
    }
    return false;
  }

  const QString snapshotDir =
      QDir(snapshotsRootForSketch(sketchFolder)).filePath(id);
  const QString snapshotFilesRoot = filesRootForSnapshot(snapshotDir);
  QDir snapshotFilesDir(snapshotFilesRoot);
  if (!snapshotFilesDir.exists()) {
    if (outError) {
      *outError = QStringLiteral("Snapshot files are missing.");
    }
    return false;
  }

  QDir sketchDir(QDir(sketchFolder).absolutePath());
  if (!sketchDir.exists()) {
    if (outError) {
      *outError = QStringLiteral("Sketch folder is not available.");
    }
    return false;
  }

  QStringList written;
  written.reserve(snapshot->files.size());

  for (int i = 0; i < snapshot->files.size(); ++i) {
    const auto& f = snapshot->files.at(i);
    const QString rel = normalizeRelativePath(f.relativePath);
    if (!isSafeRelativePath(rel) || shouldIgnoreRelativePath(rel)) {
      continue;
    }
    if (progress && !progress(i, snapshot->files.size(), rel)) {
      if (outError) {
        *outError = QStringLiteral("Snapshot restore cancelled.");
      }
      return false;
    }

    const QString sourcePath = snapshotFilesDir.filePath(rel);
    const QString destPath = sketchDir.filePath(rel);

    QByteArray bytes = readFileBytes(sourcePath, &err);
    if (!err.isEmpty() && bytes.isEmpty()) {
      if (outError) {
        *outError = QStringLiteral("Failed to read '%1' from snapshot.").arg(rel);
      }
      return false;
    }
    if (!f.sha1Hex.trimmed().isEmpty()) {
      const QString actualSha1 = QString::fromLatin1(sha1Hex(bytes));
      if (actualSha1.compare(f.sha1Hex.trimmed(), Qt::CaseInsensitive) != 0) {
        if (outError) {
          *outError = QStringLiteral("Snapshot integrity check failed for '%1'.").arg(rel);
        }
        return false;
      }
    }

    if (!writeBytesToFile(destPath, bytes, f.permissions, &err)) {
      if (outError) {
        *outError = QStringLiteral("Failed to restore '%1'.").arg(rel);
      }
      return false;
    }
    written.push_back(destPath);
  }

  if (progress) {
    (void)progress(snapshot->files.size(), snapshot->files.size(), {});
  }

  if (outWrittenFiles) {
    *outWrittenFiles = written;
  }
  return true;
}
