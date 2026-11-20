"""Require real peer completion, partial creation cleanup, or native SIGABRT."""
import os
import re
import resource
import signal
import subprocess
import sys


def main():
    executable, peer, mode = sys.argv[1:]
    if peer not in ("event", "accept", "nested"):
        raise ValueError(peer)
    workers = 4 if peer == "nested" else 2
    valid_modes = {"plain", "tx"}
    valid_modes.update(f"create-{i}" for i in range(workers))
    valid_modes.update(f"join-{i}-{kind}" for i in range(workers)
                       for kind in ("retained", "released"))
    if mode not in valid_modes:
        raise ValueError(mode)
    # The driver is a fresh single-threaded process; the exec'd child inherits
    # this core limit, with no pre-exec Python callback in the child.
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    environment = os.environ.copy()
    environment["DSSH_PEER_FAILURE"] = mode
    command = [executable]
    if peer != "event":
        command.append("5" if peer == "nested" else "3")
    child = subprocess.run(command, env=environment, stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT, text=True)
    print(child.stdout, end="")
    if any(message in child.stdout for message in
           ("AddressSanitizer", "UndefinedBehaviorSanitizer",
            "ThreadSanitizer", "runtime error:")):
        return 1
    joins = [(int(i), int(result)) for i, result in re.findall(
        r"^PEER_NATIVE_JOIN index=(\d+) success=(\d+)$", child.stdout, re.M)]
    fatal = re.fullmatch(r"join-(\d+)-(retained|released)", mode)
    if fatal:
        index = int(fatal[1])
        retained = fatal[2] == "retained"
        marker = f"PEER_JOIN_FAILURE index={index} retained={int(retained)}\n"
        expected_joins = index if retained else index + 1
        valid = child.returncode == -signal.SIGABRT and child.stdout.count(marker) == 1
    else:
        creation = re.fullmatch(r"create-(\d+)", mode)
        expected_joins = int(creation[1]) if creation else workers
        valid = child.returncode == (0 if mode == "plain" else 1)
        if creation:
            valid = valid and child.stdout.count(
                f"PEER_CREATE_FAILURE index={expected_joins}\n") == 1
        elif mode == "tx":
            valid = valid and child.stdout.count("PEER_TX_FAILURE\n") == 1
        else:
            output = "events=130 queue=0 positions=3/2 production=1" if peer == "event" else (
                f"setup_contract=1 kind={5 if peer == 'nested' else 3} "
                f"nested={int(peer == 'nested')} production=1")
            valid = valid and child.stdout.count(output) == 1
    valid = valid and joins == [(i, 1) for i in range(expected_joins)]
    if not valid:
        print(f"peer control rejected: peer={peer} mode={mode} "
              f"exit={child.returncode} joins={joins}", file=sys.stderr)
        return 1
    print(f"peer_control=1 peer={peer} mode={mode} joins={len(joins)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
