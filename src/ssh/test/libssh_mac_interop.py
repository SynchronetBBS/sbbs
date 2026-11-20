#!/usr/bin/env python3
"""Independent directional cipher/MAC checks over a local socketpair."""
import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import socket
import subprocess
import sys
import tempfile
import time

sys.dont_write_bytecode = True
from openssh_interop import pkcs8


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def stop(process):
    if process.poll() is None:
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
    return process.wait(timeout=5)


def run(args):
    evidence = Path(args.evidence).resolve()
    evidence.mkdir(parents=True, exist_ok=True)
    c2s = "hmac-sha2-" + args.c2s
    s2c = "hmac-sha2-" + ("512" if args.c2s == "256" else "256")
    cipher_c2s = "aes128-cbc" if args.cipher_c2s == "cbc" else "aes256-ctr"
    cipher_s2c = ("aes256-ctr" if args.cipher_c2s == "cbc" else "aes128-cbc") if args.cipher_c2s else cipher_c2s
    final_ciphers = [cipher_s2c, cipher_c2s] if args.swap_rekey else [cipher_c2s, cipher_s2c]
    final_macs = [s2c, c2s] if args.swap_rekey else [c2s, s2c]
    result = dict(status="FAILED", role=args.role, c2s=c2s, s2c=s2c, children=[],
                  cipher_c2s=cipher_c2s, cipher_s2c=cipher_s2c, swap_rekey=args.swap_rekey,
                  binary_sha256={args.peer: digest(args.peer), args.libssh: digest(args.libssh)})
    processes = []
    for name in ["result.json", "result.json.pending", "keygen.log", "peer.log", "libssh.log"]:
        (evidence / name).unlink(missing_ok=True)
    try:
        with tempfile.TemporaryDirectory(prefix="dssh directional mac-") as directory:
            work = Path(directory)
            private = work / "key"
            pkcs8_key = work / "key-pkcs8"
            blob = work / "public-blob"
            with (evidence / "keygen.log").open("wb") as output:
                command = [args.keygen, "-q", "-t", "rsa", "-b", "2048", "-m", "PEM",
                           "-N", "", "-f", str(private)]
                keygen = subprocess.run(command, stdout=output, stderr=subprocess.STDOUT, timeout=30)
                result["keygen"] = dict(command=command, exit=keygen.returncode)
                if keygen.returncode != 0:
                    raise RuntimeError("fixture key generation failed")
            pkcs8_key.write_text(pkcs8(private))
            pkcs8_key.chmod(0o600)
            public = private.with_suffix(".pub").read_text().split()
            if len(public) < 2 or public[0] != "ssh-rsa":
                raise RuntimeError("unexpected public fixture key")
            blob.write_bytes(base64.b64decode(public[1], validate=True))
            env = os.environ.copy()
            for key in ["DSSH_TEST_LANGUAGE_FIELDS", "DSSH_TEST_ENC", "DSSH_TEST_ENC_S2C",
                        "DSSH_TEST_REKEY_SWAP", "DSSH_TEST_MAC", "DSSH_TEST_MAC_S2C"]:
                env.pop(key, None)
            env.update(DSSH_TEST_MAC=c2s, DSSH_TEST_MAC_S2C=s2c,
                       DSSH_TEST_ENC=cipher_c2s, DSSH_TEST_ENC_S2C=cipher_s2c)
            if args.swap_rekey:
                env["DSSH_TEST_REKEY_SWAP"] = "1"
            left, right = socket.socketpair()
            with left, right, (evidence / "peer.log").open("wb") as peer_log, (evidence / "libssh.log").open("wb") as foreign_log:
                library_command = [args.peer, args.role, "curve25519-sha256", str(pkcs8_key), str(blob), "audit"]
                foreign_role = "client" if args.role == "server" else "server"
                foreign_command = [args.libssh, foreign_role, str(right.fileno()), c2s, s2c, str(pkcs8_key), "audit"]
                if args.cipher_c2s:
                    foreign_command.extend([cipher_c2s, cipher_s2c, "swap" if args.swap_rekey else "keep"])
                library = subprocess.Popen(library_command, stdin=left, stdout=left, stderr=peer_log,
                                           env=env, start_new_session=True)
                processes.append((library, library_command))
                foreign = subprocess.Popen(foreign_command, pass_fds=(right.fileno(),),
                                           stdout=foreign_log, stderr=subprocess.STDOUT,
                                           env=env, start_new_session=True)
                processes.append((foreign, foreign_command))
                left.close()
                right.close()
                until = time.monotonic() + 60
                for process in [foreign, library]:
                    code = process.wait(timeout=max(0.01, until - time.monotonic()))
                    if code != 0:
                        raise RuntimeError(f"native process exit {code}: {process.args[0]}")
            foreign_text = (evidence / "libssh.log").read_text(errors="replace")
            peer_text = (evidence / "peer.log").read_text(errors="replace")
            pattern = (rf"libssh_mac role={foreign_role} c2s={c2s} s2c={s2c} bytes-each-way=262144 "
                       r"newkeys=(\d+) direction-checks=2 signed-auth=1 exec=1 valid=1 version=([^\n]+)")
            match = re.search(pattern, foreign_text)
            if match is None or int(match[1]) < 2:
                raise RuntimeError("independent directional/transfer/rekey oracle missing")
            if foreign_text.count("Received SSH_MSG_NEWKEYS") != int(match[1]):
                raise RuntimeError("independent NEWKEYS log count disagrees")
            pattern = (rf"DeuceSSH interop: role={args.role} kex=curve25519-sha256 cipher={cipher_c2s} mac={c2s} "
                       rf"stage=channel result=0 host-checks=(\d+) signed-auth=(\d+) channel-ack=(\d+) language-checks=0 mac-s2c={s2c} algorithm-checks=2")
            native = re.search(pattern, peer_text)
            if native is None or (args.role == "client" and int(native[1]) < 2) \
                    or (args.role == "server" and (native[2] != "1" or native[3] != "1")):
                raise RuntimeError("production fixture completion missing")
            foreign_epochs = re.findall(
                r"libssh_algorithms epoch=([01]) cipher-c2s=([a-z0-9-]+) cipher-s2c=([a-z0-9-]+) "
                r"mac-c2s=([a-z0-9-]+) mac-s2c=([a-z0-9-]+)", foreign_text)
            expected_epochs = [("0", cipher_c2s, cipher_s2c, c2s, s2c),
                               ("1", *final_ciphers, *final_macs)]
            if foreign_epochs != expected_epochs:
                raise RuntimeError("independent cipher/MAC epoch snapshots disagree")
            public_epochs = re.findall(
                r"DeuceSSH algorithm epoch=([01]) direction=(c2s|s2c) cipher=([a-z0-9-]+) "
                r"mac=([a-z0-9-]+) comp=none", peer_text)
            expected_public = [("0", "c2s", cipher_c2s, c2s), ("0", "s2c", cipher_s2c, s2c),
                               ("1", "c2s", final_ciphers[0], final_macs[0]),
                               ("1", "s2c", final_ciphers[1], final_macs[1])]
            if public_epochs != expected_public:
                raise RuntimeError("production cipher/MAC epoch snapshots disagree")
            for output in [foreign_text, peer_text]:
                if any(marker in output for marker in ["ERROR: AddressSanitizer", "WARNING: ThreadSanitizer", "runtime error:"]):
                    raise RuntimeError("native sanitizer diagnostic")
            for binary, before in result["binary_sha256"].items():
                if digest(binary) != before:
                    raise RuntimeError("native program changed during execution")
            result.update(status="PASSED", independent_newkeys=int(match[1]),
                          libssh_version=match[2], direction_checks=2, bytes_each_way=262144,
                          host_checks=int(native[1]), signed_auth=int(native[2]), channel_ack=int(native[3]),
                          public_algorithm_checks=2, cipher_checks=2,
                          independent_epochs=foreign_epochs, public_epochs=public_epochs,
                          final_cipher_c2s=final_ciphers[0], final_cipher_s2c=final_ciphers[1],
                          final_mac_c2s=final_macs[0], final_mac_s2c=final_macs[1],
                          algorithm_changes_verified=args.swap_rekey)
    except Exception as error:
        result["error"] = str(error)
    finally:
        for process, command in processes:
            code = stop(process)
            result["children"].append(dict(command=command, exit=code))
        result["logs_sha256"] = {p.name: digest(p) for p in evidence.iterdir()
                                 if p.is_file() and p.name != "result.json"}
        pending = evidence / "result.json.pending"
        pending.write_text(json.dumps(result, indent=2) + "\n")
        pending.replace(evidence / "result.json")
    if result["status"] != "PASSED":
        raise RuntimeError(result.get("error", "directional MAC check failed"))
    print(f"libssh_mac_interop role={args.role} c2s={c2s} s2c={s2c} result=PASSED")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--peer", required=True)
    parser.add_argument("--libssh", required=True)
    parser.add_argument("--keygen", required=True)
    parser.add_argument("--role", choices=["client", "server"], required=True)
    parser.add_argument("--c2s", choices=["256", "512"], required=True)
    parser.add_argument("--cipher-c2s", choices=["cbc", "ctr"])
    parser.add_argument("--swap-rekey", action="store_true")
    parser.add_argument("--evidence", required=True)
    args = parser.parse_args()
    if args.swap_rekey and not args.cipher_c2s:
        parser.error("--swap-rekey requires --cipher-c2s")
    run(args)


if __name__ == "__main__":
    main()
