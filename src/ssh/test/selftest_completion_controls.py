"""Require actual standalone host assertions to retire their live owners."""
import re
import subprocess
import sys

executable, mode = sys.argv[1:]
setters = {
    'standalone-log': 'dssh_session_set_log_cb(sess, log_test_cb, &state)',
    'standalone-clock': 'dssh_session_set_timeout(sess, 200)',
    'standalone-terminate': 'dssh_session_set_terminate_cb(sess, terminate_test_cb, &count)',
}
if mode not in setters:
    raise ValueError(mode)
child = subprocess.run([executable, mode], capture_output=True, text=True)
output = child.stdout + child.stderr
print(output, end='')
valid = child.returncode == 0 and not any(text in output for text in (
    'AddressSanitizer', 'LeakSanitizer', 'ThreadSanitizer',
    'UndefinedBehaviorSanitizer', 'runtime error:', 'SELFTEST_PREMATURE_RETIRE',
    'SELFTEST_NATIVE_JOIN', 'SELFTEST_OWNER mode=', 'SELFTEST_RESOURCES mode='))
failures = re.findall(r'^  ASSERT_[A-Z_]+ failed:.*$', output, re.M)
valid &= len(failures) == 1
valid &= len(re.findall(r'^  ASSERT_EQ failed: ' + re.escape(setters[mode])
                       + r' == -4, expected 0 == 0$', output, re.M)) == 1
valid &= len(re.findall(r'^SELFTEST_STANDALONE_FAILURE mode=' + re.escape(mode)
                       + r' native=0 reported=-4$', output, re.M)) == 1
valid &= output.count(
    f'SELFTEST_STANDALONE mode={mode} fired=1 sessions=1 released=1 '
    'fds=0 borrowed=1 active=0 valid=1\n') == 1
if not valid:
    print(f'selftest completion control rejected mode={mode} exit={child.returncode}',
          file=sys.stderr)
    sys.exit(1)
print(f'selftest_completion_control mode={mode} sessions=1 valid=1')
