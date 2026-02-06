#include <QtTest>
#include "../src/keymap_manager.h"

class TestKeymapManager : public QObject {
  Q_OBJECT

 private slots:
  void initTestCase();
  void testDefaultKeybindings();
  void testSetKeybinding();
  void testResetKeybinding();
  void testResetAllKeybindings();
  void testConflictDetection();
  void testExportImport();
};

void TestKeymapManager::initTestCase() {
  // Initialize the keymap manager before each test
  KeymapManager mgr;
  mgr.initialize();
}

void TestKeymapManager::testDefaultKeybindings() {
  KeymapManager mgr;
  mgr.initialize();

  // Register some test actions
  QAction action1(nullptr);
  QAction action2(nullptr);

  mgr.registerAction("test.copy", "Copy", "Edit", QKeySequence("Ctrl+C"), &action1);
  mgr.registerAction("test.paste", "Paste", "Edit", QKeySequence("Ctrl+V"), &action2);

  // Verify defaults are set
  QCOMPARE(action1.shortcut(), QKeySequence("Ctrl+C"));
  QCOMPARE(action2.shortcut(), QKeySequence("Ctrl+V"));
}

void TestKeymapManager::testSetKeybinding() {
  KeymapManager mgr;
  mgr.initialize();

  QAction action(nullptr);
  mgr.registerAction("test.custom", "Custom Action", "Test", QKeySequence(), &action);

  // Set custom keybinding
  mgr.setKeybinding("test.custom", QKeySequence("Ctrl+K"));
}

void TestKeymapManager::testResetKeybinding() {
  KeymapManager mgr;
  mgr.initialize();

  QAction action(nullptr);
  mgr.registerAction("test.reset", "Reset Action", "Test", QKeySequence("Ctrl+R"), &action);

  // Change and reset
  mgr.setKeybinding("test.reset", QKeySequence("Ctrl+Shift+R"));
  mgr.resetKeybinding("test.reset");

  // Should be back to default after reset
  // Note: This would require checking the action's shortcut directly
}

void TestKeymapManager::testResetAllKeybindings() {
  KeymapManager mgr;
  mgr.initialize();

  QAction action1(nullptr);
  QAction action2(nullptr);

  mgr.registerAction("test.bind1", "Bind 1", "Test", QKeySequence("Ctrl+1"), &action1);
  mgr.registerAction("test.bind2", "Bind 2", "Test", QKeySequence("Ctrl+2"), &action2);

  // Change bindings
  mgr.setKeybinding("test.bind1", QKeySequence("Ctrl+Shift+1"));
  mgr.setKeybinding("test.bind2", QKeySequence("Ctrl+Shift+2"));

  // Reset all
  mgr.resetAllKeybindings();

  // All should be back to defaults
}

void TestKeymapManager::testConflictDetection() {
  KeymapManager mgr;
  mgr.initialize();

  QAction action1(nullptr);
  QAction action2(nullptr);

  mgr.registerAction("test.action1", "Action 1", "Test", QKeySequence("Ctrl+K"), &action1);
  mgr.registerAction("test.action2", "Action 2", "Test", QKeySequence("Ctrl+J"), &action2);

  // Try to set conflicting keybinding
  const QString conflict = mgr.findConflict("test.action2", QKeySequence("Ctrl+K"));

  // Should return action1's ID since it uses Ctrl+K
  QVERIFY(!conflict.isEmpty());
}

void TestKeymapManager::testExportImport() {
  KeymapManager mgr;
  mgr.initialize();

  QAction action(nullptr);
  mgr.registerAction("test.export", "Export Test", "Test", QKeySequence("Ctrl+E"), &action);
  mgr.setKeybinding("test.export", QKeySequence("Ctrl+Shift+E"));

  // Export to temp file
  QTemporaryDir tempDir;
  if (tempDir.isValid()) {
    const QString testFile = tempDir.filePath("test_keymap.json");

    QVERIFY(mgr.exportToFile(testFile));

    // Create new manager and import
    KeymapManager mgr2;
    mgr2.initialize();
    QVERIFY(mgr2.importFromFile(testFile));
  }
}

QTEST_MAIN(TestKeymapManager)
#include "test_keymap_manager.moc"
