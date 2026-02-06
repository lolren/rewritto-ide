#include "serial_plotter_widget.h"

#include "serial_plot_parser.h"
#include "serial_plot_range.h"

#include <cmath>

#include <QComboBox>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSettings>
#include <QTreeWidget>
#include <QVBoxLayout>

class SerialPlotterWidget::PlotWidget final : public QWidget {
 public:
  explicit PlotWidget(QWidget* parent = nullptr) : QWidget(parent) {
    setMinimumHeight(180);
  }

  void setAutoScaleEnabled(bool enabled) {
    rangeController_.setAutoScaleEnabled(enabled);
    update();
  }

  void setFreezeRangeEnabled(bool enabled) {
    const SerialPlotYRange autoRange = serialPlotComputeAutoRange(series_);
    rangeController_.updateAutoRange(autoRange);
    rangeController_.setFreezeEnabled(enabled);
    update();
  }

  void setManualRange(double minY, double maxY) {
    rangeController_.setManualRange(minY, maxY);
    update();
  }

  void clear() {
    series_.clear();
    seriesVisible_.clear();
    seriesLabels_.clear();
    labelToIndex_.clear();
    sampleCount_ = 0;
    update();
  }

  QStringList seriesLabels() const {
    return seriesLabels_;
  }

  bool isSeriesVisible(int index) const {
    if (index < 0 || index >= seriesVisible_.size()) {
      return true;
    }
    return seriesVisible_.at(index);
  }

  void setSeriesVisible(int index, bool visible) {
    if (index < 0 || index >= seriesVisible_.size()) {
      return;
    }
    if (seriesVisible_[index] == visible) {
      return;
    }
    seriesVisible_[index] = visible;
    update();
  }

  QColor seriesColor(int index) const {
    return QColor::fromHsv((index * 60) % 360, 180, 230);
  }

  QString toCsv() const {
    const int seriesCount = series_.size();
    if (seriesCount <= 0) {
      return {};
    }

    int sampleCount = 0;
    for (const auto& s : series_) {
      sampleCount = std::max(sampleCount, static_cast<int>(s.size()));
    }
    if (sampleCount <= 0) {
      return {};
    }

    const QLocale c = QLocale::c();
    QString out;
    out.reserve(sampleCount * seriesCount * 12);

    out += QStringLiteral("sample");
    for (int i = 0; i < seriesCount; ++i) {
      QString label =
          i < seriesLabels_.size() ? seriesLabels_.at(i) : QString{};
      label = label.trimmed();
      if (label.isEmpty()) {
        label = QStringLiteral("series_%1").arg(i + 1);
      }
      label.replace(',', '_');
      label.replace('\n', ' ');
      out += QStringLiteral(",%1").arg(label);
    }
    out += QLatin1Char('\n');

    for (int row = 0; row < sampleCount; ++row) {
      out += QString::number(row);
      for (int i = 0; i < seriesCount; ++i) {
        out += QLatin1Char(',');
        if (row < series_[i].size()) {
          const double v = series_[i][row];
          if (!std::isnan(v)) {
            out += c.toString(v, 'g', 12);
          }
        }
      }
      out += QLatin1Char('\n');
    }
    return out;
  }

  bool addSample(const QStringList& labels, const QVector<double>& values) {
    if (values.isEmpty()) {
      return false;
    }

    const int oldSeriesCount = series_.size();

    QStringList effectiveLabels = labels;
    if (!effectiveLabels.isEmpty() && effectiveLabels.size() != values.size()) {
      effectiveLabels.clear();
    }
    if (effectiveLabels.isEmpty()) {
      effectiveLabels.reserve(values.size());
      for (int i = 0; i < values.size(); ++i) {
        effectiveLabels.push_back(QStringLiteral("series_%1").arg(i + 1));
      }
    }

    for (const QString& label : effectiveLabels) {
      const QString key = label.trimmed();
      if (key.isEmpty() || labelToIndex_.contains(key)) {
        continue;
      }
      const int idx = series_.size();
      labelToIndex_.insert(key, idx);
      seriesLabels_.push_back(key);
      seriesVisible_.push_back(true);
      QVector<double> vec(sampleCount_, std::nan(""));
      series_.push_back(vec);
    }

    for (auto& s : series_) {
      s.push_back(std::nan(""));
    }
    ++sampleCount_;

    for (int i = 0; i < effectiveLabels.size(); ++i) {
      const QString key = effectiveLabels.at(i).trimmed();
      if (key.isEmpty()) {
        continue;
      }
      const int idx = labelToIndex_.value(key, -1);
      if (idx < 0 || idx >= series_.size() || series_[idx].isEmpty()) {
        continue;
      }
      series_[idx].last() = values.at(i);
    }

    if (sampleCount_ > maxSamples_) {
      const int removeCount = sampleCount_ - maxSamples_;
      for (auto& s : series_) {
        if (removeCount > 0 && removeCount <= s.size()) {
          s.remove(0, removeCount);
        }
      }
      sampleCount_ = maxSamples_;
    }
    update();
    return series_.size() != oldSeriesCount;
  }

 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    p.fillRect(rect(), palette().base());
    p.setPen(palette().mid().color());
    p.drawRect(rect().adjusted(0, 0, -1, -1));

