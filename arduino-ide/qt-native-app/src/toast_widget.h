#pragma once

#include <QFrame>

#include <functional>

class QLabel;
class QPushButton;
class QToolButton;
class QTimer;

class ToastWidget final : public QFrame {
  Q_OBJECT

 public:
  explicit ToastWidget(QWidget* parent = nullptr);

  void showToast(QString message,
                 QString actionText = {},
                 std::function<void()> action = {},
                 int timeoutMs = 5000);

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  QLabel* label_ = nullptr;
  QPushButton* actionButton_ = nullptr;
  QToolButton* closeButton_ = nullptr;
  QTimer* hideTimer_ = nullptr;
  std::function<void()> action_;

  void reposition();
};

