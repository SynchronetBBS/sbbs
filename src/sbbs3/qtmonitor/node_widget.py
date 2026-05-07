"""Node status list widget."""

from PySide6.QtCore import Qt, Slot, Signal
from PySide6.QtGui import QColor, QAction
from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QTreeWidget, QTreeWidgetItem,
    QToolBar, QMenu,
)


NODE_STATUS_NAMES = {
    "0": "WFC",
    "1": "Logon",
    "2": "New User",
    "3": "In Use",
    "4": "Quiet",
    "5": "Offline",
    "6": "Netting",
    "7": "Event Wait",
    "8": "Event Run",
    "9": "Event Limbo",
    "10": "Logout",
}

NODE_CONNECTION_NAMES = {
    65535: "Telnet",
    65534: "RLogin",
    65533: "SSH",
    65532: "Raw",
    65531: "SFTP",
    0: "Local",
}

NODE_ACTION_NAMES = [
    "Main",             # NODE_MAIN
    "Reading Msgs",     # NODE_RMSG
    "Reading Mail",     # NODE_RMAL
    "Sending Mail",     # NODE_SMAL
    "Reading G-Files",  # NODE_RTXT
    "Reading Sent Mail",# NODE_RSML
    "Posting Msg",      # NODE_PMSG
    "Auto-Msg",         # NODE_AMSG
    "External",         # NODE_XTRN
    "Defaults",         # NODE_DFLT
    "Xfer Prompt",      # NODE_XFER
    "Downloading",      # NODE_DLNG
    "Uploading",        # NODE_ULNG
    "Bidi Xfer",        # NODE_BXFR
    "Listing Files",    # NODE_LFIL
    "Logging On",       # NODE_LOGN
    "Local Chat",       # NODE_LCHT
    "Multi-Chat",       # NODE_MCHT
    "Guru Chat",        # NODE_GCHT
    "Chat Section",     # NODE_CHAT
    "Sysop",            # NODE_SYSP
    "QWK Xfer",         # NODE_TQWK
    "Private Chat",     # NODE_PCHT
    "Paging",           # NODE_PAGE
    "Seq Dev",          # NODE_RFSD
    "Custom",           # NODE_CUSTOM
]


class NodeWidget(QWidget):
    """Displays terminal node status from MQTT."""

    node_action = Signal(int, str)  # node_num, action_name
    node_message = Signal(int)      # node_num (prompt for message text in mainwindow)

    def __init__(self, dark=True, parent=None):
        super().__init__(parent)
        self._nodes = {}
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

        toolbar = QToolBar()
        for name, action_id in [
            ("Lock", "lock"), ("Down", "down"), ("Interrupt", "interrupt"),
            ("Rerun", "rerun"), ("Clear Errors", "clear_errors"),
        ]:
            act = toolbar.addAction(name)
            act.triggered.connect(lambda checked, a=action_id: self._emit_action(a))
        toolbar.addSeparator()
        msg_act = toolbar.addAction("Send Message")
        msg_act.triggered.connect(self._send_message)
        layout.addWidget(toolbar)

        self._tree = QTreeWidget()
        self._tree.setHeaderLabels(["Node", "Status", "User", "Connection", "Action", "Errors", "Description"])
        self._tree.setColumnCount(7)
        self._tree.setRootIsDecorated(False)
        self._tree.setAlternatingRowColors(True)
        self._tree.setSortingEnabled(True)
        self._tree.sortByColumn(0, Qt.SortOrder.AscendingOrder)
        self._tree.setSelectionMode(QTreeWidget.SelectionMode.ExtendedSelection)
        self._tree.setContextMenuPolicy(Qt.ContextMenuPolicy.CustomContextMenu)
        self._tree.customContextMenuRequested.connect(self._context_menu)
        self.set_dark(self._dark)
        layout.addWidget(self._tree)

    def _emit_action(self, action_id):
        for item in self._tree.selectedItems():
            node_num = item.data(0, Qt.ItemDataRole.DisplayRole)
            if isinstance(node_num, int):
                self.node_action.emit(node_num, action_id)

    def _send_message(self):
        for item in self._tree.selectedItems():
            node_num = item.data(0, Qt.ItemDataRole.DisplayRole)
            if isinstance(node_num, int):
                self.node_message.emit(node_num)

    def _context_menu(self, pos):
        menu = QMenu(self)
        for name, action_id in [
            ("Lock", "lock"), ("Down", "down"), ("Interrupt", "interrupt"),
            ("Rerun", "rerun"), ("Clear Errors", "clear_errors"),
        ]:
            act = menu.addAction(name)
            act.triggered.connect(lambda checked, a=action_id: self._emit_action(a))
        menu.addSeparator()
        msg_act = menu.addAction("Send Message")
        msg_act.triggered.connect(self._send_message)
        menu.exec(self._tree.viewport().mapToGlobal(pos))

    @Slot(int, dict)
    def update_node(self, node_num, data):
        key = str(node_num)
        if key not in self._nodes:
            item = QTreeWidgetItem()
            item.setData(0, Qt.ItemDataRole.DisplayRole, node_num)
            self._tree.addTopLevelItem(item)
            self._nodes[key] = item

        item = self._nodes[key]
        status_code = data.get("status", "")
        status_text = NODE_STATUS_NAMES.get(status_code, status_code)
        item.setText(1, status_text)
        try:
            user_num = int(data.get("user", "0"))
        except ValueError:
            user_num = 0
        if user_num:
            item.setData(2, Qt.ItemDataRole.DisplayRole, user_num)
        else:
            item.setData(2, Qt.ItemDataRole.DisplayRole, None)

        try:
            conn_val = int(data.get("connection", "0"))
        except ValueError:
            conn_val = 0
        item.setText(3, NODE_CONNECTION_NAMES.get(conn_val, str(conn_val)))

        try:
            action_val = int(data.get("action", "0"))
        except ValueError:
            action_val = 0
        if action_val < len(NODE_ACTION_NAMES):
            item.setText(4, NODE_ACTION_NAMES[action_val])
        else:
            item.setText(4, str(action_val))
        item.setText(5, data.get("errors", "0"))

        ncols = self._tree.columnCount()
        if status_text in ("In Use", "Quiet", "Logon", "New User"):
            for col in range(ncols):
                item.setForeground(col, QColor("#44FF44") if self._dark else QColor("darkGreen"))
        elif status_text == "WFC":
            for col in range(ncols):
                item.setForeground(col, QColor("#888888") if self._dark else QColor("gray"))

        errors = data.get("errors", "0")
        if errors != "0":
            item.setForeground(5, QColor("#FF4444") if self._dark else QColor("red"))

    @Slot(int, str)
    def update_node_verbose(self, node_num, description):
        key = str(node_num)
        if key not in self._nodes:
            item = QTreeWidgetItem()
            item.setText(0, key)
            self._tree.addTopLevelItem(item)
            self._nodes[key] = item
        self._nodes[key].setText(6, description)
