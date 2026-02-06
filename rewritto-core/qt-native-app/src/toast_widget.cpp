#include "toast_widget.h"

#include <algorithm>

#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QTimer>
#include <QToolButton>

ToastWidget::ToastWidget(QWidget* parent) : QFrame(parent) {
  setObjectName("ToastWidget");
  setFrameShape(QFrame::NoFrame);
  setFrameShadow(QFrame::Plain);
  setAutoFillBackground(true);
  setAttribute(Qt::WA_StyledBackground, true);
#ifdef Q_OS_WIN
  setProperty("platform", QStringLiteral("windows"));
  auto* shadow = new QGraphicsDropShadowEffect(this);
  shadow->setBlurRadius(24.0);
  shadow->setOffset(0.0, 4.0);
  shadow->setColor(QColor(0, 0, 0, 70));
  setGraphicsEffect(shadow);
#endif

  label_ = new QLabel(this);
  label_->setObjectName("toastLabel");
  label_->setWordWrap(true);
  label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  label_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

  actionButton_ = new QPushButton(this);
  actionButton_->setObjectName("toastActionButton");
  actionButton_->setAutoDefault(false);
  actionButton_->setDefault(false);
  actionButton_->hide();

  closeButton_ = new QToolButton(this);
  closeButton_->setObjectName("toastCloseButton");
  closeButton_->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
  closeButton_->setAutoRaise(true);
  closeButton_->setToolTip(tr("Dismiss"));

  auto* row = new QHBoxLayout(this);
  row->setContentsMargins(12, 10, 12, 10);
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
  const int availableWidth = std::max(260, r.width() - (kMargin * 2));
  const int targetMaxWidth = std::min(520, availableWidth);
  const int targetMinWidth = std::min(280, targetMaxWidth);
  s.setWidth(std::max(targetMinWidth, std::min(s.width(), targetMaxWidth)));
  resize(s);
  move(r.right() - width() - kMargin, r.bottom() - height() - kMargin);
}
