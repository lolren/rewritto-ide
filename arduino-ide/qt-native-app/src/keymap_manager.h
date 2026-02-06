#pragma once

#include <QHash>
#include <QKeySequence>
#include <QObject>
#include <QString>
#include <QShortcut>
#include <QVector>

class QWidget;
class QAction;

struct KeymapEntry final {
  QString id;
  QString displayName;
  QString category;
  QKeySequence defaultSequence;
  QKeySequence userSequence;
  bool isEditable = true;
};

class KeymapManager : public QObject {
  Q_OBJECT

 public:
  explicit KeymapManager(QObject* parent = nullptr);
  ~KeymapManager() override;

  // Initialize with default keybindings
  void initialize();

  // Get all registered keymap entries
  QVector<KeymapEntry> entries() const;

  // Get entries by category
  QVector<KeymapEntry> entriesForCategory(const QString& category) const;

  // Get all categories
  QVector<QString> categories() const;

  // Register an action with a keymap entry
  void registerAction(const QString& id, const QString& displayName,
                     const QString& category, const QKeySequence& defaultSequence,
                     QAction* action);

  // Apply keybindings to all registered actions
  void applyKeybindings();

  // Set a custom keybinding
  bool setKeybinding(const QString& id, const QKeySequence& sequence);

  // Reset a keybinding to default
  void resetKeybinding(const QString& id);

  // Reset all keybindings to default
  void resetAllKeybindings();

  // Check for conflicts with existing keybindings
  QString findConflict(const QString& id, const QKeySequence& sequence) const;

  // Export keymap to file
  bool exportToFile(const QString& filePath) const;

  // Import keymap from file
  bool importFromFile(const QString& filePath);

  // Save keymap to settings
  void saveToSettings();

  // Load keymap from settings
  void loadFromSettings();

 signals:
  void keybindingChanged(const QString& id, const QKeySequence& sequence);
  void keybindingReset(const QString& id);

 private:
  struct ActionInfo final {
    QAction* action = nullptr;
    QString id;
  };

  QHash<QString, KeymapEntry> entries_;
  QHash<QAction*, ActionInfo> actionsById_;
};
