"""Connected clients list widget."""

from datetime import datetime

from PySide6.QtCore import Qt, Slot
from PySide6.QtGui import QColor
from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QTreeWidget, QTreeWidgetItem, QMenu,
)


class ClientWidget(QWidget):
    """Displays connected clients from MQTT updates."""

    def __init__(self, dark=True, parent=None):
        super().__init__(parent)
        self._clients = {}
        self._dark = dark
        self._setup_ui()

    def set_dark(self, dark):
        self._dark = dark
        if dark:
            self._tree.setStyleSheet(
                "QTreeWidget { background-color: #1e1e1e; color: #cccccc; "
                "alternate-background-color: #252525; }"
            )
        else:
            self._tree.setStyleSheet("")

    def _setup_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(2, 2, 2, 2)

        self._tree = QTreeWidget()
        self._tree.setHeaderLabels([
            "Socket", "Protocol", "User", "Address", "Hostname", "Port", "Time",
        ])
        self._tree.setColumnCount(7)
        self._tree.setRootIsDecorated(False)
        self._tree.setAlternatingRowColors(True)
        self._tree.setSortingEnabled(True)
        self._tree.setContextMenuPolicy(Qt.ContextMenuPolicy.CustomContextMenu)
        self._tree.customContextMenuRequested.connect(self._context_menu)
        self.set_dark(self._dark)
        layout.addWidget(self._tree)

    def _context_menu(self, pos):
        menu = QMenu(self)
        menu.addAction("Close Socket")
        menu.addAction("Filter IP")
        menu.exec(self._tree.viewport().mapToGlobal(pos))

    @Slot(str, str, dict)
    def update_client(self, server, action, data):
        sock = data.get("socket", "")
        key = f"{server}:{sock}"

        if action == "disconnect":
            if key in self._clients:
                idx = self._tree.indexOfTopLevelItem(self._clients[key])
                if idx >= 0:
                    self._tree.takeTopLevelItem(idx)
                del self._clients[key]
            return

        if key not in self._clients:
            item = QTreeWidgetItem()
            self._tree.addTopLevelItem(item)
            self._clients[key] = item
        else:
            item = self._clients[key]

        item.setText(0, sock)
        item.setText(1, data.get("protocol", server))
        item.setText(2, data.get("username", ""))
        item.setText(3, data.get("ip", ""))
        item.setText(4, data.get("hostname", ""))
        item.setText(5, data.get("port", ""))
        ts = data.get("timestamp", "")
        try:
            dt = datetime.fromisoformat(ts)
            item.setText(6, dt.strftime("%b %d %H:%M:%S"))
        except (ValueError, TypeError):
            item.setText(6, ts)
