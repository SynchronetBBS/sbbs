"""Require actual constructor refusal/retirement and the original host failure."""
import re
import subprocess
import sys

executable, mode = sys.argv[1:]
match = re.fullmatch(r'constructor-(open|accept)-(malloc|realloc|mutex|condition)-(\d+)', mode)
if match is None:
    raise ValueError(mode)
side, operation, index_text = match.groups()
index = int(index_text)
limits = {'malloc': 7 if side == 'open' else 3, 'realloc': 3, 'mutex': 2, 'condition': 1}
if index >= limits[operation]:
    raise ValueError(mode)
child = subprocess.run([executable, mode], capture_output=True, text=True)
output = child.stdout + child.stderr
print(output, end='')
valid = child.returncode == 0 and not any(text in output for text in (
    'AddressSanitizer', 'LeakSanitizer', 'ThreadSanitizer',
    'UndefinedBehaviorSanitizer', 'runtime error:', 'SELFTEST_PREMATURE_RETIRE'))
expected_workers = 4 if side == 'open' else 5
valid &= re.findall(r'^SELFTEST_NATIVE_JOIN index=(\d+) success=(\d+)$', output, re.M) == [
    (str(i), '1') for i in range(expected_workers)]
late = side == 'accept' and operation == 'malloc' and index == 2
assertions = re.findall(r'^  ASSERT_[A-Z_]+ failed:.*$', output, re.M)
if side == 'open':
    valid &= assertions == ['  ASSERT_TRUE failed: dssh_test_channel_expired(wait, channel)']
elif late:
    valid &= assertions == ['  ASSERT_EQ failed: sarg.result == -1, expected 0 == 0']
else:
    valid &= assertions == ['  ASSERT_EQ failed: type == 92, expected SSH_MSG_CHANNEL_OPEN_CONFIRMATION == 91']
records = re.findall(r'^SELFTEST_CONSTRUCTOR mode=' + re.escape(mode)
                     + r'((?: [a-z_]+=-?\d+)+)$', output, re.M)
valid &= len(records) == 1
if len(records) == 1:
    fields = {key: int(value) for key, value in re.findall(r' ([a-z_]+)=(-?\d+)', records[0])}
    valid &= all(fields.get(key) == value for key, value in {
        'host': 0, 'fired': 1, 'live': 0, 'channel_seen': 0,
        'wait_expired': int(late), 'wait_failed': 0, 'created': expected_workers,
        'joined': expected_workers, 'sessions': 2, 'released': 2, 'fds': 2, 'valid': 1,
    }.items())
    attempts = {'malloc': 'mallocs', 'realloc': 'reallocs',
                'mutex': 'mutexes', 'condition': 'conditions'}
    valid &= fields.get(attempts[operation]) == index + 1
    valid &= fields.get('allocations') == fields.get('frees', -100) + fields.get('session_allocations', -100)
    valid &= fields.get('mutex_created') == fields.get('mutex_destroyed')
    valid &= fields.get('condition_created') == fields.get('condition_destroyed')
    valid &= 0 <= fields.get('mutex_created', -1) <= 2 and 0 <= fields.get('condition_created', -1) <= 1
    if late:
        valid &= fields.get('wait_native') == 1
    if operation == 'mutex':
        valid &= fields.get('mutex_created') == index and fields.get('condition_created') == 0
    elif operation == 'condition':
        valid &= fields.get('mutex_created') == 2 and fields.get('condition_created') == 0
if not valid:
    print(f'selftest constructor control rejected mode={mode} exit={child.returncode}', file=sys.stderr)
    sys.exit(1)
print(f'selftest_constructor_control mode={mode} workers={expected_workers} late={int(late)} valid=1')
