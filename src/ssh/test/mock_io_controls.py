"""Observe actual native descriptor prefixes, retirement and refusal paths."""
import re
import resource
import signal
import subprocess
import sys

executable, mode = sys.argv[1:]
parts = mode.split("-")
kind = parts[0]
controlled = kind in {"pair", "option", "close", "shutdown"}
command = [executable] + parts if controlled else [executable, mode]
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
child = subprocess.run(command, capture_output=True, text=True)
output = child.stdout + child.stderr
print(output, end="")
valid = not any(s in output for s in ("AddressSanitizer", "LeakSanitizer", "ThreadSanitizer",
                                      "UndefinedBehaviorSanitizer", "runtime error:"))
rows = [(k, int(i), int(slot), int(done), int(fault)) for k, i, slot, done, fault in re.findall(
    r"mock_native kind=(\w+) index=(\d+) slot=(\d+) completed=(\d+) injected=(\d+)\n", output)]
for name in {r[0] for r in rows}:
    valid &= [r[1] for r in rows if r[0] == name] == list(range(sum(r[0] == name for r in rows)))
fatal = kind in {"close", "shutdown"}
if mode == "math":
    valid &= child.returncode == 0 and output.strip() == "mock_io_math cases=10 valid=1"
elif fatal:
    index = int(parts[1])
    after = parts[2] == "after"
    observed = [r for r in rows if r[0] == kind]
    valid &= child.returncode == -signal.SIGABRT and len(observed) == index + 1
    valid &= [r[2] for r in observed] == ([1, 0, 3, 2] if kind == "close" else [0, 1, 2, 3])[:index + 1]
    valid &= all(r[3:] == (1, 0) for r in observed[:-1])
    valid &= observed[-1][3:] == (int(after), 1) if observed else False
    valid &= "mock_io_control " not in output
    if kind == "shutdown":
        valid &= not any(r[0] == "close" for r in rows)
else:
    valid &= child.returncode == 0
    matches = re.findall(r"mock_io_control mode=(\S+) target=(\d+) after=(\d+) result=(-?\d+) pairs=(\d+) options=(\d+) produced=(\d+) released=(\d+) closes=(\d+) shutdowns=(\d+) sends=(\d+) reads=(\d+) recvs=(\d+) fault=(\d+) retired=1 valid=1\n", output)
    valid &= len(matches) == 1
    if matches:
        actual, *values = matches[0]
        target, after, result, pairs, options, produced, released, closes, shutdowns, sends, reads, recvs, fault = map(int, values)
        valid &= actual == (kind if controlled else mode) and produced == released == closes
        if kind == "pair":
            index = int(parts[1])
            valid &= target == index and result == -1 and pairs == index + 1 and produced == index * 2 and options == index * 4 and shutdowns == 0
        elif kind == "option":
            index = int(parts[1])
            valid &= target == index and after == int(parts[2] == "after") and result == -1
            valid &= pairs == (1 if index < 4 else 2) and options == index + 1 and produced == (2 if index < 4 else 4) and shutdowns == 0
            injected = [r for r in rows if r[4]]
            valid &= len(injected) == 1 and injected[0][0:2] == ("option", index) and injected[0][3] == after
        else:
            valid &= result == 0 and pairs == 2 and options == 8 and produced == 4 and shutdowns == 4
            valid &= [r[2] for r in rows if r[0] == "close"] == [1, 0, 3, 2]
            valid &= all(r[3] == 1 for r in rows if r[0] in {"close", "shutdown"})
            if kind in {"read", "write", "line", "inject", "drain"} and parts[-1] in {"intr", "again", "zero", "over"}:
                valid &= fault == 1
if not valid:
    print(f"mock I/O control rejected mode={mode} exit={child.returncode}", file=sys.stderr)
    sys.exit(1)
print(f"mock_io_observed mode={mode} valid=1")
