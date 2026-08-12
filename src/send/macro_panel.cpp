#include "send/macro_panel.h"

#include "core/session.h"
#include "send/send_encoding.h"

#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QShortcut>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

namespace serialkit {

namespace {
QString slotKey(int index, const char* field) {
    return QStringLiteral("macros/slot%1/%2").arg(index).arg(field);
}
} // namespace

MacroPanel::MacroPanel(QWidget* parent)
    : QWidget(parent),
      m_crlfCheck(new QCheckBox(tr("+CRLF"), this)),
      m_bindNumpadCheck(new QCheckBox(tr("Bind numpad"), this)),
      m_autoCycleCheck(new QCheckBox(tr("Auto cycle"), this)),
      m_intervalSpin(new QSpinBox(this)),
      m_cycleTimer(new QTimer(this)) {
    m_intervalSpin->setRange(10, 3600000);
    m_intervalSpin->setValue(1000);
    m_intervalSpin->setSuffix(tr(" ms"));

    auto* sharedRow = new QHBoxLayout();
    sharedRow->setSpacing(10);
    sharedRow->addWidget(m_crlfCheck);
    sharedRow->addWidget(m_bindNumpadCheck);
    sharedRow->addWidget(m_autoCycleCheck);
    sharedRow->addWidget(m_intervalSpin);
    sharedRow->addStretch(1);

    // Bounded height + scroll, not addLayout() directly: 10 full rows would
    // otherwise keep growing the Send group and crush the Receive area on
    // anything less than a fullscreen window.
    auto* slotsContainer = new QWidget(this);
    auto* slotsLayout = new QVBoxLayout(slotsContainer);
    slotsLayout->setContentsMargins(0, 0, 0, 0);
    slotsLayout->setSpacing(4);
    for (int i = 0; i < kSlotCount; ++i) {
        buildSlot(i, slotsLayout);
    }

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidget(slotsContainer);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setMaximumHeight(220);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    layout->addLayout(sharedRow);
    layout->addWidget(scrollArea);
    setLayout(layout);

    // Connect before loadSettings() so that e.g. a persisted "auto cycle was
    // on" correctly restarts the timer via handleAutoCycleToggled, rather
    // than silently loading a checked box with no running timer behind it.
    connect(m_crlfCheck, &QCheckBox::toggled, this, &MacroPanel::savePanelSettings);
    connect(m_bindNumpadCheck, &QCheckBox::toggled, this, &MacroPanel::handleBindNumpadToggled);
    connect(m_autoCycleCheck, &QCheckBox::toggled, this, &MacroPanel::handleAutoCycleToggled);
    connect(m_intervalSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int ms) {
        savePanelSettings();
        if (m_cycleTimer->isActive()) {
            m_cycleTimer->setInterval(ms);
        }
    });
    connect(m_cycleTimer, &QTimer::timeout, this, &MacroPanel::handleCycleTick);

    loadSettings();
    updateNumpadShortcutsEnabled();
}

void MacroPanel::buildSlot(int index, QVBoxLayout* container) {
    Slot& slot = m_slots[index];
    slot.cycleCheck = new QCheckBox(tr("Cycle"), this);
    slot.cycleCheck->setToolTip(tr("Include in auto cycle"));
    slot.hexCheck = new QCheckBox(tr("Hex"), this);
    slot.nameEdit = new QLineEdit(this);
    slot.nameEdit->setPlaceholderText(tr("Name (optional)"));
    slot.nameEdit->setMaximumWidth(140);
    slot.contentEdit = new QLineEdit(this);
    slot.contentEdit->setPlaceholderText(tr("Sent as typed -- no escapes. Hex: 48 65 6C 6C 6F"));
    slot.sendButton = new QPushButton(QString::number(index), this);
    slot.sendButton->setObjectName("macroSlotButton");
    slot.sendButton->setMinimumWidth(32);

    auto* row = new QHBoxLayout();
    row->setSpacing(8);
    row->addWidget(slot.cycleCheck);
    row->addWidget(slot.hexCheck);
    row->addWidget(slot.nameEdit);
    row->addWidget(slot.contentEdit, /*stretch=*/1);
    row->addWidget(slot.sendButton);
    container->addLayout(row);

    connect(slot.sendButton, &QPushButton::clicked, this, [this, index] { sendSlot(index); });
    connect(slot.cycleCheck, &QCheckBox::toggled, this, [this, index] { saveSlotSettings(index); });
    connect(slot.hexCheck, &QCheckBox::toggled, this, [this, index] { saveSlotSettings(index); });
    connect(slot.nameEdit, &QLineEdit::editingFinished, this, [this, index] { saveSlotSettings(index); });
    connect(slot.contentEdit, &QLineEdit::editingFinished, this,
            [this, index] { saveSlotSettings(index); });

    slot.numpadShortcut = new QShortcut(QKeySequence(Qt::KeypadModifier | (Qt::Key_0 + index)), this);
    slot.numpadShortcut->setContext(Qt::WindowShortcut);
    slot.numpadShortcut->setEnabled(false);
    connect(slot.numpadShortcut, &QShortcut::activated, this, [this, index] { sendSlot(index); });
}

