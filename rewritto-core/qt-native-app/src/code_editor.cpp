#include "code_editor.h"

#include <algorithm>
#include <QAbstractTextDocumentLayout>
#include <QColor>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QScrollBar>
#include <QSet>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextOption>
#include <QTimer>

namespace {
constexpr int kBreakpointGutterWidth = 16;
constexpr int kBreakpointUserStateEnabled = 101;
constexpr int kBreakpointUserStateDisabled = 102;

QString indentUnit(int tabSize, bool insertSpaces) {
  if (!insertSpaces) {
    return QStringLiteral("\t");
  }
  return QString(tabSize, QLatin1Char(' '));
}

int clampTabSize(int tabSize) {
  if (tabSize < 1) {
    return 1;
  }
  if (tabSize > 8) {
    return 8;
  }
  return tabSize;
}

struct BracketPair final {
  QChar open;
  QChar close;
};

bool bracketPairFor(QChar c, BracketPair* out, bool* outIsOpen) {
  if (!out || !outIsOpen) {
    return false;
  }

  switch (c.unicode()) {
    case '{':
      *out = {'{', '}'};
      *outIsOpen = true;
      return true;
    case '}':
      *out = {'{', '}'};
      *outIsOpen = false;
      return true;
    case '(':
      *out = {'(', ')'};
      *outIsOpen = true;
      return true;
    case ')':
      *out = {'(', ')'};
      *outIsOpen = false;
      return true;
    case '[':
      *out = {'[', ']'};
      *outIsOpen = true;
      return true;
    case ']':
      *out = {'[', ']'};
      *outIsOpen = false;
      return true;
    default:
      return false;
  }
}

struct ParsedSnippetPlaceholder final {
  int index = 0;
  int start = 0;
  int end = 0;
};

struct ParsedSnippet final {
  QString text;
  QVector<ParsedSnippetPlaceholder> placeholders;  // excludes $0
  int finalPos = -1;                              // relative to inserted text
};

QString unescapeSnippetText(QString text) {
  QString out;
  out.reserve(text.size());
  for (int i = 0; i < text.size(); ++i) {
    const QChar c = text.at(i);
    if (c == QLatin1Char('\\') && i + 1 < text.size()) {
      out.push_back(text.at(i + 1));
      ++i;
      continue;
    }
    out.push_back(c);
  }
  return out;
}

ParsedSnippet parseLspSnippet(const QString& input) {
  ParsedSnippet snip;

  QString out;
  out.reserve(input.size());

  const auto addPlaceholder = [&](int index, const QString& defaultText) {
    const int start = out.size();
    if (!defaultText.isEmpty()) {
      out += defaultText;
    }
    const int end = out.size();

    if (index == 0) {
      if (snip.finalPos < 0) {
        snip.finalPos = end;
      }
      return;
    }

    ParsedSnippetPlaceholder p;
    p.index = index;
    p.start = start;
    p.end = end;
    snip.placeholders.push_back(p);
  };

  for (int i = 0; i < input.size();) {
    const QChar c = input.at(i);

    if (c == QLatin1Char('\\') && i + 1 < input.size()) {
      out.push_back(input.at(i + 1));
      i += 2;
      continue;
    }

    if (c != QLatin1Char('$')) {
      out.push_back(c);
      ++i;
      continue;
    }

    if (i + 1 >= input.size()) {
      out.push_back(c);
      ++i;
      continue;
    }

    const QChar next = input.at(i + 1);
    if (next.isDigit()) {
      int j = i + 1;
      int index = 0;
      while (j < input.size() && input.at(j).isDigit()) {
        index = (index * 10) + input.at(j).digitValue();
        ++j;
      }
      addPlaceholder(index, QString{});
      i = j;
      continue;
    }

    if (next != QLatin1Char('{')) {
      out.push_back(c);
      ++i;
      continue;
    }

    // Parse ${...} until the next unescaped '}'.
    int j = i + 2;
    bool found = false;
    while (j < input.size()) {
      const QChar cj = input.at(j);
      if (cj == QLatin1Char('\\') && j + 1 < input.size()) {
        j += 2;
        continue;
      }
      if (cj == QLatin1Char('}')) {
        found = true;
        break;
      }
      ++j;
    }
    if (!found) {
      out.push_back(c);
      ++i;
      continue;
    }

    const QString content = input.mid(i + 2, j - (i + 2));
    int k = 0;
    int index = 0;
    while (k < content.size() && content.at(k).isDigit()) {
      index = (index * 10) + content.at(k).digitValue();
      ++k;
    }

    QString defaultText;
    if (k < content.size()) {
      const QChar sep = content.at(k);
      if (sep == QLatin1Char(':')) {
        defaultText = unescapeSnippetText(content.mid(k + 1));
      } else if (sep == QLatin1Char('|') && content.endsWith(QLatin1Char('|'))) {
        const QStringList choices =
            content.mid(k + 1, content.size() - k - 2).split(QLatin1Char(','));
        if (!choices.isEmpty()) {
          defaultText = unescapeSnippetText(choices.first());
        }
      }
    }

    addPlaceholder(index, defaultText);
    i = j + 1;
  }

  if (snip.finalPos < 0) {
    snip.finalPos = out.size();
  }
  snip.text = out;
  return snip;
}
}  // namespace

class CodeEditor::LineNumberArea final : public QWidget {
 public:
  explicit LineNumberArea(CodeEditor* editor) : QWidget(editor), editor_(editor) {}

  QSize sizeHint() const override {
    return QSize(editor_->lineNumberAreaWidth(), 0);
  }

 protected:
  void paintEvent(QPaintEvent* event) override { editor_->lineNumberAreaPaintEvent(event); }
  void mousePressEvent(QMouseEvent* event) override {
    if (!event || !editor_) {
      return;
    }
    if (event->button() != Qt::LeftButton) {
      QWidget::mousePressEvent(event);
      return;
    }
    if (event->position().x() > kBreakpointGutterWidth) {
      const int foldLeft = qMax(0, width() - CodeEditor::kFoldIndicatorWidth);
      if (event->position().x() >= foldLeft) {
        const int y = static_cast<int>(event->position().y());
        QTextBlock block = editor_->firstVisibleBlock();
        int top = static_cast<int>(editor_->blockBoundingGeometry(block)
                                       .translated(editor_->contentOffset())
                                       .top());
        int bottom = top + static_cast<int>(editor_->blockBoundingRect(block).height());

        while (block.isValid()) {
          if (y >= top && y <= bottom) {
            const int line = block.blockNumber() + 1;
            if (editor_->canFold(line)) {
              editor_->toggleFold(line);
              event->accept();
              return;
            }
            break;
          }
          block = block.next();
          top = bottom;
          bottom = top + static_cast<int>(editor_->blockBoundingRect(block).height());
        }
      }

      QWidget::mousePressEvent(event);
      return;
    }

    const int y = static_cast<int>(event->position().y());
    QTextBlock block = editor_->firstVisibleBlock();
    int top = static_cast<int>(
        editor_->blockBoundingGeometry(block).translated(editor_->contentOffset()).top());
    int bottom = top + static_cast<int>(editor_->blockBoundingRect(block).height());

    while (block.isValid()) {
      if (y >= top && y <= bottom) {
        editor_->toggleBreakpoint(block.blockNumber() + 1);
        event->accept();
        return;
      }
      block = block.next();
      top = bottom;
      bottom = top + static_cast<int>(editor_->blockBoundingRect(block).height());
    }

    QWidget::mousePressEvent(event);
  }

 private:
  CodeEditor* editor_ = nullptr;
};

CodeEditor::CodeEditor(QWidget* parent) : QPlainTextEdit(parent) {
  lineNumberArea_ = new LineNumberArea(this);
  foldRecomputeTimer_ = new QTimer(this);
  foldRecomputeTimer_->setSingleShot(true);
  foldRecomputeTimer_->setInterval(200);
  connect(foldRecomputeTimer_, &QTimer::timeout, this,
          [this] { recomputeFoldRegions(); });

  connect(this, &QPlainTextEdit::blockCountChanged, this,
          [this](int newBlockCount) { updateLineNumberAreaWidth(newBlockCount); });
  connect(this, &QPlainTextEdit::updateRequest, this,
          [this](const QRect& rect, int dy) { updateLineNumberArea(rect, dy); });
  connect(this, &QPlainTextEdit::cursorPositionChanged, this,
          [this] { updateBracketMatch(); });
  connect(this, &QPlainTextEdit::cursorPositionChanged, this,
          [this] { updateCurrentLineHighlight(); });
  connect(this, &QPlainTextEdit::textChanged, this,
          [this] { updateBracketMatch(); });
  connect(this, &QPlainTextEdit::textChanged, this,
          [this] {
            if (!foldingEnabled_) {
              return;
            }
            if (!foldRecomputeTimer_ || !document()) {
              recomputeFoldRegions();
              return;
            }
            constexpr int kImmediateFoldRecomputeMaxBlocks = 2000;
            if (document()->blockCount() <= kImmediateFoldRecomputeMaxBlocks) {
              recomputeFoldRegions();
              return;
            }
            foldRecomputeTimer_->start();
          });
  if (auto* doc = document()) {
    connect(doc, &QTextDocument::contentsChange, this,
            [this](int pos, int charsRemoved, int charsAdded) {
              updateSnippetRanges(pos, charsRemoved, charsAdded);
            });
  }
  connect(this, &QPlainTextEdit::cursorPositionChanged, this, [this] {
    if (!snippetActive_ || snippetSettingCursor_ || snippetNav_.isEmpty()) {
      return;
    }
    const int maxIdx = std::max(0, static_cast<int>(snippetNav_.size()) - 1);
    const int idx = std::clamp(snippetCurrent_, 0, maxIdx);
    const SnippetNavItem cur = snippetNav_.at(idx);
    const int pos = textCursor().position();
    if (pos < cur.start || pos > cur.end) {
      clearSnippetSession();
    }
  });

  setEditorSettings(tabSize_, insertSpaces_);
  recomputeFoldRegions();
  updateLineNumberAreaWidth(blockCount());
  lineNumberArea_->show();
}

