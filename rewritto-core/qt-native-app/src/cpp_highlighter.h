#pragma once

#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QTextCharFormat>

class CppHighlighter final : public QSyntaxHighlighter {
  Q_OBJECT

 public:
  explicit CppHighlighter(QTextDocument* parent = nullptr);

  void setTheme(bool isDark);

 protected:
  void highlightBlock(const QString& text) override;

 private:
  struct Rule {
    QRegularExpression pattern;
    QTextCharFormat format;
  };

  QVector<Rule> rules_;

  QRegularExpression multiLineCommentStart_;
  QRegularExpression multiLineCommentEnd_;
  QTextCharFormat multiLineCommentFormat_;
};
