# Synchronet 3rd Party External Program (door) Install Instructions

The `.ini` files in this directory are in the Synchronet `install-xtrn.ini`
format with base filenames reflecting the door they are intended to help
install into a Synchronet system.

The additional keys expected are:
~~~
exe = name of the executable file
md5 = comma-separated list of MD5-sums for supported executable file versions
~~~
MD5-sums are presented in lowercase hexadecimal (e.g. same format returned by
*nix `md5sum` utility).
