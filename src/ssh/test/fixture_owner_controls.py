"""Require observed native joins, deliberate assertion cleanup, or SIGABRT."""
import os
import re
import resource
import signal
import subprocess
import sys


def main():
    executable, mode = sys.argv[1:]
    allowed = {'plain', 'tx', 'create-0', 'create-1'}
    allowed.update(f'join-{i}-{kind}' for i in range(2) for kind in ('retained', 'released'))
    allowed.update(f'assert-{i}' for i in range(3))
    if mode not in allowed:
        raise ValueError(mode)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    environment = os.environ.copy()
    environment['DSSH_PEER_FAILURE'] = mode
    assertion = re.fullmatch(r'assert-([0-2])', mode)
    command = [executable, '--fixture-assert', assertion[1]] if assertion else [
        executable, '--exact', 'fixture/ownership']
    child = subprocess.run(command, env=environment, capture_output=True, text=True)
    output = child.stdout + child.stderr
    print(output, end='')
    if any(text in output for text in ('AddressSanitizer', 'LeakSanitizer', 'ThreadSanitizer', 'UndefinedBehaviorSanitizer', 'runtime error:')):
        return 1
    joins = [(int(i), int(ret)) for i, ret in re.findall(
        r'PEER_NATIVE_JOIN index=(\d+) success=(\d+)(?:\n|$)', output, re.M)]
    fatal = re.fullmatch(r'join-(\d+)-(retained|released)', mode)
    creation = re.fullmatch(r'create-(\d+)', mode)
    if fatal:
        index = int(fatal[1])
        retained = fatal[2] == 'retained'
        expected_joins = index if retained else index + 1
        valid = child.returncode == -signal.SIGABRT and output.count(
            f'PEER_JOIN_FAILURE index={index} retained={int(retained)}\n') == 1
    elif assertion:
        expected_joins = int(assertion[1])
        valid = child.returncode == 1 and output.count('ASSERT_TRUE failed: false\n') == 1
        valid &= output.count(f'fixture_assertion=1 phase={expected_joins} created={expected_joins} joined={expected_joins} valid=1\n') == 1
    else:
        expected_joins = int(creation[1]) if creation else 2
        valid = child.returncode == 0 and output.count(
            f'fixture_owner=1 created={expected_joins} joined={expected_joins} valid=1\n') == 1
        valid &= '1 tests: 1 passed, 0 failed, 0 skipped' in output
        if creation:
            valid &= output.count(f'PEER_CREATE_FAILURE index={expected_joins}\n') == 1
        elif mode == 'tx':
            valid &= output.count('PEER_TX_FAILURE\n') == 1
    valid &= joins == [(i, 1) for i in range(expected_joins)]
    if not valid:
        print(f'fixture control rejected mode={mode} exit={child.returncode} joins={joins}', file=sys.stderr)
        return 1
    print(f'fixture_control=1 mode={mode} joins={expected_joins}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
