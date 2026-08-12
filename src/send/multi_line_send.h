#pragma once

#include <QString>
#include <QWidget>

QT_BEGIN_NAMESPACE
class QCheckBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTimer;
QT_END_NAMESPACE

namespace serialkit {

class Session;

// The one send box (merged single-line + multi-line per user request --
// see docs/ARCHITECTURE.md "UI 交互约定"): a multi-line text box that also
// serves quick one-line commands via Enter-to-send (Shift+Enter inserts a
// newline instead, chat-app convention), Hex/+CRLF checkboxes, attached
// timed/cyclic re-send, and file sending. Hex/+CRLF/timed-send apply to the
// text box only; file sending is a separate raw-byte path below a divider
// so it's never mistaken for being governed by those checkboxes.
class MultiLineSend : public QWidget {
    Q_OBJECT
public:
    explicit MultiLineSend(QWidget* parent = nullptr);

    void attachSession(Session* session) { m_session = session; }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void handleSendClicked();
    void handleTimedToggled(bool enabled);
    void handleOpenFileClicked();
    void handleSendFileClicked();

private:
    QByteArray encode() const;

    QCheckBox* m_hexCheck;
    QCheckBox* m_crlfCheck;
    QPlainTextEdit* m_input;
    QPushButton* m_sendButton;
    QCheckBox* m_timedCheck;
    QSpinBox* m_intervalSpin;
    QTimer* m_timer;

    QLineEdit* m_filePathEdit;
    QPushButton* m_openFileButton;
    QPushButton* m_sendFileButton;

    QString m_filePath;
    Session* m_session = nullptr;
};

} // namespace serialkit
