#include "send/multi_line_send.h"

#include "core/session.h"
#include "send/send_encoding.h"
#include "transport/itransport.h"

#include <QCheckBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

namespace serialkit {

namespace {
constexpr qint64 kFileChunkSize = 4096;
}

MultiLineSend::MultiLineSend(QWidget* parent)
    : QWidget(parent),
      m_hexCheck(new QCheckBox(tr("Hex"), this)),
      m_crlfCheck(new QCheckBox(tr("+CRLF"), this)),
      m_input(new QPlainTextEdit(this)),
      m_sendButton(new QPushButton(tr("Send"), this)),
      m_timedCheck(new QCheckBox(tr("Timed send"), this)),
      m_intervalSpin(new QSpinBox(this)),
      m_timer(new QTimer(this)),
      m_filePathEdit(new QLineEdit(this)),
      m_openFileButton(new QPushButton(tr("Open File..."), this)),
      m_sendFileButton(new QPushButton(tr("Send File"), this)) {
    m_input->setPlaceholderText(
        tr("Sent as typed -- no escapes. Enter sends, Shift+Enter for a new line. Hex: 48 65 6C 6C 6F"));
    m_input->setMaximumBlockCount(0);
    m_input->installEventFilter(this);

    m_intervalSpin->setRange(10, 3600000);
    m_intervalSpin->setValue(1000);
    m_intervalSpin->setSuffix(tr(" ms"));

    m_filePathEdit->setReadOnly(true);
    m_filePathEdit->setPlaceholderText(tr("No file selected"));
    m_sendFileButton->setEnabled(false);

    m_sendButton->setObjectName("primaryButton");
    m_sendFileButton->setObjectName("primaryButton");

    // Text-send controls: only Hex/+CRLF apply here, above the text box.
    auto* textRow = new QHBoxLayout();
    textRow->setSpacing(10);
    textRow->addWidget(m_hexCheck);
    textRow->addWidget(m_crlfCheck);
    textRow->addStretch(1);

    auto* textSendRow = new QHBoxLayout();
    textSendRow->setSpacing(10);
    textSendRow->addStretch(1);
    textSendRow->addWidget(m_timedCheck);
    textSendRow->addWidget(m_intervalSpin);
    textSendRow->addWidget(m_sendButton);

    // Visual break so Hex/+CRLF/timed-send above are never mistaken for
    // file-send settings -- separate row entirely below a divider line.
    auto* divider = new QFrame(this);
    divider->setFrameShape(QFrame::HLine);
    divider->setFrameShadow(QFrame::Sunken);

    auto* fileRow = new QHBoxLayout();
    fileRow->setSpacing(10);
    fileRow->addWidget(m_filePathEdit, /*stretch=*/1);
    fileRow->addWidget(m_openFileButton);
    fileRow->addWidget(m_sendFileButton);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    layout->addLayout(textRow);
    layout->addWidget(m_input);
    layout->addLayout(textSendRow);
    layout->addWidget(divider);
    layout->addLayout(fileRow);
    setLayout(layout);

    connect(m_sendButton, &QPushButton::clicked, this, &MultiLineSend::handleSendClicked);
    connect(m_timedCheck, &QCheckBox::toggled, this, &MultiLineSend::handleTimedToggled);
    connect(m_timer, &QTimer::timeout, this, &MultiLineSend::handleSendClicked);
    connect(m_intervalSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int ms) {
        if (m_timer->isActive()) {
            m_timer->setInterval(ms);
        }
    });
    connect(m_openFileButton, &QPushButton::clicked, this, &MultiLineSend::handleOpenFileClicked);
    connect(m_sendFileButton, &QPushButton::clicked, this, &MultiLineSend::handleSendFileClicked);
}

bool MultiLineSend::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_input && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        const bool isEnter = keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter;
        if (isEnter && !(keyEvent->modifiers() & Qt::ShiftModifier)) {
            handleSendClicked();
            return true; // consumed: don't insert a newline
        }
    }
    return QWidget::eventFilter(watched, event);
}

QByteArray MultiLineSend::encode() const {
    const QByteArray body = m_hexCheck->isChecked() ? decodeHexString(m_input->toPlainText())
                                                      : toRawBytes(m_input->toPlainText());
    return appendCrlfIfRequested(body, m_crlfCheck->isChecked());
}

void MultiLineSend::handleSendClicked() {
    if (!m_session) {
        return;
    }
    const QByteArray data = encode();
    if (!data.isEmpty()) {
        m_session->send(data);
    }
}

void MultiLineSend::handleTimedToggled(bool enabled) {
    // Editable before starting so the user can set the interval they want;
    // locked while running so it can't be changed out from under an active
    // timer without an explicit stop first.
    m_intervalSpin->setEnabled(!enabled);
    if (enabled) {
        m_timer->start(m_intervalSpin->value());
    } else {
        m_timer->stop();
    }
}

void MultiLineSend::handleOpenFileClicked() {
    const QString path = QFileDialog::getOpenFileName(this, tr("Select file to send"), QString(),
                                                       QString(), nullptr,
                                                       QFileDialog::DontUseNativeDialog);
    if (path.isEmpty()) {
        return;
    }
    m_filePath = path;
    m_filePathEdit->setText(path);
    m_filePathEdit->setCursorPosition(0);
    m_sendFileButton->setEnabled(true);
}

void MultiLineSend::handleSendFileClicked() {
    if (!m_session || !m_session->transport()->isOpen() || m_filePath.isEmpty()) {
        return;
    }

    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    // Sent byte-for-byte with no encoding transform, regardless of the Hex
    // /+CRLF checkboxes above -- those only apply to the text box.
    while (!file.atEnd()) {
        const QByteArray chunk = file.read(kFileChunkSize);
        if (m_session->send(chunk) < 0) {
            break;
        }
    }
}

} // namespace serialkit