    const QRect plotRect = rect().adjusted(36, 10, -10, -26);
    if (plotRect.width() <= 10 || plotRect.height() <= 10) {
      return;
    }

    const SerialPlotYRange autoRange = serialPlotComputeAutoRange(series_);
    if (!autoRange.hasValue) {
      p.setPen(palette().text().color());
      p.drawText(plotRect, Qt::AlignCenter, tr("(no data)"));
      return;
    }

    rangeController_.updateAutoRange(autoRange);
    const SerialPlotYRange drawRange = rangeController_.currentRange();
    const double minY = drawRange.minY;
    const double maxY = drawRange.maxY;

    // Grid
    p.setPen(QPen(palette().mid().color(), 1, Qt::DotLine));
    for (int i = 0; i <= 4; ++i) {
      const int y = plotRect.top() + (plotRect.height() * i) / 4;
      p.drawLine(plotRect.left(), y, plotRect.right(), y);
    }

    // Labels
    p.setPen(palette().text().color());
    p.drawText(QRect(plotRect.left() - 34, plotRect.top(), 32, 14),
               Qt::AlignRight | Qt::AlignVCenter, QString::number(maxY, 'g', 4));
    p.drawText(QRect(plotRect.left() - 34, plotRect.bottom() - 14, 32, 14),
               Qt::AlignRight | Qt::AlignVCenter, QString::number(minY, 'g', 4));

    const int sampleCount = series_.isEmpty() ? 0 : series_.first().size();
    if (sampleCount < 2) {
      return;
    }

    auto mapY = [&](double v) {
      const double t = (v - minY) / (maxY - minY);
      return plotRect.bottom() - static_cast<int>(t * plotRect.height());
    };
    auto mapX = [&](int idx) {
      const double t = static_cast<double>(idx) / static_cast<double>(sampleCount - 1);
      return plotRect.left() + static_cast<int>(t * plotRect.width());
    };

    // Series
    for (int si = 0; si < series_.size(); ++si) {
      if (si < seriesVisible_.size() && !seriesVisible_.at(si)) {
        continue;
      }
      const QColor c = seriesColor(si);
      QPen pen(c, 2);
      p.setPen(pen);

      QPainterPath path;
      bool started = false;
      for (int i = 0; i < series_[si].size(); ++i) {
        const double v = series_[si][i];
        if (std::isnan(v)) {
          started = false;
          continue;
        }
        const QPoint pt(mapX(i), mapY(v));
        if (!started) {
          path.moveTo(pt);
          started = true;
        } else {
          path.lineTo(pt);
        }
      }
      p.drawPath(path);
    }
  }

 private:
  QVector<QVector<double>> series_;
  QVector<bool> seriesVisible_;
  QStringList seriesLabels_;
  QHash<QString, int> labelToIndex_;
  int sampleCount_ = 0;
  int maxSamples_ = 250;
  SerialPlotRangeController rangeController_;
};

