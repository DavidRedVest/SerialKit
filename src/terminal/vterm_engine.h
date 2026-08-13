#pragma once

#include <QByteArray>
#include <QColor>
#include <QList>
#include <QObject>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QVector>

extern "C" {
#include <vterm.h>
}

QT_BEGIN_NAMESPACE
class QKeyEvent;
QT_END_NAMESPACE

namespace serialkit {

// One terminal cell's renderable state, translated out of a VTermScreenCell
// so TerminalView never has to touch the C API directly.
struct TerminalCell {
    QString text; // usually one glyph; VTerm allows combining chars per cell
    QColor foreground;
    QColor background;
    bool bold = false;
    bool underline = false;
    bool italic = false;
    bool reverse = false;
    bool strike = false;
};

// Qt-friendly wrapper around libvterm (see docs/ARCHITECTURE.md M2 section
// for why this exists and how it's integrated -- cmake/Libvterm.cmake pulls
// the C sources, this class is the only place `#include <vterm.h>` should
// appear outside of that build glue).
//
// Ownership/threading: single-threaded, same as the rest of the app (see
// CLAUDE.md "不要为 QSerialPort 单开线程" -- the same reasoning applies
// here, libvterm's own parsing is fast enough not to need a worker thread
// for M2's scope).
class VtermEngine : public QObject {
    Q_OBJECT
public:
    explicit VtermEngine(int rows, int cols, QObject* parent = nullptr);
    ~VtermEngine() override;

    // Feeds raw bytes received from the transport into the VT parser.
    void feed(const QByteArray& bytes);

    // Resizes the terminal grid. No-ops if unchanged.
    void resize(int rows, int cols);

    // Clears the screen and scrollback. Called when a new Session is
    // attached so a fresh connection doesn't show a previous connection's
    // leftover screen content (see TerminalView::attachSession).
    void reset();

    // Translates a Qt key event into the bytes a real terminal would send
    // and emits dataToSend() with them. Handles both control keys (arrows,
    // Ctrl+letter, etc.) and printable text.
    void sendKeyEvent(QKeyEvent* event);

    int rows() const { return m_rows; }
    int cols() const { return m_cols; }

    TerminalCell cellAt(int row, int col) const;
    QPoint cursorPosition() const { return m_cursorPos; }
    bool cursorVisible() const { return m_cursorVisible; }

    // Scrollback, index 0 = the line most recently pushed off the top of
    // the screen (i.e. closest to the visible area). Converted to
    // TerminalCell on access; stored internally as raw VTermScreenCell so
    // sb_popline (libvterm asking for a line back, e.g. after a resize) can
    // hand back full-fidelity data instead of a lossy re-encoding.
    int scrollbackLineCount() const { return m_scrollback.size(); }
    QList<TerminalCell> scrollbackLine(int index) const;

signals:
    // `cellRect` is in cell coordinates (col/row), not pixels -- TerminalView
    // maps it to a pixel QRect using its own cell size.
    void damaged(QRect cellRect);
    void cursorMoved();
    void bell();
    void titleChanged(const QString& title);
    void dataToSend(const QByteArray& bytes);
    void scrollbackChanged();

private:
    // libvterm's callbacks are plain C function pointers; these static
    // trampolines recover `this` from the `user` pointer and forward to
    // instance methods below.
    static int onDamage(VTermRect rect, void* user);
    static int onMoveRect(VTermRect dest, VTermRect src, void* user);
    static int onMoveCursor(VTermPos pos, VTermPos oldpos, int visible, void* user);
    static int onSetTermProp(VTermProp prop, VTermValue* val, void* user);
    static int onBell(void* user);
    static int onResize(int rows, int cols, void* user);
    static int onSbPushLine(int cols, const VTermScreenCell* cells, void* user);
    static int onSbPopLine(int cols, VTermScreenCell* cells, void* user);

    int handleDamage(VTermRect rect);
    int handleMoveCursor(VTermPos pos, bool visible);
    int handleSetTermProp(VTermProp prop, VTermValue* val);
    int handleBell();
    int handleResize(int rows, int cols);
    int handleSbPushLine(int cols, const VTermScreenCell* cells);
    int handleSbPopLine(int cols, VTermScreenCell* cells);

    void flushOutput();
    TerminalCell toTerminalCell(VTermScreenCell cell) const;

    static constexpr int kMaxScrollbackLines = 5000;

    VTerm* m_vterm;
    VTermScreen* m_screen;
    int m_rows;
    int m_cols;
    QPoint m_cursorPos;
    bool m_cursorVisible = true;
    QList<QVector<VTermScreenCell>> m_scrollback; // front = most recently pushed
};

} // namespace serialkit