void CodeEditor::setEditorSettings(int tabSize, bool insertSpaces) {
  tabSize_ = clampTabSize(tabSize);
  insertSpaces_ = insertSpaces;
  setTabStopDistance(tabSize_ * fontMetrics().horizontalAdvance(QLatin1Char(' ')));
}

int CodeEditor::tabSize() const {
  return tabSize_;
}

bool CodeEditor::insertSpaces() const {
  return insertSpaces_;
}

void CodeEditor::setShowIndentGuides(bool enabled) {
  if (showIndentGuides_ == enabled) {
    return;
  }
  showIndentGuides_ = enabled;
  viewport()->update();
}

bool CodeEditor::showIndentGuides() const {
  return showIndentGuides_;
}

void CodeEditor::setShowWhitespace(bool enabled) {
  if (showWhitespace_ == enabled) {
    return;
  }
  showWhitespace_ = enabled;

  if (auto* doc = document()) {
    QTextOption opt = doc->defaultTextOption();
    QTextOption::Flags flags = opt.flags();
    if (showWhitespace_) {
      flags |= QTextOption::ShowTabsAndSpaces;
    } else {
      flags &= ~QTextOption::ShowTabsAndSpaces;
    }
    opt.setFlags(flags);
    doc->setDefaultTextOption(opt);
  }

  viewport()->update();
}

bool CodeEditor::showWhitespace() const {
  return showWhitespace_;
}

void CodeEditor::setDiagnostics(const QVector<Diagnostic>& diagnostics) {
  diagnostics_ = diagnostics;
  QList<QTextEdit::ExtraSelection> selections = extraSelections();

  selections.erase(std::remove_if(selections.begin(), selections.end(),
                                  [](const QTextEdit::ExtraSelection& s) {
                                    const QString prop = s.format.property(kExtraPropertyRole).toString();
                                    return prop == QStringLiteral("diagnostic") || prop == QStringLiteral("diagnostic_bg");
                                  }),
                   selections.end());

  auto* doc = document();
  if (!doc) {
    setExtraSelections(selections);
    if (lineNumberArea_) lineNumberArea_->update();
    return;
  }

  const int docLen = doc->characterCount();
  if (docLen <= 0) {
    setExtraSelections(selections);
    if (lineNumberArea_) lineNumberArea_->update();
    return;
  }

  const auto positionFor = [doc](int line, int character) -> int {
    if (line < 0 || character < 0) {
      return -1;
    }
    const QTextBlock block = doc->findBlockByNumber(line);
    if (!block.isValid()) {
      return -1;
    }
    const int maxOffset = block.text().size();
    const int clamped = std::clamp(character, 0, maxOffset);
    return block.position() + clamped;
  };

  const auto colorForSeverity = [this](int severity) -> QColor {
    switch (severity) {
      case 1:
        return QColor(220, 50, 47);
      case 2:
        return QColor(203, 75, 22);
      case 3:
        return QColor(38, 139, 210);
      case 4:
        return QColor(88, 110, 117);
      default:
        return palette().mid().color();
    }
  };

  for (const Diagnostic& d : diagnostics) {
    int start = positionFor(d.startLine, d.startCharacter);
    int end = positionFor(d.endLine, d.endCharacter);
    if (start < 0 || end < 0) {
      continue;
    }
    if (end < start) {
      std::swap(start, end);
    }

    if (start == end) {
      if (end + 1 < docLen) {
        end = end + 1;
      } else if (start > 0) {
        start = start - 1;
      } else {
        continue;
      }
    }

    QTextCursor c(doc);
    c.setPosition(start);
    c.setPosition(end, QTextCursor::KeepAnchor);

    QTextEdit::ExtraSelection sel;
    sel.cursor = c;
    sel.format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
    sel.format.setUnderlineColor(colorForSeverity(d.severity));
    sel.format.setProperty(kExtraPropertyRole, QStringLiteral("diagnostic"));
    selections.push_back(sel);

    if (d.severity == 1) { // Error background
        QTextEdit::ExtraSelection bgSel;
        bgSel.cursor = c;
        bgSel.cursor.clearSelection();
        bgSel.cursor.select(QTextCursor::LineUnderCursor);
        bgSel.format.setBackground(QColor(220, 50, 47, 40));
        bgSel.format.setProperty(QTextFormat::FullWidthSelection, true);
        bgSel.format.setProperty(kExtraPropertyRole, QStringLiteral("diagnostic_bg"));
        selections.push_back(bgSel);
    }
  }

  setExtraSelections(selections);
  if (lineNumberArea_) {
    lineNumberArea_->update();
  }
}

void CodeEditor::clearDiagnostics() {
  diagnostics_.clear();
  QList<QTextEdit::ExtraSelection> selections = extraSelections();
  selections.erase(std::remove_if(selections.begin(), selections.end(),
                                  [](const QTextEdit::ExtraSelection& s) {
                                    const QString prop = s.format.property(kExtraPropertyRole).toString();
                                    return prop == QStringLiteral("diagnostic") || prop == QStringLiteral("diagnostic_bg");
                                  }),
                   selections.end());
  setExtraSelections(selections);
  if (lineNumberArea_) {
    lineNumberArea_->update();
  }
}

void CodeEditor::setNavigationLineHighlight(const QTextCursor& cursor) {
  QList<QTextEdit::ExtraSelection> selections = extraSelections();

  selections.erase(std::remove_if(selections.begin(), selections.end(),
                                  [](const QTextEdit::ExtraSelection& s) {
                                    return s.format.property(kExtraPropertyRole)
                                               .toString() == QStringLiteral("navline");
                                  }),
                   selections.end());

  QTextEdit::ExtraSelection sel;
  sel.cursor = cursor;
  sel.cursor.select(QTextCursor::LineUnderCursor);
  sel.format.setBackground(QColor(255, 255, 0, 64));
  sel.format.setProperty(kExtraPropertyRole, QStringLiteral("navline"));

  const auto insertBefore = std::find_if(
      selections.begin(), selections.end(), [](const QTextEdit::ExtraSelection& s) {
        return s.format.property(kExtraPropertyRole).toString() ==
               QStringLiteral("bracket");
      });
  selections.insert(insertBefore, sel);
  setExtraSelections(selections);
}

void CodeEditor::clearNavigationLineHighlight() {
  QList<QTextEdit::ExtraSelection> selections = extraSelections();
  selections.erase(std::remove_if(selections.begin(), selections.end(),
                                  [](const QTextEdit::ExtraSelection& s) {
                                    return s.format.property(kExtraPropertyRole)
                                               .toString() == QStringLiteral("navline");
                                  }),
                   selections.end());
  setExtraSelections(selections);
}

int CodeEditor::lineNumberAreaWidth() const {
  int digits = 1;
  int max = qMax(1, blockCount());
  while (max >= 10) {
    max /= 10;
    ++digits;
  }

  const int digitsSpace = 6 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
  return kBreakpointGutterWidth + digitsSpace + kFoldIndicatorWidth;
}

