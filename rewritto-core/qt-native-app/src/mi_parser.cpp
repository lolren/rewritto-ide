#include "mi_parser.h"

#include <QChar>

namespace {

static bool isRecordPrefix(const QChar ch) {
  return ch == '^' || ch == '*' || ch == '+' || ch == '=' || ch == '~' || ch == '@' ||
         ch == '&';
}

class Cursor final {
 public:
  explicit Cursor(QStringView s) : s_(s) {}

  bool atEnd() const { return i_ >= s_.size(); }
  QChar peek() const { return atEnd() ? QChar{} : s_.at(i_); }
  QChar peekNext() const { return (i_ + 1) >= s_.size() ? QChar{} : s_.at(i_ + 1); }
  void advance() {
    if (!atEnd()) {
      ++i_;
    }
  }
  bool consume(QChar ch) {
    if (peek() == ch) {
      ++i_;
      return true;
    }
    return false;
  }
  int pos() const { return i_; }
  void setPos(int p) { i_ = p; }
  QStringView viewFrom(int start) const { return s_.mid(start, i_ - start); }

 private:
  QStringView s_;
  int i_ = 0;
};

static QString parseCString(Cursor& c, QString* err) {
  if (!c.consume('"')) {
    if (err) {
      *err = QStringLiteral("expected '\"'");
    }
    return {};
  }

  QString out;
  while (!c.atEnd()) {
    const QChar ch = c.peek();
    if (ch == '"') {
      c.advance();
      return out;
    }
    if (ch == '\\') {
      c.advance();
      if (c.atEnd()) {
        break;
      }
      const QChar esc = c.peek();
      c.advance();
      switch (esc.unicode()) {
        case 'n':
          out += QChar('\n');
          break;
        case 'r':
          out += QChar('\r');
          break;
        case 't':
          out += QChar('\t');
          break;
        case 'b':
          out += QChar('\b');
          break;
        case 'f':
          out += QChar('\f');
          break;
        case '\\':
          out += QChar('\\');
          break;
        case '"':
          out += QChar('"');
          break;
        default:
          out += esc;
          break;
      }
      continue;
    }
    out += ch;
    c.advance();
  }

  if (err) {
    *err = QStringLiteral("unterminated string");
  }
  return out;
}

static QString parseName(Cursor& c) {
  const int start = c.pos();
  while (!c.atEnd()) {
    const QChar ch = c.peek();
    if (ch.isLetterOrNumber() || ch == '_' || ch == '-' || ch == '.') {
      c.advance();
      continue;
    }
    break;
  }
  return c.viewFrom(start).toString();
}

static QString parseConst(Cursor& c) {
  const int start = c.pos();
  while (!c.atEnd()) {
    const QChar ch = c.peek();
    if (ch == ',' || ch == '}' || ch == ']' ) {
      break;
    }
    c.advance();
  }
  return c.viewFrom(start).toString();
}

static MiParser::Value parseValue(Cursor& c, QString* err);

static QMap<QString, MiParser::Value> parseResults(Cursor& c, QString* err);

static MiParser::Value parseTuple(Cursor& c, QString* err) {
  if (!c.consume('{')) {
    if (err) {
      *err = QStringLiteral("expected '{'");
    }
    return MiParser::Value::makeTuple({});
  }

  QMap<QString, MiParser::Value> out;
  if (c.consume('}')) {
    return MiParser::Value::makeTuple(std::move(out));
  }

  out = parseResults(c, err);
  if (!c.consume('}')) {
    if (err && err->isEmpty()) {
      *err = QStringLiteral("expected '}'");
    }
  }
  return MiParser::Value::makeTuple(std::move(out));
}

static MiParser::Value parseList(Cursor& c, QString* err) {
  if (!c.consume('[')) {
    if (err) {
      *err = QStringLiteral("expected '['");
    }
    return MiParser::Value::makeList({});
  }

  QVector<QPair<QString, MiParser::Value>> items;
  if (c.consume(']')) {
    return MiParser::Value::makeList(std::move(items));
  }

  while (!c.atEnd()) {
    if (c.peek() == '"') {
      items.push_back(qMakePair(QString{}, MiParser::Value::makeConst(parseCString(c, err))));
    } else if (c.peek() == '{') {
      items.push_back(qMakePair(QString{}, parseTuple(c, err)));
    } else if (c.peek() == '[') {
      items.push_back(qMakePair(QString{}, parseList(c, err)));
    } else {
      const int save = c.pos();
      const QString name = parseName(c);
      if (!name.isEmpty() && c.peek() == '=') {
        c.advance();  // '='
        items.push_back(qMakePair(name, parseValue(c, err)));
      } else {
        c.setPos(save);
        items.push_back(qMakePair(QString{}, MiParser::Value::makeConst(parseConst(c))));
      }
    }

    if (c.consume(']')) {
      break;
    }
    if (!c.consume(',')) {
      // Some MI outputs omit commas in unexpected places; stop parsing.
      break;
    }
    if (c.consume(']')) {
      break;
    }
  }

  return MiParser::Value::makeList(std::move(items));
}

static MiParser::Value parseValue(Cursor& c, QString* err) {
  const QChar ch = c.peek();
  if (ch == '"') {
    return MiParser::Value::makeConst(parseCString(c, err));
  }
  if (ch == '{') {
    return parseTuple(c, err);
  }
  if (ch == '[') {
    return parseList(c, err);
  }
  return MiParser::Value::makeConst(parseConst(c));
}

static QMap<QString, MiParser::Value> parseResults(Cursor& c, QString* err) {
  QMap<QString, MiParser::Value> out;

  while (!c.atEnd()) {
    const QString name = parseName(c);
    if (name.isEmpty()) {
      if (err && err->isEmpty()) {
        *err = QStringLiteral("expected name");
      }
      break;
    }
    if (!c.consume('=')) {
      if (err && err->isEmpty()) {
        *err = QStringLiteral("expected '='");
      }
      break;
    }
    out.insert(name, parseValue(c, err));

    if (!c.consume(',')) {
      break;
    }
    if (c.peek() == '}' || c.peek() == ']') {
      break;
    }
  }

  return out;
}

}  // namespace

