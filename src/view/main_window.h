#pragma once

#include <QMainWindow>
#include <memory>

QT_BEGIN_NAMESPACE
class QTabWidget;
class QToolButton;
QT_END_NAMESPACE

namespace serialkit {

class SessionManager;
class SessionPanel;

// M3 top-level window: an outer QTabWidget of SessionPanel tabs, one per
// open connection (see docs/ARCHITECTURE.md M3 section). Starts with one
// tab pre-created so the single-connection experience from M1/M2 is
// unchanged; the "+" corner button adds more. MainWindow itself owns only
// SessionManager and outer-tab bookkeeping -- everything about a single
// connection (port picker, Hex/Terminal views, send box, macros, raw
// logging) lives in SessionPanel.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void addSessionTab();
    void closeSessionTab(int index);

private:
    void updateTabLabel(SessionPanel* panel, const QString& label);

    std::unique_ptr<SessionManager> m_sessionManager;
    QTabWidget* m_sessionTabs;
};

} // namespace serialkit
