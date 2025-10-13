#!/bin/sh
#
# Do not modify the following area:
#
# This file should be used to run Tournament Blackjack. The reason is
# that the auto update will not work if just the GAC_BJ.EXE is run.
# Check for .EXE file...
os=`uname`
os=`echo $os | tr '[A-Z]' '[a-z]' | tr ' ' '_')`
if [ -e in/gac_wh.$os ]; then
	chmod 755 gac_wh
	cp in/gac_wh.$os ./gac_wh
	rm in/gac_wh.$os
	chmod 555 gac_wh
fi
#
# Run the game with any supplied parameters.
exec ./gac_wh $@
