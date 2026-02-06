#include "lsp_client.h"

#include <QJsonDocument>

LspClient::LspClient(QObject* parent) : QObject(parent) {}

bool LspClient::isRunning() const {
  return process_ && process_->state() != QProcess::NotRunning;
}

bool LspClient::isReady() const {
  return ready_;
}

void LspClient::start(QString program, QStringList args, QString rootUri) {
  stop();

  rootUri_ = std::move(rootUri);
  documentVersions_.clear();
  pendingRequests_.clear();
  readBuffer_.clear();
  nextRequestId_ = 1;
  initializeRequestId_ = -1;
  setReady(false);

  process_ = new QProcess(this);
  process_->setProgram(std::move(program));
  process_->setArguments(std::move(args));

  connect(process_, &QProcess::readyReadStandardOutput, this, [this] {
    readBuffer_.append(process_->readAllStandardOutput());
    handleIncoming();
  });
  connect(process_, &QProcess::readyReadStandardError, this, [this] {
    const QString msg = QString::fromLocal8Bit(process_->readAllStandardError());
    if (!msg.trimmed().isEmpty()) {
      emit logMessage(msg.trimmed());
    }
  });
  connect(process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
    emit logMessage("LSP process error occurred.");
    stop();
  });
  connect(process_, &QProcess::finished, this, [this](int, QProcess::ExitStatus) {
    emit logMessage("LSP process exited.");
    stop();
  });

  process_->start();
  if (!process_->waitForStarted(2000)) {
    emit logMessage("Failed to start LSP process.");
    stop();
    return;
  }

  // initialize
  QJsonObject params;
  params.insert("processId", QJsonValue::Null);
  params.insert("rootUri", rootUri_);
  params.insert("capabilities", QJsonObject{
                                  {"textDocument", QJsonObject{
                                                    {"synchronization",
                                                     QJsonObject{
                                                       {"didSave", true},
                                                       {"willSave", false},
                                                       {"willSaveWaitUntil",
                                                        false},
                                                     }},
                                                  }},
                                });

  initializeRequestId_ = sendRequest(
      "initialize", params,
      [this](const QJsonValue&, const QJsonObject& error) {
        if (!error.isEmpty()) {
          emit logMessage("LSP initialize failed: " + error.value("message").toString());
          stop();
          return;
        }
        sendNotification("initialized", QJsonObject{});
        setReady(true);
      });
}

void LspClient::stop() {
  if (!process_) {
    return;
  }

  if (stopping_) {
    return;
  }
  stopping_ = true;

  QProcess* p = process_;
  if (p->state() != QProcess::NotRunning) {
    if (ready_) {
      (void)sendRequest("shutdown", QJsonValue::Null, nullptr);
      sendNotification("exit", QJsonValue::Null);
      p->waitForFinished(250);
    }
    p->kill();
    p->waitForFinished(1000);
  }
  p->deleteLater();

  process_ = nullptr;
  readBuffer_.clear();
  documentVersions_.clear();
  pendingRequests_.clear();
  initializeRequestId_ = -1;
  setReady(false);
  stopping_ = false;
}

void LspClient::didOpen(const QString& uri,
                        const QString& languageId,
                        const QString& text) {
  if (!isReady()) {
    return;
  }
  QJsonObject doc;
  doc.insert("uri", uri);
  doc.insert("languageId", languageId);
  doc.insert("version", 1);
  doc.insert("text", text);
  documentVersions_[uri] = 1;
  sendNotification("textDocument/didOpen", QJsonObject{{"textDocument", doc}});
}

void LspClient::didChange(const QString& uri, const QString& text) {
  if (!isReady()) {
    return;
  }
  int version = documentVersions_.value(uri, 1);
  version += 1;
  documentVersions_[uri] = version;

  QJsonObject doc;
  doc.insert("uri", uri);
  doc.insert("version", version);

  QJsonArray changes;
  changes.push_back(QJsonObject{{"text", text}});

  sendNotification("textDocument/didChange",
                   QJsonObject{{"textDocument", doc},
                               {"contentChanges", changes}});
}

void LspClient::didClose(const QString& uri) {
  if (!isReady()) {
    return;
  }
  QJsonObject doc;
  doc.insert("uri", uri);
  sendNotification("textDocument/didClose", QJsonObject{{"textDocument", doc}});
  documentVersions_.remove(uri);
}

void LspClient::sendMessage(const QJsonObject& obj) {
  if (!process_ || process_->state() == QProcess::NotRunning) {
    return;
  }
  const QByteArray json = QJsonDocument(obj).toJson(QJsonDocument::Compact);
  const QByteArray header =
      "Content-Length: " + QByteArray::number(json.size()) + "\r\n\r\n";
  process_->write(header);
  process_->write(json);
}

void LspClient::sendResponse(int id, const QJsonValue& result) {
  QJsonObject obj;
  obj.insert("jsonrpc", "2.0");
  obj.insert("id", id);
  obj.insert("result", result);
  sendMessage(obj);
}

