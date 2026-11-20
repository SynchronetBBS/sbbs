#!/usr/bin/env python3
"""Native independent SSH BREAK outcome and transfer/rekey checks."""
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


def run(args):
    directory = Path(args.evidence).resolve()
    directory.mkdir(parents=True, exist_ok=True)
    attempt = 0
    while (directory / f"result-{attempt}.json").exists():
        attempt += 1
    result = dict(status="FAILED", implementation="golang.org/x/crypto/ssh",
                  source="test/break_ssh_peer.go", commands=[], children=[],
                  binary_sha256={args.peer: digest(args.peer), args.foreign: digest(args.foreign)})
    processes = []
    try:
        with tempfile.TemporaryDirectory(prefix="dssh-break-") as temporary:
            work = Path(temporary)
            private = work / "key"
            converted = work / "pkcs8"
            blob = work / "blob"
            log = directory / f"keygen-{attempt}.log"
            command = [args.keygen, "-q", "-t", "rsa", "-b", "2048", "-m", "PEM", "-N", "", "-f", str(private)]
            with log.open("wb") as output:
                generated = subprocess.run(command, stdout=output, stderr=subprocess.STDOUT, timeout=30)
            result["commands"].append(dict(command=command, exit=generated.returncode, log=str(log), sha256=digest(log)))
            if generated.returncode != 0:
                raise RuntimeError("key generation failed")
            converted.write_text(pkcs8(private))
            converted.chmod(0o600)
            public = private.with_suffix(".pub").read_text().split()
            if len(public) < 2 or public[0] != "ssh-rsa":
                raise RuntimeError("unexpected public key fixture")
            blob.write_bytes(base64.b64decode(public[1], validate=True))
            env = {name: value for name, value in os.environ.items() if not name.startswith("DSSH_TEST_")}
            env.update(DSSH_TEST_BREAK_ACTION="1", DSSH_TEST_MAC="hmac-sha2-256",
                       DSSH_TEST_MAC_S2C="hmac-sha2-256", GOMAXPROCS="4")
            if args.terminal_mode is not None:
                env["DSSH_TEST_TERMINAL_MODE"] = args.terminal_mode
            if args.notification_flags:
                env["DSSH_TEST_NOTIFICATION_FLAGS"] = "1"
            left, right = socket.socketpair()
            native_log = directory / f"native-{attempt}.log"
            foreign_log = directory / f"foreign-{attempt}.log"
            with left, right, native_log.open("wb") as native_output, foreign_log.open("wb") as foreign_output:
                commands = [[args.peer, "server", "curve25519-sha256", str(converted), str(blob), "audit"],
                            [args.foreign, str(right.fileno()), str(converted), str(blob)]]
                try:
                    native = subprocess.Popen(commands[0], stdin=left, stdout=left, stderr=native_output,
                                              env=env, start_new_session=True)
                    processes.append((native, commands[0]))
                    foreign = subprocess.Popen(commands[1], pass_fds=(right.fileno(),), stdout=foreign_output,
                                               stderr=subprocess.STDOUT, env=env, start_new_session=True)
                    processes.append((foreign, commands[1]))
                    left.close()
                    right.close()
                    deadline = time.monotonic() + 60
                    while any(process.poll() is None for process, _ in processes):
                        failed = [(process, cmd) for process, cmd in processes
                                  if process.poll() is not None and process.returncode != 0]
                        if failed:
                            raise RuntimeError(f"native child exit {failed[0][0].returncode}: {failed[0][1][0]}")
                        if time.monotonic() >= deadline:
                            result["timed_out"] = True
                            raise RuntimeError("independent peer deadline exceeded")
                        time.sleep(0.02)
                finally:
                    for process, command in processes:
                        stopped = process.poll() is None
                        child = dict(command=command, pid=process.pid, exit=None,
                                     termination_requested=stopped, terminated_by_runner=False)
                        try:
                            if stopped:
                                try:
                                    os.killpg(process.pid, signal.SIGTERM)
                                    child["terminated_by_runner"] = True
                                except ProcessLookupError:
                                    pass
                                try:
                                    process.wait(timeout=2)
                                except subprocess.TimeoutExpired:
                                    try:
                                        os.killpg(process.pid, signal.SIGKILL)
                                        child["terminated_by_runner"] = True
                                    except ProcessLookupError:
                                        pass
                            child["exit"] = process.wait(timeout=5)
                        except BaseException as error:
                            child["cleanup_error"] = repr(error)
                        result["children"].append(child)
            if any(child["exit"] != 0 or child["terminated_by_runner"] for child in result["children"]):
                raise RuntimeError("native process completion failed")
            text = native_log.read_text()
            observed = json.loads(foreign_log.read_text())
            if "DeuceSSH break actions: calls=14 valid=1\n" not in text \
                    or "stage=channel result=0 host-checks=0 signed-auth=1 channel-ack=1" not in text \
                    or "algorithm-checks=2" not in text:
                raise RuntimeError("production action/channel completion oracle missing")
            if args.terminal_mode is not None:
                expected_mode = f"DeuceSSH terminal mode: state={args.terminal_mode} callback=1 retained=1 valid=1\n"
                if expected_mode not in text or observed.get("terminal_mode") != args.terminal_mode:
                    raise RuntimeError("independent IUTF8 callback/getter oracle mismatch")
                result["terminal_mode"] = args.terminal_mode
            if args.notification_flags:
                expected_notifications = [dict(name=name, reply=reply, accepted=False)
                                          for name in ["signal", "window-change", "exit-status", "exit-signal"]
                                          for reply in [True, False]]
                if "DeuceSSH notifications: calls=4 valid=1\n" not in text \
                        or observed.get("notifications") != expected_notifications:
                    raise RuntimeError("independent notification flag/delivery oracle mismatch")
                result["notification_flags"] = True
            expected = [dict(duration=duration, reply=reply,
                             accepted=reply and 500 <= duration <= 3000)
                        for duration in [0, 1, 499, 500, 3000, 3001, 4294967295]
                        for reply in [False, True]]
            if observed.get("cases") != expected or observed.get("valid") is not True \
                    or observed.get("stdout_bytes") != 262144 or observed.get("remote_exit_status") != 23 \
                    or observed.get("host_checks", 0) < 2:
                raise RuntimeError("independent reply/transfer/rekey oracle mismatch")
            if re.search(r"AddressSanitizer|LeakSanitizer|ThreadSanitizer|UndefinedBehaviorSanitizer|runtime error:", text):
                raise RuntimeError("native sanitizer diagnostic")
            result.update(status="PASSED", independent=observed,
                          files_sha256={str(native_log): digest(native_log), str(foreign_log): digest(foreign_log)})
    except BaseException as error:
        result["error"] = repr(error)
    (directory / f"result-{attempt}.json").write_text(json.dumps(result, indent=2) + "\n")
    (directory / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(f"independent_break status={result['status']} cases=14 evidence={directory}")
    return 0 if result["status"] == "PASSED" else 1


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--peer", required=True)
    parser.add_argument("--foreign", required=True)
    parser.add_argument("--evidence", required=True)
    parser.add_argument("--keygen", default="/usr/bin/ssh-keygen")
    parser.add_argument("--terminal-mode", choices=["none", "0", "1"])
    parser.add_argument("--notification-flags", action="store_true")
    sys.exit(run(parser.parse_args()))
