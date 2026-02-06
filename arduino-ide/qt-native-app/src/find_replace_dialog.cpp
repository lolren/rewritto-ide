#include "find_replace_dialog.h"

#include "editor_widget.h"

#include <QBoxLayout>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextDocument>

FindReplaceDialog::FindReplaceDialog(EditorWidget* editor, QWidget* parent)
    : QDialog(parent), editor_(editor) {
  setWindowTitle(tr("Find"));
  setModal(false);
  resize(560, 160);

  buildUi();
  wireSignals();
  updateModeUi();
}

void FindReplaceDialog::setReplaceMode(bool enabled) {
  replaceMode_ = enabled;
  updateModeUi();
}

bool FindReplaceDialog::replaceMode() const {
  return replaceMode_;
}

QString FindReplaceDialog::findText() const {
  return findEdit_ ? findEdit_->text() : QString{};
}

QString FindReplaceDialog::replaceText() const {
  return replaceEdit_ ? replaceEdit_->text() : QString{};
}

void FindReplaceDialog::setFindText(QString text) {
  if (findEdit_) {
    findEdit_->setText(std::move(text));
  }
}

void FindReplaceDialog::setReplaceText(QString text) {
  if (replaceEdit_) {
    replaceEdit_->setText(std::move(text));
  }
}

QTextDocument::FindFlags FindReplaceDialog::baseFindFlags() const {
  QTextDocument::FindFlags flags;
  if (caseSensitiveCheck_ && caseSensitiveCheck_->isChecked()) {
    flags |= QTextDocument::FindCaseSensitively;
  }
  if (wholeWordCheck_ && wholeWordCheck_->isChecked()) {
    flags |= QTextDocument::FindWholeWords;
  }
  return flags;
}

void FindReplaceDialog::focusFind() {
  if (findEdit_) {
    findEdit_->setFocus();
    findEdit_->selectAll();
  }
}

void FindReplaceDialog::buildUi() {
  findEdit_ = new QLineEdit(this);
  findEdit_->setPlaceholderText(tr("Find"));

  replaceEdit_ = new QLineEdit(this);
  replaceEdit_->setPlaceholderText(tr("Replace"));

  caseSensitiveCheck_ = new QCheckBox(tr("Case sensitive"), this);
  caseSensitiveCheck_->setChecked(false);

  wholeWordCheck_ = new QCheckBox(tr("Whole words"), this);
  wholeWordCheck_->setChecked(false);

  findNextButton_ = new QPushButton(tr("Find Next"), this);
  findPrevButton_ = new QPushButton(tr("Find Previous"), this);
  replaceButton_ = new QPushButton(tr("Replace"), this);
  replaceAllButton_ = new QPushButton(tr("Replace All"), this);

  statusLabel_ = new QLabel(tr("Ready"), this);
  statusLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);

  auto* grid = new QGridLayout();
  grid->addWidget(new QLabel(tr("Find:"), this), 0, 0);
  grid->addWidget(findEdit_, 0, 1, 1, 4);
  replaceLabel_ = new QLabel(tr("Replace:"), this);
  grid->addWidget(replaceLabel_, 1, 0);
  grid->addWidget(replaceEdit_, 1, 1, 1, 4);
  grid->addWidget(caseSensitiveCheck_, 2, 1);
  grid->addWidget(wholeWordCheck_, 2, 2);
  grid->setColumnStretch(1, 1);

  auto* buttonsRow = new QHBoxLayout();
  buttonsRow->addWidget(findNextButton_);
  buttonsRow->addWidget(findPrevButton_);
  buttonsRow->addStretch(1);
  buttonsRow->addWidget(replaceButton_);
  buttonsRow->addWidget(replaceAllButton_);

  auto* closeButtons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  connect(closeButtons, &QDialogButtonBox::rejected, this, &QDialog::close);

  auto* layout = new QVBoxLayout(this);
  layout->addLayout(grid);
  layout->addLayout(buttonsRow);
  layout->addWidget(statusLabel_);
  layout->addWidget(closeButtons);
}