void CodeEditor::updateLineNumberAreaWidth(int) {
  setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditor::updateLineNumberArea(const QRect& rect, int dy) {
  if (dy) {
    lineNumberArea_->scroll(0, dy);
  } else {
    lineNumberArea_->update(0, rect.y(), lineNumberArea_->width(), rect.height());
  }

  if (rect.contains(viewport()->rect())) {
    updateLineNumberAreaWidth(0);
  }
}

void CodeEditor::resizeEvent(QResizeEvent* event) {
  QPlainTextEdit::resizeEvent(event);

  const QRect cr = contentsRect();
  lineNumberArea_->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent* event) {
  QPainter painter(lineNumberArea_);

  // Rewritto-ide style line number area background
  const QColor bgColor = palette().window().color().lighter(110);
  painter.fillRect(event->rect(), bgColor);

  QTextBlock block = firstVisibleBlock();
  int blockNumber = block.blockNumber();
  int top = static_cast<int>(blockBoundingGeometry(block).translated(contentOffset()).top());
  int bottom = top + static_cast<int>(blockBoundingRect(block).height());

  // Rewritto-ide style breakpoint color (teal accent)
  const QColor bpColor = QColor(0, 151, 156);
  const QColor bpDisabled = QColor(150, 150, 150);
  painter.setRenderHint(QPainter::Antialiasing, true);

  // Line number color - slightly muted for better readability
  const QColor lineNumberColor = palette().text().color();
  const QColor currentLineNumberColor = QColor(0, 151, 156);  // Teal accent for current line

  while (block.isValid() && top <= event->rect().bottom()) {
    const bool isCurrentBlock = (block == textCursor().block());
    if (block.isVisible() && bottom >= event->rect().top()) {
      const int state = block.userState();
      const bool hasBp = (state == kBreakpointUserStateEnabled ||
                         state == kBreakpointUserStateDisabled ||
                         state == (kBreakpointUserStateEnabled | 0x100) ||
                         state == (kBreakpointUserStateDisabled | 0x100) ||
                         state == (kBreakpointUserStateEnabled | 0x200) ||
                         state == (kBreakpointUserStateDisabled | 0x200));

      const bool isConditional = (state == (kBreakpointUserStateEnabled | 0x100) ||
                                 state == (kBreakpointUserStateDisabled | 0x100));
      const bool isLogpoint = (state == (kBreakpointUserStateEnabled | 0x200) ||
                              state == (kBreakpointUserStateDisabled | 0x200));
      const bool isEnabled = (state == kBreakpointUserStateEnabled ||
                             state == (kBreakpointUserStateEnabled | 0x100) ||
                             state == (kBreakpointUserStateEnabled | 0x200));

      if (hasBp) {
        const int d = qMax(10, fontMetrics().height() / 2);
        const int cx = kBreakpointGutterWidth / 2;
        const int cy = top + (fontMetrics().height() / 2);
        const QRect r(cx - d / 2, cy - d / 2, d, d);

        if (isLogpoint) {
          // Draw square for logpoint
          painter.setPen(isEnabled ? Qt::NoPen : QPen(bpDisabled, 2));
          painter.setBrush(isEnabled ? QColor(100, 150, 200) : Qt::NoBrush);
          painter.drawRect(r.adjusted(1, 1, -1, -1));
        } else if (isConditional) {
          // Draw diamond for conditional breakpoint
          QPolygon diamond;
          diamond << r.center()
                  << QPoint(r.right(), r.center().y())
                  << r.center()
                  << QPoint(r.left(), r.center().y());
          painter.setPen(isEnabled ? Qt::NoPen : QPen(bpDisabled, 2));
          painter.setBrush(isEnabled ? QColor(200, 150, 50) : Qt::NoBrush);
          painter.drawPolygon(diamond);
        } else {
          // Normal breakpoint - circle with Rewritto-ide style
          painter.setPen(isEnabled ? Qt::NoPen : QPen(bpDisabled, 2));
          painter.setBrush(isEnabled ? bpColor : Qt::NoBrush);
          painter.drawEllipse(r);
        }
        painter.setBrush(Qt::NoBrush);
      }

      // Draw diagnostic marker
      int worstSeverity = 0;
      for (const auto& d : diagnostics_) {
          if (d.startLine == blockNumber) {
              if (worstSeverity == 0 || d.severity < worstSeverity) {
                  worstSeverity = d.severity;
              }
          }
      }
      if (worstSeverity > 0) {
          const int cx = kBreakpointGutterWidth / 2;
          const int cy = top + (fontMetrics().height() / 2);
          const int d = qMax(8, fontMetrics().height() / 3);
          const QRect r(cx - d / 2, cy - d / 2, d, d);
          
          painter.save();
          if (worstSeverity == 1) { // Error
              painter.setBrush(QColor(220, 50, 47));
              painter.setPen(Qt::NoPen);
              painter.drawRect(r);
          } else if (worstSeverity == 2) { // Warning
              painter.setBrush(QColor(203, 75, 22));
              painter.setPen(Qt::NoPen);
              // Draw triangle for warning
              QPolygon tri;
              tri << QPoint(r.center().x(), r.top())
                  << QPoint(r.left(), r.bottom())
                  << QPoint(r.right(), r.bottom());
              painter.drawPolygon(tri);
          }
          painter.restore();
      }

      const QString number = QString::number(blockNumber + 1);
      painter.setPen(isCurrentBlock ? currentLineNumberColor : lineNumberColor);
      QFont numberFont = font();
      numberFont.setBold(isCurrentBlock);
      painter.setFont(numberFont);
      painter.drawText(kBreakpointGutterWidth, top,
                       lineNumberArea_->width() - kBreakpointGutterWidth -
                           kFoldIndicatorWidth - 3,
                       fontMetrics().height(), Qt::AlignRight, number);

      const int endBlockNumber = foldEndByStartBlock_.value(blockNumber, -1);
      if (endBlockNumber > blockNumber) {
        const bool folded = blockIsFolded(block);

        const int d = qMax(8, fontMetrics().height() / 2);
        const int cx = lineNumberArea_->width() - (kFoldIndicatorWidth / 2);
        const int cy = top + (fontMetrics().height() / 2);
        const QRect r(cx - d / 2, cy - d / 2, d, d);

        QPolygon poly;
        if (folded) {
          poly << QPoint(r.left(), r.top())
               << QPoint(r.right(), r.center().y())
               << QPoint(r.left(), r.bottom());
        } else {
          poly << QPoint(r.left(), r.top())
               << QPoint(r.right(), r.top())
               << QPoint(r.center().x(), r.bottom());
        }
        painter.setPen(Qt::NoPen);
        painter.setBrush(palette().mid().color());
        painter.drawPolygon(poly);
        painter.setBrush(Qt::NoBrush);
      }
    }

    block = block.next();
    top = bottom;
    bottom = top + static_cast<int>(blockBoundingRect(block).height());
    ++blockNumber;
  }
}

QVector<int> CodeEditor::breakpoints() const {
  QVector<int> lines;
  QTextBlock block = document() ? document()->firstBlock() : QTextBlock{};
  while (block.isValid()) {
    if (block.userState() == kBreakpointUserStateEnabled ||
        block.userState() == kBreakpointUserStateDisabled) {
      lines.push_back(block.blockNumber() + 1);
    }
    block = block.next();
  }
  std::sort(lines.begin(), lines.end());
  return lines;
}

bool CodeEditor::hasBreakpoint(int line) const {
  if (line < 1 || !document()) {
    return false;
  }
  const QTextBlock block = document()->findBlockByNumber(line - 1);
  return block.isValid() &&
         (block.userState() == kBreakpointUserStateEnabled ||
          block.userState() == kBreakpointUserStateDisabled);
}

bool CodeEditor::breakpointEnabled(int line) const {
  if (line < 1 || !document()) {
    return false;
  }
  const QTextBlock block = document()->findBlockByNumber(line - 1);
  return block.isValid() && block.userState() == kBreakpointUserStateEnabled;
}

void CodeEditor::toggleBreakpoint(int line) {
  if (line < 1 || !document()) {
    return;
  }
  QTextBlock block = document()->findBlockByNumber(line - 1);
  if (!block.isValid()) {
    return;
  }
  const int state = block.userState();
  if (state == kBreakpointUserStateEnabled || state == kBreakpointUserStateDisabled) {
    block.setUserState(-1);
  } else {
    block.setUserState(kBreakpointUserStateEnabled);
  }
  if (lineNumberArea_) {
    lineNumberArea_->update();
  }
  emit breakpointsChanged(breakpoints());
}

void CodeEditor::setBreakpointEnabled(int line, bool enabled) {
  if (line < 1 || !document()) {
    return;
  }
  QTextBlock block = document()->findBlockByNumber(line - 1);
  if (!block.isValid()) {
    return;
  }
  const int state = block.userState();
  if (state != kBreakpointUserStateEnabled && state != kBreakpointUserStateDisabled) {
    return;
  }
  const int wanted = enabled ? kBreakpointUserStateEnabled : kBreakpointUserStateDisabled;
  if (state == wanted) {
    return;
  }
  block.setUserState(wanted);
  if (lineNumberArea_) {
    lineNumberArea_->update();
  }
}

void CodeEditor::setBreakpoints(const QVector<int>& lines) {
  if (!document()) {
    return;
  }
  QSet<int> wanted;
  wanted.reserve(lines.size());
  for (int line : lines) {
    if (line > 0) {
      wanted.insert(line);
    }
  }

  bool changed = false;
  QTextBlock block = document()->firstBlock();
  while (block.isValid()) {
    const int line = block.blockNumber() + 1;
    const bool shouldHave = wanted.contains(line);
    const int state = block.userState();
    const bool has =
        state == kBreakpointUserStateEnabled || state == kBreakpointUserStateDisabled;
    if (shouldHave && !has) {
      block.setUserState(kBreakpointUserStateEnabled);
      changed = true;
    } else if (!shouldHave && has) {
      block.setUserState(-1);
      changed = true;
    }
    block = block.next();
  }

  if (changed) {
    if (lineNumberArea_) {
      lineNumberArea_->update();
    }
    emit breakpointsChanged(breakpoints());
  }
}

void CodeEditor::clearAllBreakpoints() {
  if (!document()) {
    return;
  }
  bool changed = false;
  QTextBlock block = document()->firstBlock();
  while (block.isValid()) {
    if (block.userState() == kBreakpointUserStateEnabled ||
        block.userState() == kBreakpointUserStateDisabled) {
      block.setUserState(-1);
      changed = true;
    }
    block = block.next();
  }
  if (changed) {
    if (lineNumberArea_) {
      lineNumberArea_->update();
    }
    emit breakpointsChanged({});
  }
}

bool CodeEditor::hasConditionalBreakpoint(int line) const {
  if (line < 1 || !document()) {
    return false;
  }
  const QTextBlock block = document()->findBlockByNumber(line - 1);
  // For visual distinction, we use userState values
  // kBreakpointUserStateEnabled | 0x100 = conditional breakpoint
  return block.isValid() &&
         (block.userState() == (kBreakpointUserStateEnabled | 0x100) ||
          block.userState() == (kBreakpointUserStateDisabled | 0x100));
}

bool CodeEditor::hasLogpoint(int line) const {
  if (line < 1 || !document()) {
    return false;
  }
  const QTextBlock block = document()->findBlockByNumber(line - 1);
  // For visual distinction, we use userState values
  // kBreakpointUserStateEnabled | 0x200 = logpoint
  return block.isValid() &&
         (block.userState() == (kBreakpointUserStateEnabled | 0x200) ||
          block.userState() == (kBreakpointUserStateDisabled | 0x200));
}

void CodeEditor::setConditionalBreakpoint(int line, bool conditional) {
  if (line < 1 || !document()) {
    return;
  }
  QTextBlock block = document()->findBlockByNumber(line - 1);
  if (!block.isValid()) {
    return;
  }

  const int state = block.userState();
  bool isEnabled = (state == kBreakpointUserStateEnabled ||
                    state == (kBreakpointUserStateEnabled | 0x100) ||
                    state == (kBreakpointUserStateEnabled | 0x200));

  if (conditional) {
    block.setUserState(isEnabled ? (kBreakpointUserStateEnabled | 0x100)
                                : (kBreakpointUserStateDisabled | 0x100));
  } else {
    block.setUserState(isEnabled ? kBreakpointUserStateEnabled
                                : kBreakpointUserStateDisabled);
  }

  if (lineNumberArea_) {
    lineNumberArea_->update();
  }
}

void CodeEditor::setLogpoint(int line, bool isLogpoint) {
  if (line < 1 || !document()) {
    return;
  }
  QTextBlock block = document()->findBlockByNumber(line - 1);
  if (!block.isValid()) {
    return;
  }

  const int state = block.userState();
  bool isEnabled = (state == kBreakpointUserStateEnabled ||
                    state == (kBreakpointUserStateEnabled | 0x100) ||
                    state == (kBreakpointUserStateEnabled | 0x200));

  if (isLogpoint) {
    block.setUserState(isEnabled ? (kBreakpointUserStateEnabled | 0x200)
                                : (kBreakpointUserStateDisabled | 0x200));
  } else {
    block.setUserState(isEnabled ? kBreakpointUserStateEnabled
                                : kBreakpointUserStateDisabled);
  }

  if (lineNumberArea_) {
    lineNumberArea_->update();
  }
}

bool CodeEditor::canFold(int line) const {
  if (!foldingEnabled_) {
    return false;
  }
  if (line < 1 || !document()) {
    return false;
  }
  const int startBlock = line - 1;
  const auto it = foldEndByStartBlock_.find(startBlock);
  if (it == foldEndByStartBlock_.end()) {
    return false;
  }
  return it.value() > startBlock;
}

bool CodeEditor::isFolded(int line) const {
  if (!foldingEnabled_) {
    return false;
  }
  if (line < 1 || !document()) {
    return false;
  }
  const QTextBlock block = document()->findBlockByNumber(line - 1);
  if (!block.isValid()) {
    return false;
  }
  return blockIsFolded(block);
}

void CodeEditor::toggleFold(int line) {
  if (!foldingEnabled_) {
    return;
  }
  if (line < 1 || !document()) {
    return;
  }
  const int startBlockNumber = line - 1;
  const auto it = foldEndByStartBlock_.find(startBlockNumber);
  if (it == foldEndByStartBlock_.end()) {
    return;
  }
  const int endBlockNumber = it.value();
  if (endBlockNumber <= startBlockNumber) {
    return;
  }

  QTextBlock startBlock = document()->findBlockByNumber(startBlockNumber);
  if (!startBlock.isValid()) {
    return;
  }

  FoldBlockUserData* data = foldUserDataFor(startBlock, true);
  const bool willFold = !data->folded;
  data->folded = willFold;

  if (willFold) {
    QTextCursor c = textCursor();
    const int curBlock = c.blockNumber();
    if (curBlock > startBlockNumber && curBlock <= endBlockNumber) {
      c.setPosition(startBlock.position());
      setTextCursor(c);
    }
  }

  applyFoldStates();
}

void CodeEditor::unfoldAllFolds() {
  if (!document()) {
    return;
  }
  QTextBlock block = document()->firstBlock();
  while (block.isValid()) {
    if (FoldBlockUserData* data = foldUserDataFor(block, false)) {
      data->folded = false;
    }
    block = block.next();
  }
  applyFoldStates();
}

void CodeEditor::setFoldingEnabled(bool enabled) {
  if (foldingEnabled_ == enabled) {
    return;
  }
  foldingEnabled_ = enabled;
  if (foldRecomputeTimer_) {
    foldRecomputeTimer_->stop();
  }

  if (!foldingEnabled_) {
    unfoldAllFolds();
    foldEndByStartBlock_.clear();
    if (lineNumberArea_) {
      lineNumberArea_->update();
    }
    return;
  }

  recomputeFoldRegions();
}

bool CodeEditor::foldingEnabled() const {
  return foldingEnabled_;
}

bool CodeEditor::snippetSessionActive() const {
  return snippetActive_;
}

void CodeEditor::clearSnippetSession() {
  snippetActive_ = false;
  snippetNav_.clear();
  snippetCurrent_ = 0;
  snippetFinalPos_ = 0;
}

bool CodeEditor::insertSnippet(int replaceStart, int replaceEnd,
                               const QString& snippetText) {
  if (!document()) {
    return false;
  }

  clearSnippetSession();

  const int docLen = std::max(0, document()->characterCount() - 1);
  const int start = std::clamp(replaceStart, 0, docLen);
  const int end = std::clamp(replaceEnd, start, docLen);

  const ParsedSnippet snip = parseLspSnippet(snippetText);

  QTextCursor cursor(document());
  cursor.setPosition(start);
  cursor.setPosition(end, QTextCursor::KeepAnchor);
  cursor.beginEditBlock();
  cursor.insertText(snip.text);
  cursor.endEditBlock();

  QMap<int, QVector<SnippetPlaceholder>> byIndex;
  for (const auto& p : snip.placeholders) {
    SnippetPlaceholder sp;
    sp.index = p.index;
    sp.start = start + p.start;
    sp.end = start + p.end;
    byIndex[sp.index].push_back(sp);
  }

  QVector<SnippetNavItem> nav;
  nav.reserve(byIndex.size());
  QList<int> indices = byIndex.keys();
  std::sort(indices.begin(), indices.end());
  for (int idx : indices) {
    if (idx <= 0) {
      continue;
    }
    const QVector<SnippetPlaceholder> occ = byIndex.value(idx);
    if (occ.isEmpty()) {
      continue;
    }
    SnippetNavItem item;
    item.index = idx;
    item.start = occ.first().start;
    item.end = occ.first().end;
    nav.push_back(item);
  }

  snippetFinalPos_ =
      start + std::clamp(snip.finalPos, 0, static_cast<int>(snip.text.size()));
  snippetNav_ = std::move(nav);

  if (snippetNav_.isEmpty()) {
    QTextCursor c(document());
    c.setPosition(snippetFinalPos_);
    snippetSettingCursor_ = true;
    setTextCursor(c);
    snippetSettingCursor_ = false;
    ensureCursorVisible();
    return true;
  }

  snippetActive_ = true;
  snippetCurrent_ = 0;
  selectSnippetCurrent();
  return true;
}

bool CodeEditor::advanceSnippet(bool forward) {
  if (!snippetActive_ || snippetNav_.isEmpty()) {
    return false;
  }

  int next = snippetCurrent_ + (forward ? 1 : -1);
  if (next < 0) {
    next = 0;
  }

  if (next >= snippetNav_.size()) {
    const int finalPos = snippetFinalPos_;
    clearSnippetSession();
    QTextCursor c(document());
    c.setPosition(std::clamp(finalPos, 0, document()->characterCount() - 1));
    snippetSettingCursor_ = true;
    setTextCursor(c);
    snippetSettingCursor_ = false;
    ensureCursorVisible();
    return true;
  }

  snippetCurrent_ = next;
  selectSnippetCurrent();
  return true;
}

void CodeEditor::selectSnippetCurrent() {
  if (!snippetActive_ || snippetNav_.isEmpty() || !document()) {
    return;
  }
  const int maxIdx = std::max(0, static_cast<int>(snippetNav_.size()) - 1);
  const int idx = std::clamp(snippetCurrent_, 0, maxIdx);
  const SnippetNavItem cur = snippetNav_.at(idx);

  QTextCursor c(document());
  c.setPosition(std::clamp(cur.start, 0, document()->characterCount() - 1));
  c.setPosition(std::clamp(cur.end, 0, document()->characterCount() - 1),
                QTextCursor::KeepAnchor);
  snippetSettingCursor_ = true;
  setTextCursor(c);
  snippetSettingCursor_ = false;
  ensureCursorVisible();
}

void CodeEditor::updateSnippetRanges(int pos, int charsRemoved, int charsAdded) {
  if (!snippetActive_ || snippetNav_.isEmpty() || snippetSettingCursor_) {
    return;
  }
  const int delta = charsAdded - charsRemoved;
  const int changeEnd = pos + charsRemoved;

  const int maxIdx = std::max(0, static_cast<int>(snippetNav_.size()) - 1);
  const int activeIdx = std::clamp(snippetCurrent_, 0, maxIdx);
  const SnippetNavItem activeBefore = snippetNav_.at(activeIdx);

  for (int i = 0; i < snippetNav_.size(); ++i) {
    SnippetNavItem& item = snippetNav_[i];

    if (item.end < pos) {
      continue;
    }
    if (item.start >= changeEnd) {
      item.start += delta;
      item.end += delta;
      continue;
    }

    // Overlap: only allow edits fully contained in the active placeholder.
    if (i != activeIdx) {
      clearSnippetSession();
      return;
    }
    if (pos < activeBefore.start || pos > activeBefore.end || changeEnd > activeBefore.end) {
      clearSnippetSession();
      return;
    }
    item.end += delta;
  }

  // Update final cursor position too.
  if (snippetFinalPos_ < pos) {
    return;
  }
  if (snippetFinalPos_ >= changeEnd) {
    snippetFinalPos_ += delta;
    return;
  }
  snippetFinalPos_ = pos + charsAdded;
}

void CodeEditor::keyPressEvent(QKeyEvent* event) {
  // Multi-cursor keyboard shortcuts
  if (event->modifiers() & (Qt::ControlModifier | Qt::AltModifier)) {
    if (event->key() == Qt::Key_Down &&
        event->modifiers() & Qt::ControlModifier &&
        event->modifiers() & Qt::AltModifier) {
      addCursorBelow();
      return;
    }
    if (event->key() == Qt::Key_Up &&
        event->modifiers() & Qt::ControlModifier &&
        event->modifiers() & Qt::AltModifier) {
      addCursorAbove();
      return;
    }
    if (event->key() == Qt::Key_D &&
        event->modifiers() & Qt::ControlModifier &&
        event->modifiers() & Qt::ShiftModifier) {
      addCursorToNextOccurrence();
      return;
    }
    if ((event->key() == Qt::Key_D || event->key() == Qt::Key_K) &&
        event->modifiers() & Qt::ControlModifier &&
        event->modifiers() & Qt::ShiftModifier &&
        event->modifiers() & Qt::AltModifier) {
      addCursorToAllOccurrences();
      return;
    }
    if (event->key() == Qt::Key_L &&
        event->modifiers() & Qt::ControlModifier &&
        event->modifiers() & Qt::ShiftModifier) {
      addCursorToLineEnds();
      return;
    }
  }

  // Handle multi-cursor editing keys
  if (handleMultiCursorKeyEvent(event)) {
    return;
  }

  if (snippetActive_ && event->key() == Qt::Key_Tab &&
      event->modifiers() == Qt::NoModifier) {
    if (advanceSnippet(true)) {
      return;
    }
  }
  if (snippetActive_ &&
      (event->key() == Qt::Key_Backtab ||
       ((event->key() == Qt::Key_Tab) && (event->modifiers() & Qt::ShiftModifier)))) {
    if (advanceSnippet(false)) {
      return;
    }
  }

  if (event->key() == Qt::Key_Backtab ||
      ((event->key() == Qt::Key_Tab) &&
       (event->modifiers() & Qt::ShiftModifier))) {
    QTextCursor cursor = textCursor();
    const int startPos = cursor.hasSelection() ? cursor.selectionStart() : cursor.position();
    const int endPos = cursor.hasSelection() ? cursor.selectionEnd() : cursor.position();

    QTextCursor startCursor(document());
    startCursor.setPosition(startPos);
    int startBlock = startCursor.blockNumber();
    QTextCursor endCursor(document());
    endCursor.setPosition(endPos);
    int endBlock = endCursor.blockNumber();
    if (endPos > startPos && endCursor.positionInBlock() == 0) {
      endBlock = qMax(startBlock, endBlock - 1);
    }

    cursor.beginEditBlock();
    for (int b = startBlock; b <= endBlock; ++b) {
      const QTextBlock block = document()->findBlockByNumber(b);
      if (!block.isValid()) {
        continue;
      }
      const QString text = block.text();
      int removeCount = 0;
      if (!text.isEmpty() && text.at(0) == QLatin1Char('\t')) {
        removeCount = 1;
      } else {
        while (removeCount < tabSize_ && removeCount < text.size() &&
               text.at(removeCount) == QLatin1Char(' ')) {
          ++removeCount;
        }
      }
      if (removeCount <= 0) {
        continue;
      }
      QTextCursor line(document());
      line.setPosition(block.position());
      line.setPosition(block.position() + removeCount, QTextCursor::KeepAnchor);
      line.removeSelectedText();
    }
    cursor.endEditBlock();
    setTextCursor(cursor);
    return;
  }

  if (event->key() == Qt::Key_Tab && event->modifiers() == Qt::NoModifier) {
    QTextCursor cursor = textCursor();
    const QString unit = indentUnit(tabSize_, insertSpaces_);
    if (!cursor.hasSelection()) {
      cursor.insertText(unit);
      setTextCursor(cursor);
      return;
    }

    const int startPos = cursor.selectionStart();
    const int endPos = cursor.selectionEnd();
    QTextCursor startCursor(document());
    startCursor.setPosition(startPos);
    int startBlock = startCursor.blockNumber();
    QTextCursor endCursor(document());
    endCursor.setPosition(endPos);
    int endBlock = endCursor.blockNumber();
    if (endPos > startPos && endCursor.positionInBlock() == 0) {
      endBlock = qMax(startBlock, endBlock - 1);
    }

    cursor.beginEditBlock();
    for (int b = startBlock; b <= endBlock; ++b) {
      const QTextBlock block = document()->findBlockByNumber(b);
      if (!block.isValid()) {
        continue;
      }
      QTextCursor line(document());
      line.setPosition(block.position());
      line.insertText(unit);
    }
    cursor.endEditBlock();
    setTextCursor(cursor);
    return;
  }

  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
    QTextCursor cursor = textCursor();
    const QTextBlock prev = cursor.block();
    const QString prevText = prev.text();

    QString indent;
    for (QChar c : prevText) {
      if (c == QLatin1Char(' ') || c == QLatin1Char('\t')) {
        indent.push_back(c);
      } else {
        break;
      }
    }
    if (prevText.trimmed().endsWith(QLatin1Char('{'))) {
      indent.append(indentUnit(tabSize_, insertSpaces_));
    }

    cursor.beginEditBlock();
    cursor.insertBlock();
    cursor.insertText(indent);
    cursor.endEditBlock();
    setTextCursor(cursor);
    return;
  }

  QPlainTextEdit::keyPressEvent(event);
}

