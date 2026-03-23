# DeuceSSH — Bugs found during branch coverage work

(none outstanding)

## Fixed

1. Handshake/rekey failure did not set terminate flag.
   Fixed in: handshake, rekey, send_packet, recv_packet_raw.

2. send_packet buffer overflow with large MAC digest.
   Fixed: mac_len added to buffer size check.

3. curve25519 OPENSSL_cleanse on NULL raw_secret.
   In x25519_exchange (client side), malloc failure for raw_secret
   fell through to OPENSSL_cleanse(NULL, len) — segfault.
   Fixed: separate the NULL check from the EVP_PKEY_derive check.
