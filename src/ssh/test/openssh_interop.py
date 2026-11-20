#!/usr/bin/env python3
"""Bounded optional OpenSSH tests over stdio, with per-case evidence.

Private fixture keys are generated per invocation and always removed. Evidence
contains actual child exits, complete peer diagnostics, and negotiation counts.
No listening sockets, account changes, agent or user's SSH configuration is used.
"""
import argparse
import base64
import json
import os
from pathlib import Path
import pwd
import re
import select
import hashlib
import shlex
import signal
import subprocess
import sys
import tempfile
import time


OUTPUT = b"0123456789abcdef" * 16384
ERROR = b"interop-stderr\n"
ACKNOWLEDGEMENT = b"dssh-ack\n"


def stop(process):
    # ProxyCommand can still be completing after ssh exits. Its process group
    # belongs to this invocation even after the group leader has been reaped.
    try:
        os.killpg(process.pid, signal.SIGKILL)
    except ProcessLookupError:
        pass
    return process.wait(timeout=5)


def proxy(args):
    # OpenSSH owns this process group; the outer timeout kills the entire group.
    # ssh sends its proxy SIGHUP on failure too: finish reaping the actual peer
    # and recording its exit instead of losing that evidence.
    signal.signal(signal.SIGHUP, signal.SIG_IGN)
    command = [args.peer, "server", args.algorithm, args.key, args.blob, args.user]
    timed_out = False
    with Path(args.log).open("wb") as log:
        process = subprocess.Popen(command, stderr=log,
                                   env=os.environ | {"DSSH_OPENSSH_PEER_SECONDS": "120"})
        try:
            result = process.wait(timeout=130)
        except subprocess.TimeoutExpired:
            timed_out = True
            process.kill()
            result = process.wait(timeout=5)
    record = {"command": command, "exit": result}
    if timed_out:
        record.update(terminated_by_runner=True, pid=process.pid)
    # Publish only a complete record. The outer process must not observe an
    # empty or partially written JSON file while the native peer is retiring.
    target = Path(args.result)
    pending = target.with_name(target.name + ".pending")
    pending.write_text(json.dumps(record) + "\n")
    pending.replace(target)
    return 0 if result == 0 else 1


def pkcs8(private_key):
    # ssh-keygen's PEM RSA output is PKCS #1. Wrap that same key in an
    # unencrypted PKCS #8 container accepted by both library backends. This is
    # only DER framing, with no new signing/key-generation implementation.
    lines = private_key.read_text().splitlines()
    if lines[0] != "-----BEGIN RSA PRIVATE KEY-----" or lines[-1] != "-----END RSA PRIVATE KEY-----":
        raise RuntimeError("expected ssh-keygen's PKCS #1 fixture key")
    rsa = base64.b64decode("".join(lines[1:-1]), validate=True)

    def tlv(tag, value):
        size = len(value)
        if size > 16384:
            raise RuntimeError("oversized fixture key")
        if size < 128:
            length = bytes([size])
        else:
            count = (size.bit_length() + 7) // 8
            length = bytes([128 + count]) + size.to_bytes(count, "big")
        return bytes([tag]) + length + value

    algorithm = bytes.fromhex("300d06092a864886f70d0101010500")
    encoded = base64.b64encode(tlv(0x30, b"\x02\x01\x00" + algorithm + tlv(0x04, rsa))).decode("ascii")
    return "-----BEGIN PRIVATE KEY-----\n" + "\n".join(encoded[i:i + 64] for i in range(0, len(encoded), 64)) + "\n-----END PRIVATE KEY-----\n"


def read_server_output(process, until):
    # Bound each pipe read by both the expected extent and the overall deadline.
    output = bytearray()
    while len(output) < len(OUTPUT):
        left = until - time.monotonic()
        if left <= 0:
            raise RuntimeError("server stdout deadline before acknowledgement")
        ready, _, _ = select.select([process.stdout], [], [], min(0.1, left))
        if not ready:
            continue
        data = os.read(process.stdout.fileno(), min(4096, len(OUTPUT) - len(output)))
        if not data:
            raise RuntimeError(f"server stdout ended at {len(output)} bytes before acknowledgement")
        output.extend(data)
    if output != OUTPUT:
        raise RuntimeError("server stdout mismatch before acknowledgement")
    return bytes(output)


