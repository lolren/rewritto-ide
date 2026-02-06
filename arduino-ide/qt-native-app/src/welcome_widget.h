#pragma once

#include <QWidget>

#include <QStringList>

class QListWidget;
class QListWidgetItem;
class QPushButton;

class WelcomeWidget final : public QWidget {
  Q_OBJECT

 public:
  explicit WelcomeWidget(QWidget* parent = nullptr);

  void setPinnedSketches(QStringList folders);
  void setRecentSketches(QStringList folders);

 signals:
  void newSketchRequested();
  void openSketchRequested();
  void openSketchFolderRequested();
  void openSketchSelected(QString folder);
  void pinSketchRequested(QString folder, bool pinned);
  void removeRecentRequested(QString folder);
  void clearPinnedRequested();
  void clearRecentRequested();

 private:
  QPushButton* newSketchButton_ = nullptr;
  QPushButton* openSketchButton_ = nullptr;
  QPushButton* openSketchFolderButton_ = nullptr;

  QListWidget* pinnedList_ = nullptr;
  QPushButton* clearPinnedButton_ = nullptr;
  QListWidget* recentList_ = nullptr;
  QPushButton* clearRecentButton_ = nullptr;

  QStringList pinnedFolders_;
  QStringList recentFolders_;

  void rebuildLists();
  void rebuildList(QListWidget* list, const QStringList& folders, const QString& emptyLabel) const;

  void onListActivated(QListWidgetItem* item);
  void showPinnedContextMenu(const QPoint& pos);
  void showRecentContextMenu(const QPoint& pos);

  static QString displayNameForFolder(const QString& folder);
};

