#pragma once

#include <QVector>
#include <QWidget>

QT_BEGIN_NAMESPACE
class QCheckBox;
class QPlainTextEdit;
class QPushButton;
class QTimer;
QT_END_NAMESPACE

namespace serialkit {

class Session;

// Displays the raw byte stream of a Session, subscribed directly to its
// RingBuffer (RX) -- deliberately NOT behind the active IFrameStrategy, so
// it always shows every raw byte including inter-frame noise and partial
// frames (see docs/ARCHITECTURE.md §1.4).
//
// TX (Session::bytesSent) is NOT shown here by default. Earlier revisions
// merged TX/RX into one colored stream, which a real user mistook for a
// hardware loopback bug: they saw their own sent bytes appear in the box
// literally labeled "Receive" and concluded the device was echoing data it
// never received. "Show sent" is an opt-in checkbox for anyone who wants
// the merged view back -- see docs/ARCHITECTURE.md "UI 交互约定".
//
// UI updates are batched on a ~16ms timer per docs/ARCHITECTURE.md §1.3:
// incoming chunks are queued and only rendered when the timer fires, so a
// high-baud-rate stream cannot force a repaint per byte.
//
// Display is independent checkboxes, not a mode dropdown (see
// docs/ARCHITECTURE.md "UI 交互约定"): "Hex display" switches between a
// continuous space-separated hex stream and a plain decoded-text stream;
// "Timestamp" optionally prefixes each ~16ms flushed batch. Neither setting
// mutates what actually arrived -- RawLogger logs every byte regardless of
// what's checked here.
class HexView : public QWidget {
    Q_OBJECT
public:
    explicit HexView(QWidget* parent = nullptr);

    // Disconnects from any previously attached session and subscribes to
    // `session`'s RX/TX streams. Pass nullptr to just detach.
    void attachSession(Session* session);

private slots:
    void handleReceived(const QByteArray& data);
    void handleSent(const QByteArray& data);
    void flushPending();

private:
    struct PendingChunk {
        QByteArray data;
        bool isTx;
    };

    void appendChunk(const QByteArray& data, bool isTx);
    QString formatHex(const QByteArray& data) const;
    QString formatText(const QByteArray& data) const;
    void scheduleFlush();

    QCheckBox* m_hexCheck;
    QCheckBox* m_timestampCheck;
    QCheckBox* m_showSentCheck;
    QPushButton* m_clearButton;
    QPlainTextEdit* m_output;
    QTimer* m_flushTimer;
    QVector<PendingChunk> m_pending;
    Session* m_session = nullptr;
};

} // namespace serialkit