void MacroPanel::loadSettings() {
    QSettings settings;
    m_crlfCheck->setChecked(settings.value(QStringLiteral("macros/crlf"), false).toBool());
    m_bindNumpadCheck->setChecked(settings.value(QStringLiteral("macros/bindNumpad"), false).toBool());
    m_intervalSpin->setValue(settings.value(QStringLiteral("macros/intervalMs"), 1000).toInt());
    const bool autoCycle = settings.value(QStringLiteral("macros/autoCycle"), false).toBool();

    for (int i = 0; i < kSlotCount; ++i) {
        Slot& slot = m_slots[i];
        slot.nameEdit->setText(settings.value(slotKey(i, "name")).toString());
        slot.contentEdit->setText(settings.value(slotKey(i, "content")).toString());
        slot.hexCheck->setChecked(settings.value(slotKey(i, "hex"), false).toBool());
        slot.cycleCheck->setChecked(settings.value(slotKey(i, "inCycle"), false).toBool());
    }

    // Set after slot state is loaded so handleAutoCycleToggled (fired by
    // setChecked below) starts the timer against fully-restored slots.
    m_autoCycleCheck->setChecked(autoCycle);
}

void MacroPanel::saveSlotSettings(int index) {
    const Slot& slot = m_slots[index];
    QSettings settings;
    settings.setValue(slotKey(index, "name"), slot.nameEdit->text());
    settings.setValue(slotKey(index, "content"), slot.contentEdit->text());
    settings.setValue(slotKey(index, "hex"), slot.hexCheck->isChecked());
    settings.setValue(slotKey(index, "inCycle"), slot.cycleCheck->isChecked());
}

void MacroPanel::savePanelSettings() {
    QSettings settings;
    settings.setValue(QStringLiteral("macros/crlf"), m_crlfCheck->isChecked());
    settings.setValue(QStringLiteral("macros/bindNumpad"), m_bindNumpadCheck->isChecked());
    settings.setValue(QStringLiteral("macros/autoCycle"), m_autoCycleCheck->isChecked());
    settings.setValue(QStringLiteral("macros/intervalMs"), m_intervalSpin->value());
}

QByteArray MacroPanel::encodeSlot(int index) const {
    const Slot& slot = m_slots[index];
    const QByteArray body = slot.hexCheck->isChecked() ? decodeHexString(slot.contentEdit->text())
                                                         : toRawBytes(slot.contentEdit->text());
    return appendCrlfIfRequested(body, m_crlfCheck->isChecked());
}

void MacroPanel::sendSlot(int index) {
    if (!m_session) {
        return;
    }
    const QByteArray data = encodeSlot(index);
    if (!data.isEmpty()) {
        m_session->send(data);
    }
}

void MacroPanel::handleAutoCycleToggled(bool enabled) {
    savePanelSettings();
    if (enabled) {
        m_cycleCursor = 0;
        m_cycleTimer->start(m_intervalSpin->value());
    } else {
        m_cycleTimer->stop();
    }
}

void MacroPanel::handleBindNumpadToggled(bool enabled) {
    savePanelSettings();
    updateNumpadShortcutsEnabled();
}

void MacroPanel::handleCycleTick() {
    for (int i = 0; i < kSlotCount; ++i) {
        const int index = (m_cycleCursor + i) % kSlotCount;
        if (m_slots[index].cycleCheck->isChecked()) {
            sendSlot(index);
            m_cycleCursor = (index + 1) % kSlotCount;
            return;
        }
    }
}

void MacroPanel::updateNumpadShortcutsEnabled() {
    const bool enabled = m_bindNumpadCheck->isChecked() && isVisible();
    for (const Slot& slot : m_slots) {
        slot.numpadShortcut->setEnabled(enabled);
    }
}

void MacroPanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    updateNumpadShortcutsEnabled();
}

void MacroPanel::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    updateNumpadShortcutsEnabled();
}

} // namespace serialkit
