#include "arduino_cli.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

namespace {
constexpr int kMaxDiagnosticExtraLines = 2;
}  // namespace

ArduinoCli::ArduinoCli(QObject* parent) : QObject(parent) {
  process_ = new QProcess(this);
  process_->setProcessChannelMode(QProcess::MergedChannels);

  arduinoCliPath_ = resolveDefaultArduinoCliPath();
  arduinoCliConfigPath_ = resolveDefaultArduinoCliConfigPath();
  diagnosticWithColumn_ = QRegularExpression(
      R"(^(.+?):(\d+):(\d+):\s*(warning|error|note|fatal error):\s*(.*)$)");
  diagnosticNoColumn_ =
      QRegularExpression(R"(^(.+?):(\d+):\s*(warning|error|note|fatal error):\s*(.*)$)");
  toolDiagnostic_ =
      QRegularExpression(R"(^(.+?):\s*(warning|error|note|fatal error):\s*(.*)$)");
  libraryConflictStart_ = QRegularExpression(
      R"(^Multiple libraries were found for\s+\".*\"$)");
  collect2Error_ = QRegularExpression(R"(^collect2:\s*(error):\s*(.*)$)");
  undefinedReferenceObjectLine_ =
      QRegularExpression(R"(^(.+?):(?:\([^)]*\):\s*)?(undefined reference to.*)$)");
  undefinedReferenceWithLine_ =
      QRegularExpression(R"(^(.+?):(\d+):\s*(undefined reference to.*)$)");
  ldCannotFindLine_ =
      QRegularExpression(R"(^(?:.+\/)?ld:\s*(?:error:\s*)?(cannot find .+)$)");
  exitStatusLine_ = QRegularExpression(R"(^exit status\s+\d+\s*$)");
  compilationErrorLine_ = QRegularExpression(R"(^Compilation error:.*$)");

  connect(process_, &QProcess::readyReadStandardOutput, this, [this] {
    const QString chunk =
        QString::fromLocal8Bit(process_->readAllStandardOutput());
    emit outputReceived(chunk);
    consumeText(chunk);
  });
  connect(process_, &QProcess::readyReadStandardError, this, [this] {
    const QString chunk =
        QString::fromLocal8Bit(process_->readAllStandardError());
    emit outputReceived(chunk);
    consumeText(chunk);
  });
  connect(process_, &QProcess::finished, this,
          [this](int exitCode, QProcess::ExitStatus exitStatus) {
            flushPendingDiagnostic();
            emit finished(exitCode, exitStatus);
          });
  connect(process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart) {
      emit outputReceived(tr("Failed to start arduino-cli. Please ensure it is installed and in your PATH.\n"));
      emit finished(-1, QProcess::NormalExit);
    }
  });
}

void ArduinoCli::setArduinoCliPath(QString path) {
  arduinoCliPath_ = std::move(path);
}

QString ArduinoCli::arduinoCliPath() const {
  return arduinoCliPath_;
}

QString ArduinoCli::arduinoCliConfigPath() const {
  // When using the Snap-packaged arduino-cli (strict confinement), it cannot
  // read config files from hidden paths like ~/.config. Returning an empty path
  // here prevents downstream tools (e.g. arduino-language-server) from passing
  // an unusable -cli-config.
  const QString configPath = arduinoCliConfigPath_.trimmed();
  if (configPath.isEmpty()) {
    return {};
  }

  QString exe = arduinoCliPath_.trimmed();
  if (exe.isEmpty()) {
    return {};
  }
  if (QDir::isRelativePath(exe)) {
    const QString found = QStandardPaths::findExecutable(exe);
    if (!found.isEmpty()) {
      exe = found;
    }
  }

  const bool isSnap = exe.startsWith(QStringLiteral("/snap/"));
  if (!isSnap) {
    return configPath;
  }

  const QString absConfig = QFileInfo(configPath).absoluteFilePath();
  const QString allowedPrefix =
      QDir(QDir::homePath()).absoluteFilePath(QStringLiteral("snap/arduino-cli/")) +
      QLatin1Char('/');
  if (absConfig.startsWith(allowedPrefix)) {
    return absConfig;
  }

  return {};
}

QStringList ArduinoCli::withGlobalFlags(QStringList args) const {
  const QString configPath = arduinoCliConfigPath();
  if (!configPath.isEmpty()) {
    args.prepend(configPath);
    args.prepend(QStringLiteral("--config-file"));
  }
  return args;
}

bool ArduinoCli::isRunning() const {
  return process_->state() != QProcess::NotRunning;
}

void ArduinoCli::stop() {
  if (!isRunning()) {
    return;
  }
  process_->kill();
}

