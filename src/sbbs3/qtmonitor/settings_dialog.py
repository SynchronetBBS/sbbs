"""MQTT connection settings dialog."""

from PySide6.QtCore import Qt
from PySide6.QtWidgets import (
    QDialog, QVBoxLayout, QFormLayout, QLineEdit,
    QSpinBox, QDialogButtonBox, QGroupBox,
)


class SettingsDialog(QDialog):

    def __init__(self, settings, parent=None):
        super().__init__(parent)
        self._settings = settings
        self.setWindowTitle("Settings")
        self.setMinimumWidth(350)
        self._setup_ui()

    def _setup_ui(self):
        layout = QVBoxLayout(self)

        mqtt_group = QGroupBox("MQTT Broker")
        form = QFormLayout(mqtt_group)

        self._host = QLineEdit(self._settings.value("mqtt/host", ""))
        self._host.setPlaceholderText("(required)")
        form.addRow("Host:", self._host)

        self._port = QSpinBox()
        self._port.setRange(0, 65535)
        self._port.setSpecialValueText("(required)")
        self._port.setValue(int(self._settings.value("mqtt/port", 0)))
        form.addRow("Port:", self._port)

        self._bbs_id = QLineEdit(self._settings.value("mqtt/bbs_id", ""))
        self._bbs_id.setPlaceholderText("(auto-detect)")
        form.addRow("BBS ID:", self._bbs_id)

        self._username = QLineEdit(self._settings.value("mqtt/username", ""))
        self._username.setPlaceholderText("(optional)")
        form.addRow("Username:", self._username)

        self._password = QLineEdit(self._settings.value("mqtt/password", ""))
        self._password.setPlaceholderText("(optional)")
        self._password.setEchoMode(QLineEdit.EchoMode.Password)
        form.addRow("Password:", self._password)

        layout.addWidget(mqtt_group)

        psk_group = QGroupBox("TLS-PSK (for broker.js)")
        psk_form = QFormLayout(psk_group)

        self._psk_identity = QLineEdit(self._settings.value("mqtt/psk_identity", ""))
        self._psk_identity.setPlaceholderText("sysop alias (lowercased)")
        psk_form.addRow("PSK Identity:", self._psk_identity)

        self._psk_key = QLineEdit(self._settings.value("mqtt/psk_key", ""))
        self._psk_key.setPlaceholderText("sysop password (lowercased)")
        self._psk_key.setEchoMode(QLineEdit.EchoMode.Password)
        psk_form.addRow("PSK Key:", self._psk_key)

        layout.addWidget(psk_group)

        buttons = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel
        )
        buttons.accepted.connect(self._save_and_accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)


    def _save_and_accept(self):
        self._settings.setValue("mqtt/host", self._host.text())
        self._settings.setValue("mqtt/port", self._port.value())
        self._settings.setValue("mqtt/bbs_id", self._bbs_id.text())
        self._settings.setValue("mqtt/username", self._username.text())
        self._settings.setValue("mqtt/password", self._password.text())
        self._settings.setValue("mqtt/psk_identity", self._psk_identity.text())
        self._settings.setValue("mqtt/psk_key", self._psk_key.text())
        self.accept()
