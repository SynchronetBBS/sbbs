"""Observe native joins, live-frame assertion cleanup and fatal sync results."""
import os
import re
import resource
import signal
import subprocess
import sys


def main():
    executable, mode = sys.argv[1:]
    environment = os.environ.copy()
    environment['DSSH_PEER_FAILURE'] = mode
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    assertion = re.fullmatch(r'assert-([0-3])', mode)
    fatal = re.fullmatch(r'join-([0-2])-(retained|released)', mode)
    creation = re.fullmatch(r'create-([0-2])', mode)
    sync = re.fullmatch(r'sync-([a-z._]+)-(error|retained|released)', mode)
    if assertion:
        command = [executable, '--allocation-assert', assertion[1]]
    elif sync:
        command = [executable, '--allocation-sync-fatal', sync[1], sync[2]]
    elif mode in ('plain', 'tx') or fatal or creation:
        command = [executable, '--exact', 'alloc/fixture_owner']
    else:
        raise ValueError(mode)
    child = subprocess.run(command, env=environment, capture_output=True, text=True)
    output = child.stdout + child.stderr
    print(output, end='')
    if any(s in output for s in ('AddressSanitizer', 'LeakSanitizer', 'ThreadSanitizer',
                                 'UndefinedBehaviorSanitizer', 'runtime error:')):
        return 1
    joins = [(int(i), int(result)) for i, result in re.findall(
        r'PEER_NATIVE_JOIN index=(\d+) success=(\d+)(?:\n|$)', output)]
    if sync:
        expected_joins = 0
        markers = re.findall(r'ALLOCATION_SYNC_FAILURE operation=(\S+) result=(-?\d+)\n', output)
        valid = child.returncode == -signal.SIGABRT and len(markers) == 1
        valid &= bool(markers) and markers[0][0] == sync[1] and int(markers[0][1]) != 0
        valid &= output.count('DSSH_TEST_UNLOCK_FAILURE\n') == int(sync[2] != 'error')
    elif fatal:
        index = int(fatal[1])
        retained = fatal[2] == 'retained'
        expected_joins = index if retained else index + 1
        valid = child.returncode == -signal.SIGABRT and output.count(
            f'PEER_JOIN_FAILURE index={index} retained={int(retained)}\n') == 1
    elif assertion:
        expected_joins = int(assertion[1])
        valid = child.returncode == 1 and output.count('ASSERT_TRUE failed: false\n') == 1
        valid &= output.count(f'allocation_assertion phase={expected_joins} created={expected_joins} joined={expected_joins} retired=1 valid=1\n') == 1
    else:
        expected_joins = int(creation[1]) if creation else 3
        valid = child.returncode == 0 and output.count(
            f'allocation_fixture_owner created={expected_joins} joined={expected_joins} retired=1 valid=1\n') == 1
        valid &= '1 tests: 1 passed, 0 failed, 0 skipped' in output
        if creation:
            valid &= output.count(f'PEER_CREATE_FAILURE index={expected_joins}\n') == 1
        elif mode == 'tx':
            valid &= output.count('PEER_TX_FAILURE\n') == 1
    valid &= joins == [(i, 1) for i in range(expected_joins)]
    if not valid:
        print(f'allocation control rejected mode={mode} exit={child.returncode} joins={joins}', file=sys.stderr)
        return 1
    print(f'allocation_control=1 mode={mode} joins={expected_joins}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
