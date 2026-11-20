"""Require the exact arithmetic refusal in an isolated native child."""
import resource
import signal
import subprocess
import sys

executable, model, mode = sys.argv[1:]
if mode not in ("exhausted", "negative"):
    sys.exit(1)
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
child = subprocess.run([executable, mode], capture_output=True, text=True)
output = child.stdout + child.stderr
print(output, end="")
marker = "DSSH_TEST_COUNTER_" + ("EXHAUSTED" if mode == "exhausted" else "INVALID") + "\n"
valid = child.returncode == -signal.SIGABRT and output == marker
if not valid:
    print(f"counter control rejected model={model} mode={mode} exit={child.returncode}", file=sys.stderr)
    sys.exit(1)
print(f"counter_control model={model} mode={mode} valid=1")
