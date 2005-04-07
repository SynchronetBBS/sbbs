.AUTODEPEND

#		*Translator Definitions*
CC = tcc +MAKEFILE.CFG
TASM = TASM
TLINK = tlink


#		*Implicit Rules*
.c.obj:
  $(CC) -c {$< }

.cpp.obj:
  $(CC) -c {$< }

.asm.obj:
  $(TASM) {$< }


#		*List Macros*


EXE_dependencies =  \
  ibbscfg.obj \
  ..\..\mylibs\odoorh.lib \
  display.obj \
  interbbs.obj \
  ..\..\mylibs\mswaitl.obj \
  ..\..\mylibs\regkeyld.lib \
  bbscur.obj \
  bbslst.obj \
  gac_bj.obj \
  plycur.obj \
  plylst.obj

#		*Explicit Rules*
gac_bj.exe: makefile.cfg $(EXE_dependencies)
  $(TLINK) /v/x/c @&&|
c:\dosapps\tc2\lib\c0h.obj+
ibbscfg.obj+
display.obj+
interbbs.obj+
..\..\mylibs\mswaitl.obj+
bbscur.obj+
bbslst.obj+
gac_bj.obj+
plycur.obj+
plylst.obj
gac_bj,
..\..\mylibs\odoorh.lib+
..\..\mylibs\regkeyld.lib+
c:\dosapps\tc2\lib\graphics.lib+
c:\dosapps\tc2\lib\emu.lib+
c:\dosapps\tc2\lib\mathh.lib+
c:\dosapps\tc2\lib\ch.lib
|


#		*Individual File Dependencies*
ibbscfg.obj: ibbscfg.c 

display.obj: ..\..\mylibs\display.c 
	$(CC) -c ..\..\mylibs\display.c

interbbs.obj: ..\..\mylibs\interbbs.c 
	$(CC) -c ..\..\mylibs\interbbs.c

bbscur.obj: bbscur.c 

bbslst.obj: bbslst.c 

gac_bj.obj: gac_bj.c 

plycur.obj: plycur.c 

plylst.obj: plylst.c 

#		*Compiler Configuration File*
makefile.cfg: makefile.mak
  copy &&|
-mh!
-2
-v
-V
-vi-
-IC:\DOSAPPS\TC2\INCLUDE;D:\GAC_CS\MYLIBS
-LC:\DOSAPPS\TC2\LIB;D:\GAC_CS\MYLIBS
| makefile.cfg


