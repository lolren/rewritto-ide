#pragma once

#include <QObject>
#include <QString>

class SketchManager final : public QObject {
  Q_OBJECT

 public:
  explicit SketchManager(QObject* parent = nullptr);

  void openSketchFolder(const QString& folder);
  QString lastSketchPath() const;

  static bool isSketchFolder(const QString& folder);
  static bool cloneSketchFolder(const QString& sourceFolder,
                                const QString& destinationParentFolder,
                                const QString& newSketchName,
                                QString* outNewFolder = nullptr,
                                QString* outError = nullptr);

 signals:
  void sketchFolderChanged(const QString& folder);

 private:
  QString lastSketchPath_;
};
