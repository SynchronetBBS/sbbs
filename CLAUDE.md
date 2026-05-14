## Coding guidelines

This project's coding guidelines live in [CONTRIBUTING.md](CONTRIBUTING.md). Read it before making non-trivial code changes and follow the conventions it describes (style, formatting, commit messages, patch submission, etc.).

## Segfaults are bugs — always investigate

Any segfault (or other crash: access violation, abort, stack overflow) of a Synchronet executable — `sbbs.dll`, `jsexec`, `sbbscon`, `mailsrvr`, `ftpsrvr`, `websrvr`, `services`, `ntsvcs`, `smbutil`, `chksmb`, `fixsmb`, `scfg`, `sbbsctrl`, `useredit`, `syncterm`, the SMB or xpdev test binaries, etc. — is a real defect and **must be root-caused**, not worked around or ignored. This applies whether the crash happens during normal use, in a test, or in a one-off probe (jsexec, a quick MsgBase script, a build's own self-test). "Just don't do that" / "retry / skip the offending input" is not an acceptable resolution.

Investigate the root cause: get a stack trace (debugger, crash dump, core file), identify the offending function and reason (null deref, use-after-free, bad cast, uninitialized memory, struct-size mismatch, ABI break, recursion, etc.), and fix it at the source. If a fix is out of scope for the immediate task, file/track it explicitly so it isn't lost.