QList<MiParser::Record> MiParser::feed(const QByteArray& chunk) {
  buffer_.append(chunk);

  QList<Record> out;
  while (true) {
    const int nl = buffer_.indexOf('\n');
    if (nl < 0) {
      break;
    }
    QByteArray lineBytes = buffer_.left(nl);
    buffer_.remove(0, nl + 1);
    if (!lineBytes.isEmpty() && lineBytes.endsWith('\r')) {
      lineBytes.chop(1);
    }

    const QString line = QString::fromLocal8Bit(lineBytes);
    if (line.isEmpty()) {
      continue;
    }
    out.push_back(parseLine(line));
  }
  return out;
}

void MiParser::reset() {
  buffer_.clear();
}

MiParser::Record MiParser::parseLine(QStringView line, QString* errorOut) {
  Record r;
  r.raw = line.toString();

  if (line == QStringLiteral("(gdb)")) {
    r.type = Record::Type::Prompt;
    return r;
  }

  Cursor c(line);

  // Optional token prefix.
  int token = 0;
  bool haveToken = false;
  while (!c.atEnd() && c.peek().isDigit()) {
    haveToken = true;
    token = token * 10 + (c.peek().unicode() - '0');
    c.advance();
  }

  if (haveToken && !isRecordPrefix(c.peek())) {
    c.setPos(0);
    haveToken = false;
    token = 0;
  }
  if (haveToken) {
    r.token = token;
  }

  const QChar prefix = c.peek();
  if (!isRecordPrefix(prefix)) {
    r.type = Record::Type::Unknown;
    return r;
  }
  c.advance();

  if (prefix == '~' || prefix == '@' || prefix == '&') {
    QString err;
    r.streamText = parseCString(c, &err);
    if (!err.isEmpty() && errorOut) {
      *errorOut = err;
    }
    if (prefix == '~') {
      r.type = Record::Type::Console;
    } else if (prefix == '@') {
      r.type = Record::Type::Target;
    } else {
      r.type = Record::Type::Log;
    }
    return r;
  }

  const int klassStart = c.pos();
  while (!c.atEnd() && c.peek() != ',') {
    c.advance();
  }
  r.klass = line.mid(klassStart, c.pos() - klassStart).toString();

  if (prefix == '^') {
    r.type = Record::Type::Result;
  } else if (prefix == '*') {
    r.type = Record::Type::ExecAsync;
  } else if (prefix == '+') {
    r.type = Record::Type::StatusAsync;
  } else if (prefix == '=') {
    r.type = Record::Type::NotifyAsync;
  }

  if (c.consume(',')) {
    QString err;
    r.results = parseResults(c, &err);
    if (!err.isEmpty() && errorOut) {
      *errorOut = err;
    }
  }

  return r;
}

