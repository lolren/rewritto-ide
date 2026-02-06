#include "main_window.h"
#include "theme_manager.h"
#include "interface_scale_manager.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLocale>
#include <QRegularExpression>
#include <QSettings>
#include <QTimer>
#include <QTranslator>
#include <QtGlobal>

namespace {
constexpr auto kBrandOrg = "Rewritto";
constexpr auto kBrandApp = "Rewritto Ide";

constexpr auto kPreviousBrandOrg = "BlingBlink";
constexpr auto kPreviousBrandApp = "BlingBlink IDE";
constexpr auto kLegacyOrg = "Arduino";
constexpr auto kLegacyApp = "Arduino IDE (Qt Native)";

QtMessageHandler g_prevMessageHandler = nullptr;

void rewrittoMessageHandler(QtMsgType type,
                            const QMessageLogContext& context,
                            const QString& msg) {
  if (type == QtWarningMsg &&
      msg.contains(QStringLiteral("Unknown property minimum-width"))) {
    return;
  }

  if (g_prevMessageHandler) {
    g_prevMessageHandler(type, context, msg);
  }
}

QString singleInstanceServerName() {
  QString user = qEnvironmentVariable("USER");
  if (user.isEmpty()) {
    user = "user";
  }
  user.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.-]")),
               QStringLiteral("_"));
  return QStringLiteral("com.rewritto.ide.%1").arg(user);
}

void migrateLegacySettingsIfNeeded() {
  QSettings current;
  if (!current.allKeys().isEmpty()) {
    return;
  }

  auto migrateFrom = [&current](const QString& org, const QString& app) {
    QSettings legacy(org, app);
    const QStringList keys = legacy.allKeys();
    if (keys.isEmpty()) {
      return false;
    }
    for (const QString& key : keys) {
      if (!current.contains(key)) {
        current.setValue(key, legacy.value(key));
      }
    }
    current.sync();
    return true;
  };

  if (migrateFrom(kPreviousBrandOrg, kPreviousBrandApp)) {
    return;
  }
  migrateFrom(kLegacyOrg, kLegacyApp);
}
}  // namespace

int main(int argc, char* argv[]) {
  g_prevMessageHandler = qInstallMessageHandler(rewrittoMessageHandler);

  QApplication app(argc, argv);
  QApplication::setApplicationName(kBrandApp);
  QApplication::setOrganizationName(kBrandOrg);
  QApplication::setApplicationVersion(QStringLiteral("0.2.0"));

  migrateLegacySettingsIfNeeded();

  UiScaleManager::init();

  // Apply theme early so all widgets pick it up.
  ThemeManager::init();
  {
    QSettings settings;
    settings.beginGroup("Preferences");
    const QString theme = settings.value("theme", "system").toString();
    const double uiScale = settings.value("uiScale", 1.0).toDouble();
    settings.endGroup();
    UiScaleManager::apply(uiScale);
    ThemeManager::apply(theme);
  }

  {
    QSettings settings;
    settings.beginGroup("Preferences");
    QString locale = settings.value("locale", "system").toString();
    settings.endGroup();

    if (locale == "system") {
      locale = QLocale::system().name();
    }

    if (locale != "en" && !locale.isEmpty()) {
      auto* translator = new QTranslator(QCoreApplication::instance());
	      if (translator->load("rewritto_" + locale,
	                           QCoreApplication::applicationDirPath() + "/i18n")) {
	        app.installTranslator(translator);
	      }
    }
  }

  QCommandLineParser parser;
  parser.setApplicationDescription(kBrandApp);
  parser.addHelpOption();
  parser.addVersionOption();
  parser.addPositionalArgument("path",
                               "Sketch folder or file(s) to open.",
                               "[path...]");

  QCommandLineOption smokeOption("smoke-test",
                                 "Start the app, then exit automatically.");
  QCommandLineOption smokeMsOption(
      "smoke-test-ms",
      "Delay in milliseconds before exiting (used with --smoke-test).", "ms",
      "150");
  parser.addOption(smokeOption);
  parser.addOption(smokeMsOption);

  parser.process(app);

  const QStringList paths = parser.positionalArguments();

  const QString serverName = singleInstanceServerName();

  // Second instance: forward args to the first instance and exit.
  {
    QLocalSocket socket;
    socket.connectToServer(serverName);
    if (socket.waitForConnected(100)) {
      QJsonObject msg;
      msg.insert("paths", QJsonArray::fromStringList(paths));
      const QByteArray payload =
          QJsonDocument(msg).toJson(QJsonDocument::Compact);
      socket.write(payload);
      socket.flush();
      (void)socket.waitForBytesWritten(500);
      socket.disconnectFromServer();
      (void)socket.waitForDisconnected(500);
      return 0;
    }
  }

  QLocalServer server;
  if (!server.listen(serverName)) {
    QLocalServer::removeServer(serverName);
    (void)server.listen(serverName);
  }

  MainWindow window;
  window.show();

  QObject::connect(&server, &QLocalServer::newConnection, &window,
                   [&server, &window] {
                     while (QLocalSocket* client =
                                server.nextPendingConnection()) {
                       client->setProperty("buf", QByteArray{});
                       QObject::connect(client, &QLocalSocket::readyRead, &window,
                                        [client] {
                                          QByteArray buf =
                                              client->property("buf")
                                                  .toByteArray();
                                          buf.append(client->readAll());
                                          client->setProperty("buf", buf);
                                        });
                       QObject::connect(client, &QLocalSocket::disconnected,
                                        &window, [client, &window] {
                                          QByteArray buf =
                                              client->property("buf")
                                                  .toByteArray();
                                          buf.append(client->readAll());
                                          const QJsonDocument doc =
                                              QJsonDocument::fromJson(buf);
                                          if (doc.isObject()) {
                                            const QJsonArray arr =
                                                doc.object()
                                                    .value("paths")
                                                    .toArray();
                                            QStringList paths;
                                            paths.reserve(arr.size());
                                            for (const QJsonValue& v : arr) {
                                              paths.push_back(v.toString());
                                            }
                                            window.openPaths(paths);
                                          } else {
                                            window.raise();
                                            window.activateWindow();
                                          }
                                          client->deleteLater();
                                        });
                     }
                   });

  if (!paths.isEmpty()) {
    window.openPaths(paths);
  }

  if (parser.isSet(smokeOption)) {
    bool ok = false;
    int ms = parser.value(smokeMsOption).toInt(&ok);
    if (!ok || ms < 0) {
      ms = 150;
    }
    QTimer::singleShot(ms, &window, [&window] { window.close(); });
  }

  return app.exec();
}
