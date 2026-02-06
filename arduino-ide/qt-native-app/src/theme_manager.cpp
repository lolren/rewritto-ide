#include "theme_manager.h"

#include <QApplication>
#include <QGuiApplication>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleHints>
#include <QToolTip>

namespace {
bool g_inited = false;
QPalette g_defaultPalette;
QString g_defaultStyleKey;

QString normalizeStyleKey(const QString& key) {
  if (key.trimmed().isEmpty()) {
    return {};
  }
  const QString wanted = key.trimmed();
  const QString wantedLower = wanted.toLower();
  for (const QString& k : QStyleFactory::keys()) {
    if (k.toLower() == wantedLower) {
      return k;
    }
  }
  return {};
}

// Official Arduino IDE 2.x Color Palette
// Header Teal: #00878F (slightly darker than accent for better contrast)
// Accent Teal: #00979C
// Sidebar/Background Dark: #1e1e1e
// Sidebar/Background Light: #f5f5f5
// Activity Bar Dark: #333333
// Activity Bar Light: #e0e0e0
// Selection: #00979C

QString officialArduinoStyle(bool isDark) {
  const QString bg = isDark ? "#1e1e1e" : "#ffffff";
  const QString fg = isDark ? "#d4d4d4" : "#111827";
  const QString sidebarBg = isDark ? "#252526" : "#ffffff";
  const QString activityBarBg = isDark ? "#333333" : "#f3f4f6";
  const QString accent = "#00979C";
  const QString headerBg = "#00878F";
  const QString headerFg = "#ffffff";
  const QString hover = isDark ? "#2a2d2e" : "#f3f4f6";
  const QString border = isDark ? "#3c3c3c" : "#d1d5db";
  const QString separator = isDark ? "#444444" : "#e5e7eb";
  const QString treeSel = isDark ? "rgba(0, 151, 156, 0.35)" : "rgba(0, 151, 156, 0.25)";
  const QString treeFg = isDark ? "#ffffff" : "#111827";
  const QString selectionFg = isDark ? "#ffffff" : "#062326";
  const QString disabledFg = isDark ? "#8b949e" : "#6b7280";
  const QString altBase = isDark ? "#1f2122" : "#f8fafc";

  return QString(R"(
    /* Global Styles */
    QWidget {
        background-color: %1;
        color: %2;
        font-family: "Segoe UI", "Inter", "Roboto", "Fira Sans", sans-serif;
        outline: none;
    }

    QMainWindow {
        background-color: %1;
    }

    QAbstractItemView {
        background-color: %3;
        color: %2;
        alternate-background-color: %15;
        selection-background-color: %5;
        selection-color: %13;
    }

    QMainWindow::separator {
        background: %11;
        width: 1px;
        height: 1px;
    }

    QMainWindow::separator:hover {
        background: %9;
        width: 3px;
        height: 3px;
    }

    /* Toolbar (Header) - Arduino IDE 2.x style */
    QToolBar#HeaderToolBar {
        background-color: %7;
        border: none;
        border-bottom: 1px solid %6;
        padding: 6px 8px;
        spacing: 6px;
    }

    QToolBar#HeaderToolBar QToolButton {
        background: transparent;
        border: none;
        border-radius: 4px;
        padding: 6px 10px;
        color: %8;
        font-weight: 500;
    }

