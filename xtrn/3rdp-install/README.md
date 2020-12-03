# Synchronet 3rd Party External Program (door) Install Instructions

The `.ini` files in this directory are in the Synchronet `install-xtrn.ini`
format with base filenames reflecting the door they are intended to help
install into a Synchronet system.

The additional keys/sections expected are:
~~~
exe = name of the executable file (in the root/unnamed section) [one]

[md5:<sum>] Hex MD5-sum for a supported executable file version [multiple]
ver = <optional version information - start with a number, not 'v')
url = <optional distribution/install package location - start with scheme://>
~~~
MD5-sums are presented in lowercase hexadecimal (e.g. same format returned by
*nix `md5sum` utility).
