"""Require actual parameter/channel retirement and exact stream-control results."""
import re
import subprocess
import sys

executable, mode = sys.argv[1:]
allowed = {'assert-params', 'assert-channel', 'extent-boundaries', 'pattern-tail',
           'stream-partial', 'stream-corrupt'}
allowed.update(f'builder-{kind}-{i}-{phase}' for kind in ('exec', 'shell')
               for i in range(3) for phase in ('before', 'after'))
allowed.update(f'stream-write-{kind}' for kind in ('zero', 'negative', 'oversize'))
allowed.update(f'stream-read-{kind}' for kind in ('eof', 'negative', 'oversize'))
allowed.update(f'stream-{operation}-poll-{kind}' for operation in ('read', 'write')
               for kind in ('zero', 'negative', 'wrong'))
if mode not in allowed:
    raise ValueError(mode)
child = subprocess.run([executable, mode], capture_output=True, text=True)
output = child.stdout + child.stderr
print(output, end='')
valid = child.returncode == 0 and not any(text in output for text in (
    'AddressSanitizer', 'LeakSanitizer', 'ThreadSanitizer',
    'UndefinedBehaviorSanitizer', 'runtime error:', 'SELFTEST_PREMATURE_RETIRE'))
builder = re.fullmatch(r'builder-(exec|shell)-(\d+)-(before|after)', mode)
paired = mode == 'assert-channel' or mode == 'pattern-tail' or mode.startswith('stream-')
expected_joins = 5 if paired else 0
joins = [(int(index), int(success)) for index, success in re.findall(
    r'^SELFTEST_NATIVE_JOIN index=(\d+) success=(\d+)$', output, re.M)]
valid &= joins == [(i, 1) for i in range(expected_joins)]
valid &= output.count(
    f'SELFTEST_OWNER mode={mode} created={expected_joins} joined={expected_joins} '
    'sessions=2 released=2 closed=2 valid=1\n') == 1
calls = native = frees = 0
if builder:
    index = int(builder[2])
    calls, native, frees = index + 1, index + (builder[3] == 'after'), 1
    valid &= output.count(
        f'SELFTEST_PARAMETER_FAILURE mode={mode} index={index} phase=-{builder[3]}\n') == 1
elif paired:
    calls = native = 3
    frees = 1
elif mode == 'assert-params':
    calls = native = frees = 1
valid &= output.count(
    f'SELFTEST_RESOURCES mode={mode} params_calls={calls} params_native={native} '
    f'params_frees={frees} params_clean=1 open_attempts={int(paired)} '
    f'channels={2 * paired} closed={2 * paired}\n') == 1
assertions = 1 if mode.startswith('assert-') else 0
valid &= output.count('ASSERT_TRUE failed: false\n') == assertions
if mode == 'extent-boundaries':
    valid &= output.count('SELFTEST_EXTENTS cases=9 valid=1\n') == 1
if mode.startswith('stream-') or mode == 'pattern-tail':
    observations = re.findall(
        r'^SELFTEST_STREAM mode=' + re.escape(mode)
        + r' fired=(\d+) writes=(\d+) reads=(\d+) completed=(\d+) result=(\d+)$',
        output, re.M)
    valid &= len(observations) == 1
    if len(observations) == 1:
        fired, writes, reads, completed, result = map(int, observations[0])
        if mode == 'pattern-tail':
            valid &= fired == 0 and writes >= 2 and reads >= 2 and completed == 4097 and result == 1
        elif mode == 'stream-partial':
            valid &= fired == 1 and writes > 1 and reads > 1 and completed == 24 and result == 1
        else:
            valid &= fired == 1 and completed == 0 and result == 0
            if mode.startswith('stream-write-poll-'):
                valid &= writes == 0 and reads == 0
            elif mode.startswith('stream-write-'):
                valid &= writes == 1 and reads == 0
            elif mode.startswith('stream-read-poll-'):
                valid &= writes >= 1 and reads == 0
            else:
                valid &= writes >= 1 and reads >= 1
if not valid:
    print(f'selftest stream control rejected mode={mode} exit={child.returncode}', file=sys.stderr)
    sys.exit(1)
print(f'selftest_stream_control mode={mode} joins={expected_joins} valid=1')
