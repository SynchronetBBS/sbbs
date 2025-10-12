#!/bin/sh
#
# Do not modify the following area:
#
# This file should be used to run Tournament Blackjack. The reason is
# that the auto update will not work if just the GAC_BJ.EXE is run.
# Check for .EXE file...
os=`uname`
os=`echo $os | tr '[A-Z]' '[a-z]' | tr ' ' '_')`
if [ -e in/gac_bj.$os ]; then
	chmod 755 gac_bj
	cp in/gac_bj.$os ./gac_bj
	rm in/gac_bj.$os
	chmod 555 gac_bj
fi
#
# Run the game with any supplied parameters.
exec ./gac_bj $@