SerialPlotterWidget::SerialPlotterWidget(QWidget* parent) : QWidget(parent) {
  connectButton_ = new QPushButton(tr("Connect"), this);
  connectButton_->setCheckable(true);

  portLabel_ = new QLabel(tr("Port: (none)"), this);

  baudCombo_ = new QComboBox(this);
  const QVector<int> baudRates = {300,   600,   1200,  2400,   4800,   9600,  14400,
                                  19200, 28800, 38400, 57600,  115200, 230400, 460800,
                                  921600};
  for (int b : baudRates) {
    baudCombo_->addItem(QString::number(b), b);
  }
  baudCombo_->setCurrentText("115200");

  autoReconnectCheck_ = new QCheckBox(tr("Auto reconnect"), this);
  autoReconnectCheck_->setChecked(false);
  autoReconnectCheck_->setObjectName("serialPlotterAutoReconnect");

  autoScaleCheck_ = new QCheckBox(tr("Autoscale"), this);
  autoScaleCheck_->setChecked(true);
  autoScaleCheck_->setObjectName("serialPlotterAutoscale");

  freezeRangeCheck_ = new QCheckBox(tr("Freeze range"), this);
  freezeRangeCheck_->setChecked(false);
  freezeRangeCheck_->setObjectName("serialPlotterFreezeRange");

  minYSpin_ = new QDoubleSpinBox(this);
  minYSpin_->setObjectName("serialPlotterMinY");
  minYSpin_->setDecimals(4);
  minYSpin_->setRange(-1e9, 1e9);
  minYSpin_->setSingleStep(0.1);
  minYSpin_->setKeyboardTracking(false);
  minYSpin_->setValue(0.0);

  maxYSpin_ = new QDoubleSpinBox(this);
  maxYSpin_->setObjectName("serialPlotterMaxY");
  maxYSpin_->setDecimals(4);
  maxYSpin_->setRange(-1e9, 1e9);
  maxYSpin_->setSingleStep(0.1);
  maxYSpin_->setKeyboardTracking(false);
  maxYSpin_->setValue(1.0);

  pauseButton_ = new QPushButton(tr("Pause"), this);
  pauseButton_->setCheckable(true);

  saveButton_ = new QPushButton(tr("Save"), this);

  clearButton_ = new QPushButton(tr("Clear"), this);
  statusLabel_ = new QLabel(tr("Disconnected"), this);

  plot_ = new PlotWidget(this);
  plot_->setObjectName("serialPlotterPlot");

  legend_ = new QTreeWidget(this);
  legend_->setObjectName("serialPlotterLegend");
  legend_->setHeaderHidden(true);
  legend_->setRootIsDecorated(false);
  legend_->setItemsExpandable(false);
  legend_->setUniformRowHeights(true);
  legend_->setSelectionMode(QAbstractItemView::NoSelection);
  legend_->setMinimumWidth(170);
  legend_->setMaximumWidth(260);
  connect(legend_, &QTreeWidget::itemChanged, this,
          [this](QTreeWidgetItem* item, int column) {
            if (!item || column != 0 || !plot_) {
              return;
            }
            const int idx = item->data(0, Qt::UserRole).toInt();
            plot_->setSeriesVisible(idx, item->checkState(0) == Qt::Checked);
          });

  auto* topRow = new QHBoxLayout();
  topRow->addWidget(connectButton_);
  topRow->addWidget(portLabel_);
  topRow->addStretch(1);
  topRow->addWidget(new QLabel(tr("Baud:"), this));
  topRow->addWidget(baudCombo_);
  topRow->addSpacing(12);
  topRow->addWidget(autoReconnectCheck_);
  topRow->addSpacing(12);
  topRow->addWidget(autoScaleCheck_);
  topRow->addWidget(freezeRangeCheck_);
  topRow->addWidget(new QLabel(tr("Y:"), this));
  topRow->addWidget(minYSpin_);
  topRow->addWidget(new QLabel(tr("to"), this));
  topRow->addWidget(maxYSpin_);
  topRow->addSpacing(12);
  topRow->addWidget(pauseButton_);
  topRow->addWidget(saveButton_);
  topRow->addWidget(clearButton_);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(6, 6, 6, 6);
  layout->addLayout(topRow);
  layout->addWidget(statusLabel_);
  {
    auto* row = new QHBoxLayout();
    row->addWidget(plot_, 1);
    row->addWidget(legend_);
    layout->addLayout(row, 1);
  }

  {
    QSettings settings;
    settings.beginGroup("SerialPlotter");
    const int baud = settings.value("baud", 115200).toInt();
    const bool autoReconnect = settings.value("autoReconnect", false).toBool();
    const bool paused = settings.value("paused", false).toBool();
    const bool autoscale = settings.value("autoscale", true).toBool();
    const bool freezeRange = settings.value("freezeRange", false).toBool();
    const double yMin = settings.value("yMin", 0.0).toDouble();
    const double yMax = settings.value("yMax", 1.0).toDouble();
    settings.endGroup();

    const int baudIdx = baudCombo_->findData(baud);
    if (baudIdx >= 0) {
      baudCombo_->setCurrentIndex(baudIdx);
    }
    autoReconnectCheck_->setChecked(autoReconnect);
    setPaused(paused);
    if (autoScaleCheck_) {
      autoScaleCheck_->setChecked(autoscale);
    }
    if (freezeRangeCheck_) {
      freezeRangeCheck_->setChecked(freezeRange);
    }
    if (minYSpin_) {
      minYSpin_->setValue(yMin);
    }
    if (maxYSpin_) {
      maxYSpin_->setValue(yMax);
    }
    if (plot_) {
      plot_->setManualRange(yMin, yMax);
      plot_->setAutoScaleEnabled(autoscale);
      plot_->setFreezeRangeEnabled(freezeRange);
    }
  }

  auto persistSettings = [this] {
    QSettings settings;
    settings.beginGroup("SerialPlotter");
    settings.setValue("baud", baudCombo_->currentData().toInt());
    settings.setValue("autoReconnect", autoReconnectCheck_->isChecked());
    settings.setValue("paused", paused_);
    if (autoScaleCheck_) {
      settings.setValue("autoscale", autoScaleCheck_->isChecked());
    }
    if (freezeRangeCheck_) {
      settings.setValue("freezeRange", freezeRangeCheck_->isChecked());
    }
    if (minYSpin_) {
      settings.setValue("yMin", minYSpin_->value());
    }
    if (maxYSpin_) {
      settings.setValue("yMax", maxYSpin_->value());
    }
    settings.endGroup();
  };

  connect(baudCombo_, &QComboBox::currentIndexChanged, this,
          [persistSettings](int) { persistSettings(); });
  connect(autoReconnectCheck_, &QCheckBox::toggled, this,
          [persistSettings](bool) { persistSettings(); });
  auto updateRangeUi = [this] {
    const bool autoscale = autoScaleCheck_ && autoScaleCheck_->isChecked();
    if (freezeRangeCheck_) {
      freezeRangeCheck_->setEnabled(autoscale);
      if (!autoscale) {
        freezeRangeCheck_->blockSignals(true);
        freezeRangeCheck_->setChecked(false);
        freezeRangeCheck_->blockSignals(false);
      }
    }
    if (minYSpin_) {
      minYSpin_->setEnabled(!autoscale);
    }
    if (maxYSpin_) {
      maxYSpin_->setEnabled(!autoscale);
    }
  };

  if (autoScaleCheck_) {
    connect(autoScaleCheck_, &QCheckBox::toggled, this,
            [this, persistSettings, updateRangeUi](bool enabled) {
              if (plot_) {
                plot_->setAutoScaleEnabled(enabled);
              }
              updateRangeUi();
              persistSettings();
            });
  }
  if (freezeRangeCheck_) {
    connect(freezeRangeCheck_, &QCheckBox::toggled, this,
            [this, persistSettings](bool enabled) {
              if (plot_) {
                plot_->setFreezeRangeEnabled(enabled);
              }
              persistSettings();
            });
  }
  if (minYSpin_) {
    connect(minYSpin_, &QDoubleSpinBox::valueChanged, this,
            [this, persistSettings](double) {
              if (plot_ && minYSpin_ && maxYSpin_) {
                plot_->setManualRange(minYSpin_->value(), maxYSpin_->value());
              }
              persistSettings();
            });
  }
  if (maxYSpin_) {
    connect(maxYSpin_, &QDoubleSpinBox::valueChanged, this,
            [this, persistSettings](double) {
              if (plot_ && minYSpin_ && maxYSpin_) {
                plot_->setManualRange(minYSpin_->value(), maxYSpin_->value());
              }
              persistSettings();
            });
  }

  connect(clearButton_, &QPushButton::clicked, this, [this] {
    lineBuffer_.clear();
    plot_->clear();
    if (legend_) {
      legend_->clear();
    }
  });
  connect(pauseButton_, &QPushButton::toggled, this,
          [this, persistSettings](bool paused) {
            setPaused(paused);
            persistSettings();
          });

  connect(saveButton_, &QPushButton::clicked, this, [this] {
    const QString csv = plot_ ? plot_->toCsv() : QString{};
    if (csv.isEmpty()) {
      statusLabel_->setText(tr("(no data)"));
      return;
    }

    const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    const QString initial =
        QDir::homePath() +
        QStringLiteral("/serial-plotter-%1.csv").arg(stamp);
    const QString filePath = QFileDialog::getSaveFileName(
        this, tr("Save Plot Data"), initial,
        tr("CSV Files (*.csv);;All Files (*)"));
    if (filePath.isEmpty()) {
      return;
    }

    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
      showError(tr("Could not write data file."));
      return;
    }
    f.write(csv.toUtf8());
    f.close();
    statusLabel_->setText(
        tr("Saved data to %1").arg(QFileInfo(filePath).fileName()));
  });

  connect(connectButton_, &QPushButton::toggled, this, [this](bool checked) {
    if (checked) {
      const int baud = baudCombo_->currentData().toInt();
      emit connectRequested(currentPort_, baud);
    } else {
      emit disconnectRequested();
    }
  });

  updateRangeUi();
  setConnected(false);
}

