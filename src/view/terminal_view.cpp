#include "view/terminal_view.h"

#include "core/ring_buffer.h"
#include "core/session.h"
#include "terminal/vterm_engine.h"

#include <QApplication>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QSettings>
#include <QWheelEvent>
#include <algorithm>

namespace serialkit {

namespace {
constexpr int kDefaultRows = 24;
constexpr int kDefaultCols = 80;
constexpr int kMinFontPointSize = 6;
constexpr int kMaxFontPointSize = 48;
const auto kFontPointSizeKey = QStringLiteral("terminal/fontPointSize");
} // namespace

TerminalView::TerminalView(QWidget* parent)
    // No Qt parent for the engine: it's already owned by m_engine
    // (unique_ptr). Giving it a QObject parent too would double-own it --
    // QWidget/QObject teardown would try to delete an already-destroyed
    // child once our own destructor (which runs before the base class's)
    // had already freed it via the unique_ptr.
    : QWidget(parent), m_engine(std::make_unique<VtermEngine>(kDefaultRows, kDefaultCols, nullptr)) {
    m_font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);
    // Protects the receive area from being crushed when the Macros panel is
    // expanded on a non-fullscreen window, same reasoning as HexView.
    setMinimumHeight(150);

    const QSettings settings;
    const int savedPointSize = settings.value(kFontPointSizeKey, m_font.pointSize()).toInt();
    applyFontPointSize(std::clamp(savedPointSize, kMinFontPointSize, kMaxFontPointSize));

    connect(m_engine.get(), &VtermEngine::damaged, this, &TerminalView::handleDamaged);
    connect(m_engine.get(), &VtermEngine::cursorMoved, this, &TerminalView::handleCursorMoved);
    connect(m_engine.get(), &VtermEngine::bell, this, &TerminalView::handleBell);
    connect(m_engine.get(), &VtermEngine::dataToSend, this, &TerminalView::handleDataToSend);
    connect(m_engine.get(), &VtermEngine::scrollbackChanged, this, &TerminalView::handleScrollbackChanged);
}

TerminalView::~TerminalView() = default;

void TerminalView::attachSession(Session* session) {
    if (m_session == session) {
        return;
    }
    if (m_session) {
        disconnect(m_session->ringBuffer(), &RingBuffer::bytesAppended, this,
                   &TerminalView::handleReceived);
    }
    const bool attachingNewLiveSession = session != nullptr;
    m_session = session;
    if (m_session) {
        connect(m_session->ringBuffer(), &RingBuffer::bytesAppended, this,
                &TerminalView::handleReceived);
    }
    // Reset on a new live connection (not on a plain detach) so a fresh
    // session doesn't show a previous one's leftover screen content.
    if (attachingNewLiveSession) {
        m_scrollOffset = 0;
        m_engine->reset();
    }
}

void TerminalView::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(0, 0, 0));
    if (!m_engine) {
        return;
    }

    const int rows = m_engine->rows();
    const int cols = m_engine->cols();

    for (int row = 0; row < rows; ++row) {
        const bool useScrollback = row < m_scrollOffset;
        QList<TerminalCell> scrollbackRow;
        if (useScrollback) {
            scrollbackRow = m_engine->scrollbackLine(m_scrollOffset - 1 - row);
        }

        for (int col = 0; col < cols; ++col) {
            TerminalCell cell;
            if (useScrollback) {
                cell = col < scrollbackRow.size() ? scrollbackRow.at(col) : TerminalCell{};
            } else {
                cell = m_engine->cellAt(row - m_scrollOffset, col);
            }

            const QColor bg = cell.reverse ? cell.foreground : cell.background;
            const QColor fg = cell.reverse ? cell.background : cell.foreground;

            const QRect cellRect(col * m_cellWidth, row * m_cellHeight, m_cellWidth, m_cellHeight);
            painter.fillRect(cellRect, bg);

            if (!cell.text.isEmpty() && cell.text != QLatin1String(" ")) {
                QFont f = m_font;
                f.setBold(cell.bold);
                f.setItalic(cell.italic);
                f.setUnderline(cell.underline);
                f.setStrikeOut(cell.strike);
                painter.setFont(f);
                painter.setPen(fg);
                painter.drawText(cellRect.left(), cellRect.top() + m_cellAscent, cell.text);
            }
        }
    }

    if (m_scrollOffset == 0 && m_engine->cursorVisible()) {
        const QPoint cursor = m_engine->cursorPosition();
        const QRect cursorRect(cursor.x() * m_cellWidth, cursor.y() * m_cellHeight, m_cellWidth,
                                m_cellHeight);
        painter.fillRect(cursorRect, QColor(255, 255, 255, hasFocus() ? 130 : 60));
    }
}

