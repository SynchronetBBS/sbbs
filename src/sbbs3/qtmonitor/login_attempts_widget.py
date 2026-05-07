"""Failed login attempts display widget."""

from datetime import datetime

from PySide6.QtCore import Qt, Slot, Signal
from PySide6.QtGui import QColor
from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QTreeWidget, QTreeWidgetItem, QMenu,
)


class LoginAttemptsWidget(QWidget):
    """Displays failed login attempts from MQTT updates."""

    count_changed = Signal(int)  # total number of IPs with failed attempts

    def __init__(self, dark=True, parent=None):
        super().__init__(parent)
        self._attempts = {}
        self._dark = dark
        self._setup_ui()

    def _setup_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(2, 2, 2, 2)

        self._tree = QTreeWidget()
        self._tree.setHeaderLabels([
            "IP Address", "First Attempt", "Last Attempt",
            "Count", "Dupes", "Protocol", "Username",
        ])
        self._tree.setColumnCount(7)
        self._tree.setRootIsDecorated(False)
        self._tree.setAlternatingRowColors(True)
        self._tree.setSortingEnabled(True)
        self.set_dark(self._dark)
        layout.addWidget(self._tree)

    def set_dark(self, dark):
        self._dark = dark
        if dark:
            self._tree.setStyleSheet(
                "QTreeWidget { background-color: #1e1e1e; color: #cccccc; "
                "alternate-background-color: #252525; }"
            )
        else:
            self._tree.setStyleSheet("")

    @Slot(str, str, dict)
    def update_attempt(self, ip, action, data):
        if action == "clear":
            if ip in self._attempts:
                idx = self._tree.indexOfTopLevelItem(self._attempts[ip])
                if idx >= 0:
                    self._tree.takeTopLevelItem(idx)
                del self._attempts[ip]
            self.count_changed.emit(len(self._attempts))
            return

        if ip not in self._attempts:
            item = QTreeWidgetItem()
            self._tree.addTopLevelItem(item)
            self._attempts[ip] = item

        item = self._attempts[ip]
        item.setText(0, ip)
        for col, key in [(1, "first"), (2, "last")]:
            ts = data.get(key, "")
            try:
                dt = datetime.fromisoformat(ts)
                item.setText(col, dt.strftime("%b %d %H:%M:%S"))
            except (ValueError, TypeError):
                item.setText(col, ts)
        item.setText(5, data.get("protocol", ""))
        item.setText(6, data.get("username", ""))

        try:
            count = int(data.get("count", "0"))
        except ValueError:
            count = 0
        item.setData(3, Qt.ItemDataRole.DisplayRole, count)

        try:
            dupes = int(data.get("dupes", "0"))
        except ValueError:
            dupes = 0
        item.setData(4, Qt.ItemDataRole.DisplayRole, dupes)
        if count >= 10:
            for col in range(7):
                item.setForeground(col, QColor("#FF4444") if self._dark else QColor("red"))
        elif count >= 5:
            for col in range(7):
                item.setForeground(col, QColor("#FF00FF") if self._dark else QColor("magenta"))
        self.count_changed.emit(len(self._attempts))
