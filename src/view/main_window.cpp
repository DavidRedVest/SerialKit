#include "view/main_window.h"

#include "core/session_manager.h"
#include "view/session_panel.h"

#include <QTabWidget>
#include <QToolButton>

#ifndef SERIALKIT_VERSION
#define SERIALKIT_VERSION "0.0.0-dev"
#endif

namespace serialkit {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      m_sessionManager(std::make_unique<SessionManager>()),
      m_sessionTabs(new QTabWidget(this)) {
    setWindowTitle(tr("SerialKit v%1").arg(QStringLiteral(SERIALKIT_VERSION)));

    m_sessionTabs->setTabsClosable(true);
    setCentralWidget(m_sessionTabs);

    // Corner "+" button to open another concurrent connection (M3: multiple
    // simultaneous Sessions, e.g. a board's main UART plus a radio module's
    // UART). Deliberately no cap on how many -- the OS/hardware already
    // limits how many ports can be open.
    auto* addButton = new QToolButton(m_sessionTabs);
    addButton->setText(tr("+"));
    addButton->setToolTip(tr("New session"));
    connect(addButton, &QToolButton::clicked, this, &MainWindow::addSessionTab);
    m_sessionTabs->setCornerWidget(addButton, Qt::TopRightCorner);

    connect(m_sessionTabs, &QTabWidget::tabCloseRequested, this, &MainWindow::closeSessionTab);

    // Pre-create one tab so the app opens straight to a connection bar, same
    // as the M1/M2 single-session experience -- "+" is only for additional
    // sessions beyond the first.
    addSessionTab();
}

MainWindow::~MainWindow() = default;

void MainWindow::addSessionTab() {
    auto* panel = new SessionPanel(m_sessionManager.get(), m_sessionTabs);
    const int index = m_sessionTabs->addTab(panel, tr("New Session"));
    connect(panel, &SessionPanel::labelChanged, this,
            [this, panel](const QString& label) { updateTabLabel(panel, label); });
    m_sessionTabs->setCurrentIndex(index);
}

void MainWindow::closeSessionTab(int index) {
    auto* panel = qobject_cast<SessionPanel*>(m_sessionTabs->widget(index));
    if (!panel) {
        return;
    }
    panel->disconnectSession();
    m_sessionTabs->removeTab(index);
    panel->deleteLater();
}

void MainWindow::updateTabLabel(SessionPanel* panel, const QString& label) {
    // Never cache the index: closing/adding sibling tabs shifts everyone
    // else's position, so look it up fresh each time a panel reports a
    // label change.
    const int index = m_sessionTabs->indexOf(panel);
    if (index >= 0) {
        m_sessionTabs->setTabText(index, label);
    }
}

} // namespace serialkit
