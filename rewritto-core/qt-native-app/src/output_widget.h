#pragma once

#include <QWidget>

class QPlainTextEdit;

class OutputWidget final : public QWidget {
  Q_OBJECT

 public:
  explicit OutputWidget(QWidget* parent = nullptr);

  void appendText(const QString& text);
  void appendHtml(const QString& html);
  void appendLine(const QString& line);
  void clear();

 private:
  QPlainTextEdit* output_ = nullptr;
};