void TerminalView::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    recomputeGrid();
}

void TerminalView::recomputeGrid() {
    if (!m_engine || m_cellWidth <= 0 || m_cellHeight <= 0) {
        return;
    }
    const int cols = std::max(1, width() / m_cellWidth);
    const int rows = std::max(1, height() / m_cellHeight);
    m_engine->resize(rows, cols);
    update();
}

void TerminalView::applyFontPointSize(int pointSize) {
    m_fontPointSize = std::clamp(pointSize, kMinFontPointSize, kMaxFontPointSize);
    m_font.setPointSize(m_fontPointSize);

    const QFontMetrics fm(m_font);
    m_cellWidth = std::max(1, fm.horizontalAdvance(QLatin1Char('M')));
    m_cellHeight = std::max(1, fm.height());
    m_cellAscent = fm.ascent();

    recomputeGrid();

    QSettings settings;
    settings.setValue(kFontPointSizeKey, m_fontPointSize);
}

void TerminalView::zoomIn() {
    applyFontPointSize(m_fontPointSize + 1);
}

void TerminalView::zoomOut() {
    applyFontPointSize(m_fontPointSize - 1);
}

void TerminalView::resetZoom() {
    applyFontPointSize(QFontDatabase::systemFont(QFontDatabase::FixedFont).pointSize());
}

void TerminalView::keyPressEvent(QKeyEvent* event) {
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
        // Qt maps Cmd to Qt::ControlModifier on macOS, so this covers
        // Ctrl +/-/0 on Windows/Linux and Cmd +/-/0 on macOS with no extra
        // platform branching. Handled here (before forwarding to libvterm)
        // and consumed so the remote shell never sees these as keystrokes.
        switch (event->key()) {
        case Qt::Key_Plus:
        case Qt::Key_Equal: // '+' often requires Shift; '=' is the bare key
            zoomIn();
            event->accept();
            return;
        case Qt::Key_Minus:
            zoomOut();
            event->accept();
            return;
        case Qt::Key_0:
            resetZoom();
            event->accept();
            return;
        default:
            break;
        }
    }

    if (!m_engine) {
        QWidget::keyPressEvent(event);
        return;
    }
    if (m_scrollOffset != 0) {
        // Standard terminal convention: typing snaps you back to the live
        // screen.
        m_scrollOffset = 0;
        update();
    }
    m_engine->sendKeyEvent(event);
    event->accept();
}

void TerminalView::wheelEvent(QWheelEvent* event) {
    if (!m_engine) {
        event->ignore();
        return;
    }
    const int lines = event->angleDelta().y() / 40; // ~3 lines per notch
    if (lines != 0) {
        m_scrollOffset =
            std::clamp(m_scrollOffset + lines, 0, m_engine->scrollbackLineCount());
        update();
    }
    event->accept();
}

bool TerminalView::focusNextPrevChild(bool /*next*/) {
    // Keep Tab for the terminal session (e.g. shell completion) instead of
    // letting Qt consume it for focus traversal.
    return false;
}

void TerminalView::handleReceived(const QByteArray& data) {
    if (m_engine) {
        m_engine->feed(data);
    }
}

void TerminalView::handleDamaged(QRect /*cellRect*/) {
    // Whole-widget repaint: terminal grids are small (tens of cells), and
    // per-rect partial repaint is a minor optimization not worth the extra
    // bookkeeping for M2 -- see docs/ARCHITECTURE.md M2 scope notes.
    update();
}

void TerminalView::handleCursorMoved() {
    update();
}

void TerminalView::handleBell() {
    QApplication::beep();
}

void TerminalView::handleDataToSend(const QByteArray& bytes) {
    if (m_session) {
        m_session->send(bytes);
    }
}

void TerminalView::handleScrollbackChanged() {
    if (m_scrollOffset != 0) {
        update();
    }
}

} // namespace serialkit
