"""MQTT client wrapper using paho-mqtt, bridged to Qt signals."""

import paho.mqtt.client as paho_mqtt
import paho.mqtt.properties as mqtt_properties
from PySide6.QtCore import QObject, Signal, Slot, Qt
from tls_psk import make_psk_context

# broker.js sends SubscriptionIdentifier=0 when no subscription ID was
# requested — valid intent but violates MQTT 5.0 spec (must be 1+).
# Patch paho's validation to accept 0.
_orig_setattr = mqtt_properties.Properties.__setattr__
def _patched_setattr(self, name, value):
    if name == "SubscriptionIdentifier" and value == 0:
        object.__setattr__(self, name, value)
        return
    _orig_setattr(self, name, value)
mqtt_properties.Properties.__setattr__ = _patched_setattr


class MqttClient(QObject):
    """Wraps paho-mqtt with Qt signal-based topic routing.

    Paho runs its network loop on a background thread; all signals are
    emitted via queued connections so slots execute on the Qt main thread.
    """

    connected = Signal()
    disconnected = Signal()
    error_occurred = Signal(str)

    log_message = Signal(str, int, str, str)  # server, level, timestamp, text
    node_status = Signal(int, dict)            # node_num, fields
    node_verbose = Signal(int, str)            # node_num, description
    client_update = Signal(str, str, dict)     # server, action, fields
    server_state = Signal(str, str)            # server, state
    server_stat = Signal(str, str, str)        # server, stat_name, value
    login_attempt = Signal(str, str, dict)     # ip, action, fields

    _emit_connected = Signal()
    _emit_disconnected = Signal()
    _emit_error = Signal(str)
    _emit_message = Signal(str, bytes)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._bbs_id = None
        self._host = "localhost"
        self._port = 1883
        self._username = None
        self._password = None
        self._psk_identity = None
        self._psk_key = None
        self._client = None

        self._emit_connected.connect(self.connected, Qt.QueuedConnection)
        self._emit_disconnected.connect(self.disconnected, Qt.QueuedConnection)
        self._emit_error.connect(self.error_occurred, Qt.QueuedConnection)
        self._emit_message.connect(self._dispatch_message, Qt.QueuedConnection)

    def configure(self, host="localhost", port=1883, bbs_id=None,
                  username=None, password=None,
                  psk_identity=None, psk_key=None):
        self._host = host
        self._port = port
        self._bbs_id = bbs_id
        self._username = username
        self._password = password
        self._psk_identity = psk_identity
        self._psk_key = psk_key

    def connect_to_broker(self):
        if self._client is not None:
            self.disconnect_from_broker()

        self._client = paho_mqtt.Client(
            paho_mqtt.CallbackAPIVersion.VERSION2,
            protocol=paho_mqtt.MQTTv5,
        )

        if self._psk_identity and self._psk_key:
            ctx = make_psk_context(self._psk_identity, self._psk_key)
            self._client.tls_set_context(ctx)
            self._client.tls_insecure_set(True)

        if self._username:
            self._client.username_pw_set(self._username, self._password)
        elif self._password and self._psk_identity:
            self._client.username_pw_set(self._psk_identity, self._password)

        self._client.on_connect = self._on_connect
        self._client.on_disconnect = self._on_disconnect
        self._client.on_message = self._on_message
        self._client.on_connect_fail = self._on_connect_fail
        self._client.reconnect_delay_set(min_delay=5, max_delay=60)
        self._client.enable_logger()

        try:
            self._client.connect_async(self._host, self._port)
            self._client.loop_start()
        except Exception as e:
            self._emit_error.emit(f"Connect error: {e}")

    def disconnect_from_broker(self):
        if self._client is not None:
            self._client.loop_stop()
            self._client.disconnect()
            self._client = None

    def _topic_prefix(self):
        if self._bbs_id:
            return f"sbbs/{self._bbs_id}"
        return "sbbs/+"

    def publish(self, topic_suffix, payload):
        if self._client is None or not self._bbs_id:
            return
        topic = f"sbbs/{self._bbs_id}/{topic_suffix}"
        data = payload.encode() if isinstance(payload, str) else payload
        self._client.publish(topic, data)

    def recycle_server(self, server):
        self.publish(f"host/+/server/{server}/recycle", "")

    def pause_server(self, server):
        self.publish(f"host/+/server/{server}/pause", "")

    def resume_server(self, server):
        self.publish(f"host/+/server/{server}/resume", "")

    def clear_server(self, server):
        self.publish(f"host/+/server/{server}/clear", "")

    def trigger_event(self, code):
        self.publish("exec", code)

    def trigger_callout(self, hub_id):
        self.publish("call", hub_id)

    def send_node_message(self, node_num, message):
        self.publish(f"node/{node_num}/msg", message)

    def set_node(self, node_num, prop, value):
        self.publish(f"node/{node_num}/set/{prop}", str(value))

    # -- paho callbacks (called from paho's network thread) --

    def _on_connect(self, client, userdata, flags, reason_code, properties=None):
        if reason_code == 0:
            prefix = self._topic_prefix()
            subs = [
                f"{prefix}/host/+/server/+/log/#",
                f"{prefix}/log/#",
                f"{prefix}/node/#",
                f"{prefix}/host/+/server/+/client/action/#",
                f"{prefix}/host/+/server/+/client",
                f"{prefix}/host/+/server/+",
                f"{prefix}/host/+/server/+/state/#",
                f"{prefix}/host/+/server/+/served",
                f"{prefix}/host/+/server/+/highwater",
                f"{prefix}/host/+/server/+/error_count",
                f"{prefix}/host/+/login_attempts/#",
            ]
            for topic in subs:
                client.subscribe(topic)
            self._emit_connected.emit()
        else:
            self._emit_error.emit(
                f"MQTT connect refused: {reason_code} ({reason_code.getName()})"
                if hasattr(reason_code, "getName")
                else f"MQTT connect refused: {reason_code}"
            )

    def _on_connect_fail(self, client, userdata):
        host = getattr(client, '_host', self._host)
        port = getattr(client, '_port', self._port)
        self._emit_error.emit(
            f"MQTT connection failed to {host}:{port} "
            f"(check hostname, port, and network)"
        )

    def _on_disconnect(self, client, userdata, flags, reason_code, properties=None):
        if reason_code != 0:
            self._emit_error.emit(f"MQTT disconnected unexpectedly: {reason_code}")
        self._emit_disconnected.emit()

    def _on_message(self, client, userdata, msg):
        self._emit_message.emit(msg.topic, msg.payload)

    # -- Qt-thread message dispatch --

    @Slot(str, bytes)
    def _dispatch_message(self, topic, payload):
        parts = topic.split("/")
        if len(parts) >= 2 and parts[0] == "sbbs" and not self._bbs_id:
            self._bbs_id = parts[1]

        text = payload.decode("utf-8", errors="replace")

        # sbbs/{id}/host/{h}/server/{srv}/log/{level}
        if len(parts) >= 8 and parts[4] == "server" and parts[6] == "log":
            server = parts[5]
            level = self._parse_log_level(parts[-1])
            timestamp, msg_text = self._split_tsv_payload(text)
            self.log_message.emit(server, level, timestamp, msg_text)
            return

        # sbbs/{id}/log/{level}
        if len(parts) >= 4 and parts[2] == "log":
            level = self._parse_log_level(parts[-1])
            timestamp, msg_text = self._split_tsv_payload(text)
            self.log_message.emit("bbs", level, timestamp, msg_text)
            return

        # sbbs/{id}/node/{n}/status (structured)
        if len(parts) >= 5 and parts[2] == "node" and parts[4] == "status":
            try:
                node_num = int(parts[3])
            except ValueError:
                return
            fields = text.split("\t")
            names = ["status", "action", "user", "connection",
                     "misc", "aux", "extaux", "errors"]
            self.node_status.emit(node_num, dict(zip(names, fields)))
            return

        # sbbs/{id}/node/{n} (verbose, human-readable)
        if len(parts) == 4 and parts[2] == "node":
            try:
                node_num = int(parts[3])
            except ValueError:
                return
            self.node_verbose.emit(node_num, text)
            return

        # sbbs/{id}/host/{h}/server/{srv}/client/action/{act}
        if (len(parts) >= 9 and parts[4] == "server"
                and parts[6] == "client" and parts[7] == "action"):
            server = parts[5]
            action = parts[8]
            fields = text.split("\t")
            names = ["timestamp", "protocol", "usernum", "username",
                     "ip", "hostname", "port", "socket"]
            self.client_update.emit(server, action, dict(zip(names, fields)))
            return

        # sbbs/{id}/host/{h}/server/{srv}/state/{state} (ignored — use server-level topic)
        if len(parts) >= 8 and parts[4] == "server" and parts[6] == "state":
            return

        # sbbs/{id}/host/{h}/login_attempts/{ip}
        if len(parts) >= 6 and parts[4] == "login_attempts":
            ip = "/".join(parts[5:])
            if not text:
                self.login_attempt.emit(ip, "clear", {})
            else:
                fields = text.split("\t")
                names = ["first", "last", "count", "dupes", "protocol", "username"]
                self.login_attempt.emit(ip, "update", dict(zip(names, fields)))
            return

        # sbbs/{id}/host/{h}/server/{srv} (server-level status, retained)
        if len(parts) == 6 and parts[4] == "server":
            state = text.split("\t", 1)[0]
            self.server_state.emit(parts[5], state)
            return

        # sbbs/{id}/host/{h}/server/{srv}/client (count)
        if (len(parts) == 7 and parts[4] == "server" and parts[6] == "client"):
            self.server_stat.emit(parts[5], "clients", text)
            return

        # sbbs/{id}/host/{h}/server/{srv}/{stat} (served/highwater/error_count)
        if (len(parts) == 7 and parts[4] == "server"
                and parts[6] in ("served", "highwater", "error_count")):
            self.server_stat.emit(parts[5], parts[6], text)
            return

    def _parse_log_level(self, level_str):
        try:
            return int(level_str)
        except ValueError:
            pass
        level_map = {
            "emergency": 0, "alert": 1, "critical": 2, "error": 3,
            "warn": 4, "warning": 4, "notice": 5, "info": 6, "debug": 7,
        }
        return level_map.get(level_str, 6)

    def _split_tsv_payload(self, text):
        fields = text.split("\t", 1)
        if len(fields) > 1:
            return fields[0], fields[1]
        return "", text
