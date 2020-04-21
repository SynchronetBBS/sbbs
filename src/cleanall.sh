#!/bin/sh
make -C conio clean $@
make -C sbbs3 clean $@
make -C sbbs3/scfg clean $@
make -C sbbs3/uedit clean $@
make -C sbbs3/umonitor clean $@
make -C smblib clean $@
make -C uifc clean $@
make -C xpdev clean $@
make -C encode clean $@
make -C hash clean $@