    QToolBar#HeaderToolBar QToolButton:hover {
        background-color: rgba(255, 255, 255, 0.15);
    }

    QToolBar#HeaderToolBar QToolButton:pressed {
        background-color: rgba(255, 255, 255, 0.25);
    }

    QToolBar#HeaderToolBar QToolButton:checked {
        background-color: rgba(255, 255, 255, 0.2);
    }

    QToolBar#HeaderToolBar QLabel {
        color: %8;
        font-weight: 600;
        font-size: 12px;
        margin-left: 4px;
        margin-right: 4px;
    }

    QToolBar#HeaderToolBar QWidget {
        background: transparent;
        color: %8;
    }

    QToolBar#HeaderToolBar QComboBox {
        background-color: rgba(255, 255, 255, 0.15);
        border: 1px solid rgba(255, 255, 255, 0.3);
        border-radius: 4px;
        padding: 4px 24px 4px 8px;
        color: %8;
        min-height: 20px;
    }

    QToolBar#HeaderToolBar QComboBox:hover {
        background-color: rgba(255, 255, 255, 0.2);
        border: 1px solid rgba(255, 255, 255, 0.4);
    }

    QToolBar#HeaderToolBar QComboBox::drop-down {
        border: none;
        width: 24px;
        padding-right: 4px;
        subcontrol-origin: padding;
        subcontrol-position: right top;
        position: absolute;
        top: 2px;
        bottom: 2px;
        right: 2px;
    }

    QToolBar#HeaderToolBar QComboBox::down-arrow {
        image: url(:/icons/arrow-down-light.png);
        width: 10px;
        height: 10px;
    }

    QToolBar#HeaderToolBar QComboBox::drop-down:hover {
        background-color: rgba(255, 255, 255, 0.1);
    }

    QToolBar#HeaderToolBar QComboBox QAbstractItemView {
        background-color: %3;
        border: 1px solid %6;
        selection-background-color: %5;
        selection-color: %13;
        color: %2;
    }

    /* Activity Bar (Left Vertical Strip) */
    QToolBar#ActivityBar {
        background-color: %4;
        border: none;
        border-right: 1px solid %11;
        padding: 0px;
        spacing: 0px;
        min-width: 48px;
        max-width: 48px;
    }

    QToolBar#ActivityBar QToolButton {
        background: transparent;
        border: none;
        padding: 12px;
        border-left: 3px solid transparent;
        icon-size: 24px;
    }

    QToolBar#ActivityBar QToolButton:hover {
        background-color: %9;
    }

    QToolBar#ActivityBar QToolButton:checked {
        background-color: rgba(0, 151, 156, 0.15);
        border-left: 3px solid %5;
    }

    /* Dock Widgets / Sidebar Panels */
    QDockWidget {
        border: none;
        background-color: %3;
    }

    QDockWidget::title {
        background-color: %4;
        padding: 8px 12px;
        text-transform: none;
        font-weight: 600;
        font-size: 13px;
        color: %2;
        border-bottom: 1px solid %11;
    }

    QDockWidget::close-button, QDockWidget::float-button {
        background: transparent;
        border: none;
        padding: 4px;
        icon-size: 16px;
        border-radius: 4px;
        min-width: 24px;
        min-height: 24px;
        width: 24px;
        height: 24px;
    }

    QDockWidget::close-button:hover, QDockWidget::float-button:hover {
        background: %9;
    }

    QDockWidget::close-button:pressed, QDockWidget::float-button:pressed {
        background: rgba(0, 151, 156, 0.25);
    }

    /* Tabs */
    QTabWidget::pane {
        border: none;
        border-top: 1px solid %11;
        background-color: %3;
    }

    QTabBar::tab {
        background-color: transparent;
        color: %2;
        padding: 10px 16px;
        border: none;
        border-right: 1px solid %11;
        min-width: 100px;
        margin-top: 0px;
    }

    QTabBar::tab:selected {
        background-color: %1;
        color: %2;
        border-top: 3px solid %5;
        font-weight: 600;
    }

    QTabBar::tab:hover:!selected {
        background-color: %9;
        color: %2;
    }

    QTabBar::tab:first {
        border-left: none;
    }

    QTabBar::tab:last {
        border-right: none;
    }

    /* Editor Tabs */
    QTabWidget#EditorTabs QTabBar::tab {
        background-color: %3;
        border-top: none;
        border-bottom: none;
        border-right: 1px solid %11;
        padding: 8px 16px;
        min-width: 120px;
    }

    QTabWidget#EditorTabs QTabBar::tab:selected {
        background-color: %1;
        border-top: 3px solid %5;
    }

    QTabWidget#EditorTabs QTabBar::close-button {
        background: transparent;
        border: none;
        padding: 2px;
        border-radius: 3px;
    }

    QTabWidget#EditorTabs QTabBar::close-button:hover {
        background: %9;
    }

    /* Tree/List Views */
    QTreeView, QListView {
        background-color: %3;
        border: none;
        alternate-background-color: %3;
        selection-background-color: %10;
        selection-color: %12;
        show-decoration-selected: 1;
    }

    QTreeView::item, QListView::item {
        padding: 6px 8px;
        min-height: 24px;
    }

    QTreeView::item:hover {
        background-color: %9;
    }

    QTreeView::item:selected {
        background-color: %10;
        color: %12;
    }

    QTreeView::branch {
        background: transparent;
    }

    QTreeView::branch:has-children:!has-siblings:closed,
    QTreeView::branch:closed:has-children:has-siblings {
        border-image: none;
        image: url(:/icons/tree-closed.png);
    }

    QTreeView::branch:open:has-children:!has-siblings,
    QTreeView::branch:open:has-children:has-siblings {
        border-image: none;
        image: url(:/icons/tree-open.png);
    }

    /* ScrollBars - Modern Thin Style */
    QScrollBar:vertical {
        background: transparent;
        width: 14px;
        margin: 0px;
    }

    QScrollBar::handle:vertical {
        background: rgba(128, 128, 128, 0.4);
        min-height: 30px;
        border-radius: 7px;
        margin: 2px;
    }

    QScrollBar::handle:vertical:hover {
        background: rgba(128, 128, 128, 0.6);
    }

    QScrollBar::handle:vertical:pressed {
        background: rgba(128, 128, 128, 0.8);
    }

    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
        height: 0px;
    }

    QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
        background: none;
    }

    QScrollBar:horizontal {
        background: transparent;
        height: 14px;
        margin: 0px;
    }

    QScrollBar::handle:horizontal {
        background: rgba(128, 128, 128, 0.4);
        min-width: 30px;
        border-radius: 7px;
        margin: 2px;
    }

    QScrollBar::handle:horizontal:hover {
        background: rgba(128, 128, 128, 0.6);
    }

    QScrollBar::handle:horizontal:pressed {
        background: rgba(128, 128, 128, 0.8);
    }

    QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
        width: 0px;
    }

    QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
        background: none;
    }

    /* Editor (Placeholder for CodeEditor) */
    QPlainTextEdit#OutputTextEdit {
        background-color: %1;
        border: none;
        font-family: "JetBrains Mono", "Cascadia Code", "Consolas", "Menlo", monospace;
        font-size: 12px;
        selection-background-color: %5;
        selection-color: %13;
    }

    QPlainTextEdit {
        selection-background-color: %5;
        selection-color: %13;
    }

    /* Status Bar */
    QStatusBar {
        background-color: %7;
        color: %8;
        border: none;
        border-top: 1px solid rgba(255, 255, 255, 0.1);
        padding: 2px 4px;
    }

    QStatusBar QLabel {
        background: transparent;
        padding: 0 8px;
        color: %8;
    }

    QStatusBar QProgressBar {
        background-color: rgba(0, 0, 0, 0.2);
        border: none;
        border-radius: 3px;
        height: 16px;
        text-align: center;
    }

    QStatusBar QProgressBar::chunk {
        background-color: %8;
        border-radius: 2px;
    }

    /* ComboBox (General) */
    QComboBox {
        background-color: %3;
        border: 1px solid %6;
        border-radius: 4px;
        padding: 4px 24px 4px 8px;
        color: %2;
        min-height: 22px;
    }

    QComboBox:hover {
        border: 1px solid %5;
    }
    QComboBox::drop-down {
        border: none;
        width: 24px;
        padding-right: 4px;
        subcontrol-origin: padding;
        subcontrol-position: right top;
        position: absolute;
        top: 2px;
        bottom: 2px;
        right: 2px;
    }

    QComboBox::down-arrow {
        image: url(:/icons/arrow-down.png);
        width: 10px;
        height: 10px;
    }
    /* Buttons */
    QPushButton {
        background-color: %5;
        color: white;
        border: none;
        border-radius: 4px;
        padding: 6px 16px;
        font-weight: 500;
    }

    QPushButton:hover {
        background-color: %7;
    }

    QPushButton:pressed {
        background-color: %7;
    }

    QPushButton:disabled {
        background-color: %9;
        color: #888;
    }

    /* Text Buttons */
    QPushButton[text-only="true"] {
        background: transparent;
        color: %5;
        border: none;
    }

    QPushButton[text-only="true"]:hover {
        background: %9;
    }

    /* Line Edit */
    QLineEdit {
        background-color: %3;
        border: 1px solid %6;
        border-radius: 4px;
        padding: 4px 8px;
        color: %2;
        selection-background-color: %5;
        selection-color: %13;
    }

    QLineEdit:focus {
        border: 2px solid %5;
        padding: 3px 7px;
    }

    /* Menu */
    QMenu {
        background-color: %3;
        border: 1px solid %6;
        padding: 4px;
    }

    QMenu::item {
        padding: 8px 24px;
        border-radius: 4px;
    }

    QMenu::item:selected {
        background-color: %5;
        color: %13;
    }

    QMenu::item:disabled {
        color: %14;
    }

    QMenu::separator {
        height: 1px;
        background: %11;
        margin: 4px 8px;
    }

    /* Tooltips */
    QToolTip {
        background-color: %1;
        color: %2;
        border: 1px solid %6;
        padding: 4px 8px;
        border-radius: 4px;
    }

    /* Group Box */
    QGroupBox {
        border: 1px solid %6;
        border-radius: 6px;
        margin-top: 12px;
        padding-top: 8px;
        font-weight: 600;
    }

    QGroupBox::title {
        subcontrol-origin: margin;
        left: 12px;
        padding: 0 6px;
    }

    /* Table View */
    QTableView {
        background-color: %3;
        alternate-background-color: %9;
        gridline-color: %11;
        selection-background-color: %5;
        selection-color: %13;
    }

    QTableView::item:selected {
        background-color: %5;
        color: %13;
    }

    QTableView QTableCornerButton::section {
        background-color: %3;
        border: 1px solid %11;
    }

    QTableView QHeaderView::section {
        background-color: %4;
        color: %2;
        padding: 6px 8px;
        border: none;
        border-right: 1px solid %11;
        border-bottom: 1px solid %11;
        font-weight: 600;
    }

    /* CheckBox and RadioButton */
    QCheckBox {
        spacing: 8px;
    }

    QCheckBox::indicator {
        width: 18px;
        height: 18px;
        border: 2px solid %6;
        border-radius: 3px;
        background-color: %3;
    }

    QCheckBox::indicator:checked {
        background-color: %5;
        border-color: %5;
        image: url(:/icons/check.png);
    }

    QCheckBox::indicator:hover {
        border-color: %5;
    }

    QRadioButton::indicator {
        width: 18px;
        height: 18px;
        border: 2px solid %6;
        border-radius: 9px;
        background-color: %3;
    }

    QRadioButton::indicator:checked {
        background-color: %5;
        border-color: %5;
    }

    /* SpinBox */
    QSpinBox, QDoubleSpinBox {
        background-color: %3;
        border: 1px solid %6;
        border-radius: 4px;
        padding: 4px 8px;
        color: %2;
    }

    QSpinBox:focus, QDoubleSpinBox:focus {
        border: 2px solid %5;
    }

    QSpinBox::up-button, QDoubleSpinBox::up-button,
    QSpinBox::down-button, QDoubleSpinBox::down-button {
        background: transparent;
        border: none;
        width: 16px;
    }

    QSpinBox::up-button:hover, QDoubleSpinBox::up-button:hover,
    QSpinBox::down-button:hover, QDoubleSpinBox::down-button:hover {
        background: %9;
    }

    QSpinBox::up-arrow, QDoubleSpinBox::up-arrow {
        image: url(:/icons/arrow-up.png);
    }

    QSpinBox::down-arrow, QDoubleSpinBox::down-arrow {
        image: url(:/icons/arrow-down.png);
    }

    /* Slider */
    QSlider::groove:horizontal {
        height: 6px;
        background: %4;
        border-radius: 3px;
    }

    QSlider::handle:horizontal {
        width: 16px;
        height: 16px;
        background: %5;
        border-radius: 8px;
        margin: -5px 0;
    }

    QSlider::handle:horizontal:hover {
        background: %6;
    }

    /* Welcome Widget */
    QWidget#WelcomeWidget {
        background-color: %1;
    }

    QWidget#WelcomeWidget QLabel {
        color: %2;
        background: transparent;
    }

    QWidget#WelcomeWidget QPushButton {
        background-color: %5;
        color: white;
        border: none;
        border-radius: 4px;
        padding: 10px 20px;
        font-weight: 600;
        min-width: 120px;
    }

    QWidget#WelcomeWidget QPushButton:hover {
        background-color: %6;
    }

    QWidget#WelcomeWidget QPushButton:disabled {
        background-color: %9;
        color: #888;
    }

    QWidget#WelcomeWidget QGroupBox {
        border: 1px solid %6;
        border-radius: 8px;
        margin-top: 12px;
        padding-top: 16px;
        font-weight: 600;
        background-color: %3;
    }

    QWidget#WelcomeWidget QGroupBox::title {
        subcontrol-origin: margin;
        left: 16px;
        padding: 0 8px;
        color: %2;
    }

    QWidget#WelcomeWidget QListWidget {
        background-color: %1;
        border: none;
        border-radius: 4px;
        outline: none;
    }

    QWidget#WelcomeWidget QListWidget::item {
        padding: 10px 12px;
        border-radius: 4px;
        margin: 2px;
    }

    QWidget#WelcomeWidget QListWidget::item:hover {
        background-color: %9;
    }

    QWidget#WelcomeWidget QListWidget::item:selected {
        background-color: %5;
        color: white;
    }

    /* Toast Widget */
    QWidget#ToastWidget {
        background-color: %5;
        color: white;
        border-radius: 8px;
        padding: 16px 24px;
    }

    /* Output Widget */
    QWidget#OutputPanel {
        background-color: %1;
        border: none;
    }

    QPlainTextEdit#OutputTextEdit {
        background-color: %1;
        color: %2;
        border: none;
        selection-background-color: %5;
        selection-color: %13;
    }

    QComboBox:disabled, QLineEdit:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled {
        color: %14;
    }

    QTreeView::item:disabled, QListView::item:disabled, QTableView::item:disabled {
        color: %14;
    }
  )").arg(bg,
          fg,
          sidebarBg,
          activityBarBg,
          accent,
          border,
          headerBg,
          headerFg,
          hover,
          treeSel,
          separator,
          treeFg,
          selectionFg,
          disabledFg,
          altBase);
}

