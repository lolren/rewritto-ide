#pragma once

#include <functional>

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QProcess>

class LspClient final : public QObject {
  Q_OBJECT

 public:
  explicit LspClient(QObject* parent = nullptr);

 bool isRunning() const;
  bool isReady() const;

  void start(QString program, QStringList args, QString rootUri);
  void stop();

  void didOpen(const QString& uri, const QString& languageId, const QString& text);
  void didChange(const QString& uri, const QString& text);
  void didClose(const QString& uri);

  using ResponseHandler = std::function<void(const QJsonValue& result,
                                             const QJsonObject& error)>;
  int request(const QString& method, const QJsonValue& params, ResponseHandler handler);

 signals:
  void readyChanged(bool ready);
  void logMessage(QString message);
  void publishDiagnostics(QString uri, QJsonArray diagnostics);

 private:
  QProcess* process_ = nullptr;
  bool stopping_ = false;
  QByteArray readBuffer_;
  int nextRequestId_ = 1;
  bool ready_ = false;
  QString rootUri_;
  QHash<QString, int> documentVersions_;
  QHash<int, ResponseHandler> pendingRequests_;
  int initializeRequestId_ = -1;

  void sendMessage(const QJsonObject& obj);
  void sendResponse(int id, const QJsonValue& result);
  void sendError(int id, int code, const QString& message);
  int sendRequest(const QString& method, const QJsonValue& params, ResponseHandler handler);
  void sendNotification(const QString& method, const QJsonValue& params);

  void handleIncoming();
  void handleMessage(const QJsonObject& msg);
  void setReady(bool ready);
};
