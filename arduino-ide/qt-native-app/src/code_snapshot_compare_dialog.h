#pragma once

#include <QByteArray>
#include <QDialog>
#include <QHash>
#include <QString>
#include <QVector>

#include "code_snapshot_store.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QPlainTextEdit;
class QTableWidget;

class CodeSnapshotCompareDialog final : public QDialog {
  Q_OBJECT

 public:
  explicit CodeSnapshotCompareDialog(QString sketchFolder,
                                     QHash<QString, QByteArray> currentFiles,
                                     QWidget* parent = nullptr);

 private:
  struct DiffEntry final {
    QString relativePath;
    QString statusText;
    bool leftExists = false;
    bool rightExists = false;
    int deltaBytes = 0;
    QByteArray leftBytes;
    QByteArray rightBytes;
    bool unchanged = false;
  };

  QString sketchFolder_;
  QHash<QString, QByteArray> currentFiles_;
  QVector<CodeSnapshotStore::SnapshotMeta> snapshots_;
  QHash<QString, QHash<QString, QByteArray>> snapshotFilesById_;
  QVector<DiffEntry> visibleEntries_;

  QComboBox* leftCombo_ = nullptr;
  QComboBox* rightCombo_ = nullptr;
  QCheckBox* showUnchangedCheck_ = nullptr;
  QLabel* summaryLabel_ = nullptr;
  QTableWidget* filesTable_ = nullptr;
  QLabel* leftLabel_ = nullptr;
  QLabel* rightLabel_ = nullptr;
  QPlainTextEdit* leftPreview_ = nullptr;
  QPlainTextEdit* rightPreview_ = nullptr;

  void populateSourceCombos();
  bool loadSnapshotFiles(const QString& snapshotId, QString* outError);
  const QHash<QString, QByteArray>* filesForSourceId(const QString& sourceId,
                                                     QString* outError);
  QString displayLabelForSourceId(const QString& sourceId) const;
  void rebuildComparison();
  void updatePreview();
};

