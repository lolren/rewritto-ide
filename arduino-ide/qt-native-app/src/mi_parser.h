#pragma once

#include <QByteArray>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringView>
#include <QPair>
#include <QVector>
#include <utility>

class MiParser final {
 public:
  struct Value;
  struct Record;

  struct Value {
    enum class Kind {
      Const,
      Tuple,
      List,
    };

    Kind kind = Kind::Const;
    QString constValue;
    QMap<QString, Value> tuple;
    QVector<QPair<QString, Value>> list;  // empty key => value, non-empty => name=value

    static Value makeConst(QString v) {
      Value out;
      out.kind = Kind::Const;
      out.constValue = std::move(v);
      return out;
    }
    static Value makeTuple(QMap<QString, Value> m) {
      Value out;
      out.kind = Kind::Tuple;
      out.tuple = std::move(m);
      return out;
    }
    static Value makeList(QVector<QPair<QString, Value>> v) {
      Value out;
      out.kind = Kind::List;
      out.list = std::move(v);
      return out;
    }
  };

  struct Record {
    enum class Type {
      Unknown,
      Prompt,
      Result,      // ^done,foo=bar
      ExecAsync,   // *stopped,reason=...
      StatusAsync, // +download,...
      NotifyAsync, // =thread-created,...
      Console,     // ~"console stream"
      Target,      // @"target stream"
      Log,         // &"log stream"
    };

    Type type = Type::Unknown;
    int token = -1;
    QString klass;
    QMap<QString, Value> results;
    QString streamText;
    QString raw;
  };

  QList<Record> feed(const QByteArray& chunk);
  void reset();

  static Record parseLine(QStringView line, QString* errorOut = nullptr);

 private:
  QByteArray buffer_;
};