void CodeEditor::paintEvent(QPaintEvent* event) {
  QPlainTextEdit::paintEvent(event);

  if (!showIndentGuides_) {
    return;
  }
  if (!event) {
    return;
  }

  const qreal tabW = tabStopDistance();
  if (tabW <= 0.0) {
    return;
  }

  QPainter painter(viewport());
  painter.setRenderHint(QPainter::Antialiasing, false);

  QColor color = palette().mid().color();
  color.setAlpha(90);
  QPen pen(color);
  pen.setStyle(Qt::DotLine);
  painter.setPen(pen);

  QTextBlock block = firstVisibleBlock();
  int top =
      static_cast<int>(blockBoundingGeometry(block).translated(contentOffset()).top());
  int bottom = top + static_cast<int>(blockBoundingRect(block).height());

  const QRect clip = event->rect();

  while (block.isValid() && top <= clip.bottom()) {
    if (block.isVisible() && bottom >= clip.top()) {
      const QString text = block.text();
      int columns = 0;
      for (const QChar ch : text) {
        if (ch == QLatin1Char('\t')) {
          columns += tabSize_;
          continue;
        }
        if (ch == QLatin1Char(' ')) {
          ++columns;
          continue;
        }
        break;
      }

      const int levels = columns / tabSize_;
      for (int level = 1; level <= levels; ++level) {
        const int x = static_cast<int>(contentOffset().x() + (level * tabW));
        if (x < clip.left() || x > clip.right()) {
          continue;
        }
        painter.drawLine(x, top, x, bottom);
      }
    }

    block = block.next();
    top = bottom;
    bottom = top + static_cast<int>(blockBoundingRect(block).height());
  }
}

