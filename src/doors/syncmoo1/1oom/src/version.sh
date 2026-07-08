#!/bin/sh

TOPSRCDIR=$1
OUTFILE=$2
GIT=$3
VERSION1OOM=$4

if test x$GIT != "xno" && test -d $TOPSRCDIR/.git
    then
    VERSIONSTR=`$GIT -C $TOPSRCDIR describe --tags`
else
    VERSIONSTR=$VERSION1OOM
fi

OUTSTR="#define VERSION_STR \"$VERSIONSTR\""

if test ! -e $OUTFILE
    then
    echo $OUTSTR > $OUTFILE
    exit
fi

CURSTR=`cat $OUTFILE`

if test "$OUTSTR" != "$CURSTR"
    then
    echo $OUTSTR > $OUTFILE
fi
