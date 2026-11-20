"""Require actual assertion cleanup and exact native pre-free observations."""
import re
import subprocess
import sys

executable, mode = sys.argv[1:]
child = subprocess.run([executable, '--unit-owner', mode], capture_output=True, text=True)
output = child.stdout + child.stderr
print(output, end='')
expected = {
    'empty': (0, 0, 0), 'buffer-refusal': (1, 0, 0), 'buffer': (1, 1, 0),
    'queue-refusal': (2, 1, 0), 'queue': (2, 2, 0), 'queue-frozen': (2, 11, 0),
    'params-refusal': (3, 11, 0), 'params': (3, 18, 0),
    'temporary-refusal': (4, 18, 0), 'all': (4, 19, 1),
    'nested': (4, 19, 1), 'normal': (4, 19, 1),
}
if mode not in expected:
    sys.exit(1)
owners, frees, temporary = expected[mode]
normal = mode == 'normal'
valid = child.returncode == (0 if normal else 1)
valid &= output.count('ASSERT_TRUE failed: false\n') == (0 if normal else 1)
valid &= not any(marker in output for marker in (
    'AddressSanitizer', 'LeakSanitizer', 'ThreadSanitizer',
    'UndefinedBehaviorSanitizer', 'runtime error:'))
valid &= output.count(f'chan_unit_control mode={mode} owners={owners} frees={frees} '
                      f'temporary={temporary} nested={int(mode == "nested")} valid=1\n') == 1
records = re.findall(r'chan_unit_owner test=(\S+) registered=(\d+) retired=(\d+) '
                     r'temporary=(\d+) assertion=(\d+) valid=(\d+)\n', output)
required = [('chan_unit_owner_case', str(owners), str(owners), str(temporary), str(int(not normal)), '1')]
if mode == 'nested':
    required.append(('parent', '1', '1', '0', '0', '1'))
valid &= records == required
if not valid:
    print(f'channel unit control rejected mode={mode} exit={child.returncode}', file=sys.stderr)
    sys.exit(1)
print(f'chan_unit_observed mode={mode} valid=1')
