"""Observe allocation controllers' actual native prefixes and assertion cleanup."""
import os
import re
import resource
import signal
import subprocess
import sys

executable, mode, kind = sys.argv[1:]
if kind not in ("direct", "parser"):
    sys.exit(1)
scenario = kind == "parser" and mode in ("skip-reply", "bad-request", "provider-error")
assertion = re.fullmatch(r'assert-([0-3])', mode)
creation = re.fullmatch(r'create-([0-2])', mode)
fatal = re.fullmatch(r'join-([0-2])-(retained|released)', mode)
if assertion:
    command = [executable, f'--allocation-{kind}-assert', assertion[1]]
elif mode in ('plain', 'tx') or creation or fatal or scenario:
    command = [executable, f'--allocation-{kind}-owner']
else:
    sys.exit(1)
environment = os.environ.copy()
environment['DSSH_PEER_FAILURE'] = mode
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
child = subprocess.run(command, env=environment, capture_output=True, text=True)
output = child.stdout + child.stderr
print(output, end='')
valid = not any(s in output for s in (
    'AddressSanitizer', 'LeakSanitizer', 'ThreadSanitizer',
    'UndefinedBehaviorSanitizer', 'runtime error:'))
joins = [(int(i), int(ok)) for i, ok in re.findall(
    r'PEER_NATIVE_JOIN index=(\d+) success=(\d+)(?:\n|$)', output)]
if fatal:
    index = int(fatal[1])
    retained = fatal[2] == 'retained'
    expected_joins = index if retained else index + 1
    valid &= child.returncode == -signal.SIGABRT
    valid &= output.count(f'PEER_JOIN_FAILURE index={index} retained={int(retained)}\n') == 1
elif assertion:
    expected_joins = int(assertion[1])
    valid &= child.returncode == 1 and output.count('ASSERT_TRUE failed: false\n') == 1
    valid &= output.count(f'allocation_{kind}_assertion phase={expected_joins} '
                          f'created={expected_joins} joined={expected_joins} retired=1 valid=1\n') == 1
else:
    expected_joins = int(creation[1]) if creation else 2 if mode == 'tx' else 3
    valid &= child.returncode == 0
    records = re.findall(rf'allocation_{kind}_control created=(\d+) joined=(\d+) retired=1 '
                         r'completed=(\d+) fixture=(-?\d+) valid=1\n', output)
    valid &= len(records) == 1
    if records:
        created, joined, completed, fixture = map(int, records[0])
        valid &= created == joined == expected_joins
        valid &= completed == int(mode == 'plain')
        valid &= fixture == 0 if mode == 'plain' else fixture < 0
    if creation:
        valid &= output.count(f'PEER_CREATE_FAILURE index={expected_joins}\n') == 1
    elif mode == 'tx':
        valid &= output.count('PEER_TX_FAILURE\n') == 1
valid &= joins == [(i, 1) for i in range(expected_joins)]
if scenario:
    valid &= output.count(f'allocation_parser_model mode={mode} hit=1\n') == 1
if not valid:
    print(f'{kind} KEX control rejected mode={mode} exit={child.returncode}', file=sys.stderr)
    sys.exit(1)
print(f'allocation_{kind}_observed mode={mode} valid=1')