void ArduinoCli::consumeText(const QString& chunk) {
  lineBuffer_.append(chunk);
  while (true) {
    const int idx = lineBuffer_.indexOf('\n');
    if (idx < 0) {
      break;
    }
    QString line = lineBuffer_.left(idx);
    lineBuffer_.remove(0, idx + 1);
    consumeLine(std::move(line));
  }
}

void ArduinoCli::consumeLine(QString line) {
  if (line.endsWith('\r')) {
    line.chop(1);
  }

  auto normalizeSeverity = [](QString sev) {
    sev = sev.trimmed().toLower();
    if (sev == QStringLiteral("fatal error")) {
      return QStringLiteral("error");
    }
    return sev;
  };

  const QString trimmed = line.trimmed();
  if (trimmed.isEmpty()) {
    flushPendingDiagnostic();
    includeContextLines_.clear();
    return;
  }

  auto prefixContext = [this](QString msg) {
    if (includeContextLines_.isEmpty()) {
      return msg;
    }
    msg = includeContextLines_.join(QLatin1Char('\n')) + QLatin1Char('\n') + msg;
    includeContextLines_.clear();
    return msg;
  };

  const bool isIncludeContextStart =
      trimmed.startsWith(QStringLiteral("In file included from "));
  const bool isIncludeContextCont =
      !includeContextLines_.isEmpty() && trimmed.startsWith(QStringLiteral("from "));
  if (isIncludeContextStart || isIncludeContextCont) {
    flushPendingDiagnostic();
    includeContextLines_.push_back(trimmed);
    constexpr int kMaxContextLines = 3;
    while (includeContextLines_.size() > kMaxContextLines) {
      includeContextLines_.removeFirst();
    }
    return;
  }

  const bool isContinuation =
      hasPendingDiagnostic_ &&
      (!line.isEmpty() &&
       (line.at(0).isSpace() || line.at(0) == '^' || line.at(0) == '|'));
  if (isContinuation) {
    if (pendingDiagnostic_.extraLines < kMaxDiagnosticExtraLines) {
      pendingDiagnostic_.message += QLatin1Char('\n') + trimmed;
      ++pendingDiagnostic_.extraLines;
    }
    return;
  }

  auto match = diagnosticWithColumn_.match(trimmed);
  if (match.hasMatch()) {
    flushPendingDiagnostic();
    pendingDiagnostic_.filePath = match.captured(1);
    pendingDiagnostic_.line = match.captured(2).toInt();
    pendingDiagnostic_.column = match.captured(3).toInt();
    pendingDiagnostic_.severity = normalizeSeverity(match.captured(4));
    pendingDiagnostic_.message = prefixContext(match.captured(5));
    pendingDiagnostic_.extraLines = 0;
    hasPendingDiagnostic_ = true;
    return;
  }

  match = diagnosticNoColumn_.match(trimmed);
  if (match.hasMatch()) {
    flushPendingDiagnostic();
    pendingDiagnostic_.filePath = match.captured(1);
    pendingDiagnostic_.line = match.captured(2).toInt();
    pendingDiagnostic_.column = 0;
    pendingDiagnostic_.severity = normalizeSeverity(match.captured(3));
    pendingDiagnostic_.message = prefixContext(match.captured(4));
    pendingDiagnostic_.extraLines = 0;
    hasPendingDiagnostic_ = true;
    return;
  }

  match = toolDiagnostic_.match(trimmed);
  if (match.hasMatch()) {
    flushPendingDiagnostic();
    pendingDiagnostic_.filePath = match.captured(1);
    pendingDiagnostic_.line = 0;
    pendingDiagnostic_.column = 0;
    pendingDiagnostic_.severity = normalizeSeverity(match.captured(2));
    pendingDiagnostic_.message = prefixContext(match.captured(3));
    pendingDiagnostic_.extraLines = 0;
    hasPendingDiagnostic_ = true;
    return;
  }

  {
    static const QRegularExpression platformNotInstalled(
        R"(platform\s+(?:not\s+installed|not\s+found)\s*:\s*([A-Za-z0-9_.+\-]+:[A-Za-z0-9_.+\-]+))",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression platformIsNotInstalled(
        R"(platform\s+\"?([A-Za-z0-9_.+\-]+:[A-Za-z0-9_.+\-]+)\"?\s+is\s+not\s+installed)",
        QRegularExpression::CaseInsensitiveOption);

    QRegularExpressionMatch m = platformNotInstalled.match(trimmed);
    if (!m.hasMatch()) {
      m = platformIsNotInstalled.match(trimmed);
    }
    if (m.hasMatch()) {
      flushPendingDiagnostic();
      pendingDiagnostic_.filePath = QStringLiteral("Platform");
      pendingDiagnostic_.line = 0;
      pendingDiagnostic_.column = 0;
      pendingDiagnostic_.severity = QStringLiteral("error");
      pendingDiagnostic_.message = prefixContext(trimmed);
      pendingDiagnostic_.extraLines = 0;
      hasPendingDiagnostic_ = true;
      return;
    }
  }

  {
    static const QRegularExpression execNotFound(
        R"(exec:\s*\"([^\"]+)\":\s*executable file not found in\s+\$PATH)",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression forkExecNoSuchFile(
        R"(fork/exec\s+([^\s:]+):\s*no such file or directory)",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression shellNotFound(
        R"((?:^|\n)(?:sh|bash):\s*\d+:\s*([^\s:]+):\s*not found\s*$)",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression commandNotFound(
        R"((?:^|\n)([^\s:]+):\s*command not found\s*$)",
        QRegularExpression::CaseInsensitiveOption);

    QRegularExpressionMatch m = execNotFound.match(trimmed);
    if (!m.hasMatch()) {
      m = forkExecNoSuchFile.match(trimmed);
    }
    if (!m.hasMatch()) {
      m = shellNotFound.match(trimmed);
    }
    if (!m.hasMatch()) {
      m = commandNotFound.match(trimmed);
    }
    if (m.hasMatch()) {
      flushPendingDiagnostic();
      pendingDiagnostic_.filePath = m.captured(1).trimmed();
      pendingDiagnostic_.line = 0;
      pendingDiagnostic_.column = 0;
      pendingDiagnostic_.severity = QStringLiteral("error");
      pendingDiagnostic_.message = prefixContext(trimmed);
      pendingDiagnostic_.extraLines = 0;
      hasPendingDiagnostic_ = true;
      return;
    }
  }

  {
    static const QRegularExpression toolPrefix(
        R"(^(avrdude|bossac|dfu-util|esptool\.py|esptool|picotool|openocd)\s*:\s*(.*)$)");
    const QRegularExpressionMatch tm = toolPrefix.match(trimmed);
    if (tm.hasMatch()) {
      const QString tool = tm.captured(1).trimmed();
      const QString msg = tm.captured(2).trimmed();
      const QString lower = msg.toLower();

      QString sev;
      if (lower.contains(QStringLiteral("warning"))) {
        sev = QStringLiteral("warning");
      }
      if (lower.contains(QStringLiteral("error")) ||
          lower.contains(QStringLiteral("failed")) ||
          lower.contains(QStringLiteral("can't")) ||
          lower.contains(QStringLiteral("cannot")) ||
          lower.contains(QStringLiteral("no such file")) ||
          lower.contains(QStringLiteral("permission denied")) ||
          lower.contains(QStringLiteral("timed out")) ||
          lower.contains(QStringLiteral("timeout")) ||
          lower.contains(QStringLiteral("not in sync")) ||
          lower.contains(QStringLiteral("invalid"))) {
        sev = QStringLiteral("error");
      }

      if (!sev.isEmpty()) {
        flushPendingDiagnostic();
        pendingDiagnostic_.filePath = tool;
        pendingDiagnostic_.line = 0;
        pendingDiagnostic_.column = 0;
        pendingDiagnostic_.severity = sev;
        pendingDiagnostic_.message = prefixContext(msg.isEmpty() ? trimmed : msg);
        pendingDiagnostic_.extraLines = 0;
        hasPendingDiagnostic_ = true;
        return;
      }
    }
  }

  {
    const QString lower = trimmed.toLower();
    if (lower.startsWith(QStringLiteral("error during upload")) ||
        lower.startsWith(QStringLiteral("failed uploading")) ||
        lower.startsWith(QStringLiteral("uploading error")) ||
        lower.startsWith(QStringLiteral("failed to upload"))) {
      flushPendingDiagnostic();
      pendingDiagnostic_.filePath = QStringLiteral("Upload");
      pendingDiagnostic_.line = 0;
      pendingDiagnostic_.column = 0;
      pendingDiagnostic_.severity = QStringLiteral("error");
      pendingDiagnostic_.message = prefixContext(trimmed);
      pendingDiagnostic_.extraLines = 0;
      hasPendingDiagnostic_ = true;
      return;
    }
  }

  match = collect2Error_.match(trimmed);
  if (match.hasMatch()) {
    flushPendingDiagnostic();
    pendingDiagnostic_.filePath = QStringLiteral("collect2");
    pendingDiagnostic_.line = 0;
    pendingDiagnostic_.column = 0;
    pendingDiagnostic_.severity = normalizeSeverity(match.captured(1));
    pendingDiagnostic_.message = prefixContext(match.captured(2));
    pendingDiagnostic_.extraLines = 0;
    hasPendingDiagnostic_ = true;
    return;
  }

  if (libraryConflictStart_.match(trimmed).hasMatch()) {
    flushPendingDiagnostic();
    pendingDiagnostic_.filePath = QString{};
    pendingDiagnostic_.line = 0;
    pendingDiagnostic_.column = 0;
    pendingDiagnostic_.severity = QStringLiteral("note");
    pendingDiagnostic_.message = prefixContext(trimmed);
    pendingDiagnostic_.extraLines = 0;
    hasPendingDiagnostic_ = true;
    return;
  }

  match = undefinedReferenceWithLine_.match(trimmed);
  if (match.hasMatch()) {
    flushPendingDiagnostic();
    pendingDiagnostic_.filePath = match.captured(1).trimmed();
    pendingDiagnostic_.line = match.captured(2).toInt();
    pendingDiagnostic_.column = 0;
    pendingDiagnostic_.severity = QStringLiteral("error");
    pendingDiagnostic_.message = prefixContext(match.captured(3).trimmed());
    pendingDiagnostic_.extraLines = 0;
    hasPendingDiagnostic_ = true;
    return;
  }

  match = undefinedReferenceObjectLine_.match(trimmed);
  if (match.hasMatch()) {
    flushPendingDiagnostic();
    pendingDiagnostic_.filePath = match.captured(1);
    pendingDiagnostic_.line = 0;
    pendingDiagnostic_.column = 0;
    pendingDiagnostic_.severity = QStringLiteral("error");
    pendingDiagnostic_.message = prefixContext(match.captured(2));
    pendingDiagnostic_.extraLines = 0;
    hasPendingDiagnostic_ = true;
    return;
  }

  match = ldCannotFindLine_.match(trimmed);
  if (match.hasMatch()) {
    flushPendingDiagnostic();
    pendingDiagnostic_.filePath = QStringLiteral("ld");
    pendingDiagnostic_.line = 0;
    pendingDiagnostic_.column = 0;
    pendingDiagnostic_.severity = QStringLiteral("error");
    pendingDiagnostic_.message = prefixContext(match.captured(1));
    pendingDiagnostic_.extraLines = 0;
    hasPendingDiagnostic_ = true;
    return;
  }

  if (trimmed.contains(QStringLiteral("undefined reference to"))) {
    flushPendingDiagnostic();
    pendingDiagnostic_.filePath = QStringLiteral("ld");
    pendingDiagnostic_.line = 0;
    pendingDiagnostic_.column = 0;
    pendingDiagnostic_.severity = QStringLiteral("error");
    pendingDiagnostic_.message = prefixContext(trimmed);
    pendingDiagnostic_.extraLines = 0;
    hasPendingDiagnostic_ = true;
    return;
  }

  if (exitStatusLine_.match(trimmed).hasMatch()) {
    flushPendingDiagnostic();
    pendingDiagnostic_.filePath = QString{};
    pendingDiagnostic_.line = 0;
    pendingDiagnostic_.column = 0;
    pendingDiagnostic_.severity = QStringLiteral("error");
    pendingDiagnostic_.message = prefixContext(trimmed);
    pendingDiagnostic_.extraLines = 0;
    hasPendingDiagnostic_ = true;
    return;
  }

  if (compilationErrorLine_.match(trimmed).hasMatch()) {
    flushPendingDiagnostic();
    pendingDiagnostic_.filePath = QString{};
    pendingDiagnostic_.line = 0;
    pendingDiagnostic_.column = 0;
    pendingDiagnostic_.severity = QStringLiteral("error");
    pendingDiagnostic_.message = prefixContext(trimmed);
    pendingDiagnostic_.extraLines = 0;
    hasPendingDiagnostic_ = true;
    return;
  }

  includeContextLines_.clear();
  flushPendingDiagnostic();
}

void ArduinoCli::flushPendingDiagnostic() {
  if (!hasPendingDiagnostic_) {
    return;
  }
  emit diagnosticFound(pendingDiagnostic_.filePath, pendingDiagnostic_.line,
                       pendingDiagnostic_.column, pendingDiagnostic_.severity,
                       pendingDiagnostic_.message);
  hasPendingDiagnostic_ = false;
  pendingDiagnostic_ = PendingDiagnostic{};
}

void ArduinoCli::run(QStringList args, QString workingDirectory) {
  if (isRunning()) {
    emit outputReceived("arduino-cli is already running.\n");
    return;
  }

  if (arduinoCliPath_.isEmpty()) {
    emit outputReceived("arduino-cli path is not configured.\n");
    return;
  }

  args = withGlobalFlags(std::move(args));

  const QFileInfo cliInfo(arduinoCliPath_);
  const QString program =
      cliInfo.exists() ? cliInfo.absoluteFilePath() : arduinoCliPath_;

  if (!workingDirectory.isEmpty()) {
    process_->setWorkingDirectory(workingDirectory);
  } else {
    process_->setWorkingDirectory({});
  }

  emit outputReceived(QString("Running: %1 %2\n")
                          .arg(program, args.join(' ')));

  lineBuffer_.clear();
  hasPendingDiagnostic_ = false;
  pendingDiagnostic_ = PendingDiagnostic{};
  emit started();
  process_->start(program, args);
}

QString ArduinoCli::resolveDefaultArduinoCliPath() {
  const QString env = qEnvironmentVariable("ARDUINO_CLI_PATH");
  if (!env.isEmpty()) {
    return env;
  }

  const QString appDir = QCoreApplication::applicationDirPath();
  const QString bundled = appDir + QStringLiteral("/arduino-cli");
  if (QFileInfo::exists(bundled)) {
    return bundled;
  }

  // Development fallback: when running a native build from the build dir, reuse
  // the arduino-cli downloaded for AppImage packaging if present.
  const QStringList devCandidates = {
      QDir(appDir).absoluteFilePath(QStringLiteral(".tools/appimage/arduino-cli/arduino-cli")),
      QDir(appDir).absoluteFilePath(QStringLiteral("../.tools/appimage/arduino-cli/arduino-cli")),
      QDir(appDir).absoluteFilePath(
          QStringLiteral("../arduino-ide/qt-native-app/.tools/appimage/arduino-cli/arduino-cli")),
  };
  for (const QString& candidate : devCandidates) {
    const QFileInfo fi(candidate);
    if (fi.exists() && fi.isFile() && fi.isExecutable()) {
      return fi.absoluteFilePath();
    }
  }

  return QStringLiteral("arduino-cli");
}

QString ArduinoCli::resolveDefaultArduinoCliConfigPath() {
  const QString env = qEnvironmentVariable("ARDUINO_CLI_CONFIG_FILE");
  if (!env.isEmpty()) {
    return env;
  }

  const QString home = QDir::homePath();
  const QString path = home + QStringLiteral("/.arduinoIDE/arduino-cli.yaml");
  if (QFileInfo::exists(path)) {
    return path;
  }

  if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
    return {};
  }

  if (!QFileInfo::exists(path)) {
    const QString appConfig = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    const QStringList legacyCandidates = {
        appConfig.isEmpty() ? QString{} : (appConfig + QStringLiteral("/arduino-cli.yaml")),
        home + QStringLiteral("/.config/blingblink-ide/arduino-cli.yaml"),
        home + QStringLiteral("/.arduino15/arduino-cli.yaml"),
        home + QStringLiteral("/.config/arduino-ide-qt-native/arduino-cli.yaml"),
        home + QStringLiteral("/.config/Arduino/Arduino IDE (Qt Native)/arduino-cli.yaml"),
        home + QStringLiteral("/.config/Arduino/Arduino IDE/arduino-cli.yaml"),
    };

    for (const QString& legacyPath : legacyCandidates) {
      if (legacyPath.trimmed().isEmpty()) {
        continue;
      }
      if (!QFileInfo::exists(legacyPath) || !QFileInfo(legacyPath).isFile()) {
        continue;
      }
      QDir().mkpath(QFileInfo(path).absolutePath());
      if (QFile::exists(path)) {
        QFile::remove(path);
      }
      if (QFile::copy(legacyPath, path)) {
        break;
      }
    }
  }
  if (!QFileInfo::exists(path)) {
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
      const QString dataDir = home + QStringLiteral("/.arduino15");
      const QString userDir = home + QStringLiteral("/Arduino");
      QDir().mkpath(userDir);
      f.write("# arduino-cli configuration (shared with Arduino IDE)\n");
      f.write("directories:\n");
      f.write(QStringLiteral("    builtin:\n").toUtf8());
      f.write(QStringLiteral("        libraries: %1/libraries\n").arg(dataDir).toUtf8());
      f.write(QStringLiteral("    data: %1\n").arg(dataDir).toUtf8());
      f.write(QStringLiteral("    user: %1\n").arg(userDir).toUtf8());
      f.close();
    }
  }
  return path;
}