void LspClient::sendError(int id, int code, const QString& message) {
  QJsonObject err;
  err.insert("code", code);
  err.insert("message", message);

  QJsonObject obj;
  obj.insert("jsonrpc", "2.0");
  obj.insert("id", id);
  obj.insert("error", err);
  sendMessage(obj);
}

int LspClient::sendRequest(const QString& method,
                           const QJsonValue& params,
                           ResponseHandler handler) {
  const int id = nextRequestId_++;
  QJsonObject obj;
  obj.insert("jsonrpc", "2.0");
  obj.insert("id", id);
  obj.insert("method", method);
  obj.insert("params", params);
  if (handler) {
    pendingRequests_.insert(id, std::move(handler));
  }
  sendMessage(obj);
  return id;
}

int LspClient::request(const QString& method,
                       const QJsonValue& params,
                       ResponseHandler handler) {
  if (!process_ || process_->state() == QProcess::NotRunning) {
    if (handler) {
      QJsonObject err;
      err.insert("code", -32002);
      err.insert("message", "LSP process not running.");
      handler(QJsonValue{}, err);
    }
    return -1;
  }
  return sendRequest(method, params, std::move(handler));
}

void LspClient::sendNotification(const QString& method, const QJsonValue& params) {
  QJsonObject obj;
  obj.insert("jsonrpc", "2.0");
  obj.insert("method", method);
  obj.insert("params", params);
  sendMessage(obj);
}

void LspClient::handleIncoming() {
  while (true) {
    const int headerEnd = readBuffer_.indexOf("\r\n\r\n");
    if (headerEnd < 0) {
      return;
    }
    const QByteArray headerBytes = readBuffer_.left(headerEnd);
    const QList<QByteArray> headerLines = headerBytes.split('\n');
    int contentLength = -1;
    for (QByteArray line : headerLines) {
      line = line.trimmed();
      if (line.toLower().startsWith("content-length:")) {
        const QByteArray value = line.mid(strlen("content-length:")).trimmed();
        contentLength = value.toInt();
        break;
      }
    }
    if (contentLength < 0) {
      // Not an LSP message; dump as log and clear.
      emit logMessage(QString::fromLocal8Bit(readBuffer_));
      readBuffer_.clear();
      return;
    }
    const int payloadStart = headerEnd + 4;
    if (readBuffer_.size() < payloadStart + contentLength) {
      return;
    }
    const QByteArray payload =
        readBuffer_.mid(payloadStart, contentLength);
    readBuffer_.remove(0, payloadStart + contentLength);

    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject()) {
      continue;
    }
    handleMessage(doc.object());
  }
}

void LspClient::handleMessage(const QJsonObject& msg) {
  const QString method = msg.value("method").toString();
  const bool hasId = msg.contains("id");
  const int id = msg.value("id").toInt(-1);

  if (!method.isEmpty()) {
    // Requests from server
    if (hasId && id >= 0) {
      if (method == "window/workDoneProgress/create") {
        sendResponse(id, QJsonValue::Null);
        return;
      }
      if (method == "workspace/configuration") {
        const QJsonObject params = msg.value("params").toObject();
        const QJsonArray items = params.value("items").toArray();
        QJsonArray result;
        for (int i = 0; i < items.size(); ++i) {
          result.push_back(QJsonObject{});
        }
        sendResponse(id, result);
        return;
      }
      if (method == "workspace/workspaceFolders") {
        QJsonArray folders;
        if (!rootUri_.isEmpty()) {
          folders.push_back(QJsonObject{{"uri", rootUri_}, {"name", "workspace"}});
        }
        sendResponse(id, folders);
        return;
      }
      if (method == "client/registerCapability" ||
          method == "client/unregisterCapability") {
        sendResponse(id, QJsonValue::Null);
        return;
      }
      if (method == "window/showMessageRequest") {
        sendResponse(id, QJsonValue::Null);
        return;
      }

      sendError(id, -32601, "Method not found");
      return;
    }

    // Notifications from server
    if (method == "textDocument/publishDiagnostics") {
      const QJsonObject params = msg.value("params").toObject();
      emit publishDiagnostics(params.value("uri").toString(),
                              params.value("diagnostics").toArray());
      return;
    }
    if (method == "window/logMessage" || method == "window/showMessage") {
      const QJsonObject params = msg.value("params").toObject();
      emit logMessage(params.value("message").toString());
      return;
    }
    return;
  }

  // response
  if (hasId && id >= 0) {
    QJsonValue result;
    QJsonObject error;
    if (msg.contains("error")) {
      error = msg.value("error").toObject();
    } else if (msg.contains("result")) {
      result = msg.value("result");
    }

    if (id == initializeRequestId_) {
      // invoke handler if stored; it will set ready.
      const auto handler = pendingRequests_.take(id);
      if (handler) {
        handler(result, error);
      } else if (error.isEmpty()) {
        sendNotification("initialized", QJsonObject{});
        setReady(true);
      }
      return;
    }

    const auto handler = pendingRequests_.take(id);
    if (handler) {
      handler(result, error);
    }
  }
}

void LspClient::setReady(bool ready) {
  if (ready_ == ready) {
    return;
  }
  ready_ = ready;
  emit readyChanged(ready_);
}
