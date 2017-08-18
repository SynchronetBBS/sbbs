#!/bin/sh
os=`uname | tr "[A-Z]" "[a-z]"`
if uname -m | egrep -v "(i[3456789]*|x)86" > /dev/null
	then
	machine=`uname -m | tr "[A-Z]" "[a-z]" | tr " " "_"`
fi
if uname -m | egrep "64" > /dev/null
	then
	machine=`uname -m | tr "[A-Z]" "[a-z]" | tr " " "_"`
fi
if [ $machine = "x86_64" ]
	then
	machine=x64
fi
os=$os.$machine
exename=`basename $0`
dirname=`dirname $0`
if [ -x $dirname\/gcc.$os.exe.release/$exename ]
	then exec $dirname\/gcc.$os.exe.release/$exename $@
elif [ -x $dirname\/gcc.$os.exe.debug/$exename ]
	then exec $dirname\/gcc.$os.exe.debug/$exename $@
elif [ -x $dirname\/clang.$os.exe.release/$exename ]
	then exec $dirname\/clang.$os.exe.release/$exename $@
elif [ -x $dirname\/clang.$os.exe.debug/$exename ]
	then exec $dirname\/clang.$os.exe.debug/$exename $@
elif [ -x $dirname\/icc.$os.exe.release/$exename ]
	then exec $dirname\/icc.$os.exe.release/$exename $@
elif [ -x $dirname\/icc.$os.exe.debug/$exename ]
	then exec $dirname\/icc.$os.exe.debug/$exename $@
elif [ -x $dirname\/*.$os.exe.release/$exename ]
	then exec $dirname\/*.$os.exe.release/$exename $@
elif [ -x $dirname\/*.$os.exe.debug/$exename ]
	then exec $dirname\/*.$os.exe.debug/$exename $@
elif [ -x $dirname\/*.$os.exe.*/$exename ]
	then exec $dirname\/*.$os.exe.*/$exename $@
fi
