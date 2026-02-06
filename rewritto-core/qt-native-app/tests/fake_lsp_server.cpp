#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <iostream>

static bool readMessage(QJsonObject& out) {
  std::string line;
  int contentLength = -1;
  while (std::getline(std::cin, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.empty()) {
      break;
    }
    const std::string prefix = "Content-Length:";
    if (line.rfind(prefix, 0) == 0) {
      const std::string value = line.substr(prefix.size());
      contentLength = std::stoi(value);
    }
  }
  if (contentLength <= 0) {
    return false;
  }

  QByteArray payload;
  payload.resize(contentLength);
  std::cin.read(payload.data(), contentLength);
  if (std::cin.gcount() != contentLength) {
    return false;
  }

  const QJsonDocument doc = QJsonDocument::fromJson(payload);
  if (!doc.isObject()) {
    return false;
  }
  out = doc.object();
  return true;
}

static void sendObject(const QJsonObject& obj) {
  const QByteArray json = QJsonDocument(obj).toJson(QJsonDocument::Compact);
  std::cout << "Content-Length: " << json.size() << "\r\n\r\n";
  std::cout.write(json.constData(), json.size());
  std::cout.flush();
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);

  bool initialized = false;

  while (true) {
    QJsonObject msg;
    if (!readMessage(msg)) {
      return 0;
    }

    const QString method = msg.value("method").toString();
    const bool hasId = msg.contains("id");
    const int id = msg.value("id").toInt(-1);

    // Notifications
    if (!hasId) {
      if (method == "exit") {
        return 0;
      }
      continue;
    }

    // Requests
    if (id < 0) {
      continue;
    }

    if (method == "initialize") {
      QJsonObject result;
      result.insert("capabilities", QJsonObject{});
      sendObject(QJsonObject{
          {"jsonrpc", "2.0"},
          {"id", id},
          {"result", result},
      });

      // Emit one diagnostics notification after initialize.
      if (!initialized) {
        initialized = true;
        QJsonArray diags;
        diags.push_back(QJsonObject{
            {"range",
             QJsonObject{
                 {"start", QJsonObject{{"line", 0}, {"character", 0}}},
                 {"end", QJsonObject{{"line", 0}, {"character", 1}}},
             }},
            {"severity", 2},
            {"message", "fake warning"},
        });
        sendObject(QJsonObject{
            {"jsonrpc", "2.0"},
            {"method", "textDocument/publishDiagnostics"},
            {"params",
             QJsonObject{
                 {"uri", "file:///tmp/fake.cpp"},
                 {"diagnostics", diags},
             }},
        });
      }
      continue;
    }

    if (method == "textDocument/hover") {
      QJsonObject result;
      result.insert("contents",
                    QJsonObject{{"kind", "markdown"}, {"value", "**hover**"}});
      sendObject(QJsonObject{
          {"jsonrpc", "2.0"},
          {"id", id},
          {"result", result},
      });
      continue;
    }

    if (method == "textDocument/completion") {
      QJsonArray items;
      items.push_back(QJsonObject{
          {"label", "foo"},
          {"insertText", "foo"},
      });
      QJsonObject result;
      result.insert("isIncomplete", false);
      result.insert("items", items);
      sendObject(QJsonObject{
          {"jsonrpc", "2.0"},
          {"id", id},
          {"result", result},
      });
      continue;
    }

    if (method == "textDocument/definition" || method == "textDocument/references") {
      QJsonArray locs;
      locs.push_back(QJsonObject{
          {"uri", "file:///tmp/fake.cpp"},
          {"range",
           QJsonObject{
               {"start", QJsonObject{{"line", 1}, {"character", 2}}},
               {"end", QJsonObject{{"line", 1}, {"character", 3}}},
           }},
      });
      sendObject(QJsonObject{
          {"jsonrpc", "2.0"},
          {"id", id},
          {"result", locs},
      });
      continue;
    }

    if (method == "textDocument/formatting") {
      QJsonArray edits;
      edits.push_back(QJsonObject{
          {"range",
           QJsonObject{
               {"start", QJsonObject{{"line", 0}, {"character", 0}}},
               {"end", QJsonObject{{"line", 0}, {"character", 0}}},
           }},
          {"newText", "// formatted\n"},
      });
      sendObject(QJsonObject{
          {"jsonrpc", "2.0"},
          {"id", id},
          {"result", edits},
      });
      continue;
    }

    if (method == "textDocument/rename") {
      QJsonArray edits;
      edits.push_back(QJsonObject{
          {"range",
           QJsonObject{
               {"start", QJsonObject{{"line", 0}, {"character", 0}}},
               {"end", QJsonObject{{"line", 0}, {"character", 0}}},
           }},
          {"newText", "// rename edit\n"},
      });
      QJsonObject changes;
      changes.insert("file:///tmp/fake.cpp", edits);
      QJsonObject result;
      result.insert("changes", changes);
      sendObject(QJsonObject{
          {"jsonrpc", "2.0"},
          {"id", id},
          {"result", result},
      });
      continue;
    }

    if (method == "textDocument/documentSymbol") {
      QJsonArray children;
      children.push_back(QJsonObject{
          {"name", "loop"},
          {"kind", 12},
          {"selectionRange",
           QJsonObject{
               {"start", QJsonObject{{"line", 10}, {"character", 0}}},
               {"end", QJsonObject{{"line", 10}, {"character", 4}}},
           }},
      });

      QJsonArray result;
      result.push_back(QJsonObject{
          {"name", "setup"},
          {"detail", "void"},
          {"kind", 12},
          {"selectionRange",
           QJsonObject{
               {"start", QJsonObject{{"line", 0}, {"character", 0}}},
               {"end", QJsonObject{{"line", 0}, {"character", 5}}},
           }},
          {"children", children},
      });

      sendObject(QJsonObject{
          {"jsonrpc", "2.0"},
          {"id", id},
          {"result", result},
      });
      continue;
    }

    if (method == "shutdown") {
      sendObject(QJsonObject{
          {"jsonrpc", "2.0"},
          {"id", id},
          {"result", QJsonValue::Null},
      });
      return 0;
    }

    // Default: method not found
    sendObject(QJsonObject{
        {"jsonrpc", "2.0"},
        {"id", id},
        {"error",
         QJsonObject{{"code", -32601}, {"message", "Method not found"}}},
    });
  }
}