void FindReplaceDialog::wireSignals() {
  auto emitFlags = [this] { emit findFlagsChanged(baseFindFlags()); };

  auto updateEnabled = [this] {
    const bool hasFind = !findText().trimmed().isEmpty();
    findNextButton_->setEnabled(hasFind);
    findPrevButton_->setEnabled(hasFind);
    replaceButton_->setEnabled(replaceMode_ && hasFind);
    replaceAllButton_->setEnabled(replaceMode_ && hasFind);
  };

  connect(findEdit_, &QLineEdit::returnPressed, this, [this] { doFind(false); });
  connect(findNextButton_, &QPushButton::clicked, this, [this] { doFind(false); });
  connect(findPrevButton_, &QPushButton::clicked, this, [this] { doFind(true); });

  connect(replaceEdit_, &QLineEdit::returnPressed, this,
          [this] { doReplaceOne(); });
  connect(replaceButton_, &QPushButton::clicked, this, [this] { doReplaceOne(); });
  connect(replaceAllButton_, &QPushButton::clicked, this, [this] { doReplaceAll(); });

  connect(findEdit_, &QLineEdit::textChanged, this,
          [this, updateEnabled](const QString& text) {
    statusLabel_->setText(tr("Ready"));
    emit findTextChanged(text);
    updateEnabled();
  });

  connect(replaceEdit_, &QLineEdit::textChanged, this,
          [this](const QString& text) { emit replaceTextChanged(text); });

  connect(caseSensitiveCheck_, &QCheckBox::toggled, this, [this, emitFlags](bool) {
    statusLabel_->setText(tr("Ready"));
    emitFlags();
  });
  connect(wholeWordCheck_, &QCheckBox::toggled, this, [this, emitFlags](bool) {
    statusLabel_->setText(tr("Ready"));
    emitFlags();
  });
  emitFlags();

  updateEnabled();
}

void FindReplaceDialog::updateModeUi() {
  setWindowTitle(replaceMode_ ? tr("Find/Replace") : tr("Find"));

  if (replaceLabel_) {
    replaceLabel_->setVisible(replaceMode_);
  }
  replaceEdit_->setVisible(replaceMode_);
  replaceButton_->setVisible(replaceMode_);
  replaceAllButton_->setVisible(replaceMode_);

  const bool hasFind = !findText().trimmed().isEmpty();
  findNextButton_->setEnabled(hasFind);
  findPrevButton_->setEnabled(hasFind);
  replaceButton_->setEnabled(replaceMode_ && hasFind);
  replaceAllButton_->setEnabled(replaceMode_ && hasFind);
}

void FindReplaceDialog::doFind(bool backward) {
  if (!editor_) {
    return;
  }
  const QString text = findText();
  if (text.trimmed().isEmpty()) {
    return;
  }

  QTextDocument::FindFlags flags = baseFindFlags();
  if (backward) {
    flags |= QTextDocument::FindBackward;
  }

  if (editor_->find(text, flags)) {
    statusLabel_->setText(tr("Match found"));
  } else {
    statusLabel_->setText(tr("No match"));
  }
}

void FindReplaceDialog::doReplaceOne() {
  if (!editor_) {
    return;
  }
  const QString find = findText();
  if (find.trimmed().isEmpty()) {
    return;
  }

  const QTextDocument::FindFlags flags = baseFindFlags();

  if (editor_->replaceOne(find, replaceText(), flags)) {
    statusLabel_->setText(tr("Replaced"));
  } else {
    statusLabel_->setText(tr("No match"));
  }
}

void FindReplaceDialog::doReplaceAll() {
  if (!editor_) {
    return;
  }
  const QString find = findText();
  if (find.trimmed().isEmpty()) {
    return;
  }

  const QTextDocument::FindFlags flags = baseFindFlags();

  const int count = editor_->replaceAll(find, replaceText(), flags);
  statusLabel_->setText(tr("Replaced %1 occurrences").arg(count));
}