void SerialPlotterWidget::setCurrentPort(QString port) {
  currentPort_ = std::move(port);
  portLabel_->setText(currentPort_.isEmpty() ? tr("Port: (none)")
                                            : tr("Port: %1").arg(currentPort_));
  if (!connectButton_->isChecked()) {
    connectButton_->setEnabled(!currentPort_.isEmpty());
  }
}

QString SerialPlotterWidget::currentPort() const {
  return currentPort_;
}

bool SerialPlotterWidget::autoReconnectEnabled() const {
  return autoReconnectCheck_ && autoReconnectCheck_->isChecked();
}

void SerialPlotterWidget::setConnected(bool connected) {
  connectButton_->blockSignals(true);
  connectButton_->setChecked(connected);
  connectButton_->blockSignals(false);

  connectButton_->setText(connected ? tr("Disconnect") : tr("Connect"));
  connectButton_->setEnabled(connected || !currentPort_.isEmpty());
  baudCombo_->setEnabled(!connected);
  statusLabel_->setText(connected ? tr("Connected") : tr("Disconnected"));
}

void SerialPlotterWidget::appendData(QByteArray data) {
  if (paused_ || data.isEmpty()) {
    return;
  }
  lineBuffer_.append(data);

  SerialPlotParser parser;
  while (true) {
    const int idx = lineBuffer_.indexOf('\n');
    if (idx < 0) {
      return;
    }
    QByteArray line = lineBuffer_.left(idx);
    lineBuffer_.remove(0, idx + 1);
    if (!line.isEmpty() && line.endsWith('\r')) {
      line.chop(1);
    }
    const QString text = QString::fromUtf8(line).trimmed();
    if (text.isEmpty()) {
      continue;
    }
    const SerialPlotSample sample = parser.parseSample(text);
    if (!sample.values.isEmpty()) {
      if (plot_->addSample(sample.labels, sample.values)) {
        rebuildLegend();
      }
    }
  }
}

