#pragma once

#include <QWidget>
#include <array>

QT_BEGIN_NAMESPACE
class QCheckBox;
class QLineEdit;
class QPushButton;
class QShortcut;
class QSpinBox;
class QTimer;
class QVBoxLayout;
QT_END_NAMESPACE

namespace serialkit {

class Session;

// Collapsible macro panel modeled on XCOM's "多条发送" tab and SSCOM's
// "多条字符串发送" panel -- see docs/ARCHITECTURE.md "UI 交互约定" for the
// exact synthesis of the two (per-row Hex checkbox like SSCOM, fixed 10
// slots + shared-period numbered cycling like XCOM, no per-row CRLF since
// neither reference has one there).
//
// Hidden by default; MainWindow toggles visibility via a button. Content,
// per-slot Hex/name/cycle-participation, and the panel-level shared
// settings (+CRLF, bind-numpad, auto-cycle, interval) persist across
// restarts via QSettings.
class MacroPanel : public QWidget {
    Q_OBJECT
public:
    explicit MacroPanel(QWidget* parent = nullptr);

    void attachSession(Session* session) { m_session = session; }

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private slots:
    void handleAutoCycleToggled(bool enabled);
    void handleBindNumpadToggled(bool enabled);
    void handleCycleTick();
    void savePanelSettings();

private:
    struct Slot {
        QCheckBox* cycleCheck = nullptr;
        QCheckBox* hexCheck = nullptr;
        QLineEdit* nameEdit = nullptr;
        QLineEdit* contentEdit = nullptr;
        QPushButton* sendButton = nullptr;
        QShortcut* numpadShortcut = nullptr;
    };

    static constexpr int kSlotCount = 10;

    void buildSlot(int index, QVBoxLayout* container);
    void loadSettings();
    void saveSlotSettings(int index);
    QByteArray encodeSlot(int index) const;
    void sendSlot(int index);
    void updateNumpadShortcutsEnabled();

    QCheckBox* m_crlfCheck;
    QCheckBox* m_bindNumpadCheck;
    QCheckBox* m_autoCycleCheck;
    QSpinBox* m_intervalSpin;
    QTimer* m_cycleTimer;
    int m_cycleCursor = 0;

    std::array<Slot, kSlotCount> m_slots;
    Session* m_session = nullptr;
};

} // namespace serialkit
