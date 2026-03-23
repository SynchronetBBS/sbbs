# DeuceSSH — Bugs found during branch coverage work

(none outstanding)

## Fixed

1. Handshake/rekey failure did not set terminate flag.
   Fixed in: handshake, rekey, send_packet, recv_packet_raw.

2. send_packet buffer overflow with large MAC digest.
   Fixed: mac_len added to buffer size check.
