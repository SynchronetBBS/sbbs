#!/usr/bin/env python3
"""Run the actual POSIX client against a libssh-only loopback peer."""
import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import re
import selectors
import signal
import socket
import subprocess
import time

STREAM_SIZE = 262144
OUTPUT = b"0123456789abcdef" * (STREAM_SIZE // 16)
ERROR = bytes(range(256)) * (STREAM_SIZE // 256)
INPUT = b"example-input\n"


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def stop(process):
    if process is None:
        return
    if process.poll() is None:
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
    process.wait(timeout=5)


def collect(process, data, deadline, outputs, keep_stdin=False):
    limits = {"stdout": STREAM_SIZE + 4096, "stderr": STREAM_SIZE + 65536}
    offset = 0
    with selectors.DefaultSelector() as selector:
        for name in outputs:
            stream = getattr(process, name)
            os.set_blocking(stream.fileno(), False)
            selector.register(stream, selectors.EVENT_READ, name)
        if data:
            os.set_blocking(process.stdin.fileno(), False)
            selector.register(process.stdin, selectors.EVENT_WRITE, "stdin")
        elif not keep_stdin:
            process.stdin.close()
        while selector.get_map():
            left = deadline - time.monotonic()
            if left <= 0:
                raise RuntimeError("client stream collection deadline")
            for key, event in selector.select(min(left, 0.1)):
                stream, name = key.fileobj, key.data
                if name == "stdin":
                    try:
                        written = os.write(stream.fileno(), data[offset:])
                    except BrokenPipeError:
                        written = 0
                    except BlockingIOError:
                        continue
                    if written > 0:
                        offset += written
                    if written == 0 or offset == len(data):
                        selector.unregister(stream)
                        stream.close()
                    continue
                try:
                    block = os.read(stream.fileno(), 65536)
                except BlockingIOError:
                    continue
                if not block:
                    selector.unregister(stream)
                    stream.close()
                    continue
                if len(block) > limits[name] - len(outputs[name]):
                    raise RuntimeError(name + " exceeds fixture capture limit")
                outputs[name].extend(block)
    return bytes(outputs["stdout"]), bytes(outputs["stderr"])


def run(args):
    evidence = Path(args.evidence).resolve()
    evidence.mkdir(parents=True, exist_ok=True)
    # Never merge a new result with stale per-case files. The audit coordinator
    # archives earlier evidence before reusing this directory.
    for name in ["result.json", "result.json.pending", "public-key.log", "server.log",
                 "server-stdout.log", "client.log", "client-failed-stdout.bin"]:
        path = evidence / name
        if path.exists():
            path.unlink()
    result = {"status": "FAILED", "client_fault": args.client_fault, "key_algorithm": args.algorithm, "auth": args.auth,
              "mode": args.mode, "negative_pin": args.negative_pin,
              "wrong_password": args.wrong_password, "reject_command": args.reject_command,
              "binary_sha256": {args.client: digest(args.client), args.server: digest(args.server)},
              "key_sha256": digest(args.key), "commands": []}
    client = server = None
    client_command = server_command = None
    outputs = {"stdout": bytearray(), "stderr": bytearray()}
    started = time.monotonic()
    stdout = stderr = b""
    try:
        public_command = [args.server, "--public-key", args.key]
        public = subprocess.run(public_command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=10)
        (evidence / "public-key.log").write_bytes(public.stderr)
        result["commands"].append({"command": public_command, "exit": public.returncode})
        if public.returncode != 0 or len(public.stdout) > 16384:
            raise RuntimeError("independent key export failed")
        blob = base64.b64decode(public.stdout.strip(), validate=True)
        length = int.from_bytes(blob[:4], "big")
        key_type = b"ssh-rsa" if args.algorithm == "rsa-sha2-256" else b"ssh-ed25519"
        if length != len(key_type) or blob[4:4 + length] != key_type:
            raise RuntimeError("independent exported key type disagrees")
        actual_pin = "SHA256:" + base64.b64encode(hashlib.sha256(blob).digest()).decode().rstrip("=")
        expected_pin = actual_pin
        if args.negative_pin:
            expected_pin = actual_pin[:7] + ("B" if actual_pin[7] == "A" else "A") + actual_pin[8:]
        negative = args.negative_pin or args.wrong_password or args.reject_command or bool(args.client_fault)
        password = "wrong-password" if args.wrong_password else "example-password"
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
            listener.bind(("127.0.0.1", 0))
            listener.listen(1)
            listener.setblocking(False)
            port = str(listener.getsockname()[1])
            client_command = [args.client]
            if args.client_fault:
                client_command.append(args.client_fault)
            elif not args.direct:
                client_command.append("--run-client")
            password_read, password_write = os.pipe()
            try:
                credential = (password + "\n").encode()
                # This fixture's short credential fits in an empty native pipe.
                if os.write(password_write, credential) != len(credential):
                    raise RuntimeError("credential pipe write was incomplete")
                os.close(password_write)
                password_write = -1
                client_command.extend([expected_pin, "audit", str(password_read), args.host, port])
                if args.mode == "exec":
                    client_command.append("rejected-command" if args.reject_command else "dssh-example")
                client = subprocess.Popen(client_command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                          stderr=subprocess.PIPE, pass_fds=(password_read,), start_new_session=True)
            finally:
                os.close(password_read)
                if password_write != -1:
                    os.close(password_write)
            connected = None
            until = time.monotonic() + 15
            while connected is None:
                if client.poll() is not None:
                    raise RuntimeError("client exited before connecting")
                if time.monotonic() >= until:
                    raise RuntimeError("client did not connect")
                try:
                    connected, address = listener.accept()
                except BlockingIOError:
                    with selectors.DefaultSelector() as selector:
                        selector.register(listener, selectors.EVENT_READ)
                        selector.select(0.1)
            if address[0] != "127.0.0.1":
                connected.close()
                raise RuntimeError("unexpected peer address")
        with connected, (evidence / "server.log").open("wb") as log, (evidence / "server-stdout.log").open("wb") as server_output:
            server_command = [args.server, str(connected.fileno()), args.key, args.algorithm,
                              args.auth, args.mode, "negative" if negative else "positive"]
            server = subprocess.Popen(server_command, stdin=subprocess.DEVNULL, stdout=server_output,
                                      stderr=log, pass_fds=(connected.fileno(),), start_new_session=True)
        stdout, stderr = collect(client, INPUT if args.mode == "shell" else b"", time.monotonic() + 60,
                                 outputs, keep_stdin=args.mode == "shell-idle")
        client_exit = client.wait(timeout=5)
        server_exit = server.wait(timeout=10)
        result["commands"].extend([{"command": client_command, "exit": client_exit},
                                   {"command": server_command, "exit": server_exit}])
        foreign = (evidence / "server.log").read_text()
        pattern = (r"libssh_example newkeys=(\d+) auth=([01]) terminal=([01]) eof=([01]) closed=([01]) "
                   r"input=(\d+) password=(\d+) kbi=(\d+) negative=([01]) valid=1 version=([^\s]+)")
        match = re.search(pattern, foreign)
        expected_exit = -signal.SIGABRT if args.client_fault == "join" else (1 if negative else 0)
        if match is None or server_exit != 0 or client_exit != expected_exit:
            raise RuntimeError("native process completion disagrees")
        original_stderr = stderr
        if not args.direct:
            credential_report = b"example_credential closed=1 allocated=1 cleared=1 valid=1\n"
            if args.client_fault != "join" and credential_report not in stderr:
                raise RuntimeError("credential retirement/clearing observation missing")
            # Keep caller diagnostics separate from the exact remote stderr check.
            stderr = stderr.replace(credential_report, b"")
            result["credential_ownership"] = "descriptor retired before DNS; owned allocation cleared and freed"
        values = list(map(int, match.groups()[:9]))
        if foreign.count("Received SSH_MSG_NEWKEYS") != values[0]:
            raise RuntimeError("independent NEWKEYS count disagrees")
        fingerprint_pattern = re.compile(rb"Host key: " + args.algorithm.encode() + rb" (\d+) "
                                         + re.escape(actual_pin.encode()) + rb"\n")
        fingerprints = fingerprint_pattern.findall(stderr)
        if (not args.client_fault and not fingerprints) or any(int(bits) < 2048 if args.algorithm == "rsa-sha2-256" else int(bits) != 256
                                   for bits in fingerprints):
            raise RuntimeError("actual host fingerprint observation missing")
        if args.client_fault:
            if args.client_fault == "join":
                if b"example_join_failure joined=1 owned=1\n" not in stderr:
                    raise RuntimeError("checked-join abort control disagrees")
                ownership = "joined worker; caller abort observed"
            else:
                ownership_match = re.search(
                    rb"example_ownership fault=" + args.client_fault.encode()
                    + rb" hits=1 status=1 sessions=(\d+)/(\d+) builders=(\d+)/(\d+) "
                    + rb"channels=(\d+)/(\d+) workers=(\d+)/(\d+) sockets=(\d+)/(\d+) valid=1\n", stderr)
                if ownership_match is None:
                    raise RuntimeError("caller failure/retirement observation missing")
                counts = list(map(int, ownership_match.groups()))
                if any(counts[i] != counts[i + 1] for i in range(0, 10, 2)) or counts[-1] < 1:
                    raise RuntimeError("caller resources not retired")
                ownership = ownership_match.group(0).decode().strip()
            result["caller_ownership"] = ownership
            diagnostic = stderr
        elif args.negative_pin:
            if stdout or values[0] != 0 or values[1] != 0 or b"handshake failed:" not in stderr:
                raise RuntimeError("wrong-pin rejection stage disagrees")
            diagnostic = stderr
        elif negative:
            if stdout or values[0] < 1:
                raise RuntimeError("negative authentication/command stage disagrees")
            if args.wrong_password and (values[1] != 0 or values[6] != 1):
                raise RuntimeError("wrong-password control disagrees")
            if args.reject_command and (values[1] != 1 or values[2] != 0):
                raise RuntimeError("command rejection control disagrees")
            diagnostic = stderr
        else:
            if len(fingerprints) < 2 or stdout != OUTPUT or values[1:3] != [1, 1] or values[4] != 1 or (args.mode != "shell-idle" and values[3] != 1) or values[0] < 2:
                raise RuntimeError("positive data/EOF/CLOSE/rekey observations disagree")
            if values[5] != (len(INPUT) if args.mode == "shell" else 0):
                raise RuntimeError("input/EOF observation disagrees")
            if args.auth == "password" and (values[6] != 1 or values[7] != 0):
                raise RuntimeError("password path disagrees")
            if args.auth == "kbi" and (values[6] != 0 or values[7] != 2):
                raise RuntimeError("keyboard-interactive path disagrees")
            if args.auth == "fallback" and (values[6] != 1 or values[7] != 2):
                raise RuntimeError("fallback path disagrees")
            if args.auth == "partial" and (values[6] != 1 or values[7] != 2):
                raise RuntimeError("partial-success path disagrees")
            prefix, separator, payload = stderr.partition(b"Authenticated.\n")
            if args.auth == "partial" and (b"Auth methods: password\n" not in prefix
                                           or b"Auth methods: keyboard-interactive\n" not in prefix):
                raise RuntimeError("continuation list was not refreshed after partial success")
            if not separator or fingerprint_pattern.sub(b"", payload) != ERROR:
                raise RuntimeError("extended output is incomplete or contains unexpected diagnostics")
            diagnostic = prefix + separator + b"[remote stderr payload verified and omitted]\n"
            diagnostic += b"".join(m.group(0) for m in fingerprint_pattern.finditer(payload))
        for data in [diagnostic, foreign.encode()]:
            if any(marker in data for marker in [b"ERROR: AddressSanitizer", b"WARNING: ThreadSanitizer", b"runtime error:"]):
                raise RuntimeError("native sanitizer diagnostic")
        if digest(args.key) != result["key_sha256"] or any(digest(p) != h for p, h in result["binary_sha256"].items()):
            raise RuntimeError("key or program changed during execution")
        result.update(status="PASSED", actual_pin=actual_pin, host_checks=len(fingerprints),
                      independent_newkeys=values[0], authenticated=values[1], input_bytes=values[5],
                      password_calls=values[6], kbi_calls=values[7], libssh_version=match[10],
                      stdout_bytes=len(stdout), stdout_sha256=hashlib.sha256(stdout).hexdigest(),
                      raw_stderr_bytes=len(original_stderr), raw_stderr_sha256=hashlib.sha256(original_stderr).hexdigest(),
                      remote_stderr_bytes=0 if negative else len(ERROR),
                      remote_stderr_sha256=None if negative else hashlib.sha256(ERROR).hexdigest())
        (evidence / "client.log").write_bytes(diagnostic)
    except Exception as error:
        result["error"] = str(error)
    finally:
        children = []
        for role, process, command in [("client", client, client_command), ("server", server, server_command)]:
            if process is None:
                continue
            live = process.poll() is None
            try:
                stop(process)
            except Exception as error:
                result.update(status="FAILED", cleanup_error=str(error))
            children.append({"role": role, "pid": process.pid, "command": command,
                             "exit": process.returncode, "terminated_by_runner": live})
            if role == "client" and process.returncode is not None:
                # Preserve diagnostics even when failure precedes collect().
                for name in outputs:
                    stream = getattr(process, name)
                    if stream is not None and not stream.closed:
                        while len(outputs[name]) < STREAM_SIZE + 131072:
                            try:
                                block = os.read(stream.fileno(), 65536)
                            except BlockingIOError:
                                break
                            if not block:
                                break
                            outputs[name].extend(block)
            for name in ["stdin", "stdout", "stderr"]:
                stream = getattr(process, name)
                if stream is not None and not stream.closed:
                    stream.close()
        result["children"] = children
        if result["status"] != "PASSED":
            (evidence / "client-failed-stdout.bin").write_bytes(outputs["stdout"])
            (evidence / "client.log").write_bytes(outputs["stderr"])
        result["seconds"] = round(time.monotonic() - started, 3)
        result["logs_sha256"] = {p.name: digest(p) for p in evidence.iterdir()
                                 if p.is_file() and p.name not in ["result.json", "result.json.pending"]}
        pending = evidence / "result.json.pending"
        pending.write_text(json.dumps(result, indent=2) + "\n")
        pending.replace(evidence / "result.json")
    print(json.dumps(result, sort_keys=True))
    return 0 if result["status"] == "PASSED" else 1


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--client", required=True)
    parser.add_argument("--server", required=True)
    parser.add_argument("--key", required=True)
    parser.add_argument("--algorithm", required=True, choices=["rsa-sha2-256", "ssh-ed25519"])
    parser.add_argument("--auth", choices=["password", "kbi", "fallback", "partial"], default="password")
    parser.add_argument("--mode", choices=["exec", "shell", "shell-idle"], default="exec")
    parser.add_argument("--host", choices=["127.0.0.1", "localhost"], default="127.0.0.1")
    parser.add_argument("--negative-pin", action="store_true")
    parser.add_argument("--wrong-password", action="store_true")
    parser.add_argument("--reject-command", action="store_true")
    parser.add_argument("--direct", action="store_true")
    parser.add_argument("--client-fault", choices=["registration", "init", "terminate_cb", "verify_cb",
                        "handshake", "auth_query", "start", "params_init", "command", "pty", "term",
                        "size", "open", "worker", "output", "stdin", "poll", "close", "join"])
    parser.add_argument("--evidence", required=True)
    args = parser.parse_args()
    if sum([args.negative_pin, args.wrong_password, args.reject_command, bool(args.client_fault)]) > 1:
        parser.error("select at most one negative control")
    if args.reject_command and args.mode != "exec":
        parser.error("command rejection requires exec mode")
    return run(args)


if __name__ == "__main__":
    raise SystemExit(main())