void CodeEditor::updateBracketMatch() {
  QList<QTextEdit::ExtraSelection> selections = extraSelections();

  selections.erase(std::remove_if(selections.begin(), selections.end(),
                                  [](const QTextEdit::ExtraSelection& s) {
                                    return s.format.property(kExtraPropertyRole)
                                               .toString() == QStringLiteral("bracket");
                                  }),
                   selections.end());
  selections.erase(std::remove_if(selections.begin(), selections.end(),
                                  [](const QTextEdit::ExtraSelection& s) {
                                    return s.format.property(kExtraPropertyRole)
                                               .toString() == QStringLiteral("scope");
                                  }),
                   selections.end());

  const QTextCursor cursor = textCursor();
  const int docLen = document() ? document()->characterCount() : 0;
  if (!document() || docLen <= 0) {
    setExtraSelections(selections);
    return;
  }

  const auto charAt = [this, docLen](int pos) -> QChar {
    if (pos < 0 || pos >= docLen) {
      return {};
    }
    return document()->characterAt(pos);
  };

  BracketPair pair;
  bool isOpen = false;
  int bracketPos = -1;

  const int curPos = cursor.position();
  if (curPos >= 0 && curPos < docLen && bracketPairFor(charAt(curPos), &pair, &isOpen)) {
    bracketPos = curPos;
  } else if (curPos - 1 >= 0 && bracketPairFor(charAt(curPos - 1), &pair, &isOpen)) {
    bracketPos = curPos - 1;
  }

  if (bracketPos < 0) {
    setExtraSelections(selections);
    return;
  }

  int matchPos = -1;
  if (isOpen) {
    int depth = 0;
    for (int i = bracketPos; i < docLen; ++i) {
      const QChar c = charAt(i);
      if (c == pair.open) {
        ++depth;
      } else if (c == pair.close) {
        --depth;
        if (depth == 0) {
          matchPos = i;
          break;
        }
      }
    }
  } else {
    int depth = 0;
    for (int i = bracketPos; i >= 0; --i) {
      const QChar c = charAt(i);
      if (c == pair.close) {
        ++depth;
      } else if (c == pair.open) {
        --depth;
        if (depth == 0) {
          matchPos = i;
          break;
        }
      }
    }
  }

  if (matchPos < 0) {
    setExtraSelections(selections);
    return;
  }

  if (pair.open == QLatin1Char('{') && pair.close == QLatin1Char('}')) {
    const int start = std::min(bracketPos, matchPos);
    const int end = std::max(bracketPos, matchPos);
    if (end > start + 1) {
      QTextCursor c(document());
      c.setPosition(start + 1);
      c.setPosition(end, QTextCursor::KeepAnchor);
      QColor scopeBg = palette().alternateBase().color();
      scopeBg.setAlpha(28);
      QTextEdit::ExtraSelection sel;
      sel.cursor = c;
      sel.format.setBackground(scopeBg);
      sel.format.setProperty(kExtraPropertyRole, QStringLiteral("scope"));
      selections.push_back(sel);
    }
  }

  QColor bg = palette().highlight().color();
  bg.setAlpha(70);

  auto makeSel = [this, bg](int pos) {
    QTextEdit::ExtraSelection sel;
    QTextCursor c(document());
    c.setPosition(pos);
    c.setPosition(pos + 1, QTextCursor::KeepAnchor);
    sel.cursor = c;
    sel.format.setBackground(bg);
    sel.format.setProperty(kExtraPropertyRole, QStringLiteral("bracket"));
    return sel;
  };

  selections.push_back(makeSel(bracketPos));
  if (matchPos != bracketPos) {
    selections.push_back(makeSel(matchPos));
  }
  setExtraSelections(selections);
}

