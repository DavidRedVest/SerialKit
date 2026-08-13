#pragma once

#include <QFont>
#include <QRect>
#include <QWidget>
#include <memory>

QT_BEGIN_NAMESPACE
class QPaintEvent;
class QResizeEvent;
class QKeyEvent;
class QWheelEvent;
QT_END_NAMESPACE

namespace serialkit {

class Session;
class VtermEngine;

// Renders a Session's raw byte stream as a live VT100/xterm-ish terminal
// (see docs/ARCHITECTURE.md M2 section). Subscribes directly to
// RingBuffer::bytesAppended, the same as HexView -- NOT through
// IFrameStrategy -- because a live terminal session needs the full
// continuous byte stream with no delimiter/timeout framing applied. See
// "UI 交互约定" for the full reasoning; this mirrors why HexView bypasses
// the Framer too.
class TerminalView : public QWidget {
    Q_OBJECT
public:
    explicit TerminalView(QWidget* parent = nullptr);
    ~TerminalView() override;

    // Disconnects from any previously attached session's raw byte stream
    // and subscribes to `session`'s. Resets the terminal screen/scrollback
    // when switching to a genuinely new session, so a fresh connection
    // doesn't show a previous connection's leftover content -- but does
    // NOT clear on a plain detach (pass nullptr), matching HexView's
    // "leave content until the user explicitly clears" behavior.
    void attachSession(Session* session);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    bool focusNextPrevChild(bool next) override;

private slots:
    void handleReceived(const QByteArray& data);
    void handleDamaged(QRect cellRect);
    void handleCursorMoved();
    void handleBell();
    void handleDataToSend(const QByteArray& bytes);
    void handleScrollbackChanged();

private:
    void recomputeGrid();
    void applyFontPointSize(int pointSize);
    void zoomIn();
    void zoomOut();
    void resetZoom();

    std::unique_ptr<VtermEngine> m_engine;
    Session* m_session = nullptr;
    QFont m_font;
    int m_fontPointSize = 0;
    int m_cellWidth = 1;
    int m_cellHeight = 1;
    int m_cellAscent = 0;
    int m_scrollOffset = 0; // 0 = live screen; >0 = scrolled back N lines
};

} // namespace serialkit
