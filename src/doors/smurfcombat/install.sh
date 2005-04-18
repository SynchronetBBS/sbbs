#!/bin/sh

mv smurf.`uname` smurf
mv libODoors.so.`uname` libODoors.so.3.2
ln -s libODoors.so.3.2 libODoors.so
rm -f *.FreeBSD *.NetBSD *.OpenBSD *.Linux *.exe *.dll *.bat > /dev/null 2>&1
