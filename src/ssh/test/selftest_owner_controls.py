"""Check actual self-test prerequisites, worker-prefix cleanup and fatal joins."""
import re
import resource
import signal
import subprocess
import sys

executable, mode = sys.argv[1:]
allowed = {'plain', 'assert-body'}
allowed.update(f'create-{i}' for i in range(5))
allowed.update(f'join-{i}-{phase}' for i in range(5) for phase in ('retained', 'released'))
allowed.update(f'setter-{i}-{phase}' for i in range(4) for phase in ('before', 'after'))
allowed.update(f'assert-{stage}-{i}' for stage in ('hs', 'auth') for i in range(3))
if mode not in allowed:
    raise ValueError(mode)
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
child = subprocess.run([executable, mode], capture_output=True, text=True)
output = child.stdout + child.stderr
print(output, end='')
valid = not any(text in output for text in (
    'AddressSanitizer', 'LeakSanitizer', 'ThreadSanitizer',
    'UndefinedBehaviorSanitizer', 'runtime error:', 'SELFTEST_PREMATURE_RETIRE'))
joins = [(int(index), int(success)) for index, success in re.findall(
    r'^SELFTEST_NATIVE_JOIN index=(\d+) success=(\d+)$', output, re.M)]
fatal = re.fullmatch(r'join-(\d+)-(retained|released)', mode)
creation = re.fullmatch(r'create-(\d+)', mode)
setter = re.fullmatch(r'setter-(\d+)-(before|after)', mode)
assertion = re.fullmatch(r'assert-(hs|auth)-(\d+)', mode)
if fatal:
    index = int(fatal[1])
    retained = fatal[2] == 'retained'
    expected = index if retained else index + 1
    valid &= child.returncode == -signal.SIGABRT
    valid &= output.count(f'SELFTEST_JOIN_FAILURE index={index} retained={int(retained)}\n') == 1
    valid &= 'SELFTEST_OWNER' not in output
else:
    expected = 5
    sessions = 2
    if creation:
        expected = int(creation[1])
        valid &= output.count(f'SELFTEST_CREATE_FAILURE index={expected}\n') == 1
    if setter:
        index = int(setter[1])
        expected = 0
        sessions = (0, 1, 1, 2)[index]
        valid &= output.count(f'SELFTEST_SETTER_FAILURE index={index} phase=-{setter[2]}\n') == 1
    if assertion:
        expected = int(assertion[2]) + (2 if assertion[1] == 'auth' else 0)
    if assertion or mode == 'assert-body':
        valid &= output.count('ASSERT_TRUE failed: false\n') == 1
    else:
        valid &= 'ASSERT_TRUE failed:' not in output
    valid &= child.returncode == 0
    valid &= output.count(
        f'SELFTEST_OWNER mode={mode} created={expected} joined={expected} '
        f'sessions={sessions} released={sessions} closed=2 valid=1\n') == 1
valid &= joins == [(i, 1) for i in range(expected)]
if not valid:
    print(f'selftest owner control rejected mode={mode} exit={child.returncode} joins={joins}', file=sys.stderr)
    sys.exit(1)
print(f'selftest_owner_control mode={mode} joins={expected} valid=1')
