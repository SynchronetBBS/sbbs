#!/bin/sh
DIR=$(dirname $0)
make -C $DIR/conio clean $@
make -C $DIR/sbbs3 clean $@
make -C $DIR/sbbs3/scfg clean $@
make -C $DIR/sbbs3/uedit clean $@
make -C $DIR/sbbs3/umonitor clean $@
make -C $DIR/smblib clean $@
make -C $DIR/uifc clean $@
make -C $DIR/xpdev clean $@
make -C $DIR/encode clean $@
make -C $DIR/hash clean $@
make -C $DIR/sftp clean $@
