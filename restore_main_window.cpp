#include "main_window.h"

#include "qt_host.h"

#include <QCommandLineParser>
#include <QFile>
#include <QWebChannel>
#include <QWebEnginePage>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineView>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setWindowTitle("Rewritto-ide (Qt)");

  webView_ = new QWebEngineView(this);
  webView_->setContextMenuPolicy(Qt::NoContextMenu);
  setCentralWidget(webView_);

  webChannel_ = new QWebChannel(webView_->page());
  qtHost_ = new QtHost(this, webView_, webChannel_);
  webChannel_->registerObject(QStringLiteral("qtHost"), qtHost_);
  webView_->page()->setWebChannel(webChannel_);

  {
    QFile bridgeFile(QStringLiteral(":/qt-app/resources/bridge.js"));
    if (bridgeFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
      QWebEngineScript bridgeScript;
      bridgeScript.setName(QStringLiteral("rewritto-ide-qt-bridge"));
      bridgeScript.setInjectionPoint(QWebEngineScript::DocumentCreation);
      bridgeScript.setRunsOnSubFrames(true);
      bridgeScript.setWorldId(QWebEngineScript::MainWorld);
      bridgeScript.setSourceCode(QString::fromUtf8(bridgeFile.readAll()));
      webView_->page()->scripts().insert(bridgeScript);
    }
  }

  loadInitialUrl();
}

void MainWindow::loadInitialUrl() {
  const QUrl url = resolveInitialUrl();
  if (url.isValid()) {
    webView_->setUrl(url);
  } else {
    webView_->setUrl(QUrl(QStringLiteral("about:blank")));
  }
}

QUrl MainWindow::resolveInitialUrl() {
  QCommandLineParser parser;
  parser.setApplicationDescription("Rewritto-ide Qt shell (prototype)");
  parser.addHelpOption();
  QCommandLineOption urlOpt(QStringList{QStringLiteral("u"), QStringLiteral("url")},
                            QStringLiteral("URL to load (e.g. http://127.0.0.1:PORT/)"),
                            QStringLiteral("url"));
  parser.addOption(urlOpt);
  parser.process(*QCoreApplication::instance());

  if (!parser.isSet(urlOpt)) {
    return {};
  }
  return QUrl::fromUserInput(parser.value(urlOpt));
}

void MainWindow::closeEvent(QCloseEvent* event) {
  qtHost_->notifyAboutToClose();
  QMainWindow::closeEvent(event);
}
