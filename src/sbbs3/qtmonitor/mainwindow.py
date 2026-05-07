"""Main window — 4-pane docking layout with MQTT-driven widgets."""

from PySide6.QtCore import Qt, Slot, QSettings
from PySide6.QtGui import QAction, QIcon, QKeySequence
from PySide6.QtWidgets import (
    QMainWindow, QDockWidget, QToolBar, QStatusBar, QLabel, QTabBar,
    QMenuBar, QMessageBox, QInputDialog, QWidget,
)

from mqtt_client import MqttClient
from log_widget import LogWidget
from node_widget import NodeWidget
from client_widget import ClientWidget
from stats_widget import StatsWidget
from login_attempts_widget import LoginAttemptsWidget
from settings_dialog import SettingsDialog

SERVERS = ["term", "mail", "ftp", "web", "srvc"]
SERVER_LABELS = {
    "term": "Telnet", "mail": "Mail", "ftp": "FTP",
    "web": "Web", "srvc": "Services",
}


class SBBSMonitor(QMainWindow):

    def __init__(self, parent=None, mqtt_host=None, mqtt_port=None,
                 bbs_id=None, mqtt_user=None, mqtt_pass=None,
                 psk_id=None, psk_key=None):
        super().__init__(parent)
        self.setWindowTitle("Synchronet Monitor")
        self.resize(1200, 800)

        self._settings = QSettings("Synchronet", "Monitor")
        self._mqtt = MqttClient(self)
        self._log_panes = {}
        self._log_docks = {}
        self._server_status = {}

        self._setup_central()
        self._setup_docks()
        self._setup_menus()
        self._setup_statusbar()
        self._connect_mqtt_signals()
        self._restore_state()
        self._setup_toolbar()

        self._apply_global_style()

        if mqtt_host:
            self._settings.setValue("mqtt/host", mqtt_host)
        if mqtt_port:
            self._settings.setValue("mqtt/port", mqtt_port)
        if bbs_id is not None:
            self._settings.setValue("mqtt/bbs_id", bbs_id)
        if mqtt_user:
            self._settings.setValue("mqtt/username", mqtt_user)
        if mqtt_pass:
            self._settings.setValue("mqtt/password", mqtt_pass)
        if psk_id:
            self._settings.setValue("mqtt/psk_identity", psk_id)
        if psk_key:
            self._settings.setValue("mqtt/psk_key", psk_key)

        host = self._settings.value("mqtt/host", "")
        port = int(self._settings.value("mqtt/port", 0))
        bid = self._settings.value("mqtt/bbs_id", "") or None
        user = self._settings.value("mqtt/username", "") or None
        pw = self._settings.value("mqtt/password", "") or None
        pid = self._settings.value("mqtt/psk_identity", "") or None
        pkey = self._settings.value("mqtt/psk_key", "") or None

        if not host or not port:
            self._statusbar.showMessage(
                "MQTT not configured — use File > Settings or pass -m HOST -p PORT", 10000)
            return
        self._mqtt.configure(host, port, bid,
                             user or None, pw or None,
                             pid or None, pkey or None)
        self._mqtt.connect_to_broker()

    _DARK_STYLE = """
        QMainWindow { background-color: #2b2b2b; }
        QDockWidget { color: #cccccc; }
        QDockWidget::title {
            background-color: #353535; padding: 4px;
            border: 1px solid #555;
        }
        QMenuBar { background-color: #353535; color: #cccccc; }
        QMenuBar::item:selected { background-color: #505050; }
        QMenu { background-color: #353535; color: #cccccc; }
        QMenu::item:selected { background-color: #505050; }
        QToolBar { background-color: #353535; border: none; spacing: 2px; }
        QToolButton { color: #cccccc; padding: 4px 8px; }
        QToolButton:hover { background-color: #505050; }
        QStatusBar { background-color: #353535; color: #aaaaaa; }
        QPushButton { background-color: #404040; color: #cccccc;
                      border: 1px solid #555; padding: 3px 8px; }
        QPushButton:hover { background-color: #505050; }
        QComboBox { background-color: #404040; color: #cccccc;
                    border: 1px solid #555; }
    """

    def _apply_global_style(self):
        dark = self._settings.value("ui/dark_mode", True, type=bool)
        self._set_dark_mode(dark)

    def _set_dark_mode(self, dark):
        self._dark = dark
        self.setStyleSheet(self._DARK_STYLE if dark else "")
        self._node_widget.set_dark(dark)
        self._stats_widget.set_dark(dark)
        self._client_widget.set_dark(dark)
        self._login_attempts_widget.set_dark(dark)
        for pane in self._log_panes.values():
            pane.set_dark(dark)
        if hasattr(self, "_dark_action"):
            self._dark_action.setChecked(dark)
        self._settings.setValue("ui/dark_mode", dark)

    @Slot(bool)
    def _toggle_dark_mode(self, checked):
        self._set_dark_mode(checked)

    def _setup_central(self):
        self.setCentralWidget(QWidget())
        self.centralWidget().hide()

    def _setup_docks(self):
        self._node_widget = NodeWidget()
        self._node_dock = self._make_dock("Nodes", self._node_widget)
        self.addDockWidget(Qt.DockWidgetArea.TopDockWidgetArea, self._node_dock)

        self._stats_widget = StatsWidget()
        self._stats_dock = self._make_dock("Statistics", self._stats_widget)
        self.addDockWidget(Qt.DockWidgetArea.TopDockWidgetArea, self._stats_dock)

        self.splitDockWidget(self._node_dock, self._stats_dock, Qt.Orientation.Horizontal)

        first_log_dock = None
        for server in SERVERS:
            label = SERVER_LABELS[server]
            log = LogWidget(label)
            self._log_panes[server] = log
            dock = self._make_dock(label, log)
            self._log_docks[server] = dock
            self.addDockWidget(Qt.DockWidgetArea.BottomDockWidgetArea, dock)
            if first_log_dock is None:
                first_log_dock = dock
            else:
                self.tabifyDockWidget(first_log_dock, dock)

        bbs_log = LogWidget("BBS")
        self._log_panes["bbs"] = bbs_log
        bbs_dock = self._make_dock("BBS", bbs_log)
        self._log_docks["bbs"] = bbs_dock
        self.addDockWidget(Qt.DockWidgetArea.BottomDockWidgetArea, bbs_dock)
        if first_log_dock:
            self.tabifyDockWidget(first_log_dock, bbs_dock)

        events_log = LogWidget("Events")
        self._log_panes["events"] = events_log
        events_dock = self._make_dock("Events", events_log)
        self._log_docks["events"] = events_dock
        self.addDockWidget(Qt.DockWidgetArea.BottomDockWidgetArea, events_dock)
        if first_log_dock:
            self.tabifyDockWidget(first_log_dock, events_dock)

        self._client_widget = ClientWidget()
        self._client_dock = self._make_dock("Clients", self._client_widget)
        self.addDockWidget(Qt.DockWidgetArea.BottomDockWidgetArea, self._client_dock)
        if first_log_dock:
            self.splitDockWidget(first_log_dock, self._client_dock, Qt.Orientation.Horizontal)

        self._login_attempts_widget = LoginAttemptsWidget()
        self._login_attempts_dock = self._make_dock("Login Attempts", self._login_attempts_widget)
        self.addDockWidget(Qt.DockWidgetArea.BottomDockWidgetArea, self._login_attempts_dock)
        self.tabifyDockWidget(self._client_dock, self._login_attempts_dock)

        if first_log_dock:
            first_log_dock.raise_()

    def _make_dock(self, title, widget):
        dock = QDockWidget(title, self)
        dock.setObjectName(f"dock_{title}")
        dock.setWidget(widget)
        dock.setAllowedAreas(
            Qt.DockWidgetArea.AllDockWidgetAreas
        )
        dock.setFeatures(
            QDockWidget.DockWidgetFeature.DockWidgetMovable
            | QDockWidget.DockWidgetFeature.DockWidgetFloatable
            | QDockWidget.DockWidgetFeature.DockWidgetClosable
        )
        return dock

    def _setup_menus(self):
        menubar = self.menuBar()

        # File
        file_menu = menubar.addMenu("&File")
        settings_act = file_menu.addAction("&Settings...")
        settings_act.setShortcut("Ctrl+,")
        settings_act.triggered.connect(self._show_settings)
        file_menu.addSeparator()
        quit_act = file_menu.addAction("&Quit")
        quit_act.setShortcut(QKeySequence.StandardKey.Quit)
        quit_act.triggered.connect(self.close)

        # View
        view_menu = menubar.addMenu("&View")
        nda = self._node_dock.toggleViewAction()
        nda.setShortcut("Alt+1")
        view_menu.addAction(nda)
        sda = self._stats_dock.toggleViewAction()
        sda.setShortcut("Alt+2")
        view_menu.addAction(sda)
        cda = self._client_dock.toggleViewAction()
        cda.setShortcut("Alt+3")
        view_menu.addAction(cda)
        lda = self._login_attempts_dock.toggleViewAction()
        lda.setShortcut("Alt+4")
        view_menu.addAction(lda)
        view_menu.addSeparator()
        shortcut_num = 5
        for key in list(SERVERS) + ["bbs", "events"]:
            dock = self._log_docks.get(key)
            if dock:
                act = dock.toggleViewAction()
                if shortcut_num <= 9:
                    act.setShortcut(f"Alt+{shortcut_num}")
                    shortcut_num += 1
                view_menu.addAction(act)
        view_menu.addSeparator()
        self._dark_action = view_menu.addAction("&Dark Mode")
        self._dark_action.setCheckable(True)
        self._dark_action.setChecked(self._settings.value("ui/dark_mode", True, type=bool))
        self._dark_action.toggled.connect(self._toggle_dark_mode)
        view_menu.addSeparator()
        reset_act = view_menu.addAction("&Reset Layout")
        reset_act.setShortcut("Ctrl+Shift+R")
        reset_act.triggered.connect(self._reset_layout)

        # BBS
        bbs_menu = menubar.addMenu("&BBS")
        bbs_menu.addAction("&Recycle").triggered.connect(
            lambda: self._mqtt.recycle_server("term"))
        bbs_menu.addAction("&Pause").triggered.connect(
            lambda: self._mqtt.pause_server("term"))
        bbs_menu.addAction("Resu&me").triggered.connect(
            lambda: self._mqtt.resume_server("term"))
        bbs_menu.addSeparator()
        bbs_menu.addAction("&Clear Login Attempts").triggered.connect(
            lambda: self._mqtt.clear_server("term"))
        bbs_menu.addSeparator()
        bbs_menu.addAction("Force &Timed Event...").triggered.connect(
            self._force_timed_event)
        bbs_menu.addAction("Force &Network Callout...").triggered.connect(
            self._force_network_callout)

        # Per-server menus
        for server in SERVERS:
            if server == "term":
                continue
            label = SERVER_LABELS[server]
            srv_menu = menubar.addMenu(f"&{label}")
            srv_menu.addAction("&Recycle").triggered.connect(
                lambda checked, s=server: self._mqtt.recycle_server(s))
            srv_menu.addAction("&Pause").triggered.connect(
                lambda checked, s=server: self._mqtt.pause_server(s))
            srv_menu.addAction("Resu&me").triggered.connect(
                lambda checked, s=server: self._mqtt.resume_server(s))
            srv_menu.addSeparator()
            srv_menu.addAction("&Clear Login Attempts").triggered.connect(
                lambda checked, s=server: self._mqtt.clear_server(s))

    def _setup_toolbar(self):
        toolbar = QToolBar("Main")
        toolbar.setObjectName("toolbar_main")
        toolbar.setMovable(False)
        self.addToolBar(toolbar)

        self._toolbar_actions = []

        for name, dock in [("Nodes", self._node_dock),
                           ("Stats", self._stats_dock),
                           ("Clients", self._client_dock)]:
            act = QAction(name, self)
            act.triggered.connect(lambda checked, d=dock: self._show_dock(d))
            toolbar.addAction(act)
            self._toolbar_actions.append(act)

        toolbar.addSeparator()

        for server in SERVERS:
            label = SERVER_LABELS[server]
            act = QAction(label, self)
            act.triggered.connect(lambda checked, s=server: self._raise_log(s))
            toolbar.addAction(act)
            self._toolbar_actions.append(act)

        toolbar.addSeparator()
        self._connect_btn = toolbar.addAction("Connect")
        self._connect_btn.triggered.connect(self._toggle_connection)

    def _show_dock(self, dock):
        if not dock.isVisible():
            dock.setVisible(True)
        dock.raise_()
        title = dock.windowTitle()
        for tab_bar in self.findChildren(QTabBar):
            for i in range(tab_bar.count()):
                if tab_bar.tabText(i) == title:
                    tab_bar.setCurrentIndex(i)
                    return

    def _raise_log(self, server):
        dock = self._log_docks.get(server)
        if dock:
            self._show_dock(dock)

    def _setup_statusbar(self):
        self._statusbar = QStatusBar()
        self.setStatusBar(self._statusbar)
        self._stat_per_server = {}

        self._server_state_labels = {}
        for server in SERVERS:
            label = SERVER_LABELS[server]
            lbl = QLabel(f"{label}: --")
            self._server_state_labels[server] = lbl
            self._statusbar.addWidget(lbl)

        self._clients_label = QLabel("Clients: 0")
        self._served_label = QLabel("Served: 0")
        self._failed_label = QLabel("Failed: 0")
        self._errors_label = QLabel("Errors: 0")
        self._mqtt_label = QLabel("MQTT: Disconnected")
        self._statusbar.addPermanentWidget(self._clients_label)
        self._statusbar.addPermanentWidget(self._served_label)
        self._statusbar.addPermanentWidget(self._failed_label)
        self._statusbar.addPermanentWidget(self._errors_label)
        self._statusbar.addPermanentWidget(self._mqtt_label)

    def _connect_mqtt_signals(self):
        self._mqtt.connected.connect(self._on_mqtt_connected)
        self._mqtt.disconnected.connect(self._on_mqtt_disconnected)
        self._mqtt.error_occurred.connect(self._on_mqtt_error)
        self._mqtt.log_message.connect(self._on_log_message)
        self._mqtt.node_status.connect(self._node_widget.update_node)
        self._mqtt.node_verbose.connect(self._node_widget.update_node_verbose)
        self._mqtt.client_update.connect(self._client_widget.update_client)
        self._mqtt.server_state.connect(self._on_server_state)
        self._mqtt.server_stat.connect(self._on_server_stat)
        self._mqtt.login_attempt.connect(self._login_attempts_widget.update_attempt)
        self._login_attempts_widget.count_changed.connect(
            lambda count: self._failed_label.setText(f"Failed: {count}")
        )

        self._node_widget.node_action.connect(self._on_node_action)
        self._node_widget.node_message.connect(self._on_node_message)

    @Slot()
    def _on_mqtt_connected(self):
        self._statusbar.showMessage("Connected to MQTT broker", 3000)
        self._update_mqtt_label("Connected")
        self._connect_btn.setText("Disconnect")

    @Slot()
    def _on_mqtt_disconnected(self):
        self._update_mqtt_label("Disconnected")
        self._connect_btn.setText("Connect")

    @Slot(str)
    def _on_mqtt_error(self, msg):
        self._statusbar.showMessage(msg, 5000)
        self._update_mqtt_label("Error")
        events = self._log_panes.get("events")
        if events:
            events.append_log(3, "", msg)

    def _update_mqtt_label(self, state):
        self._mqtt_label.setText(f"MQTT: {state}")

    @Slot(str, int, str, str)
    def _on_log_message(self, server, level, timestamp, text):
        pane = self._log_panes.get(server)
        if pane:
            pane.append_log(level, timestamp, text)
        events = self._log_panes.get("events")
        if events:
            prefix = SERVER_LABELS.get(server, server)
            events.append_log(level, timestamp, f"[{prefix}] {text}")

    @Slot(str, str)
    def _on_server_state(self, server, state):
        self._server_status[server] = state
        lbl = self._server_state_labels.get(server)
        if lbl:
            label = SERVER_LABELS.get(server, server)
            lbl.setText(f"{label}: {state}")

    @Slot(str, str, str)
    def _on_server_stat(self, server, stat_name, value):
        if server not in self._stat_per_server:
            self._stat_per_server[server] = {}
        try:
            self._stat_per_server[server][stat_name] = int(value)
        except ValueError:
            return
        self._update_stat_totals()

    def _update_stat_totals(self):
        clients = sum(s.get("clients", 0) for s in self._stat_per_server.values())
        served = sum(s.get("served", 0) for s in self._stat_per_server.values())
        errors = sum(s.get("error_count", 0) for s in self._stat_per_server.values())
        self._clients_label.setText(f"Clients: {clients}")
        self._served_label.setText(f"Served: {served}")
        self._errors_label.setText(f"Errors: {errors}")

    @Slot()
    def _force_timed_event(self):
        code, ok = QInputDialog.getText(self, "Force Timed Event", "Event code:")
        if ok and code:
            self._mqtt.trigger_event(code)
            self._statusbar.showMessage(f"Triggered event: {code}", 3000)

    @Slot()
    def _force_network_callout(self):
        hub_id, ok = QInputDialog.getText(self, "Force Network Callout", "QHub ID:")
        if ok and hub_id:
            self._mqtt.trigger_callout(hub_id)
            self._statusbar.showMessage(f"Triggered callout: {hub_id}", 3000)

    @Slot(int)
    def _on_node_message(self, node_num):
        text, ok = QInputDialog.getText(
            self, "Send Message", f"Message to node {node_num}:")
        if ok and text:
            self._mqtt.send_node_message(node_num, text)

    @Slot(int, str)
    def _on_node_action(self, node_num, action):
        prop_map = {
            "lock": "lock",
            "down": "down",
            "interrupt": "interrupt",
            "rerun": "rerun",
            "clear_errors": "errors",
        }
        prop = prop_map.get(action, action)
        val = "0" if action == "clear_errors" else "1"
        self._mqtt.set_node(node_num, prop, val)

    @Slot()
    def _toggle_connection(self):
        if self._mqtt._client is None:
            self._mqtt.connect_to_broker()
        else:
            self._mqtt.disconnect_from_broker()

    @Slot()
    def _show_settings(self):
        dlg = SettingsDialog(self._settings, self)
        if dlg.exec():
            host = self._settings.value("mqtt/host", "localhost")
            port = int(self._settings.value("mqtt/port", 1883))
            bbs_id = self._settings.value("mqtt/bbs_id", "") or None
            user = self._settings.value("mqtt/username", "") or None
            pw = self._settings.value("mqtt/password", "") or None
            pid = self._settings.value("mqtt/psk_identity", "") or None
            pkey = self._settings.value("mqtt/psk_key", "") or None
            self._mqtt.disconnect_from_broker()
            self._mqtt.configure(host, port, bbs_id, user, pw, pid, pkey)
            self._mqtt.connect_to_broker()

    @Slot()
    def _reset_layout(self):
        self._settings.remove("window/state")
        from PySide6.QtCore import QTimer
        QTimer.singleShot(0, self._do_reset_layout)

    def _do_reset_layout(self):
        # Unfloat everything and make visible
        for dock in self.findChildren(QDockWidget):
            dock.setFloating(False)
            dock.setVisible(True)

        # Re-tabify log panes together
        log_docks = [self._log_docks[k] for k in list(SERVERS) + ["bbs", "events"]
                     if k in self._log_docks]
        for i in range(1, len(log_docks)):
            self.tabifyDockWidget(log_docks[0], log_docks[i])

        # Re-tabify clients and login attempts
        self.tabifyDockWidget(self._client_dock, self._login_attempts_dock)

        if log_docks:
            log_docks[0].raise_()


    def _restore_state(self):
        geom = self._settings.value("window/geometry")
        if geom:
            self.restoreGeometry(geom)
        state = self._settings.value("window/state")
        if state:
            self.restoreState(state)

    def closeEvent(self, event):
        self._settings.setValue("window/geometry", self.saveGeometry())
        self._settings.setValue("window/state", self.saveState())
        self._mqtt.disconnect_from_broker()
        super().closeEvent(event)
