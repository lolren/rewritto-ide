#pragma once

#include <QDialog>
#include <QTextDocument>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;

class EditorWidget;

class FindReplaceDialog final : public QDialog {
  Q_OBJECT

 public:
  explicit FindReplaceDialog(EditorWidget* editor, QWidget* parent = nullptr);

  void setReplaceMode(bool enabled);
  bool replaceMode() const;

  QString findText() const;
  QString replaceText() const;

  void setFindText(QString text);
  void setReplaceText(QString text);

  QTextDocument::FindFlags baseFindFlags() const;

  void focusFind();

 signals:
  void findTextChanged(QString text);
  void replaceTextChanged(QString text);
  void findFlagsChanged(QTextDocument::FindFlags flags);

 private:
  EditorWidget* editor_ = nullptr;
  bool replaceMode_ = false;

  QLineEdit* findEdit_ = nullptr;
  QLabel* replaceLabel_ = nullptr;
  QLineEdit* replaceEdit_ = nullptr;
  QCheckBox* caseSensitiveCheck_ = nullptr;
  QCheckBox* wholeWordCheck_ = nullptr;
  QPushButton* findNextButton_ = nullptr;
  QPushButton* findPrevButton_ = nullptr;
  QPushButton* replaceButton_ = nullptr;
  QPushButton* replaceAllButton_ = nullptr;
  QLabel* statusLabel_ = nullptr;

  void buildUi();
  void wireSignals();
  void updateModeUi();

  void doFind(bool backward);
  void doReplaceOne();
  void doReplaceAll();
};
