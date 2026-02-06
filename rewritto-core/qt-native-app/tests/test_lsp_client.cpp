#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTimer>

#include "lsp_client.h"

class TestLspClient final : public QObject {
  Q_OBJECT

 private slots:
  void initializeAndReceiveDiagnostics();
  void requestsReturnResults();
};

void TestLspClient::initializeAndReceiveDiagnostics() {
  const QString server = qEnvironmentVariable("FAKE_LSP_SERVER");
  QVERIFY2(!server.isEmpty(), "FAKE_LSP_SERVER env var must be set by CTest.");

  LspClient client;
  struct StopOnReturn {
    LspClient* c;
    ~StopOnReturn() { c->stop(); }
  } stop{&client};

  QSignalSpy readySpy(&client, &LspClient::readyChanged);
  QSignalSpy diagSpy(&client, &LspClient::publishDiagnostics);

  client.start(server, {}, "file:///tmp");

  QVERIFY(readySpy.wait(2000));
  QVERIFY(client.isReady());

  if (diagSpy.count() == 0) {
    QVERIFY(diagSpy.wait(2000));
  }
  QVERIFY(diagSpy.count() >= 1);

  const auto args = diagSpy.takeFirst();
  const QString uri = args.at(0).toString();
  const QJsonArray diagnostics = args.at(1).toJsonArray();
  QVERIFY(uri.startsWith("file:///"));
  QVERIFY(!diagnostics.isEmpty());

  client.stop();
  QVERIFY(!client.isRunning());
}

void TestLspClient::requestsReturnResults() {
  const QString server = qEnvironmentVariable("FAKE_LSP_SERVER");
  QVERIFY2(!server.isEmpty(), "FAKE_LSP_SERVER env var must be set by CTest.");

  LspClient client;
  struct StopOnReturn {
    LspClient* c;
    ~StopOnReturn() { c->stop(); }
  } stop{&client};

  QSignalSpy readySpy(&client, &LspClient::readyChanged);
  client.start(server, {}, "file:///tmp");
  QVERIFY(readySpy.wait(2000));
  QVERIFY(client.isReady());

  QEventLoop loop;
  QTimer timeout;
  timeout.setSingleShot(true);
  timeout.setInterval(2000);
  connect(&timeout, &QTimer::timeout, &loop, [&] { loop.quit(); });
  timeout.start();

  bool gotCompletion = false;
  client.request("textDocument/completion",
                 QJsonObject{{"textDocument", QJsonObject{{"uri", "file:///tmp/fake.cpp"}}},
                             {"position", QJsonObject{{"line", 0}, {"character", 0}}}},
                 [&](const QJsonValue& result, const QJsonObject& error) {
                   QVERIFY(error.isEmpty());
                   QVERIFY(result.isObject());
                   const QJsonArray items =
                       result.toObject().value("items").toArray();
                   QVERIFY(!items.isEmpty());
                   QCOMPARE(items.first().toObject().value("label").toString(),
                            QString("foo"));
                   gotCompletion = true;
                   loop.quit();
                 });

  loop.exec();
  QVERIFY2(gotCompletion, "completion response not received");

  bool gotHover = false;
  client.request("textDocument/hover",
                 QJsonObject{{"textDocument", QJsonObject{{"uri", "file:///tmp/fake.cpp"}}},
                             {"position", QJsonObject{{"line", 0}, {"character", 0}}}},
                 [&](const QJsonValue& result, const QJsonObject& error) {
                   QVERIFY(error.isEmpty());
                   QVERIFY(result.isObject());
                   QCOMPARE(result.toObject()
                                .value("contents")
                                .toObject()
                                .value("value")
                                .toString(),
                            QString("**hover**"));
                   gotHover = true;
                 });

  QTRY_VERIFY_WITH_TIMEOUT(gotHover, 2000);

  bool gotSymbols = false;
  client.request("textDocument/documentSymbol",
                 QJsonObject{{"textDocument", QJsonObject{{"uri", "file:///tmp/fake.cpp"}}}},
                 [&](const QJsonValue& result, const QJsonObject& error) {
                   QVERIFY(error.isEmpty());
                   QVERIFY(result.isArray());
                   const QJsonArray arr = result.toArray();
                   QVERIFY(!arr.isEmpty());
                   QCOMPARE(arr.first().toObject().value("name").toString(),
                            QString("setup"));
                   gotSymbols = true;
                 });
  QTRY_VERIFY_WITH_TIMEOUT(gotSymbols, 2000);
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  TestLspClient tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "test_lsp_client.moc"
