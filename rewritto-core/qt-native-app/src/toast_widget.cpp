#include "toast_widget.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QTimer>
#include <QToolButton>

ToastWidget::ToastWidget(QWidget* parent) : QFrame(parent) {
  setObjectName("ToastWidget");
  setFrameShape(QFrame::StyledPanel);
  setFrameShadow(QFrame::Raised);
  setAutoFillBackground(true);

  label_ = new QLabel(this);
  label_->setWordWrap(true);
  label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  label_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

  actionButton_ = new QPushButton(this);
  actionButton_->hide();

  closeButton_ = new QToolButton(this);
  closeButton_->setIcon(style()->standardIcon(QStyle::SP_DialogCloseButton));
  closeButton_->setAutoRaise(true);
  closeButton_->setToolTip(tr("Dismiss"));

  auto* row = new QHBoxLayout(this);
  row->setContentsMargins(10, 8, 10, 8);
  row->setSpacing(8);
  row->addWidget(label_, 1);
  row->addWidget(actionButton_);
  row->addWidget(closeButton_);

  hideTimer_ = new QTimer(this);
  hideTimer_->setSingleShot(true);
  connect(hideTimer_, &QTimer::timeout, this, [this] { hide(); });

  connect(actionButton_, &QPushButton::clicked, this, [this] {
    const auto action = action_;
    hide();
    if (action) {
      action();
    }
  });

  connect(closeButton_, &QToolButton::clicked, this, [this] { hide(); });

  if (auto* p = parentWidget()) {
    p->installEventFilter(this);
  }

  hide();
}

void ToastWidget::showToast(QString message,
                            QString actionText,
                            std::function<void()> action,
                            int timeoutMs) {
  action_ = std::move(action);
  label_->setText(message.trimmed());

  const bool hasActionText = !actionText.trimmed().isEmpty();
  actionButton_->setVisible(hasActionText);
  actionButton_->setText(actionText.trimmed());

  adjustSize();
  reposition();
  show();
  raise();

  if (timeoutMs > 0) {
    hideTimer_->start(timeoutMs);
  } else {
    hideTimer_->stop();
  }
}

bool ToastWidget::eventFilter(QObject* watched, QEvent* event) {
  if (watched == parentWidget() && event &&
      (event->type() == QEvent::Resize || event->type() == QEvent::Move ||
       event->type() == QEvent::Show)) {
    reposition();
  }
  return QFrame::eventFilter(watched, event);
}

void ToastWidget::reposition() {
  auto* p = parentWidget();
  if (!p) {
    return;
  }
  constexpr int kMargin = 12;
  const QRect r = p->contentsRect();
  QSize s = sizeHint();
  // Keep a reasonable maximum width so long messages wrap.
  s.setWidth(std::min(s.width(), 440));
  resize(s);
  move(r.right() - width() - kMargin, r.bottom() - height() - kMargin);
}