def run(args):
    evidence = Path(args.evidence).resolve()
    evidence.mkdir(parents=True, exist_ok=True)
    records = []
    processes = []
    temporary = None
    result = {"role": args.role, "algorithm": args.algorithm,
              "negative_host": args.negative_host, "status": "FAILED",
              "client_exchange_seconds": args.client_exchange_seconds,
              "native_peer_seconds": args.client_exchange_seconds - 20 if args.role == "client" else 120}
    cipher = os.environ.get("DSSH_TEST_ENC", "aes256-ctr")
    result["cipher"] = cipher
    mac = os.environ.get("DSSH_TEST_MAC", "hmac-sha2-256")
    result["mac"] = mac
    language_fields = "DSSH_TEST_LANGUAGE_FIELDS" in os.environ
    result["language_fields"] = language_fields
    for name in ("proxy-result.json", "proxy-result.json.pending", "result.json", "openssh.log", "peer.log", "stderr.log"):
        (evidence / name).unlink(missing_ok=True)

    def command(argv, name):
        with (evidence / name).open("wb") as log:
            completed = subprocess.run(argv, stdout=log, stderr=subprocess.STDOUT, timeout=30)
        records.append({"command": argv, "exit": completed.returncode})
        if completed.returncode != 0:
            raise RuntimeError(f"{name}: exit {completed.returncode}")

    try:
        if cipher not in ("aes256-ctr", "aes128-cbc"):
            raise RuntimeError("unsupported fixture cipher")
        if mac not in ("hmac-sha2-256", "hmac-sha2-512"):
            raise RuntimeError("unsupported fixture MAC")
        # All credentials stay in this private directory, including paths that
        # exercise shell/config quoting. It is removed even on failed runs.
        temporary = tempfile.TemporaryDirectory(prefix="dssh interop-")
        work = Path(temporary.name)
        key = work / "fixture-key"
        library_key = work / "fixture-key-pkcs8"
        blob = work / "fixture-blob"
        known = work / "known-hosts"
        authorized = work / "authorized-keys"
        config = work / "sshd-config"
        user = pwd.getpwuid(os.getuid()).pw_name
        command([args.keygen, "-q", "-t", "rsa", "-b", "2048", "-m", "PEM",
                 "-N", "", "-f", str(key)], "keygen.log")
        library_key.write_text(pkcs8(key))
        library_key.chmod(0o600)
        public = (work / "fixture-key.pub").read_text().split()
        if len(public) < 2 or public[0] != "ssh-rsa":
            raise RuntimeError("unexpected generated public key")
        blob.write_bytes(base64.b64decode(public[1], validate=True))
        if args.negative_host:
            if args.role != "client":
                raise RuntimeError("negative host control requires the library client role")
            pinned = bytearray(blob.read_bytes())
            pinned[-1] ^= 1
            blob.write_bytes(pinned)
        known.write_text("dssh-interop " + " ".join(public[:2]) + "\n")
        authorized.write_text(" ".join(public[:2]) + "\n")
        peer_command = [args.peer, args.role, args.algorithm, str(library_key), str(blob), user]

        if args.role == "server":
            proxy_result = evidence / "proxy-result.json"
            proxy_command = [sys.executable, str(Path(__file__).resolve()), "proxy",
                             "--peer", args.peer, "--algorithm", args.algorithm,
                             "--key", str(library_key), "--blob", str(blob), "--user", user,
                             "--result", str(proxy_result), "--log", str(evidence / "peer.log")]
            # OpenSSH expands percent tokens after its shell parser.
            proxy_text = shlex.join(proxy_command).replace("%", "%%")
            ssh = [args.ssh, "-F", "/dev/null", "-T", "-v", "-E", str(evidence / "openssh.log"),
                   "-o", "BatchMode=yes", "-o", "IdentitiesOnly=yes",
                   "-o", "IdentityAgent=none", "-o", "PreferredAuthentications=publickey",
                   "-o", "PubkeyAcceptedAlgorithms=rsa-sha2-256",
                   "-o", "HostKeyAlgorithms=rsa-sha2-256", "-o", "StrictHostKeyChecking=yes",
                   "-o", f"UserKnownHostsFile={shlex.quote(str(known))}",
                   "-o", "GlobalKnownHostsFile=/dev/null", "-o", "VerifyHostKeyDNS=no",
                   "-o", "HostKeyAlias=dssh-interop", "-o", "Compression=no",
                   "-o", "RekeyLimit=128K", "-o", f"KexAlgorithms={args.algorithm}",
                   "-c", cipher, "-m", mac, "-i", str(key),
                   "-o", "ProxyCommand=" + proxy_text, "-l", user, "dssh-interop", "dssh-interop"]
            with (evidence / "stderr.log").open("wb") as error:
                until = time.monotonic() + 140
                child = subprocess.Popen(ssh, stdin=subprocess.PIPE,
                                         stdout=subprocess.PIPE, stderr=error, start_new_session=True)
                processes.append(child)
                output = read_server_output(child, until)
                tail, _ = child.communicate(input=ACKNOWLEDGEMENT,
                                            timeout=max(0.1, until - time.monotonic()))
                output += tail
                result.update(stdout_bytes=len(output), stdout_sha256=hashlib.sha256(output).hexdigest())
            records.append({"command": ssh, "exit": child.returncode})
            # Command exit can precede native retirement during an in-flight
            # rekey. Keep the same overall deadline and the fixture credentials
            # until the proxy has reaped that peer and published its result.
            while not proxy_result.exists() and time.monotonic() < until:
                time.sleep(0.01)
            if not proxy_result.exists():
                raise RuntimeError("native peer result missing at the overall deadline")
            peer_result = json.loads(proxy_result.read_text())
            records.append(peer_result)
            if child.returncode != 23 or output != OUTPUT:
                raise RuntimeError(f"OpenSSH command exit={child.returncode}, stdout={len(output)} bytes")
            if peer_result["exit"] != 0:
                raise RuntimeError("production server fixture failed")
            error_text = (evidence / "stderr.log").read_bytes()
            peer_text = (evidence / "peer.log").read_text()
            if error_text != ERROR or "result=0" not in peer_text or "signed-auth=1" not in peer_text \
                    or "channel-ack=1" not in peer_text:
                raise RuntimeError("stderr/authentication/acknowledgement oracle missing")
            result.update(channel_ack_bytes=len(ACKNOWLEDGEMENT))
        else:
            # Quoted OpenSSH configuration paths support spaces. The forced
            # command is fixed test data, independent of the requested text.
            # Stage the exact bulk payload once, keeping producer work independent
            # of shell loop scheduling. The peer's channel window bounds pacing.
            output_file = work / "fixture-output"
            output_file.write_bytes(OUTPUT)
            forced = ("cat " + shlex.quote(str(output_file))
                      + " || exit 24; printf 'interop-stderr\\n' >&2; exit 23")
            log_level = "DEBUG2" if args.algorithm == "diffie-hellman-group-exchange-sha256" else "DEBUG1"
            config.write_text(
                f'HostKey "{key}"\nAuthorizedKeysFile "{authorized}"\n'
                f"AllowUsers {user}\nStrictModes no\nUsePAM no\n"
                "PasswordAuthentication no\nKbdInteractiveAuthentication no\n"
                "AuthenticationMethods publickey\nPubkeyAcceptedAlgorithms rsa-sha2-256\n"
                f"HostKeyAlgorithms rsa-sha2-256\nCiphers {cipher}\nMACs {mac}\n"
                f"KexAlgorithms {args.algorithm}\nCompression no\nRekeyLimit 128K\n"
                "PermitUserEnvironment no\nAllowAgentForwarding no\nAllowTcpForwarding no\n"
                f"X11Forwarding no\nPermitTTY no\nLogLevel {log_level}\n"
                "ForceCommand /bin/sh -c " + shlex.quote(forced) + "\n")
            daemon = [args.sshd, "-i", "-e", "-f", str(config)]
            command([args.sshd, "-t", "-f", str(config)], "sshd-config.log")
            descriptors = []
            with (evidence / "openssh.log").open("wb") as daemon_log, (evidence / "peer.log").open("wb") as peer_log:
                try:
                    server_read, client_write = os.pipe()
                    descriptors.extend([server_read, client_write])
                    client_read, server_write = os.pipe()
                    descriptors.extend([client_read, server_write])
                    server = subprocess.Popen(daemon, stdin=server_read, stdout=server_write,
                                              stderr=daemon_log, start_new_session=True)
                    processes.append(server)
                    client = subprocess.Popen(peer_command, stdin=client_read, stdout=client_write,
                                              stderr=peer_log, start_new_session=True,
                                              env=os.environ | {"DSSH_OPENSSH_PEER_SECONDS": str(args.client_exchange_seconds - 20)})
                    processes.append(client)
                finally:
                    for fd in descriptors:
                        os.close(fd)
                until = time.monotonic() + args.client_exchange_seconds
                client_exit = client.wait(timeout=max(0.1, until - time.monotonic()))
                server_exit = server.wait(timeout=max(0.1, until - time.monotonic()))
            records.extend([{"command": peer_command, "exit": client_exit},
                            {"command": daemon, "exit": server_exit}])
            peer_text = (evidence / "peer.log").read_text()
            if args.negative_host:
                if client_exit != 1 or server_exit != 255 or "stage=handshake" not in peer_text \
                        or "host-checks=0" not in peer_text or "result=0" in peer_text:
                    raise RuntimeError("host-key rejection control did not fail at its required boundary")
            elif client_exit != 0 or server_exit != 0 or "result=0" not in peer_text:
                raise RuntimeError(f"production client/sshd exits={client_exit}/{server_exit}")
        log = (evidence / "openssh.log").read_text()
        newkeys = log.count("SSH2_MSG_NEWKEYS received")
        if (not args.negative_host and newkeys < 2) or f"kex: algorithm: {args.algorithm}" not in log:
            raise RuntimeError(f"forced method/rekey oracle missing, NEWKEYS={newkeys}")
        ciphers = re.findall(r"kex: (client->server|server->client) cipher: (\S+)", log)
        if {direction for direction, _ in ciphers} != {"client->server", "server->client"} \
                or any(selected != cipher for _, selected in ciphers):
            raise RuntimeError("forced directional cipher oracle missing")
        if f"cipher={cipher}" not in peer_text:
            raise RuntimeError("production peer cipher oracle missing")
        macs = re.findall(r"kex: (client->server|server->client) cipher: \S+ MAC: (\S+)", log)
        if {direction for direction, _ in macs} != {"client->server", "server->client"} \
                or any(selected != mac for _, selected in macs) or f"mac={mac}" not in peer_text:
            raise RuntimeError("forced directional MAC oracle missing")
        if language_fields and not args.negative_host and "language-checks=2" not in peer_text:
            raise RuntimeError("initial/rekey public language-field checks missing")
        result.update(status="PASSED", openssh_newkeys_received=newkeys)
        if not args.negative_host:
            result.update(stdout_bytes=len(OUTPUT), stderr_bytes=len(ERROR), remote_exit_status=23)
    except Exception as error:
        result["error"] = str(error)
    finally:
        for process in processes:
            before = process.poll()
            exit_code = stop(process)
            if before is None:
                records.append({"command": process.args, "pid": process.pid,
                                "terminated_by_runner": True, "exit": exit_code})
        if temporary is not None:
            temporary.cleanup()
        result["commands"] = records
        (evidence / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))
    if result["status"] != "PASSED":
        for name in ("sshd-config.log", "openssh.log", "peer.log", "stderr.log"):
            path = evidence / name
            if path.exists():
                print(f"--- {name} ---\n{path.read_text(errors='replace')}")
        return 1
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="mode", required=True)
    run_parser = subparsers.add_parser("run")
    for name in ("peer", "algorithm", "ssh", "sshd", "keygen", "evidence"):
        run_parser.add_argument("--" + name, required=True)
    run_parser.add_argument("--role", required=True, choices=("client", "server"))
    run_parser.add_argument("--negative-host", action="store_true")
    run_parser.add_argument("--client-exchange-seconds", type=int, choices=(140, 540), default=140,
                            help="client exchange budget; 540 for instrumented native group validation")
    proxy_parser = subparsers.add_parser("proxy")
    for name in ("peer", "algorithm", "key", "blob", "user", "result", "log"):
        proxy_parser.add_argument("--" + name, required=True)
    args = parser.parse_args()
    return proxy(args) if args.mode == "proxy" else run(args)


if __name__ == "__main__":
    sys.exit(main())
