"""Require actual runner results, executed leaves and native-boundary outcomes."""
import re
import subprocess
import sys

executable, mode = sys.argv[1:]
if mode == "io-real":
    with open(executable, "rb") as refused_stdout:
        child = subprocess.run([executable, mode], stdout=refused_stdout, stderr=subprocess.PIPE, text=True)
    output = child.stderr
else:
    child = subprocess.run([executable, mode], capture_output=True, text=True)
    output = child.stdout + child.stderr
print(output, end="")
valid = not any(s in output for s in ("AddressSanitizer", "LeakSanitizer", "ThreadSanitizer",
                                      "UndefinedBehaviorSanitizer", "runtime error:"))
configuration = {"missing-exact", "extra", "empty", "bad-option", "duplicate", "null-name", "null-function"}
if mode in configuration or mode in {"signal-first", "signal-last", "write-first", "flush-first", "io-real"}:
    expected, leaves, clocks = 2, 0, 0
elif mode == "none":
    expected, leaves, clocks = 1, 0, 0
elif mode in {"all", "substring"}:
    expected, leaves, clocks = 0, 3 if mode == "all" else 2, 6 if mode == "all" else 4
else:
    expected = 1 if mode in {"fail", "positive", "negative"} else 2 if mode in {"write-last", "flush-last"} else 0
    leaves, clocks = 1, 2
rows = re.findall(r"runner_observed mode=(\S+) result=(\d+) leaves=(\d+) clocks=(\d+) signals=(\d+) writes=(\d+) flushes=(\d+) native=1\n", output)
valid &= child.returncode == expected and len(rows) == 1
if rows:
    name, result, actual_leaves, actual_clocks, signals, writes, flushes = rows[0]
    valid &= (name, int(result), int(actual_leaves), int(actual_clocks)) == (mode, expected, leaves, clocks)
    if mode.startswith("signal-"):
        valid &= int(signals) == 1
if mode in {"clock-first", "clock-last", "nano-negative", "nano-over", "reverse"}:
    valid &= "PASS (timing unavailable)\n" in output
if mode == "fraction":
    valid &= "PASS (200ms)\n" in output
if mode in {"positive", "negative"}:
    valid &= f"FAIL (invalid result {2 if mode == 'positive' else -2})" in output
    valid &= "1 tests: 0 passed, 1 failed, 0 skipped" in output
if mode == "none":
    valid &= "0 tests: 0 passed, 0 failed, 0 skipped" in output
if mode in {"all", "substring", "skip", "fail", "positive", "negative", "exact", "clock-first", "clock-last", "nano-negative", "nano-over", "reverse", "fraction", "flush-last"}:
    passed = 2 if mode in {"all", "substring"} else 0 if mode in {"skip", "fail", "positive", "negative"} else 1
    skipped = 1 if mode in {"all", "skip"} else 0
    failed = 1 if mode in {"fail", "positive", "negative"} else 0
    valid &= f"{leaves} tests: {passed} passed, {failed} failed, {skipped} skipped" in output
if not valid:
    print(f"runner control rejected mode={mode} exit={child.returncode}", file=sys.stderr)
    sys.exit(1)
print(f"runner_control mode={mode} valid=1")
