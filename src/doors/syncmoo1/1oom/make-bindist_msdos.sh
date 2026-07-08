#!/bin/sh

# based on make-bindist_win32.sh from VICE 3.1
# Written by
#  Marco van den Heuvel <blackystardust68@yahoo.com>

STRIP=$1
TOPSRCDIR=$2
ZIPKIND=$3

EXECUTABLES="1oom_classic_alleg4 1oom_cmdline 1oom_gfxconv 1oom_lbxedit 1oom_lbxview_alleg4 1oom_pbxdump 1oom_pbxmake 1oom_saveconv"

for i in $EXECUTABLES
do
    if [ ! -e src/$i.exe ]
    then
        echo Error: file $i not found, do a \"make\" first
        exit 1
    fi
done

if [ -e $TOPSRCDIR/.git ]
    then
    VERSIONSTR=`git -C $TOPSRCDIR describe --tags`
else
    VERSIONSTR=vUnknown
fi

PACKAGESTR=1oom-$VERSIONSTR-msdos
BINDISTDIR=$PACKAGESTR
ZIPNAME=$PACKAGESTR.zip

mkdir $BINDISTDIR
mkdir $BINDISTDIR/doc

echo $PACKAGESTR > $BINDISTDIR/1version.txt

for i in $EXECUTABLES
do
    $STRIP src/$i.exe
done

cp src/1oom_classic_alleg4.exe $BINDISTDIR/1classic.exe
cp src/1oom_cmdline.exe $BINDISTDIR/1cmdline.exe
cp src/1oom_gfxconv.exe $BINDISTDIR/1gfxconv.exe
cp src/1oom_lbxview_alleg4.exe $BINDISTDIR/1lbxview.exe
cp src/1oom_lbxedit.exe $BINDISTDIR/1lbxedit.exe
cp src/1oom_pbxmake.exe $BINDISTDIR/1pbxmake.exe
cp src/1oom_pbxdump.exe $BINDISTDIR/1pbxdump.exe
cp src/1oom_saveconv.exe $BINDISTDIR/1savconv.exe

for p in pbxin format list usage
do
    for i in $TOPSRCDIR/doc/$p*.txt
    do
        FNAME=${i#$TOPSRCDIR/doc/}
        cp $i $BINDISTDIR/doc/${FNAME/$p\_/}
    done
done

cp $TOPSRCDIR/README.md $BINDISTDIR/1README.txt
cp $TOPSRCDIR/COPYING $BINDISTDIR/1COPYING.txt

unix2dos -q $BINDISTDIR/*.txt $BINDISTDIR/doc/*.txt

if [ -e extrabindist_common ]
    then
    cp extrabindist_common/* $BINDISTDIR
fi

if [ -e extrabindist_msdos ]; then
    cp extrabindist_msdos/* $BINDISTDIR
fi

if test x"$ZIPKIND" = "xzip"; then
    zip -r -9 -q $ZIPNAME $BINDISTDIR || die
    echo zip $ZIPNAME created
    rm -f -r $BINDISTDIR
else
    echo dir $BINDISTDIR created
fi
