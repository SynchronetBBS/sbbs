"""System statistics display widget."""

from PySide6.QtCore import Qt, Slot
from PySide6.QtGui import QFont
from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QGridLayout, QGroupBox, QLabel,
)


class StatsWidget(QWidget):
    """Displays system statistics, matching the ctrl StatsForm layout."""

    def __init__(self, dark=True, parent=None):
        super().__init__(parent)
        self._labels = {}
        self._dark = dark
        self._setup_ui()

    def set_dark(self, dark):
        self._dark = dark
        if dark:
            self.setStyleSheet(
                "QGroupBox { color: #aaaaaa; border: 1px solid #555; "
                "border-radius: 4px; margin-top: 8px; padding-top: 12px; } "
                "QGroupBox::title { subcontrol-origin: margin; left: 8px; } "
                "QLabel { color: #cccccc; } "
            )
        else:
            self.setStyleSheet("")

    def _setup_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(4, 4, 4, 4)

        value_font = QFont("Monospace", 10)
        value_font.setStyleHint(QFont.StyleHint.Monospace)

        groups = [
            ("Today", [
                ("logons", "Logons"), ("timeon", "Time On"),
                ("email", "Email"), ("feedback", "Feedback"),
                ("newusers", "New Users"), ("posts", "Posts"),
            ]),
            ("Total", [
                ("tlogons", "Logons"), ("ttimeon", "Time On"),
                ("temail", "Email"), ("tfeedback", "Feedback"),
                ("tusers", "Users"),
            ]),
            ("Uploads Today", [
                ("ulfiles", "Files"), ("ulbytes", "Bytes"),
            ]),
            ("Downloads Today", [
                ("dlfiles", "Files"), ("dlbytes", "Bytes"),
            ]),
        ]

        for group_name, fields in groups:
            group = QGroupBox(group_name)
            grid = QGridLayout(group)
            for row, (key, label) in enumerate(fields):
                name_lbl = QLabel(f"{label}:")
                val_lbl = QLabel("0")
                val_lbl.setFont(value_font)
                val_lbl.setAlignment(Qt.AlignmentFlag.AlignRight | Qt.AlignmentFlag.AlignVCenter)
                val_lbl.setMinimumWidth(80)
                grid.addWidget(name_lbl, row, 0)
                grid.addWidget(val_lbl, row, 1)
                self._labels[key] = val_lbl
            layout.addWidget(group)

        layout.addStretch()
        self.set_dark(self._dark)

    def set_stat(self, key, value):
        if key in self._labels:
            self._labels[key].setText(str(value))
