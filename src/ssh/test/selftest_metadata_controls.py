"""Require actual host rejection, native cleanup, and bounded clock controls."""
import re
import subprocess
import sys

executable, mode = sys.argv[1:]
allowed = {'metadata-rekey-' + kind for kind in ('native', 'corrupt', 'worker')}
allowed.update('metadata-sid-' + kind for kind in ('native', 'zero', 'null', 'oversize', 'corrupt', 'length'))
allowed.update('metadata-clock-' + kind for kind in ('start', 'end'))
allowed.update('metadata-getter-' + kind for kind in ('kex', 'key', 'enc', 'mac', 'comp', 'direction'))
allowed.add('metadata-boundaries')
if mode not in allowed:
    raise ValueError(mode)
child = subprocess.run([executable, mode], capture_output=True, text=True)
output = child.stdout + child.stderr
print(output, end='')
valid = child.returncode == 0 and not any(text in output for text in (
    'AddressSanitizer', 'LeakSanitizer', 'ThreadSanitizer',
    'UndefinedBehaviorSanitizer', 'runtime error:', 'SELFTEST_PREMATURE_RETIRE'))
assertions = re.findall(r'^  ASSERT_[A-Z_]+ failed:.*$', output, re.M)
if mode == 'metadata-boundaries':
    valid &= not assertions and 'SELFTEST_NATIVE_JOIN' not in output
    valid &= output.count('SELFTEST_METADATA_BOUNDARIES snapshots=6 waits=15 predicates=8 valid=1\n') == 1
else:
    native = mode.endswith('-native')
    workers = 4 if mode.startswith(('metadata-getter-', 'metadata-clock-')) else 5
    valid &= re.findall(r'^SELFTEST_NATIVE_JOIN index=(\d+) success=(\d+)$', output, re.M) == [
        (str(i), '1') for i in range(workers)]
    valid &= len(assertions) == int(not native)
    if assertions:
        expected = (
            'ASSERT_MEM_EQ failed: buf != data' if mode == 'metadata-rekey-corrupt' else
            'ASSERT_EQ failed: sarg.result == -1, expected 0 == 0' if mode == 'metadata-rekey-worker' else
            'ASSERT_MEM_EQ failed: ctx.client->trans.session_id != sid_before' if mode == 'metadata-sid-corrupt' else
            'ASSERT_EQ_U failed: ctx.client->trans.session_id_sz' if mode == 'metadata-sid-length' else
            'ASSERT_TRUE failed: selftest_snapshot_id(' if mode.startswith('metadata-sid-') else
            'ASSERT_TRUE failed: dssh_test_runner_clock(&t0)' if mode == 'metadata-clock-start' else
            'ASSERT_TRUE failed: selftest_elapsed(&t0)' if mode == 'metadata-clock-end' else
            'ASSERT_STR_EQ failed:' if mode == 'metadata-getter-direction' else
            'ASSERT_NOT_NULL failed:')
        valid &= expected in assertions[0]
    records = re.findall(r'^SELFTEST_METADATA mode=' + re.escape(mode)
                         + r'((?: [a-z_]+=-?\d+)+)$', output, re.M)
    valid &= len(records) == 1
    if len(records) == 1:
        fields = {k: int(v) for k, v in re.findall(r' ([a-z_]+)=(-?\d+)', records[0])}
        valid &= all(fields.get(k) == v for k, v in {
            'host': int(native), 'fired': int(not native), 'created': workers,
            'joined': workers, 'sessions': 2, 'released': 2, 'fds': 2, 'valid': 1,
        }.items())
        valid &= fields.get('channels') == fields.get('closed') == (0 if workers == 4 else 2)
        if mode.startswith('metadata-clock-'):
            valid &= fields.get('clocks') == (1 if mode.endswith('-start') else 2)
        if mode.startswith('metadata-getter-'):
            valid &= 1 <= fields.get('queries', 0) <= 20
if not valid:
    print(f'selftest metadata control rejected mode={mode} exit={child.returncode}', file=sys.stderr)
    sys.exit(1)
print(f'selftest_metadata_control mode={mode} valid=1')
