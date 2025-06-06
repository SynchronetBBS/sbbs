## Contributing Changes to the Synchronet Source Repository

Merge requests are considered and accepted at [gitlab.synchro.net](https://gitlab.synchro.net).
Do not submit pull/merge requests in the Synchronet *mirror projects* on github.com or gitlab.com; they won't be considered.

When submitting merge requests to existing files, unless you have prior agreement with the maintainers:
* Use the dominant coding style of the file(s) being modified
* Do not perform tab to space conversions (or vice-versa)
* Do not add trailing white-space; incidental trimming of trailing whitespace is okay
* Do not include changes that are not relevant to the merge request description/message
* Keep merge requests to a single topical change (e.g. don't combine new features with bug fixes with typo-fixes and style changes)

### Commit Content
In general, if it's a large set of changes, your best bet of getting it accepted and merged into the repo would be to discuss the concept of the change with the developers in the [Synchronet Programming conference](http://web.synchro.net/?page=001-forum.ssjs&sub=syncprog) **first**.

When modifying the C/C++ source files:
* Do not call functions from `ctype.h` (e.g. `isprint()`, `isspace()`, `isdigit()`, etc.) - use the `xpdev/gen_defs.h IS_*` macros instead.
* Use safe string handling (e.g. xpdev's `SAFECOPY()` instead of `strcpy()`, `SAFEPRINTF()` or `snprintf()` instead of `sprintf()`).
* For boolean variables and parameter types, use `bool` (not `BOOL` unless Win32 API compatibility is required) and use `JSBool` when libmozjs API compatibility is required.
* Use of the `long int` type (signed or unsigned) should be avoided since it is inconsistent in size among the supported build environments / target platforms; use `int` or sized-types instead.

When modifying JavaScript (`.js`) files:
* Specify `"use strict"` when adding new files and write conforming code
* Don't introduce/use 3rd party libraries unnecessarily

When adding files:
* Prefer all lowercase characters in filenames
* Don't include spaces or other special characters (e.g. that require URL-encoding) in filenames
* Don't use misleading file extensions/suffixes (e.g. don't use `*.doc` for plain text files)

### Branches
Only approved and authenticated "developers" can create new branches in the `main/sbbs`repo. If, as a developer, you are creating a new branch in the `main/sbbs` repository, use the branch naming scheme: `<your-name>/<topic>`, where *your-name* is your abbreviated name, user-name, or alias and *topic* helps to identify the intended purpose of the branch. Branches created in user repositories (e.g. forks of main/sbbs) need not contain the `<your-name>/` prefix.

### Commit Messages
* Try to keep the commit title (first line) to 70 characters or less.
* When a comment is related to an [issue](https://gitlab.synchro.net/main/sbbs/-/issues), use the proper commit message syntax for automatic issue management as documented [here](https://docs.gitlab.com/ce/user/project/issues/managing_issues.html#closing-issues-automatically).

### Other types of contributions
If you were interested in contributing money, not code, then paypal to rob at synchro dot net.

Thank you for contributing!
