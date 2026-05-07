#!/usr/bin/env python3
"""Synchronet BBS Monitor — PySide6/MQTT prototype."""

import argparse
import sys
from PySide6.QtWidgets import QApplication
from mainwindow import SBBSMonitor

def main():
    parser = argparse.ArgumentParser(description="Synchronet BBS Monitor")
    parser.add_argument("-m", "--mqtt-host", metavar="HOST",
                        help="MQTT broker hostname (default: localhost)")
    parser.add_argument("-p", "--mqtt-port", metavar="PORT", type=int,
                        help="MQTT broker port (default: 1883)")
    parser.add_argument("-i", "--bbs-id", metavar="ID",
                        help="BBS system ID for MQTT topics (default: myBBS)")
    parser.add_argument("-u", "--mqtt-user", metavar="USER",
                        help="MQTT broker username")
    parser.add_argument("-P", "--mqtt-pass", metavar="PASS",
                        help="MQTT broker password (for broker.js: system password)")
    parser.add_argument("--psk-id", metavar="IDENTITY",
                        help="TLS-PSK identity (sysop alias, lowercased)")
    parser.add_argument("--psk-key", metavar="KEY",
                        help="TLS-PSK key (sysop password, lowercased)")
    args, qt_args = parser.parse_known_args()

    app = QApplication([sys.argv[0]] + qt_args)
    app.setApplicationName("Synchronet Monitor")
    app.setOrganizationName("Synchronet")
    window = SBBSMonitor(
        mqtt_host=args.mqtt_host,
        mqtt_port=args.mqtt_port,
        bbs_id=args.bbs_id,
        mqtt_user=args.mqtt_user,
        mqtt_pass=args.mqtt_pass,
        psk_id=args.psk_id,
        psk_key=args.psk_key,
    )
    window.show()
    return app.exec()

if __name__ == "__main__":
    sys.exit(main())
