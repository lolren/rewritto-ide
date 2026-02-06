#include "cpp_highlighter.h"

#include <QColor>
#include <QTextCharFormat>

namespace {
QTextCharFormat makeFormat(const QColor& color, bool bold = false) {
  QTextCharFormat fmt;
  fmt.setForeground(color);
  if (bold) {
    fmt.setFontWeight(QFont::Bold);
  }
  return fmt;
}
}  // namespace

CppHighlighter::CppHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent) {
  // Keywords (C/C++)
  {
    Rule r;
    r.pattern = QRegularExpression(
        R"(\b(auto|break|case|catch|char|class|const|constexpr|continue|default|delete|do|double|else|enum|explicit|extern|false|float|for|friend|goto|if|inline|int|long|mutable|namespace|new|nullptr|operator|private|protected|public|register|reinterpret_cast|return|short|signed|sizeof|static|struct|switch|template|this|throw|true|try|typedef|typename|union|unsigned|using|virtual|void|volatile|while)\b)");
    r.format = makeFormat(QColor("#569CD6"), true);
    rules_.push_back(r);
  }

  // Sketch identifiers
  {
    Rule r;
    r.pattern = QRegularExpression(
        R"(\b(setup|loop|HIGH|LOW|INPUT|OUTPUT|INPUT_PULLUP|LED_BUILTIN|pinMode|digitalWrite|digitalRead|analogRead|analogWrite|delay|millis|micros|Serial|Serial1|Serial2|Serial3|Wire|SPI)\b)");
    r.format = makeFormat(QColor("#4EC9B0"), true);
    rules_.push_back(r);
  }

  // Preprocessor
  {
    Rule r;
    r.pattern = QRegularExpression(R"(^\s*#\s*\w+.*$)");
    r.format = makeFormat(QColor("#C586C0"), true);
    rules_.push_back(r);
  }

  // Single-line comments
  {
    Rule r;
    r.pattern = QRegularExpression(R"(//[^\n]*)");
    r.format = makeFormat(QColor("#6A9955"));
    rules_.push_back(r);
  }

  // Strings
  {
    Rule r;
    r.pattern = QRegularExpression(R"("([^"\\]|\\.)*")");
    r.format = makeFormat(QColor("#CE9178"));
    rules_.push_back(r);
  }

  // Character literals
  {
    Rule r;
    r.pattern = QRegularExpression(R"('([^'\\]|\\.)*')");
    r.format = makeFormat(QColor("#CE9178"));
    rules_.push_back(r);
  }

  // Numbers
  {
    Rule r;
    r.pattern =
        QRegularExpression(R"(\b(0x[0-9A-Fa-f]+|\d+(\.\d+)?)\b)");
    r.format = makeFormat(QColor("#B5CEA8"));
    rules_.push_back(r);
  }

  // Multiline comments
  multiLineCommentStart_ = QRegularExpression(R"(/\*)");
  multiLineCommentEnd_ = QRegularExpression(R"(\*/)");
  
  setTheme(true); // Default to dark
}

void CppHighlighter::setTheme(bool isDark) {
  rules_.clear();

  const QColor keywordColor = isDark ? QColor("#569CD6") : QColor("#0000FF");
  const QColor sketchIdColor = isDark ? QColor("#4EC9B0") : QColor("#00979C");
  const QColor preprocColor = isDark ? QColor("#C586C0") : QColor("#5E6E5E");
  const QColor commentColor = isDark ? QColor("#6A9955") : QColor("#434F54");
  const QColor stringColor = isDark ? QColor("#CE9178") : QColor("#005C5F");
  const QColor numberColor = isDark ? QColor("#B5CEA8") : QColor("#000000"); // Usually plain in light

  // Keywords (C/C++)
  {
    Rule r;
    r.pattern = QRegularExpression(
        R"(\b(auto|break|case|catch|char|class|const|constexpr|continue|default|delete|do|double|else|enum|explicit|extern|false|float|for|friend|goto|if|inline|int|long|mutable|namespace|new|nullptr|operator|private|protected|public|register|reinterpret_cast|return|short|signed|sizeof|static|struct|switch|template|this|throw|true|try|typedef|typename|union|unsigned|using|virtual|void|volatile|while)\b)");
    r.format = makeFormat(keywordColor, true);
    rules_.push_back(r);
  }

  // Sketch identifiers
  {
    Rule r;
    r.pattern = QRegularExpression(
        R"(\b(setup|loop|HIGH|LOW|INPUT|OUTPUT|INPUT_PULLUP|LED_BUILTIN|pinMode|digitalWrite|digitalRead|analogRead|analogWrite|delay|millis|micros|Serial|Serial1|Serial2|Serial3|Wire|SPI)\b)");
    r.format = makeFormat(sketchIdColor, true);
    rules_.push_back(r);
  }

  // Preprocessor
  {
    Rule r;
    r.pattern = QRegularExpression(R"(^\s*#\s*\w+.*$)");
    r.format = makeFormat(preprocColor, true);
    rules_.push_back(r);
  }

  // Single-line comments
  {
    Rule r;
    r.pattern = QRegularExpression(R"(//[^\n]*)");
    r.format = makeFormat(commentColor);
    rules_.push_back(r);
  }

  // Strings
  {
    Rule r;
    r.pattern = QRegularExpression(R"("([^"\\]|\\.)*")");
    r.format = makeFormat(stringColor);
    rules_.push_back(r);
  }

  // Character literals
  {
    Rule r;
    r.pattern = QRegularExpression(R"('([^'\\]|\\.)*')");
    r.format = makeFormat(stringColor);
    rules_.push_back(r);
  }

  // Numbers
  {
    Rule r;
    r.pattern =
        QRegularExpression(R"(\b(0x[0-9A-Fa-f]+|\d+(\.\d+)?)\b)");
    r.format = makeFormat(numberColor);
    rules_.push_back(r);
  }
  
  multiLineCommentFormat_ = makeFormat(commentColor);
  rehighlight();
}

void CppHighlighter::highlightBlock(const QString& text) {
  for (const Rule& r : rules_) {
    auto it = r.pattern.globalMatch(text);
    while (it.hasNext()) {
      const auto m = it.next();
      setFormat(m.capturedStart(), m.capturedLength(), r.format);
    }
  }

  setCurrentBlockState(0);

  int startIndex = 0;
  if (previousBlockState() != 1) {
    const auto match = multiLineCommentStart_.match(text);
    startIndex = match.hasMatch() ? match.capturedStart() : -1;
  } else {
    startIndex = 0;
  }

  while (startIndex >= 0) {
    const auto endMatch = multiLineCommentEnd_.match(text, startIndex);
    int endIndex = endMatch.hasMatch() ? endMatch.capturedStart() : -1;
    int commentLength = 0;
    if (endIndex == -1) {
      setCurrentBlockState(1);
      commentLength = text.length() - startIndex;
    } else {
      commentLength = endIndex - startIndex + endMatch.capturedLength();
    }
    setFormat(startIndex, commentLength, multiLineCommentFormat_);
    const auto nextStart = multiLineCommentStart_.match(text, startIndex + commentLength);
    startIndex = nextStart.hasMatch() ? nextStart.capturedStart() : -1;
  }
}