QPalette arduinoPalette(bool isDark) {
  QPalette p;
  if (isDark) {
    p.setColor(QPalette::Window, QColor("#1e1e1e"));
    p.setColor(QPalette::WindowText, QColor("#d4d4d4"));
    p.setColor(QPalette::Base, QColor("#1e1e1e"));
    p.setColor(QPalette::AlternateBase, QColor("#252526"));
    p.setColor(QPalette::ToolTipBase, Qt::white);
    p.setColor(QPalette::ToolTipText, QColor("#111827"));
    p.setColor(QPalette::Text, QColor("#d4d4d4"));
    p.setColor(QPalette::Button, QColor("#252526"));
    p.setColor(QPalette::ButtonText, QColor("#d4d4d4"));
    p.setColor(QPalette::BrightText, Qt::red);
    p.setColor(QPalette::Link, QColor(0, 151, 156));
    p.setColor(QPalette::Highlight, QColor(0, 151, 156));
    p.setColor(QPalette::HighlightedText, Qt::white);
  } else {
    p.setColor(QPalette::Window, QColor("#ffffff"));
    p.setColor(QPalette::WindowText, QColor("#111827"));
    p.setColor(QPalette::Base, QColor("#ffffff"));
    p.setColor(QPalette::AlternateBase, QColor("#f3f4f6"));
    p.setColor(QPalette::ToolTipBase, Qt::white);
    p.setColor(QPalette::ToolTipText, QColor("#111827"));
    p.setColor(QPalette::Text, QColor("#111827"));
    p.setColor(QPalette::Button, QColor("#f3f4f6"));
    p.setColor(QPalette::ButtonText, QColor("#111827"));
    p.setColor(QPalette::Link, QColor(0, 151, 156));
    p.setColor(QPalette::Highlight, QColor(0, 151, 156));
    p.setColor(QPalette::HighlightedText, Qt::white);
  }
  return p;
}

}  // namespace

