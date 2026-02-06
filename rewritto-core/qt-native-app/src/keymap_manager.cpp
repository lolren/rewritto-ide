#include "keymap_manager.h"

#include <QAction>
#include <QApplication>
#include <QFile>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequenceEdit>
#include <QMainWindow>
#include <QMessageBox>
#include <QSettings>
#include <QShortcut>
#include <QTextStream>

namespace {
constexpr auto kSettingsGroup = "KeymapManager";
constexpr auto kKeybindingsKey = "keybindings";

QString keySequenceToString(const QKeySequence& seq) {
  if (seq.isEmpty()) {
    return QString();
  }
  return seq.toString(QKeySequence::PortableText);
}

QKeySequence stringToKeySequence(const QString& str) {
  if (str.isEmpty()) {
    return QKeySequence();
  }
  return QKeySequence::fromString(str, QKeySequence::PortableText);
}
}  // namespace

KeymapManager::KeymapManager(QObject* parent) : QObject(parent) {}

KeymapManager::~KeymapManager() = default;

void KeymapManager::initialize() {
  // Default keybindings will be registered when actions are created
  // via registerAction()
}

QVector<KeymapEntry> KeymapManager::entries() const {
  QVector<KeymapEntry> result;
  result.reserve(entries_.size());
  for (const auto& entry : entries_) {
    result.push_back(entry);
  }
  return result;
}

QVector<KeymapEntry> KeymapManager::entriesForCategory(const QString& category) const {
  QVector<KeymapEntry> result;
  for (const auto& entry : entries_) {
    if (entry.category == category) {
      result.push_back(entry);
    }
  }
  return result;
}

QVector<QString> KeymapManager::categories() const {
  QSet<QString> cats;
  for (const auto& entry : entries_) {
    cats.insert(entry.category);
  }
  return cats.values();
}

void KeymapManager::registerAction(const QString& id, const QString& displayName,
                                  const QString& category,
                                  const QKeySequence& defaultSequence,
                                  QAction* action) {
  if (!action || id.isEmpty()) {
    return;
  }

  KeymapEntry entry;
  entry.id = id;
  entry.displayName = displayName;
  entry.category = category;
  entry.defaultSequence = defaultSequence;
  entry.userSequence = QKeySequence();
  entry.isEditable = true;

  entries_[id] = entry;

  ActionInfo info;
  info.action = action;
  info.id = id;
  actionsById_[action] = info;

  // Apply initial keybinding
  action->setShortcut(defaultSequence);
}

void KeymapManager::applyKeybindings() {
  for (auto it = actionsById_.begin(); it != actionsById_.end(); ++it) {
    const ActionInfo& info = it.value();
    const KeymapEntry& entry = entries_.value(info.id);

    QKeySequence seq = entry.userSequence;
    if (seq.isEmpty()) {
      seq = entry.defaultSequence;
    }

    if (info.action) {
      info.action->setShortcut(seq);
    }
  }
}

bool KeymapManager::setKeybinding(const QString& id, const QKeySequence& sequence) {
  if (!entries_.contains(id)) {
    return false;
  }

  // Check for conflicts
  const QString conflictId = findConflict(id, sequence);
  if (!conflictId.isEmpty() && conflictId != id) {
    const KeymapEntry& conflictEntry = entries_[conflictId];
    // We'll allow the conflict but could warn the user
    // The UI layer should handle conflict resolution
  }

  entries_[id].userSequence = sequence;
  applyKeybindings();

  emit keybindingChanged(id, sequence);
  return true;
}

void KeymapManager::resetKeybinding(const QString& id) {
  if (!entries_.contains(id)) {
    return;
  }

  entries_[id].userSequence = QKeySequence();
  applyKeybindings();

  emit keybindingReset(id);
}

void KeymapManager::resetAllKeybindings() {
  for (auto& entry : entries_) {
    entry.userSequence = QKeySequence();
  }
  applyKeybindings();
}

QString KeymapManager::findConflict(const QString& id,
                                   const QKeySequence& sequence) const {
  if (sequence.isEmpty()) {
    return QString();
  }

  for (const auto& entry : entries_) {
    if (entry.id == id) {
      continue;
    }

    QKeySequence seq = entry.userSequence;
    if (seq.isEmpty()) {
      seq = entry.defaultSequence;
    }

    if (seq == sequence) {
      return entry.id;
    }
  }

  return QString();
}

bool KeymapManager::exportToFile(const QString& filePath) const {
  QJsonObject root;

  QJsonArray bindings;
  for (const auto& entry : entries_) {
    if (!entry.userSequence.isEmpty()) {
      QJsonObject obj;
      obj.insert("id", entry.id);
      obj.insert("sequence", keySequenceToString(entry.userSequence));
      bindings.append(obj);
    }
  }

  root.insert("keybindings", bindings);
  root.insert("version", 1);

  QJsonDocument doc(root);

  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly)) {
    return false;
  }

  file.write(doc.toJson(QJsonDocument::Indented));
  file.close();

  return true;
}

bool KeymapManager::importFromFile(const QString& filePath) {
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }

  const QByteArray data = file.readAll();
  file.close();

  QJsonParseError parseError;
  QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
  if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
    return false;
  }

  QJsonObject root = doc.object();

  // Check version
  const int version = root.value("version").toInt(0);
  if (version < 1) {
    return false;
  }

  QJsonArray bindings = root.value("keybindings").toArray();
  for (const QJsonValue& v : bindings) {
    if (!v.isObject()) {
      continue;
    }

    QJsonObject obj = v.toObject();
    const QString id = obj.value("id").toString();
    const QString seqStr = obj.value("sequence").toString();

    if (entries_.contains(id)) {
      entries_[id].userSequence = stringToKeySequence(seqStr);
    }
  }

  applyKeybindings();
  saveToSettings();

  return true;
}

void KeymapManager::saveToSettings() {
  QSettings settings;
  settings.beginGroup(kSettingsGroup);

  QJsonObject root;
  for (const auto& entry : entries_) {
    if (!entry.userSequence.isEmpty()) {
      root.insert(entry.id, keySequenceToString(entry.userSequence));
    }
  }

  settings.setValue(kKeybindingsKey, QJsonDocument(root).toJson(QJsonDocument::Compact));
  settings.endGroup();
}

void KeymapManager::loadFromSettings() {
  QSettings settings;
  settings.beginGroup(kSettingsGroup);

  const QByteArray data = settings.value(kKeybindingsKey).toByteArray();
  settings.endGroup();

  if (data.isEmpty()) {
    return;
  }

  QJsonParseError parseError;
  QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
  if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
    return;
  }

  QJsonObject root = doc.object();
  for (const QString& id : root.keys()) {
    if (entries_.contains(id)) {
      entries_[id].userSequence = stringToKeySequence(root.value(id).toString());
    }
  }

  applyKeybindings();
}
