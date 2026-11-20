"""Check assertion truth, single evaluation, native calls and live cleanup."""
import re
import subprocess
import sys

executable, mode = sys.argv[1:]
if mode == "diagnostic-error":
    with open(executable, "rb") as refused_stderr:
        child = subprocess.run([executable, "eq-mixed"], capture_output=False,
                               stdout=subprocess.PIPE, stderr=refused_stderr, text=True)
    output = child.stdout
    actual_mode = "eq-mixed"
else:
    child = subprocess.run([executable, mode], capture_output=True, text=True)
    output = child.stdout + child.stderr
    actual_mode = mode
print(output, end="")
valid = child.returncode == 0 and not any(s in output for s in ("AddressSanitizer", "LeakSanitizer",
    "ThreadSanitizer", "UndefinedBehaviorSanitizer", "runtime error:"))
if mode == "values":
    valid &= output.strip() == "assertion_values cases=24 pairs=576 bounds=4 size_guard=1 valid=1"
else:
    passed = mode in {"eq-wide", "ok-wide", "memory-zero", "string-equal"}
    actual = 0 if mode == "positive-error" else 1
    expected = 0 if mode in {"positive-error", "ok-wide", "ok-negative"} else 1
    length = 1 if mode.startswith("memory-") else 0
    memory = 1 if mode == "memory-different" else 0
    string = 1 if mode in {"string-different", "string-equal"} else 0
    pattern = (f"assertion_observed mode={actual_mode} result={int(passed)} actual={actual} expected={expected} "
               f"length={length} memory={memory} string={string} cleanups={int(not passed)} "
               f"live={int(not passed)} retired=1\n")
    valid &= output.count(pattern) == 1
    if mode == "eq-mixed":
        limit = re.search(r"unsigned_max=(\d+)\n", output)
        valid &= limit is not None and f"expected expected_unsigned() == {limit[1]}\n" in output
if not valid:
    print(f"assertion control rejected mode={mode} exit={child.returncode}", file=sys.stderr)
    sys.exit(1)
print(f"assertion_control mode={mode} valid=1")
