"""Require actual integration callback native prefixes and direct fatal exits."""
import re
import resource
import signal
import subprocess
import sys

executable, mode = sys.argv[1:]
allowed = {f'socket-{operation}-{kind}'
           for operation in ('send', 'recv')
           for kind in ('native', 'eintr', 'error-before', 'error-after',
                        'zero', 'oversize', 'short', 'short-error')}
allowed.add('socket-recv-eof')
allowed.update(f'socket-poll-{kind}' for kind in (
    'native', 'eintr', 'error-before', 'error-after', 'zero', 'count',
    'nval', 'hup', 'err', 'other', 'terminated'))
allowed.update(f'socket-line-{kind}' for kind in (
    'native', 'eintr', 'error-before', 'error-after', 'zero', 'oversize',
    'full', 'capacity', 'eof', 'empty-eof'))
allowed.update(f'callback-unlock-{kind}' for kind in ('native', 'retained', 'released'))
if mode not in allowed:
    raise ValueError(mode)
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
child = subprocess.run([executable, mode], capture_output=True, text=True)
output = child.stdout + child.stderr
print(output, end='')
valid = not any(text in output for text in (
    'AddressSanitizer', 'LeakSanitizer', 'ThreadSanitizer',
    'UndefinedBehaviorSanitizer', 'runtime error:', 'SELFTEST_PREMATURE_RETIRE',
    'ASSERT_', 'SELFTEST_NATIVE_JOIN'))
fatal = mode in ('callback-unlock-retained', 'callback-unlock-released')
if fatal:
    completed = int(mode.endswith('-released'))
    valid &= child.returncode == -signal.SIGABRT
    valid &= output.count(
        f'SELFTEST_CALLBACK_UNLOCK mode={mode} native={completed} reported_error=1\n') == 1
    valid &= 'SELFTEST_OWNER' not in output and 'SELFTEST_RESOURCES' not in output
else:
    valid &= child.returncode == 0 and 'SELFTEST_CALLBACK_UNLOCK' not in output
    valid &= output.count(
        f'SELFTEST_OWNER mode={mode} created=0 joined=0 sessions=1 released=1 closed=2 valid=1\n') == 1
    valid &= output.count(
        f'SELFTEST_RESOURCES mode={mode} params_calls=0 params_native=0 params_frees=0 '
        'params_clean=1 open_attempts=0 channels=0 closed=0\n') == 1
    if mode.startswith('socket-'):
        observations = re.findall(
            r'^SELFTEST_SOCKET mode=' + re.escape(mode)
            + r' calls=(\d+) native=(\d+) hits=(\d+) bytes=(\d+) result=(-?\d+) line_count=(\d+) valid=1$',
            output, re.M)
        valid &= len(observations) == 1
        if len(observations) == 1:
            calls, native, hits, transferred, result, count = map(int, observations[0])
            valid &= 0 <= native <= calls <= 3 and 0 <= hits <= 2 and 0 <= transferred <= 6
            valid &= result in (0, -1) and count in (0, 2, 3)
            if mode.endswith('-error-before'):
                valid &= native == 0 and hits == 1 and transferred == 0 and result == -1
            elif mode.endswith('-error-after'):
                valid &= native == 1 and hits == 1 and result == -1
            elif mode.endswith('-short-error'):
                valid &= calls == 2 and native == 1 and hits == 2 and transferred == 2 and result == -1
            elif mode.endswith('-short'):
                valid &= calls == 2 and native == 2 and hits == 1 and transferred == 6 and result == 0
            elif mode.endswith('-eintr'):
                valid &= calls == native + 1 and hits == 1 and result == 0
    else:
        valid &= 'SELFTEST_SOCKET' not in output
if not valid:
    print(f'selftest socket control rejected mode={mode} exit={child.returncode}', file=sys.stderr)
    sys.exit(1)
print(f'selftest_socket_control mode={mode} fatal={int(fatal)} valid=1')