void CodeEditor::updateCurrentLineHighlight() {
  QList<QTextEdit::ExtraSelection> selections = extraSelections();

  selections.erase(std::remove_if(selections.begin(), selections.end(),
                                  [](const QTextEdit::ExtraSelection& s) {
                                    return s.format.property(kExtraPropertyRole)
                                               .toString() ==
                                           QStringLiteral("currentLine");
                                  }),
                   selections.end());

  auto* doc = document();
  if (!doc) {
    setExtraSelections(selections);
    return;
  }

  QColor bg = palette().alternateBase().color();
  bg.setAlpha(55);

  QTextEdit::ExtraSelection sel;
  sel.cursor = textCursor();
  sel.cursor.clearSelection();
  sel.cursor.select(QTextCursor::LineUnderCursor);
  sel.format.setBackground(bg);
  sel.format.setProperty(QTextFormat::FullWidthSelection, true);
  sel.format.setProperty(kExtraPropertyRole, QStringLiteral("currentLine"));

  const auto insertBefore = std::find_if(
      selections.begin(), selections.end(), [](const QTextEdit::ExtraSelection& s) {
        return s.format.property(kExtraPropertyRole).toString() ==
               QStringLiteral("bracket");
      });
  selections.insert(insertBefore, sel);

  setExtraSelections(selections);
}

CodeEditor::FoldBlockUserData* CodeEditor::foldUserDataFor(const QTextBlock& block,
                                                          bool create) const {
  if (!block.isValid()) {
    return nullptr;
  }

  if (auto* raw = block.userData()) {
    if (auto* d = dynamic_cast<FoldBlockUserData*>(raw)) {
      if (d->magic == FoldBlockUserData::kMagic) {
        return d;
      }
    }
  }

  if (!create) {
    return nullptr;
  }

  auto* d = new FoldBlockUserData();
  const_cast<QTextBlock&>(block).setUserData(d);
  return d;
}

bool CodeEditor::blockIsFolded(const QTextBlock& block) const {
  FoldBlockUserData* d = foldUserDataFor(block, false);
  return d && d->folded;
}

void CodeEditor::recomputeFoldRegions() {
  foldEndByStartBlock_.clear();
  if (!document() || !foldingEnabled_) {
    if (lineNumberArea_) {
      lineNumberArea_->update();
    }
    return;
  }

  bool hasAnyFolded = false;
  QVector<int> stack;
  stack.reserve(128);

  QTextBlock block = document()->firstBlock();
  while (block.isValid()) {
    if (!hasAnyFolded) {
      if (FoldBlockUserData* data = foldUserDataFor(block, false)) {
        hasAnyFolded = data->folded;
      }
    }

    const int blockNo = block.blockNumber();
    const QString text = block.text();
    for (const QChar c : text) {
      if (c == QLatin1Char('{')) {
        stack.push_back(blockNo);
        continue;
      }
      if (c == QLatin1Char('}')) {
        if (stack.isEmpty()) {
          continue;
        }
        const int start = stack.takeLast();
        if (blockNo > start) {
          foldEndByStartBlock_[start] = blockNo;
        }
      }
    }
    block = block.next();
  }

  if (lineNumberArea_) {
    lineNumberArea_->update();
  }

  if (hasAnyFolded) {
    applyFoldStates();
  }
}

