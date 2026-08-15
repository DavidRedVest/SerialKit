#include "view/session_panel.h"

#include "core/ring_buffer.h"
#include "core/session.h"
#include "core/session_manager.h"
#include "send/macro_panel.h"
#include "send/multi_line_send.h"
#include "transport/serial_transport.h"
#include "view/hex_view.h"
#include "view/terminal_view.h"

#include <QComboBox>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

namespace serialkit {

namespace {
const QList<int> kCommonBaudRates = {1200,   2400,    4800,    9600,   19200,
                                      38400,  57600,   115200,  230400, 460800,
                                      921600, 1000000, 1500000};
const QList<int> kDataBitsOptions = {5, 6, 7, 8};
const QStringList kParityOptions = {"None", "Odd", "Even", "Mark", "Space"};
const QStringList kStopBitsOptions = {"1", "1.5", "2"};
} // namespace

SessionPanel::SessionPanel(SessionManager* sessionManager, QWidget* parent)
    : QWidget(parent),
      m_sessionManager(sessionManager),
      m_portSelector(new QComboBox(this)),
      m_baudSelector(new QComboBox(this)),
      m_dataBitsSelector(new QComboBox(this)),
      m_paritySelector(new QComboBox(this)),
      m_stopBitsSelector(new QComboBox(this)),
      m_refreshButton(new QPushButton(tr("Refresh"), this)),
      m_connectButton(new QPushButton(tr("Connect"), this)),
      m_statusLabel(new QLabel(tr("Disconnected"), this)),
      m_byteCountLabel(new QLabel(this)),
      m_logButton(new QPushButton(tr("Start Logging..."), this)),
      m_logPathEdit(new QLineEdit(this)),
      m_hexView(new HexView(this)),
      m_terminalView(new TerminalView(this)),
      m_multiLineSend(new MultiLineSend(this)),
      m_macroToggleButton(new QPushButton(tr("▸ Macros"), this)),
      m_macroPanel(new MacroPanel(this)) {
    m_baudSelector->setEditable(true);
    for (const int baud : kCommonBaudRates) {
        m_baudSelector->addItem(QString::number(baud));
    }
    m_baudSelector->setCurrentText(QStringLiteral("115200"));

    for (const int bits : kDataBitsOptions) {
        m_dataBitsSelector->addItem(QString::number(bits));
    }
    m_dataBitsSelector->setCurrentText(QStringLiteral("8"));

    m_paritySelector->addItems(kParityOptions);
    m_paritySelector->setCurrentText(QStringLiteral("None"));

    m_stopBitsSelector->addItems(kStopBitsOptions);
    m_stopBitsSelector->setCurrentText(QStringLiteral("1"));

    m_connectButton->setObjectName("primaryButton");

    auto* topBar = new QWidget(this);
    auto* topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(8);
    topLayout->addWidget(new QLabel(tr("Port:"), topBar));
    topLayout->addWidget(m_portSelector, /*stretch=*/1);
    topLayout->addWidget(m_refreshButton);
    topLayout->addWidget(new QLabel(tr("Baud:"), topBar));
    topLayout->addWidget(m_baudSelector);
    topLayout->addWidget(new QLabel(tr("Data:"), topBar));
    topLayout->addWidget(m_dataBitsSelector);
    topLayout->addWidget(new QLabel(tr("Parity:"), topBar));
    topLayout->addWidget(m_paritySelector);
    topLayout->addWidget(new QLabel(tr("Stop:"), topBar));
    topLayout->addWidget(m_stopBitsSelector);
    topLayout->addWidget(m_connectButton);

    // Raw log controls: not part of the connection bar because they're
    // orthogonal to it (you can imagine starting a log before connecting),
    // and not part of either Hex/Terminal tab page since RawLogger captures
    // regardless of which view is in front -- same reasoning as the byte
    // counters below. Read-only QLineEdit for the path (not QLabel), same
    // convention as the file-send path display -- see docs/ARCHITECTURE.md
    // "UI 交互约定" #9.
    m_logPathEdit->setReadOnly(true);
    m_logPathEdit->setPlaceholderText(tr("No log file"));
    m_logButton->setEnabled(false);
    auto* logBar = new QWidget(this);
    auto* logLayout = new QHBoxLayout(logBar);
    logLayout->setContentsMargins(0, 0, 0, 0);
    logLayout->setSpacing(8);
    logLayout->addWidget(m_logButton);
    logLayout->addWidget(m_logPathEdit, /*stretch=*/1);

    // Hex and Terminal are two views over the same raw byte stream (see
    // docs/ARCHITECTURE.md M2 section), not two exclusive data sources --
    // both stay attached to the session and keep updating regardless of
    // which tab is in front, so switching tabs never loses data. What
    // differs per tab is purely how much UI chrome that mode needs: Hex
    // mode drives the port from the Send box below it, Terminal mode is
    // driven by typing directly into the terminal, so its page has no Send
    // area at all and gets the full page height instead.
    auto* hexPage = new QWidget(this);
    auto* hexPageLayout = new QVBoxLayout(hexPage);
    hexPageLayout->setContentsMargins(0, 0, 0, 0);
    hexPageLayout->setSpacing(12);

    auto* receiveGroup = new QGroupBox(tr("Receive"), hexPage);
    auto* receiveLayout = new QVBoxLayout(receiveGroup);
    receiveLayout->setSpacing(8);
    receiveLayout->addWidget(m_hexView);

    m_macroPanel->hide();
    auto* macroToggleRow = new QHBoxLayout();
    macroToggleRow->addWidget(m_macroToggleButton);
    macroToggleRow->addStretch(1);

    auto* sendGroup = new QGroupBox(tr("Send"), hexPage);
    auto* sendLayout = new QVBoxLayout(sendGroup);
    sendLayout->setSpacing(10);
    sendLayout->addWidget(m_multiLineSend);
    sendLayout->addLayout(macroToggleRow);
    sendLayout->addWidget(m_macroPanel);

    hexPageLayout->addWidget(receiveGroup, /*stretch=*/2);
    hexPageLayout->addWidget(sendGroup, /*stretch=*/1);

    auto* modeTabs = new QTabWidget(this);
    modeTabs->addTab(hexPage, tr("Hex"));
    modeTabs->addTab(m_terminalView, tr("Terminal"));

    // Status row: connection state + byte counters. This used to live on
    // QMainWindow::statusBar(), but that's a single global bar shared by
    // the whole window -- with M3 allowing more than one SessionPanel at
    // once, a global bar can't show "which session" a status belongs to.
    // Each panel gets its own, styled to match the old statusBar() layout
    // (state on the left, counters on the right).
    auto* statusRow = new QWidget(this);
    auto* statusLayout = new QHBoxLayout(statusRow);
    statusLayout->setContentsMargins(0, 0, 0, 0);
    statusLayout->addWidget(m_statusLabel, /*stretch=*/1);
    statusLayout->addWidget(m_byteCountLabel);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(12);
    mainLayout->addWidget(topBar);
    mainLayout->addWidget(logBar);
    mainLayout->addWidget(modeTabs, /*stretch=*/1);
    mainLayout->addWidget(statusRow);

    updateByteCountLabel();

    connect(m_refreshButton, &QPushButton::clicked, this, &SessionPanel::refreshPorts);
    connect(m_connectButton, &QPushButton::clicked, this, &SessionPanel::toggleConnection);
    connect(m_macroToggleButton, &QPushButton::clicked, this, &SessionPanel::toggleMacroPanel);
    connect(m_logButton, &QPushButton::clicked, this, &SessionPanel::toggleLogging);

    refreshPorts();
}

SessionPanel::~SessionPanel() = default;

bool SessionPanel::isConnected() const {
    return m_session != nullptr;
}

void SessionPanel::refreshPorts() {
    const QString previouslySelected = m_portSelector->currentText();
    m_portSelector->clear();
    m_portSelector->addItems(SerialTransport::availablePortNames());
    const int index = m_portSelector->findText(previouslySelected);
    if (index >= 0) {
        m_portSelector->setCurrentIndex(index);
    }
}

void SessionPanel::disconnectSession() {
    if (!m_session) {
        return;
    }
    // Logging never survives a disconnect (see header/ARCHITECTURE.md): a
    // new connection gets a new Session and a new RawLogger, so leaving the
    // button in "Stop Logging" state here would point at a file handle that
    // no longer receives anything. setConnectedUiState(false) below
    // disables the button; just reset its text/path here.
    m_logButton->setText(tr("Start Logging..."));
    m_logPathEdit->clear();

    m_hexView->attachSession(nullptr);
    m_terminalView->attachSession(nullptr);
    m_multiLineSend->attachSession(nullptr);
    m_macroPanel->attachSession(nullptr);
    m_sessionManager->removeSession(m_session);
    m_session = nullptr;
    setConnectedUiState(false);
    emit labelChanged(tr("New Session"));
}

void SessionPanel::toggleConnection() {
    if (m_session) {
        disconnectSession();
        return;
    }

    const QString portName = m_portSelector->currentText();
    if (portName.isEmpty()) {
        handleTransportError(tr("No serial port selected"));
        return;
    }

    auto transport = std::make_unique<SerialTransport>();
    Session* session = m_sessionManager->addSession(std::make_unique<Session>(std::move(transport)));
    connect(session->transport(), &ITransport::errorOccurred, this,
            &SessionPanel::handleTransportError);

    QVariantMap params;
    params["portName"] = portName;
    params["baudRate"] = m_baudSelector->currentText().toInt();
    params["dataBits"] = m_dataBitsSelector->currentText().toInt();
    params["parity"] = m_paritySelector->currentText().toLower();
    params["stopBits"] = m_stopBitsSelector->currentText().toDouble();

    if (!session->transport()->open(params)) {
        m_sessionManager->removeSession(session);
        return;
    }

    m_session = session;
    session->setName(portName);

    m_rxBytes = 0;
    m_txBytes = 0;
    updateByteCountLabel();
    connect(session->ringBuffer(), &RingBuffer::bytesAppended, this, [this](const QByteArray& chunk) {
        m_rxBytes += chunk.size();
        updateByteCountLabel();
    });
    connect(session, &Session::bytesSent, this, [this](const QByteArray& chunk) {
        m_txBytes += chunk.size();
        updateByteCountLabel();
    });

    m_hexView->attachSession(session);
    m_terminalView->attachSession(session);
    m_multiLineSend->attachSession(session);
    m_macroPanel->attachSession(session);
    m_logButton->setEnabled(true);
    setConnectedUiState(true);
    emit labelChanged(portName);
}

void SessionPanel::toggleMacroPanel() {
    const bool willShow = !m_macroPanel->isVisible();
    m_macroPanel->setVisible(willShow);
    m_macroToggleButton->setText(willShow ? tr("▾ Macros") : tr("▸ Macros"));
}

void SessionPanel::toggleLogging() {
    if (!m_session) {
        return;
    }
    if (m_session->rawLogger()->isLogging()) {
        m_session->rawLogger()->stop();
        m_logButton->setText(tr("Start Logging..."));
        return;
    }

    const QString path = QFileDialog::getSaveFileName(this, tr("Choose log file"), QString(),
                                                       QString(), nullptr,
                                                       QFileDialog::DontUseNativeDialog);
    if (path.isEmpty()) {
        return;
    }
    if (!m_session->rawLogger()->start(path)) {
        handleTransportError(tr("Could not open log file: %1").arg(path));
        return;
    }
    m_logPathEdit->setText(path);
    m_logPathEdit->setCursorPosition(0);
    m_logButton->setText(tr("Stop Logging"));
}

void SessionPanel::handleTransportError(const QString& message) {
    m_statusLabel->setText(tr("Error: %1").arg(message));
}

void SessionPanel::setConnectedUiState(bool connected) {
    m_connectButton->setText(connected ? tr("Disconnect") : tr("Connect"));
    m_portSelector->setEnabled(!connected);
    m_baudSelector->setEnabled(!connected);
    m_dataBitsSelector->setEnabled(!connected);
    m_paritySelector->setEnabled(!connected);
    m_stopBitsSelector->setEnabled(!connected);
    m_refreshButton->setEnabled(!connected);
    if (!connected) {
        m_logButton->setEnabled(false);
    }
    m_statusLabel->setText(connected ? tr("Connected: %1").arg(m_portSelector->currentText())
                                      : tr("Disconnected"));
}

void SessionPanel::updateByteCountLabel() {
    m_byteCountLabel->setText(tr("RX: %1 B   TX: %2 B").arg(m_rxBytes).arg(m_txBytes));
}

} // namespace serialkit