void ThemeManager::init() {
  if (g_inited) {
    return;
  }
  g_inited = true;
  g_defaultPalette = QApplication::palette();

  const QString styleName = QApplication::style() ? QApplication::style()->objectName() : QString{};
  g_defaultStyleKey = normalizeStyleKey(styleName);
}

void ThemeManager::apply(const QString& theme) {
  init();

  const QString t = theme.trimmed().toLower();

  if (t == "system") {
    bool dark = false;
    if (QGuiApplication::styleHints()) {
      dark = QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    }
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    QApplication::setPalette(arduinoPalette(dark));
    qApp->setStyleSheet(officialArduinoStyle(dark));
    return;
  }

  if (t == "arduino" || t == "light" || t == "y2k") {
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    QApplication::setPalette(arduinoPalette(false));
    qApp->setStyleSheet(officialArduinoStyle(false));
    return;
  }

  if (t == "dark" || t == "oceanic" || t == "cyber") {
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    QApplication::setPalette(arduinoPalette(true));
    qApp->setStyleSheet(officialArduinoStyle(true));
    return;
  }

  // Fallback
  qApp->setStyleSheet(QString{});
  if (!g_defaultStyleKey.isEmpty()) {
    if (QStyle* style = QStyleFactory::create(g_defaultStyleKey)) {
      QApplication::setStyle(style);
    }
  }
  QApplication::setPalette(g_defaultPalette);
}
