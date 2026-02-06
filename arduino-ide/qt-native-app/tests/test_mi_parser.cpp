#include <QtTest/QtTest>

#include "mi_parser.h"

class TestMiParser final : public QObject {
  Q_OBJECT

 private slots:
  void parsesPrompt() {
    const MiParser::Record r = MiParser::parseLine(QStringLiteral("(gdb)"));
    QCOMPARE(r.type, MiParser::Record::Type::Prompt);
  }

  void parsesStream() {
    const MiParser::Record r =
        MiParser::parseLine(QStringLiteral("~\"hello\\nworld\\\"\\\"\""));
    QCOMPARE(r.type, MiParser::Record::Type::Console);
    QCOMPARE(r.streamText, QStringLiteral("hello\nworld\"\""));
  }

  void parsesAsyncStopped() {
    const MiParser::Record r = MiParser::parseLine(
        QStringLiteral("*stopped,reason=\"breakpoint-hit\",thread-id=\"1\","
                       "frame={func=\"loop\",file=\"sketch.ino\",fullname=\"/tmp/sketch.ino\",line=\"10\"}"));
    QCOMPARE(r.type, MiParser::Record::Type::ExecAsync);
    QCOMPARE(r.klass, QStringLiteral("stopped"));
    QCOMPARE(r.results.value(QStringLiteral("reason")).constValue,
             QStringLiteral("breakpoint-hit"));
    QVERIFY(r.results.contains(QStringLiteral("frame")));
    const auto frame = r.results.value(QStringLiteral("frame"));
    QCOMPARE(frame.kind, MiParser::Value::Kind::Tuple);
    QCOMPARE(frame.tuple.value(QStringLiteral("func")).constValue, QStringLiteral("loop"));
    QCOMPARE(frame.tuple.value(QStringLiteral("fullname")).constValue,
             QStringLiteral("/tmp/sketch.ino"));
  }

  void parsesStackFrames() {
    const MiParser::Record r = MiParser::parseLine(
        QStringLiteral("2^done,stack=[frame={level=\"0\",func=\"loop\",fullname=\"/tmp/sketch.ino\",line=\"10\"},"
                       "frame={level=\"1\",func=\"main\",fullname=\"/tmp/main.cpp\",line=\"50\"}]"));
    QCOMPARE(r.type, MiParser::Record::Type::Result);
    QCOMPARE(r.token, 2);
    QCOMPARE(r.klass, QStringLiteral("done"));
    QVERIFY(r.results.contains(QStringLiteral("stack")));
    const auto stack = r.results.value(QStringLiteral("stack"));
    QCOMPARE(stack.kind, MiParser::Value::Kind::List);
    QCOMPARE(stack.list.size(), 2);
    QCOMPARE(stack.list.at(0).first, QStringLiteral("frame"));
    QCOMPARE(stack.list.at(0).second.kind, MiParser::Value::Kind::Tuple);
    QCOMPARE(stack.list.at(0).second.tuple.value(QStringLiteral("level")).constValue,
             QStringLiteral("0"));
    QCOMPARE(stack.list.at(1).second.tuple.value(QStringLiteral("func")).constValue,
             QStringLiteral("main"));
  }

  void parsesVariablesList() {
    const MiParser::Record r = MiParser::parseLine(
        QStringLiteral("3^done,variables=[{name=\"x\",value=\"42\",type=\"int\"},{name=\"s\",value=\"\\\"hi\\\"\"}]"));
    QCOMPARE(r.type, MiParser::Record::Type::Result);
    QCOMPARE(r.token, 3);
    QVERIFY(r.results.contains(QStringLiteral("variables")));
    const auto vars = r.results.value(QStringLiteral("variables"));
    QCOMPARE(vars.kind, MiParser::Value::Kind::List);
    QCOMPARE(vars.list.size(), 2);
    QCOMPARE(vars.list.at(0).first, QString{});
    QCOMPARE(vars.list.at(0).second.kind, MiParser::Value::Kind::Tuple);
    QCOMPARE(vars.list.at(0).second.tuple.value(QStringLiteral("name")).constValue,
             QStringLiteral("x"));
    QCOMPARE(vars.list.at(0).second.tuple.value(QStringLiteral("value")).constValue,
             QStringLiteral("42"));
    QCOMPARE(vars.list.at(1).second.tuple.value(QStringLiteral("value")).constValue,
             QStringLiteral("\"hi\""));
  }

  void feedSplitsLines() {
    MiParser p;
    const QList<MiParser::Record> recs =
        p.feed(QByteArray("1^done\n*running\n(gdb)\n"));
    QCOMPARE(recs.size(), 3);
    QCOMPARE(recs.at(0).type, MiParser::Record::Type::Result);
    QCOMPARE(recs.at(0).token, 1);
    QCOMPARE(recs.at(1).type, MiParser::Record::Type::ExecAsync);
    QCOMPARE(recs.at(1).klass, QStringLiteral("running"));
    QCOMPARE(recs.at(2).type, MiParser::Record::Type::Prompt);
  }
};

QTEST_MAIN(TestMiParser)
#include "test_mi_parser.moc"
