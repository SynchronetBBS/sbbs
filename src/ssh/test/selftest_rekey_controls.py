"""Require real completed exchanges or exact host rejection after omitted arming."""
import re
import subprocess
import sys

executable, mode = sys.argv[1:]
allowed = {'rekey-' + case + '-' + kind
           for case in ('data', 'manual', 'channels', 'inflight')
           for kind in ('native', 'omitted')}
allowed.add('rekey-boundaries')
if mode not in allowed:
    raise ValueError(mode)
child = subprocess.run([executable, mode], capture_output=True, text=True)
output = child.stdout + child.stderr
print(output, end='')
valid = child.returncode == 0 and not any(text in output for text in (
    'AddressSanitizer', 'LeakSanitizer', 'ThreadSanitizer',
    'UndefinedBehaviorSanitizer', 'runtime error:', 'SELFTEST_PREMATURE_RETIRE'))
assertions = re.findall(r'^  ASSERT_[A-Z_]+ failed:.*$', output, re.M)
if mode == 'rekey-boundaries':
    valid &= not assertions and 'SELFTEST_NATIVE_JOIN' not in output
    valid &= output.count('SELFTEST_REKEY_BOUNDARIES cases=12 valid=1\n') == 1
else:
    native = mode.endswith('-native')
    valid &= re.findall(r'^SELFTEST_NATIVE_JOIN index=(\d+) success=(\d+)$', output, re.M) == [
        (str(i), '1') for i in range(5)]
    valid &= len(assertions) == int(not native)
    if assertions:
        valid &= 'ASSERT_TRUE failed: selftest_rekey_wait(ctx.client, ctx.server, rekeys_before)' in assertions[0]
    records = re.findall(r'^SELFTEST_REKEY mode=' + re.escape(mode)
                         + r'((?: [a-z_]+=-?\d+)+)$', output, re.M)
    valid &= len(records) == 1
    if len(records) == 1:
        fields = {k: int(v) for k, v in re.findall(r' ([a-z_]+)=(-?\d+)', records[0])}
        valid &= all(fields.get(k) == v for k, v in {
            'host': int(native), 'omitted': int(not native), 'arms': 1,
            'created': 5, 'joined': 5, 'sessions': 2, 'released': 2,
            'fds': 2, 'channels': 2, 'closed': 2, 'valid': 1,
        }.items())
        valid &= (fields.get('client', 0) > 0 and fields.get('server', 0) > 0) if native else (
            fields.get('client') == fields.get('server') == 0)
if not valid:
    print(f'selftest rekey control rejected mode={mode} exit={child.returncode}', file=sys.stderr)
    sys.exit(1)
print(f'selftest_rekey_control mode={mode} valid=1')
