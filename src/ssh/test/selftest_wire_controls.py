"""Require wire observations and actual host rejection of the negative cases."""
import re
import subprocess
import sys

executable, mode = sys.argv[1:]
allowed = {'wire-env-native', 'wire-env-omitted', 'wire-env-reply',
           'wire-break-native', 'wire-break-duplicate'}
if mode not in allowed:
    raise ValueError(mode)
child = subprocess.run([executable, mode], capture_output=True, text=True)
output = child.stdout + child.stderr
print(output, end='')
changed = mode == 'wire-env-reply'
duplicate = mode == 'wire-break-duplicate'
valid = child.returncode == 0 and not any(text in output for text in (
    'AddressSanitizer', 'LeakSanitizer', 'ThreadSanitizer',
    'UndefinedBehaviorSanitizer', 'runtime error:', 'SELFTEST_PREMATURE_RETIRE'))
joins = [(int(i), int(result)) for i, result in re.findall(
    r'^SELFTEST_NATIVE_JOIN index=(\d+) success=(\d+)$', output, re.M)]
valid &= joins == [(i, 1) for i in range(5)]
assertions = re.findall(r'^  ASSERT_[A-Z_]+ failed:.*$', output, re.M)
if duplicate:
    valid &= assertions == ['  ASSERT_FALSE failed: atomic_load(&action.duplicate)']
elif changed:
    valid &= not assertions or assertions == ['  ASSERT_NOT_NULL failed: ch is NULL']
else:
    valid &= not assertions
expected_result = r'[01]' if changed else str(int(not duplicate))
observations = re.findall(
    r'^SELFTEST_WIRE mode=' + re.escape(mode) + r' host=(' + expected_result + r')'
    + r' duplicate=' + str(int(duplicate))
    + r' created=5 joined=5 sessions=2 released=2 fds=2 channels=(\d+) closed=(\d+) borrowed=0 valid=1$',
    output, re.M)
valid &= len(observations) == 1
if len(observations) == 1:
    result, created, closed = map(int, observations[0])
    valid &= len(assertions) == int(result == 0)
    valid &= created == closed and (0 <= created <= 2 if changed else created == 2)
if mode.startswith('wire-env-'):
    records = re.findall(r'^SELFTEST_WIRE_ENV mode=' + re.escape(mode)
                         + r'((?: [a-z_]+=\d+)+)$', output, re.M)
    valid &= len(records) == 1
    if len(records) == 1:
        fields = dict((name, int(value)) for name, value in re.findall(
            r' ([a-z_]+)=(\d+)', records[0]))
        if mode == 'wire-env-native':
            valid &= all(fields.get(direction + '_' + kind) == 1
                         for direction in ('tx', 'rx')
                         for kind in ('lang', 'term', 'exec', 'success'))
            valid &= all(fields.get(key) == 0 for key in (
                'tx_failure', 'rx_failure', 'tx_bad', 'rx_bad', 'omitted', 'changed'))
            valid &= fields.get('oracle') == 1
        elif mode == 'wire-env-omitted':
            valid &= all(fields.get(key) == 0 for key in (
                'tx_lang', 'rx_lang', 'tx_term', 'rx_term', 'tx_bad', 'rx_bad', 'changed', 'oracle'))
            valid &= fields.get('omitted') == 2
            valid &= all(fields.get(direction + '_' + kind) == 1
                         for direction in ('tx', 'rx') for kind in ('exec', 'success'))
        else:
            valid &= fields.get('changed') == 1 and fields.get('oracle') == 0
            valid &= fields.get('tx_bad', 0) >= 1 and fields.get('rx_bad', 0) >= 1
            valid &= fields.get('tx_failure', 0) >= 1 and fields.get('rx_failure', 0) >= 1
if not valid:
    print(f'selftest wire control rejected mode={mode} exit={child.returncode}', file=sys.stderr)
    sys.exit(1)
result = int(observations[0][0])
print(f'selftest_wire_control mode={mode} host={result} valid=1')
