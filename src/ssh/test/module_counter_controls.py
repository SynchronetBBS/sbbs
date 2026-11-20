"""Observe an actual module callback's arithmetic refusal in a native child."""
import resource
import signal
import subprocess
import sys

executable, binding, mode = sys.argv[1:]
if binding not in ("enc-init", "enc-encrypt", "enc-decrypt", "mac-init", "mac-generate"):
    sys.exit(1)
if mode not in ("exhausted", "negative"):
    sys.exit(1)
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
child = subprocess.run([executable, binding, mode], capture_output=True, text=True)
print(child.stdout, end="")
print(child.stderr, end="")
ready = f"module_counter_ready binding={binding} mode={mode} valid=1\n"
marker = "DSSH_TEST_COUNTER_" + ("EXHAUSTED" if mode == "exhausted" else "INVALID") + "\n"
valid = child.returncode == -signal.SIGABRT and child.stdout == ready and child.stderr == marker
if not valid:
    print(f"module counter control rejected binding={binding} mode={mode} exit={child.returncode}", file=sys.stderr)
    sys.exit(1)
print(f"module_counter_control binding={binding} mode={mode} valid=1")
