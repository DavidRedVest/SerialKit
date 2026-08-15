#pragma once

#include <QWidget>

QT_BEGIN_NAMESPACE
class QComboBox;
class QPushButton;
class QLabel;
class QLineEdit;
QT_END_NAMESPACE

namespace serialkit {

class HexView;
class MacroPanel;
class MultiLineSend;
class Session;
class SessionManager;
class TerminalView;

// One connection's worth of UI: port picker + connect/disconnect, a status
// row (connection state + byte counters -- moved off QMainWindow::statusBar
// in M3, since that's a single global bar and can't represent "which
// session" once more than one is open), the Hex/Terminal top-level tabs
// (from M2), and raw-log start/stop controls. MainWindow (M3) owns a
// QTabWidget of these, one per open session -- see docs/ARCHITECTURE.md
// M3 section.
//
// Does not own the SessionManager; takes a non-owning pointer and adds/
// removes its own Session from it directly as the user connects/
// disconnects, mirroring how MainWindow drove SessionManager pre-M3.
class SessionPanel : public QWidget {
    Q_OBJECT
public:
    explicit SessionPanel(SessionManager* sessionManager, QWidget* parent = nullptr);
    ~SessionPanel() override;

    // Disconnects the current session (if any), removing it from the
    // SessionManager. Safe to call when already disconnected. MainWindow
    // calls this when the tab hosting this panel is closed.
    void disconnectSession();

    bool isConnected() const;

signals:
    // Emitted whenever the text MainWindow should show on this panel's
    // outer tab changes: the port name once connected, a generic
    // placeholder once disconnected.
    void labelChanged(const QString& label);

private slots:
    void refreshPorts();
    void toggleConnection();
    void handleTransportError(const QString& message);
    void toggleMacroPanel();
    void toggleLogging();

private:
    void setConnectedUiState(bool connected);
    void updateByteCountLabel();

    SessionManager* m_sessionManager;
    Session* m_session = nullptr;

    QComboBox* m_portSelector;
    QComboBox* m_baudSelector;
    QComboBox* m_dataBitsSelector;
    QComboBox* m_paritySelector;
    QComboBox* m_stopBitsSelector;
    QPushButton* m_refreshButton;
    QPushButton* m_connectButton;
    QLabel* m_statusLabel;
    QLabel* m_byteCountLabel;
    qint64 m_rxBytes = 0;
    qint64 m_txBytes = 0;

    QPushButton* m_logButton;
    QLineEdit* m_logPathEdit;

    HexView* m_hexView;
    TerminalView* m_terminalView;
    MultiLineSend* m_multiLineSend;
    QPushButton* m_macroToggleButton;
    MacroPanel* m_macroPanel;
};

} // namespace serialkit