void CodeEditor::applyFoldStates() {
  if (!document()) {
    return;
  }

  const int vScroll = verticalScrollBar() ? verticalScrollBar()->value() : 0;
  const int hScroll = horizontalScrollBar() ? horizontalScrollBar()->value() : 0;

  // First, unhide everything.
  QTextBlock block = document()->firstBlock();
  while (block.isValid()) {
    if (!block.isVisible()) {
      block.setVisible(true);
      const int lineCount = block.layout() ? block.layout()->lineCount() : 1;
      block.setLineCount(qMax(1, lineCount));
    }
    block = block.next();
  }

  // Then apply folds in document order.
  block = document()->firstBlock();
  while (block.isValid()) {
    FoldBlockUserData* data = foldUserDataFor(block, false);
    if (!data || !data->folded) {
      block = block.next();
      continue;
    }

    const int startNo = block.blockNumber();
    const auto it = foldEndByStartBlock_.find(startNo);
    if (it == foldEndByStartBlock_.end() || it.value() <= startNo) {
      data->folded = false;
      block = block.next();
      continue;
    }

    const int endNo = it.value();
    QTextBlock b = block.next();
    while (b.isValid() && b.blockNumber() <= endNo) {
      b.setVisible(false);
      b.setLineCount(0);
      b = b.next();
    }

    block = block.next();
  }

  document()->markContentsDirty(0, document()->characterCount());
  viewport()->update();
  if (lineNumberArea_) {
    lineNumberArea_->update();
  }

  if (verticalScrollBar()) {
    verticalScrollBar()->setValue(vScroll);
  }
  if (horizontalScrollBar()) {
    horizontalScrollBar()->setValue(hScroll);
  }
}

// Multi-cursor implementation

void CodeEditor::addCursorBelow() {
  if (!document()) {
    return;
  }

  QTextCursor currentCursor = textCursor();
  QTextBlock currentBlock = currentCursor.block();
  if (!currentBlock.isValid()) {
    return;
  }

  // Check if we already have a cursor on the next line at the same column
  const int currentColumn = currentCursor.positionInBlock();
  QTextBlock nextBlock = currentBlock.next();
  if (!nextBlock.isValid()) {
    return;
  }

  // Check for duplicate cursor at this position
  const int targetPosition = nextBlock.position() + qMin(currentColumn, nextBlock.text().length());
  for (const auto& info : additionalCursors_) {
    if (info.position == targetPosition) {
      // Cursor already exists, move it further down
      QTextBlock checkBlock = nextBlock.next();
      while (checkBlock.isValid()) {
        const int checkPos = checkBlock.position() + qMin(currentColumn, checkBlock.text().length());
        bool duplicate = false;
        for (const auto& existing : additionalCursors_) {
          if (existing.position == checkPos) {
            duplicate = true;
            break;
          }
        }
        if (!duplicate) {
          CursorInfo info;
          info.position = checkPos;
          info.anchor = checkPos;
          additionalCursors_.push_back(info);
          updateMultiCursorRendering();
          return;
        }
        checkBlock = checkBlock.next();
      }
      return;
    }
  }

  CursorInfo info;
  info.position = targetPosition;
  info.anchor = targetPosition;
  additionalCursors_.push_back(info);
  updateMultiCursorRendering();
}

void CodeEditor::addCursorAbove() {
  if (!document()) {
    return;
  }

  QTextCursor currentCursor = textCursor();
  QTextBlock currentBlock = currentCursor.block();
  if (!currentBlock.isValid()) {
    return;
  }

  const int currentColumn = currentCursor.positionInBlock();
  QTextBlock prevBlock = currentBlock.previous();
  if (!prevBlock.isValid()) {
    return;
  }

  // Check for duplicate cursor at this position
  const int targetPosition = prevBlock.position() + qMin(currentColumn, prevBlock.text().length());
  for (const auto& info : additionalCursors_) {
    if (info.position == targetPosition) {
      // Cursor already exists, move it further up
      QTextBlock checkBlock = prevBlock.previous();
      while (checkBlock.isValid()) {
        const int checkPos = checkBlock.position() + qMin(currentColumn, checkBlock.text().length());
        bool duplicate = false;
        for (const auto& existing : additionalCursors_) {
          if (existing.position == checkPos) {
            duplicate = true;
            break;
          }
        }
        if (!duplicate) {
          CursorInfo info;
          info.position = checkPos;
          info.anchor = checkPos;
          additionalCursors_.push_back(info);
          updateMultiCursorRendering();
          return;
        }
        checkBlock = checkBlock.previous();
      }
      return;
    }
  }

  CursorInfo info;
  info.position = targetPosition;
  info.anchor = targetPosition;
  additionalCursors_.push_back(info);
  updateMultiCursorRendering();
}

QString CodeEditor::currentWordUnderCursor() const {
  QTextCursor cursor = textCursor();
  if (cursor.hasSelection()) {
    return cursor.selectedText();
  }

  // Get the word under cursor
  cursor.select(QTextCursor::WordUnderCursor);
  return cursor.selectedText();
}

void CodeEditor::addCursorToNextOccurrence() {
  if (!document()) {
    return;
  }

  QString searchWord = currentWordUnderCursor();
  if (searchWord.isEmpty()) {
    return;
  }

  QTextCursor currentCursor = textCursor();
  int searchStart = currentCursor.hasSelection() ? currentCursor.selectionEnd() : currentCursor.position();

  // Search for next occurrence
  QTextCursor findCursor(document());
  findCursor.setPosition(searchStart);
  findCursor = document()->find(searchWord, findCursor);

  if (!findCursor.isNull()) {
    // Check if this cursor already exists
    const int newPos = findCursor.position();
    const int newAnchor = findCursor.anchor();
    for (const auto& info : additionalCursors_) {
      if (info.position == newPos && info.anchor == newAnchor) {
        // Try next occurrence
        findCursor = document()->find(searchWord, findCursor);
        if (findCursor.isNull()) {
          return;
        }
        break;
      }
    }

    CursorInfo info;
    info.position = findCursor.position();
    info.anchor = findCursor.anchor();
    additionalCursors_.push_back(info);
    updateMultiCursorRendering();
  }
}

void CodeEditor::addCursorToAllOccurrences() {
  if (!document()) {
    return;
  }

  QString searchWord = currentWordUnderCursor();
  if (searchWord.isEmpty()) {
    return;
  }

  clearAdditionalCursors();

  QTextCursor findCursor(document());
  findCursor.setPosition(0);

  while (true) {
    findCursor = document()->find(searchWord, findCursor);
    if (findCursor.isNull()) {
      break;
    }

    // Skip the main cursor position
    QTextCursor mainCursor = textCursor();
    if (findCursor.position() == mainCursor.position() &&
        findCursor.anchor() == mainCursor.anchor()) {
      continue;
    }

    CursorInfo info;
    info.position = findCursor.position();
    info.anchor = findCursor.anchor();
    additionalCursors_.push_back(info);
  }

  updateMultiCursorRendering();
}

void CodeEditor::addCursorToLineEnds() {
  if (!document()) {
    return;
  }

  QTextCursor currentCursor = textCursor();
  if (!currentCursor.hasSelection()) {
    return;
  }

  clearAdditionalCursors();

  const int startPos = qMin(currentCursor.selectionStart(), currentCursor.selectionEnd());
  const int endPos = qMax(currentCursor.selectionStart(), currentCursor.selectionEnd());

  QTextCursor startCursor(document());
  startCursor.setPosition(startPos);
  QTextCursor endCursor(document());
  endCursor.setPosition(endPos);

  QTextBlock startBlock = startCursor.block();
  QTextBlock endBlock = endCursor.block();

  QTextBlock block = startBlock;
  while (block.isValid() && block.blockNumber() <= endBlock.blockNumber()) {
    CursorInfo info;
    // Position at the end of the line (before newline)
    info.position = block.position() + block.text().length();
    info.anchor = info.position;
    additionalCursors_.push_back(info);
    block = block.next();
  }

  updateMultiCursorRendering();
}

void CodeEditor::clearAdditionalCursors() {
  if (additionalCursors_.isEmpty()) {
    return;
  }
  additionalCursors_.clear();
  updateMultiCursorRendering();
}

int CodeEditor::cursorCount() const {
  return 1 + additionalCursors_.size();
}

bool CodeEditor::hasMultipleCursors() const {
  return !additionalCursors_.isEmpty();
}

void CodeEditor::updateMultiCursorRendering() {
  QList<QTextEdit::ExtraSelection> selections = extraSelections();

  // Remove old multi-cursor selections
  selections.erase(std::remove_if(selections.begin(), selections.end(),
                                  [](const QTextEdit::ExtraSelection& s) {
                                    return s.format.property(kExtraPropertyRole)
                                               .toString() == QStringLiteral("multicursor");
                                  }),
                   selections.end());

  if (!additionalCursors_.isEmpty() && document()) {
    // Render additional cursors
    for (const auto& info : additionalCursors_) {
      QTextCursor cursor(document());
      cursor.setPosition(info.anchor);
      cursor.setPosition(info.position, QTextCursor::KeepAnchor);

      QTextEdit::ExtraSelection sel;
      sel.cursor = cursor;

      if (info.position != info.anchor) {
        // Selection
        sel.format.setBackground(palette().highlight().color());
        sel.format.setForeground(palette().highlightedText().color());
      } else {
        // Cursor only - draw a thin line
        sel.cursor.clearSelection();
        sel.format.setBackground(QColor(0, 120, 215, 100));
      }
      sel.format.setProperty(kExtraPropertyRole, QStringLiteral("multicursor"));
      selections.push_back(sel);
    }
  }

  setExtraSelections(selections);
}

