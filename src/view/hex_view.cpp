#include "view/hex_view.h"

#include "core/ring_buffer.h"
#include "core/session.h"

#include <QCheckBox>
#include <QDateTime>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextCursor>
#include <QTimer>
#include <QVBoxLayout>

namespace serialkit {

namespace {
constexpr int kBatchIntervalMs = 16;
}

HexView::HexView(QWidget* parent)
    : QWidget(parent),
      m_hexCheck(new QCheckBox(tr("Hex display"), this)),
      m_timestampCheck(new QCheckBox(tr("Timestamp"), this)),
      m_showSentCheck(new QCheckBox(tr("Show sent"), this)),
      m_clearButton(new QPushButton(tr("Clear"), this)),
      m_output(new QPlainTextEdit(this)),
      m_flushTimer(new QTimer(this)) {
    m_output->setReadOnly(true);
    m_output->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    m_output->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    // Protects the receive area from being crushed when the Macros panel is
    // expanded on a non-fullscreen window (see docs/ARCHITECTURE.md "UI 交互
    // 约定").
    m_output->setMinimumHeight(150);

    auto* toolbar = new QHBoxLayout();
    toolbar->setSpacing(10);
    toolbar->addWidget(m_hexCheck);
    toolbar->addWidget(m_timestampCheck);
    toolbar->addWidget(m_showSentCheck);
    toolbar->addStretch(1);
    toolbar->addWidget(m_clearButton);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    layout->addLayout(toolbar);
    layout->addWidget(m_output);
    setLayout(layout);

    m_flushTimer->setSingleShot(true);
    m_flushTimer->setInterval(kBatchIntervalMs);
    connect(m_flushTimer, &QTimer::timeout, this, &HexView::flushPending);
    connect(m_clearButton, &QPushButton::clicked, m_output, &QPlainTextEdit::clear);
}

void HexView::attachSession(Session* session) {
    if (m_session == session) {
        return;
    }
    if (m_session) {
        disconnect(m_session->ringBuffer(), &RingBuffer::bytesAppended, this,
                   &HexView::handleReceived);
        disconnect(m_session, &Session::bytesSent, this, &HexView::handleSent);
    }
    m_session = session;
    if (m_session) {
        connect(m_session->ringBuffer(), &RingBuffer::bytesAppended, this,
                &HexView::handleReceived);
        connect(m_session, &Session::bytesSent, this, &HexView::handleSent);
    }
}

void HexView::handleReceived(const QByteArray& data) {
    appendChunk(data, /*isTx=*/false);
}

void HexView::handleSent(const QByteArray& data) {
    // Off by default: showing sent bytes in a box labeled "Receive" reads as
    // a hardware loopback to users, even though it's just an echo of their
    // own send -- see the class comment.
    if (!m_showSentCheck->isChecked()) {
        return;
    }
    appendChunk(data, /*isTx=*/true);
}

void HexView::appendChunk(const QByteArray& data, bool isTx) {
    if (data.isEmpty()) {
        return;
    }
    m_pending.append({data, isTx});
    scheduleFlush();
}

void HexView::scheduleFlush() {
    if (!m_flushTimer->isActive()) {
        m_flushTimer->start();
    }
}

void HexView::flushPending() {
    if (m_pending.isEmpty()) {
        return;
    }

    QTextCharFormat txFormat;
    txFormat.setForeground(QColor(0, 90, 200));
    const QTextCharFormat rxFormat;

    QTextCursor cursor(m_output->document());
    cursor.movePosition(QTextCursor::End);

    for (const auto& chunk : m_pending) {
        QString text =
            m_hexCheck->isChecked() ? formatHex(chunk.data) : formatText(chunk.data);
        if (m_timestampCheck->isChecked()) {
            const QString stamp = QDateTime::currentDateTime().toString("[HH:mm:ss.zzz] ");
            text = QLatin1Char('\n') + stamp + text;
        }
        cursor.setCharFormat(chunk.isTx ? txFormat : rxFormat);
        cursor.insertText(text);
    }
    m_pending.clear();

    m_output->setTextCursor(cursor);
    m_output->ensureCursorVisible();
}

QString HexView::formatHex(const QByteArray& data) const {
    QString result;
    result.reserve(data.size() * 3);
    for (int i = 0; i < data.size(); ++i) {
        const auto byte = static_cast<quint8>(data.at(i));
        result += QString("%1 ").arg(byte, 2, 16, QLatin1Char('0')).toUpper();
    }
    return result;
}

QString HexView::formatText(const QByteArray& data) const {
    // \r\n (and a lone \r) render as an actual line break; every other byte
    // maps to exactly one visible token (printable char or '.') -- nothing
    // is silently dropped, see docs/ARCHITECTURE.md "UI 交互约定".
    QString text;
    text.reserve(data.size());
    for (int i = 0; i < data.size(); ++i) {
        const auto byte = static_cast<quint8>(data.at(i));
        if (byte == '\r') {
            const bool crlf = (i + 1 < data.size()) && data.at(i + 1) == '\n';
            if (!crlf) {
                text += QLatin1Char('\n');
            }
            continue;
        }
        if (byte == '\n') {
            text += QLatin1Char('\n');
        } else if (byte >= 0x20 && byte < 0x7f) {
            text += QChar(byte);
        } else {
            text += QLatin1Char('.');
        }
    }
    return text;
}

} // namespace serialkit