void SerialPlotterWidget::showError(QString message) {
  if (message.trimmed().isEmpty()) {
    return;
  }
  statusLabel_->setText(tr("Error: %1").arg(message));
}

void SerialPlotterWidget::setPaused(bool paused) {
  paused_ = paused;
  pauseButton_->blockSignals(true);
  pauseButton_->setChecked(paused);
  pauseButton_->blockSignals(false);
  pauseButton_->setText(paused ? tr("Resume") : tr("Pause"));
}

void SerialPlotterWidget::rebuildLegend() {
  if (!legend_ || !plot_) {
    return;
  }

  legend_->blockSignals(true);
  legend_->clear();

  const QStringList labels = plot_->seriesLabels();
  for (int i = 0; i < labels.size(); ++i) {
    const QString label = labels.at(i).trimmed();
    if (label.isEmpty()) {
      continue;
    }

    auto* item = new QTreeWidgetItem(legend_);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setText(0, label);
    item->setData(0, Qt::UserRole, i);
    item->setCheckState(0, plot_->isSeriesVisible(i) ? Qt::Checked : Qt::Unchecked);

    QPixmap swatch(10, 10);
    swatch.fill(plot_->seriesColor(i));
    item->setIcon(0, QIcon(swatch));
  }
  legend_->blockSignals(false);
}
