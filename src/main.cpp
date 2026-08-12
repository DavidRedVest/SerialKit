#include "view/app_style.h"
#include "view/main_window.h"

#include <QApplication>
#include <QStyleFactory>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("serialkit");
    QApplication::setOrganizationName("serialkit");

    // Fusion fully respects QSS on every platform (Qt's native macOS/Windows
    // styles override many properties to preserve OS look), which is what
    // makes the styling in app_style.cpp actually show up consistently --
    // also serves the project's "three platforms look the same" goal.
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    app.setStyleSheet(serialkit::appStyleSheet());

    serialkit::MainWindow window;
    window.resize(1000, 700);
    window.show();

    return QApplication::exec();
}