void CodeEditor::insertTextAtAllCursors(const QString& text) {
  if (!document()) {
    return;
  }

  document()->begin();

  // Insert at additional cursors first (in reverse order to preserve positions)
  for (int i = additionalCursors_.size() - 1; i >= 0; --i) {
    const CursorInfo& info = additionalCursors_[i];
    QTextCursor cursor(document());
    cursor.setPosition(info.position);
    cursor.insertText(text);

    // Update cursor info
    CursorInfo updated = info;
    updated.position += text.length();
    if (updated.anchor >= updated.position) {
      updated.anchor = updated.position;
    } else {
      updated.anchor += text.length();
    }
    additionalCursors_[i] = updated;
  }

  document()->end();
  updateMultiCursorRendering();
}

void CodeEditor::deleteTextAtAllCursors() {
  if (!document()) {
    return;
  }

  document()->begin();

  // Delete at additional cursors first (in reverse order to preserve positions)
  for (int i = additionalCursors_.size() - 1; i >= 0; --i) {
    const CursorInfo& info = additionalCursors_[i];
    QTextCursor cursor(document());
    const int start = qMin(info.position, info.anchor);
    const int end = qMax(info.position, info.anchor);

    if (start != end) {
      cursor.setPosition(start);
      cursor.setPosition(end, QTextCursor::KeepAnchor);
      cursor.removeSelectedText();
    } else if (start > 0) {
      cursor.setPosition(start - 1);
      cursor.setPosition(start, QTextCursor::KeepAnchor);
      cursor.removeSelectedText();
    }

    // Update cursor info
    CursorInfo updated = info;
    const int deletedLength = end - start;
    if (deletedLength > 0) {
      updated.position = start;
      updated.anchor = start;
    } else if (start > 0) {
      updated.position = start - 1;
      updated.anchor = start - 1;
    }
    additionalCursors_[i] = updated;
  }

  document()->end();
  updateMultiCursorRendering();
}

void CodeEditor::syncAdditionalCursorsFromSelection() {
  QTextCursor currentCursor = textCursor();
  if (!currentCursor.hasSelection() || !additionalCursors_.isEmpty()) {
    return;
  }

  const int startPos = qMin(currentCursor.selectionStart(), currentCursor.selectionEnd());
  const int endPos = qMax(currentCursor.selectionStart(), currentCursor.selectionEnd());

  QTextCursor startCursor(document());
  startCursor.setPosition(startPos);
  QTextCursor endCursor(document());
  endCursor.setPosition(endPos);

  QTextBlock startBlock = startCursor.block();
  QTextBlock endBlock = endCursor.block();

  // Only create cursors for multi-line selections
  if (startBlock.blockNumber() >= endBlock.blockNumber()) {
    return;
  }

  QTextBlock block = startBlock.next();
  while (block.isValid() && block.blockNumber() < endBlock.blockNumber()) {
    CursorInfo info;
    info.position = block.position() + qMin(currentCursor.positionInBlock(), block.text().length());
    info.anchor = info.position;
    additionalCursors_.push_back(info);
    block = block.next();
  }

  updateMultiCursorRendering();
}

QVector<QTextCursor> CodeEditor::allTextCursors() const {
  QVector<QTextCursor> cursors;
  if (!document()) {
    return cursors;
  }

  cursors.reserve(additionalCursors_.size() + 1);
  cursors.append(textCursor());

  for (const auto& info : additionalCursors_) {
    QTextCursor cursor(document());
    cursor.setPosition(info.anchor);
    cursor.setPosition(info.position, QTextCursor::KeepAnchor);
    cursors.append(cursor);
  }

  return cursors;
}

bool CodeEditor::handleMultiCursorKeyEvent(QKeyEvent* event) {
  if (additionalCursors_.isEmpty()) {
    return false;
  }

  // Handle Escape to clear multi-cursors
  if (event->key() == Qt::Key_Escape) {
    clearAdditionalCursors();
    return true;
  }

  // Handle character insertion
  const QString text = event->text();
  if (!text.isEmpty() && text.at(0).isPrint() &&
      !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier))) {
    insertTextAtAllCursors(text);

    // Also insert at main cursor
    QTextCursor cursor = textCursor();
    cursor.insertText(text);
    setTextCursor(cursor);
    return true;
  }

  // Handle Backspace
  if (event->key() == Qt::Key_Backspace) {
    deleteTextAtAllCursors();
    return true;
  }

  // Handle Delete
  if (event->key() == Qt::Key_Delete) {
    deleteTextAtAllCursors();
    return true;
  }

  // Handle Enter/Return
  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
    const QString indentUnit = [&]() {
      if (!insertSpaces_) {
        return QStringLiteral("\t");
      }
      return QString(tabSize_, QLatin1Char(' '));
    }();

    // Get current line's indent
    QTextCursor mainCursor = textCursor();
    const QTextBlock prevBlock = mainCursor.block();
    QString indent;
    for (QChar c : prevBlock.text()) {
      if (c == QLatin1Char(' ') || c == QLatin1Char('\t')) {
        indent.push_back(c);
      } else {
        break;
      }
    }
    if (prevBlock.text().trimmed().endsWith(QLatin1Char('{'))) {
      indent.append(indentUnit);
    }

    const QString lineBreak = QStringLiteral("\n") + indent;
    insertTextAtAllCursors(lineBreak);

    // Also insert at main cursor
    mainCursor.insertText(lineBreak);
    setTextCursor(mainCursor);
    return true;
  }

  // Handle Tab - add indentation to all cursors
  if (event->key() == Qt::Key_Tab && event->modifiers() == Qt::NoModifier) {
    const QString indentUnit = [&]() {
      if (!insertSpaces_) {
        return QStringLiteral("\t");
      }
      return QString(tabSize_, QLatin1Char(' '));
    }();
    insertTextAtAllCursors(indentUnit);

    // Also insert at main cursor
    QTextCursor cursor = textCursor();
    cursor.insertText(indentUnit);
    setTextCursor(cursor);
    return true;
  }

  // Handle Shift+Tab - remove indentation from all cursors
  if (event->key() == Qt::Key_Backtab ||
      ((event->key() == Qt::Key_Tab) && (event->modifiers() & Qt::ShiftModifier))) {
    document()->begin();

    for (int i = additionalCursors_.size() - 1; i >= 0; --i) {
      const CursorInfo& info = additionalCursors_[i];
      QTextBlock block = document()->findBlock(info.position);
      if (!block.isValid()) {
        continue;
      }

      const QString text = block.text();
      int removeCount = 0;
      if (!text.isEmpty() && text.at(0) == QLatin1Char('\t')) {
        removeCount = 1;
      } else {
        while (removeCount < tabSize_ && removeCount < text.size() &&
               text.at(removeCount) == QLatin1Char(' ')) {
          ++removeCount;
        }
      }

      if (removeCount > 0) {
        QTextCursor lineCursor(document());
        lineCursor.setPosition(block.position());
        lineCursor.setPosition(block.position() + removeCount, QTextCursor::KeepAnchor);
        lineCursor.removeSelectedText();

        // Update cursor info
        CursorInfo updated = info;
        updated.position -= removeCount;
        updated.anchor -= removeCount;
        additionalCursors_[i] = updated;
      }
    }

    document()->end();
    updateMultiCursorRendering();

    // Also handle main cursor
    QTextCursor cursor = textCursor();
    const int startPos = cursor.selectionStart();
    const int endPos = cursor.selectionEnd();

    QTextCursor startCursor(document());
    startCursor.setPosition(startPos);
    int startBlock = startCursor.blockNumber();
    QTextCursor endCursor(document());
    endCursor.setPosition(endPos);
    int endBlock = endCursor.blockNumber();
    if (endPos > startPos && endCursor.positionInBlock() == 0) {
      endBlock = qMax(startBlock, endBlock - 1);
    }

    cursor.beginEditBlock();
    for (int b = startBlock; b <= endBlock; ++b) {
      const QTextBlock block = document()->findBlockByNumber(b);
      if (!block.isValid()) {
        continue;
      }
      const QString text = block.text();
      int removeCount = 0;
      if (!text.isEmpty() && text.at(0) == QLatin1Char('\t')) {
        removeCount = 1;
      } else {
        while (removeCount < tabSize_ && removeCount < text.size() &&
               text.at(removeCount) == QLatin1Char(' ')) {
          ++removeCount;
        }
      }
      if (removeCount <= 0) {
        continue;
      }
      QTextCursor line(document());
      line.setPosition(block.position());
      line.setPosition(block.position() + removeCount, QTextCursor::KeepAnchor);
      line.removeSelectedText();
    }
    cursor.endEditBlock();
    setTextCursor(cursor);
    return true;
  }

  return false;
}
