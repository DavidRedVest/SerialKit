#include "view/app_style.h"

namespace serialkit {

QString appStyleSheet() {
    return QStringLiteral(R"(
        QWidget {
            font-size: 13px;
        }

        QGroupBox {
            border: 1px solid #c6cad2;
            border-radius: 8px;
            margin-top: 14px;
            padding-top: 12px;
            background-color: #fafbfc;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 12px;
            padding: 0 6px;
            font-weight: 600;
            color: #2b2f38;
        }

        QPushButton {
            border: 1px solid #c6cad2;
            border-radius: 5px;
            padding: 4px 14px;
            background-color: #f2f3f5;
        }
        QPushButton:hover {
            background-color: #e6e8eb;
        }
        QPushButton:pressed {
            background-color: #d9dce0;
        }
        QPushButton:disabled {
            color: #9aa0a8;
            background-color: #f2f3f5;
        }

        QPushButton#primaryButton {
            background-color: #2f6fed;
            border: 1px solid #2f6fed;
            color: white;
            font-weight: 600;
        }
        QPushButton#primaryButton:hover {
            background-color: #2a63d4;
        }
        QPushButton#primaryButton:pressed {
            background-color: #234fb0;
        }
        QPushButton#primaryButton:disabled {
            background-color: #a9c0f2;
            border: 1px solid #a9c0f2;
            color: #eef2fc;
        }

        /* Macro slot number buttons: same accent color as primaryButton but
           tight padding -- primaryButton's padding (4px 14px) leaves almost
           no room for a single digit at the compact width these need. */
        QPushButton#macroSlotButton {
            background-color: #2f6fed;
            border: 1px solid #2f6fed;
            color: white;
            font-weight: 600;
            padding: 4px 2px;
        }
        QPushButton#macroSlotButton:hover {
            background-color: #2a63d4;
        }
        QPushButton#macroSlotButton:pressed {
            background-color: #234fb0;
        }

        QLineEdit, QPlainTextEdit, QSpinBox, QComboBox {
            border: 1px solid #c6cad2;
            border-radius: 4px;
            padding: 3px 6px;
            background-color: white;
            selection-background-color: #2f6fed;
        }
        QLineEdit:focus, QPlainTextEdit:focus, QSpinBox:focus, QComboBox:focus {
            border: 1px solid #2f6fed;
        }
        QLineEdit:disabled, QComboBox:disabled, QSpinBox:disabled {
            background-color: #f2f3f5;
            color: #9aa0a8;
        }

        QCheckBox {
            spacing: 6px;
        }

        QStatusBar {
            border-top: 1px solid #c6cad2;
        }
    )");
}

} // namespace serialkit
