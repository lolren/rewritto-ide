#pragma once

#include <QDateTime>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>
#include <optional>

class CodeSnapshotStore final {
 public:
  static constexpr int kSnapshotVersion = 1;

  struct SnapshotFile final {
    QString relativePath;
    qint64 sizeBytes = 0;
    QString sha1Hex;
    int permissions = 0;  // QFileDevice::Permissions serialized as int
  };

  struct SnapshotMeta final {
    QString id;
    QDateTime createdAtUtc;
    QString comment;
    int fileCount = 0;
    qint64 totalBytes = 0;
  };

  struct Snapshot final {
    SnapshotMeta meta;
    QVector<SnapshotFile> files;
  };

  using ProgressCallback =
      std::function<bool(int done, int total, const QString& currentRelativePath)>;

  struct CreateOptions final {
    QString sketchFolder;
    QString comment;
    QHash<QString, QByteArray> fileOverrides;  // relative path -> bytes
  };

  static QString snapshotsRootForSketch(const QString& sketchFolder);
  static QVector<SnapshotMeta> listSnapshots(const QString& sketchFolder,
                                             QString* outError = nullptr);
  static std::optional<Snapshot> readSnapshot(const QString& sketchFolder,
                                              const QString& id,
                                              QString* outError = nullptr);

  static bool createSnapshot(const CreateOptions& options,
                             SnapshotMeta* outSnapshot,
                             QString* outError = nullptr,
                             ProgressCallback progress = {});
  static bool updateSnapshotComment(const QString& sketchFolder,
                                    const QString& id,
                                    const QString& comment,
                                    QString* outError = nullptr);
  static bool deleteSnapshot(const QString& sketchFolder,
                             const QString& id,
                             QString* outError = nullptr);
  static bool restoreSnapshot(const QString& sketchFolder,
                              const QString& id,
                              QStringList* outWrittenFiles = nullptr,
                              QString* outError = nullptr,
                              ProgressCallback progress = {});
};
