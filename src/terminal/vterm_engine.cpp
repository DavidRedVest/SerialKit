#include "terminal/vterm_engine.h"

#include <QKeyEvent>

namespace serialkit {

VtermEngine::VtermEngine(int rows, int cols, QObject* parent)
    : QObject(parent), m_rows(rows), m_cols(cols) {
    m_vterm = vterm_new(rows, cols);
    vterm_set_utf8(m_vterm, 1);
    m_screen = vterm_obtain_screen(m_vterm);
    vterm_screen_enable_altscreen(m_screen, 1);

    static const VTermScreenCallbacks callbacks = {
        .damage = &VtermEngine::onDamage,
        .moverect = &VtermEngine::onMoveRect,
        .movecursor = &VtermEngine::onMoveCursor,
        .settermprop = &VtermEngine::onSetTermProp,
        .bell = &VtermEngine::onBell,
        .resize = &VtermEngine::onResize,
        .sb_pushline = &VtermEngine::onSbPushLine,
        .sb_popline = &VtermEngine::onSbPopLine,
    };
    vterm_screen_set_callbacks(m_screen, &callbacks, this);
    vterm_screen_reset(m_screen, 1);
}

VtermEngine::~VtermEngine() {
    vterm_free(m_vterm);
}

void VtermEngine::feed(const QByteArray& bytes) {
    vterm_input_write(m_vterm, bytes.constData(), static_cast<size_t>(bytes.size()));
    vterm_screen_flush_damage(m_screen);
}

void VtermEngine::resize(int rows, int cols) {
    if (rows == m_rows && cols == m_cols) {
        return;
    }
    vterm_set_size(m_vterm, rows, cols);
    m_rows = rows;
    m_cols = cols;
}

void VtermEngine::reset() {
    m_scrollback.clear();
    vterm_screen_reset(m_screen, 1);
    emit scrollbackChanged();
    emit damaged(QRect(0, 0, m_cols, m_rows));
}

void VtermEngine::sendKeyEvent(QKeyEvent* event) {
    int mod = VTERM_MOD_NONE;
    const Qt::KeyboardModifiers qmods = event->modifiers();
    if (qmods & Qt::ShiftModifier) mod |= VTERM_MOD_SHIFT;
    if (qmods & Qt::AltModifier) mod |= VTERM_MOD_ALT;
    if (qmods & Qt::ControlModifier) mod |= VTERM_MOD_CTRL;

    VTermKey vkey = VTERM_KEY_NONE;
    switch (event->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter: vkey = VTERM_KEY_ENTER; break;
        case Qt::Key_Tab: vkey = VTERM_KEY_TAB; break;
        case Qt::Key_Backspace: vkey = VTERM_KEY_BACKSPACE; break;
        case Qt::Key_Escape: vkey = VTERM_KEY_ESCAPE; break;
        case Qt::Key_Up: vkey = VTERM_KEY_UP; break;
        case Qt::Key_Down: vkey = VTERM_KEY_DOWN; break;
        case Qt::Key_Left: vkey = VTERM_KEY_LEFT; break;
        case Qt::Key_Right: vkey = VTERM_KEY_RIGHT; break;
        case Qt::Key_Insert: vkey = VTERM_KEY_INS; break;
        case Qt::Key_Delete: vkey = VTERM_KEY_DEL; break;
        case Qt::Key_Home: vkey = VTERM_KEY_HOME; break;
        case Qt::Key_End: vkey = VTERM_KEY_END; break;
        case Qt::Key_PageUp: vkey = VTERM_KEY_PAGEUP; break;
        case Qt::Key_PageDown: vkey = VTERM_KEY_PAGEDOWN; break;
        default:
            if (event->key() >= Qt::Key_F1 && event->key() <= Qt::Key_F35) {
                vkey = static_cast<VTermKey>(
                    VTERM_KEY_FUNCTION(event->key() - Qt::Key_F1 + 1));
            }
            break;
    }

    if (vkey != VTERM_KEY_NONE) {
        vterm_keyboard_key(m_vterm, vkey, static_cast<VTermModifier>(mod));
        flushOutput();
        return;
    }

    if ((mod & VTERM_MOD_CTRL) && event->key() >= Qt::Key_A && event->key() <= Qt::Key_Z) {
        // Derive from the key code, not text(): Qt often delivers an
        // already-control-transformed character in text() for Ctrl+letter
        // (e.g. Ctrl+C -> "\x03"), which would double-transform if handed
        // straight to vterm_keyboard_unichar alongside VTERM_MOD_CTRL.
        const uint32_t base = 'a' + (event->key() - Qt::Key_A);
        vterm_keyboard_unichar(m_vterm, base, static_cast<VTermModifier>(mod));
        flushOutput();
        return;
    }

    const QString text = event->text();
    if (!text.isEmpty()) {
        for (const QChar ch : text) {
            vterm_keyboard_unichar(m_vterm, ch.unicode(), static_cast<VTermModifier>(mod));
        }
        flushOutput();
    }
}

TerminalCell VtermEngine::cellAt(int row, int col) const {
    VTermPos pos;
    pos.row = row;
    pos.col = col;
    VTermScreenCell cell;
    if (!vterm_screen_get_cell(m_screen, pos, &cell)) {
        return TerminalCell{};
    }
    return toTerminalCell(cell);
}

QList<TerminalCell> VtermEngine::scrollbackLine(int index) const {
    QList<TerminalCell> line;
    if (index < 0 || index >= m_scrollback.size()) {
        return line;
    }
    const QVector<VTermScreenCell>& raw = m_scrollback.at(index);
    line.reserve(raw.size());
    for (const VTermScreenCell& cell : raw) {
        line.append(toTerminalCell(cell));
    }
    return line;
}

void VtermEngine::flushOutput() {
    char buffer[4096];
    size_t n;
    while ((n = vterm_output_read(m_vterm, buffer, sizeof(buffer))) > 0) {
        emit dataToSend(QByteArray(buffer, static_cast<int>(n)));
    }
}

TerminalCell VtermEngine::toTerminalCell(VTermScreenCell cell) const {
    TerminalCell out;

    char32_t codepoints[VTERM_MAX_CHARS_PER_CELL];
    qsizetype count = 0;
    for (int i = 0; i < VTERM_MAX_CHARS_PER_CELL && cell.chars[i] != 0; ++i) {
        codepoints[count++] = static_cast<char32_t>(cell.chars[i]);
    }
    if (count > 0) {
        out.text = QString::fromUcs4(codepoints, count);
    }

    VTermColor fg = cell.fg;
    VTermColor bg = cell.bg;
    vterm_screen_convert_color_to_rgb(m_screen, &fg);
    vterm_screen_convert_color_to_rgb(m_screen, &bg);
    out.foreground = QColor(fg.rgb.red, fg.rgb.green, fg.rgb.blue);
    out.background = QColor(bg.rgb.red, bg.rgb.green, bg.rgb.blue);

    out.bold = cell.attrs.bold;
    out.underline = cell.attrs.underline != 0;
    out.italic = cell.attrs.italic;
    out.reverse = cell.attrs.reverse;
    out.strike = cell.attrs.strike;

    return out;
}

int VtermEngine::onDamage(VTermRect rect, void* user) {
    return static_cast<VtermEngine*>(user)->handleDamage(rect);
}

int VtermEngine::handleDamage(VTermRect rect) {
    emit damaged(QRect(rect.start_col, rect.start_row, rect.end_col - rect.start_col,
                        rect.end_row - rect.start_row));
    return 1;
}

int VtermEngine::onMoveRect(VTermRect dest, VTermRect /*src*/, void* user) {
    // M2 scope: correctness over minimal-redraw -- a scrolled region is
    // just treated as damage over its destination rather than a real blit.
    return static_cast<VtermEngine*>(user)->handleDamage(dest);
}

int VtermEngine::onMoveCursor(VTermPos pos, VTermPos /*oldpos*/, int visible, void* user) {
    return static_cast<VtermEngine*>(user)->handleMoveCursor(pos, visible != 0);
}

int VtermEngine::handleMoveCursor(VTermPos pos, bool visible) {
    m_cursorPos = QPoint(pos.col, pos.row);
    m_cursorVisible = visible;
    emit cursorMoved();
    return 1;
}

int VtermEngine::onSetTermProp(VTermProp prop, VTermValue* val, void* user) {
    return static_cast<VtermEngine*>(user)->handleSetTermProp(prop, val);
}

int VtermEngine::handleSetTermProp(VTermProp prop, VTermValue* val) {
    if (prop == VTERM_PROP_TITLE && val->string.str) {
        emit titleChanged(QString::fromUtf8(val->string.str, static_cast<int>(val->string.len)));
    }
    return 1;
}

int VtermEngine::onBell(void* user) {
    return static_cast<VtermEngine*>(user)->handleBell();
}

int VtermEngine::handleBell() {
    emit bell();
    return 1;
}

int VtermEngine::onResize(int rows, int cols, void* user) {
    return static_cast<VtermEngine*>(user)->handleResize(rows, cols);
}

int VtermEngine::handleResize(int rows, int cols) {
    m_rows = rows;
    m_cols = cols;
    return 1;
}

int VtermEngine::onSbPushLine(int cols, const VTermScreenCell* cells, void* user) {
    return static_cast<VtermEngine*>(user)->handleSbPushLine(cols, cells);
}

int VtermEngine::handleSbPushLine(int cols, const VTermScreenCell* cells) {
    QVector<VTermScreenCell> line(cols);
    for (int i = 0; i < cols; ++i) {
        line[i] = cells[i];
    }
    m_scrollback.prepend(line);
    while (m_scrollback.size() > kMaxScrollbackLines) {
        m_scrollback.removeLast();
    }
    emit scrollbackChanged();
    return 1;
}

int VtermEngine::onSbPopLine(int cols, VTermScreenCell* cells, void* user) {
    return static_cast<VtermEngine*>(user)->handleSbPopLine(cols, cells);
}

int VtermEngine::handleSbPopLine(int cols, VTermScreenCell* cells) {
    if (m_scrollback.isEmpty()) {
        return 0;
    }
    const QVector<VTermScreenCell> line = m_scrollback.takeFirst();
    for (int i = 0; i < cols; ++i) {
        if (i < line.size()) {
            cells[i] = line.at(i);
        } else {
            // This row was pushed to scrollback when the terminal was
            // narrower than it is now (e.g. a resize happened in between,
            // which font zoom now makes routine). libvterm's resize() walks
            // this buffer by stepping `pos.col += cells[pos.col].width`, so
            // leaving trailing cells uninitialized risks a garbage width of
            // 0 -- which turns that walk into an infinite loop. A real
            // blank cell (width 1) keeps it moving.
            cells[i] = VTermScreenCell{};
            cells[i].width = 1;
        }
    }
    emit scrollbackChanged();
    return 1;
}

} // namespace serialkit
