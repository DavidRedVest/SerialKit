#pragma once

#include <QMainWindow>
#include <memory>

QT_BEGIN_NAMESPACE
class QComboBox;
class QPushButton;
class QLabel;
QT_END_NAMESPACE

namespace serialkit {

class HexView;
class MacroPanel;
class MultiLineSend;
class SessionManager;
class TerminalView;

// M1/M2 top-level window: port picker + connect/disconnect, then a
// top-level Hex/Terminal tab widget -- Hex's page bundles Receive (HexView)
// + Send (compose box, macro panel); Terminal's page is just TerminalView,
// full height, no Send area (see docs/ARCHITECTURE.md "UI 交互约定").
// Wraps exactly one Session (SessionManager still enforces the "list of
// sessions" shape so M3 can lift the length-1 restriction without touching
// this class's collaborators, only its own connect/tab logic).
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void refreshPorts();
    void toggleConnection();
    void handleTransportError(const QString& message);
    void toggleMacroPanel();

private:
    void setConnectedUiState(bool connected);
    void updateByteCountLabel();

    std::unique_ptr<SessionManager> m_sessionManager;

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

    HexView* m_hexView;
    TerminalView* m_terminalView;
    MultiLineSend* m_multiLineSend;
    QPushButton* m_macroToggleButton;
    MacroPanel* m_macroPanel;
};

} // namespace serialkit
