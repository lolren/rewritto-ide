#pragma once

#include <QWidget>

class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QTreeWidget;

class SerialPlotterWidget final : public QWidget {
  Q_OBJECT

 public:
  explicit SerialPlotterWidget(QWidget* parent = nullptr);

  void setCurrentPort(QString port);
  QString currentPort() const;
  bool autoReconnectEnabled() const;

 public slots:
  void setConnected(bool connected);
  void appendData(QByteArray data);
  void showError(QString message);

 signals:
  void connectRequested(QString port, int baudRate);
  void disconnectRequested();

 private:
  class PlotWidget;

  QString currentPort_;
  QByteArray lineBuffer_;
  bool paused_ = false;

  QPushButton* connectButton_ = nullptr;
  QLabel* portLabel_ = nullptr;
  QComboBox* baudCombo_ = nullptr;
  QCheckBox* autoReconnectCheck_ = nullptr;
  QCheckBox* autoScaleCheck_ = nullptr;
  QCheckBox* freezeRangeCheck_ = nullptr;
  QDoubleSpinBox* minYSpin_ = nullptr;
  QDoubleSpinBox* maxYSpin_ = nullptr;
  QPushButton* pauseButton_ = nullptr;
  QPushButton* saveButton_ = nullptr;
  QPushButton* clearButton_ = nullptr;
  QLabel* statusLabel_ = nullptr;
  PlotWidget* plot_ = nullptr;
  QTreeWidget* legend_ = nullptr;

  void setPaused(bool paused);
  void rebuildLegend();
};
