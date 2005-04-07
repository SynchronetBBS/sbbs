#
# Borland C++ IDE generated makefile
#
.AUTODEPEND


#
# Borland C++ tools
#
IMPLIB  = Implib
BCCDOS  = Bcc +BccDos.cfg 
TLINK   = TLink
TLIB    = TLib
TASM    = Tasm
#
# IDE macros
#


#
# Options
#
IDE_LFLAGSDOS =  -LF:\BC45\LIB
IDE_BFLAGS = 
LLATDOS_gac_whdexe =  -LE:\BC45\LIB;E:\GAC_CS\MYLIBS -c -Tde
RLATDOS_gac_whdexe = 
BLATDOS_gac_whdexe = 
CNIEAT_gac_whdexe = -IE:\BC45\INCLUDE;E:\GAC_CS\MYLIBS -D
LNIEAT_gac_whdexe = -s
LEAT_gac_whdexe = $(LLATDOS_gac_whdexe)
REAT_gac_whdexe = $(RLATDOS_gac_whdexe)
BEAT_gac_whdexe = $(BLATDOS_gac_whdexe)
CLATW16_ddbod6bodoorldlib = 
LLATW16_ddbod6bodoorldlib = 
RLATW16_ddbod6bodoorldlib = 
BLATW16_ddbod6bodoorldlib = 
CEAT_ddbod6bodoorldlib = $(CEAT_gac_whdexe) $(CLATW16_ddbod6bodoorldlib)
CNIEAT_ddbod6bodoorldlib = -IE:\BC45\INCLUDE;E:\GAC_CS\MYLIBS -D
LNIEAT_ddbod6bodoorldlib = -s
LEAT_ddbod6bodoorldlib = $(LEAT_gac_whdexe) $(LLATW16_ddbod6bodoorldlib)
REAT_ddbod6bodoorldlib = $(REAT_gac_whdexe) $(RLATW16_ddbod6bodoorldlib)
BEAT_ddbod6bodoorldlib = $(BEAT_gac_whdexe) $(BLATW16_ddbod6bodoorldlib)
CLATW16_ddbmylibsbregkeylddlib = 
LLATW16_ddbmylibsbregkeylddlib = 
RLATW16_ddbmylibsbregkeylddlib = 
BLATW16_ddbmylibsbregkeylddlib = 
CEAT_ddbmylibsbregkeylddlib = $(CEAT_gac_whdexe) $(CLATW16_ddbmylibsbregkeylddlib)
CNIEAT_ddbmylibsbregkeylddlib = -IE:\BC45\INCLUDE;E:\GAC_CS\MYLIBS -D
LNIEAT_ddbmylibsbregkeylddlib = -s
LEAT_ddbmylibsbregkeylddlib = $(LEAT_gac_whdexe) $(LLATW16_ddbmylibsbregkeylddlib)
REAT_ddbmylibsbregkeylddlib = $(REAT_gac_whdexe) $(RLATW16_ddbmylibsbregkeylddlib)
BEAT_ddbmylibsbregkeylddlib = $(BEAT_gac_whdexe) $(BLATW16_ddbmylibsbregkeylddlib)

#
# Dependency List
#
Dep_gac_wh = \
   gac_wh.exe

gac_wh : BccDos.cfg $(Dep_gac_wh)
  echo MakeNode 

Dep_gac_whdexe = \
   display.obj\
   gregedit.obj\
   netmail.obj\
   bbscur.obj\
   bbslst.obj\
   gamesdk.obj\
   gamesdki.obj\
   gamesdko.obj\
   ibbscfg.obj\
   plycur.obj\
   plylst.obj\
   ..\od6\odoorl.lib\
   gac_wh_s.obj\
   wahoo.obj\
   ..\mylibs\mswaitl.obj\
   ..\mylibs\regkeyld.lib

gac_wh.exe : $(Dep_gac_whdexe)
  $(TLINK)   @&&|
 /v $(IDE_LFLAGSDOS) $(LEAT_gac_whdexe) $(LNIEAT_gac_whdexe) +
E:\BC45\LIB\c0l.obj+
display.obj+
gregedit.obj+
netmail.obj+
bbscur.obj+
bbslst.obj+
gamesdk.obj+
gamesdki.obj+
gamesdko.obj+
ibbscfg.obj+
plycur.obj+
plylst.obj+
gac_wh_s.obj+
wahoo.obj+
..\mylibs\mswaitl.obj
$<,$*
..\od6\odoorl.lib+
..\mylibs\regkeyld.lib+
E:\BC45\LIB\emu.lib+
E:\BC45\LIB\mathl.lib+
E:\BC45\LIB\cl.lib

|

display.obj :  ..\gamesdk\display.c
  $(BCCDOS) -P- -c @&&|
 $(CEAT_gac_whdexe) $(CNIEAT_gac_whdexe) -o$@ ..\gamesdk\display.c
|

gregedit.obj :  ..\gamesdk\gregedit.c
  $(BCCDOS) -P- -c @&&|
 $(CEAT_gac_whdexe) $(CNIEAT_gac_whdexe) -o$@ ..\gamesdk\gregedit.c
|

netmail.obj :  ..\gamesdk\netmail.c
  $(BCCDOS) -P- -c @&&|
 $(CEAT_gac_whdexe) $(CNIEAT_gac_whdexe) -o$@ ..\gamesdk\netmail.c
|

bbscur.obj :  ..\gamesdk\bbscur.c
  $(BCCDOS) -P- -c @&&|
 $(CEAT_gac_whdexe) $(CNIEAT_gac_whdexe) -o$@ ..\gamesdk\bbscur.c
|

bbslst.obj :  ..\gamesdk\bbslst.c
  $(BCCDOS) -P- -c @&&|
 $(CEAT_gac_whdexe) $(CNIEAT_gac_whdexe) -o$@ ..\gamesdk\bbslst.c
|

gamesdk.obj :  ..\gamesdk\gamesdk.c
  $(BCCDOS) -P- -c @&&|
 $(CEAT_gac_whdexe) $(CNIEAT_gac_whdexe) -o$@ ..\gamesdk\gamesdk.c
|

gamesdki.obj :  ..\gamesdk\gamesdki.c
  $(BCCDOS) -P- -c @&&|
 $(CEAT_gac_whdexe) $(CNIEAT_gac_whdexe) -o$@ ..\gamesdk\gamesdki.c
|

gamesdko.obj :  ..\gamesdk\gamesdko.c
  $(BCCDOS) -P- -c @&&|
 $(CEAT_gac_whdexe) $(CNIEAT_gac_whdexe) -o$@ ..\gamesdk\gamesdko.c
|

ibbscfg.obj :  ..\gamesdk\ibbscfg.c
  $(BCCDOS) -P- -c @&&|
 $(CEAT_gac_whdexe) $(CNIEAT_gac_whdexe) -o$@ ..\gamesdk\ibbscfg.c
|

plycur.obj :  ..\gamesdk\plycur.c
  $(BCCDOS) -P- -c @&&|
 $(CEAT_gac_whdexe) $(CNIEAT_gac_whdexe) -o$@ ..\gamesdk\plycur.c
|

plylst.obj :  ..\gamesdk\plylst.c
  $(BCCDOS) -P- -c @&&|
 $(CEAT_gac_whdexe) $(CNIEAT_gac_whdexe) -o$@ ..\gamesdk\plylst.c
|

gac_wh_s.obj :  gac_wh_s.c
  $(BCCDOS) -P- -c @&&|
 $(CEAT_gac_whdexe) $(CNIEAT_gac_whdexe) -o$@ gac_wh_s.c
|

wahoo.obj :  wahoo.c
  $(BCCDOS) -P- -c @&&|
 $(CEAT_gac_whdexe) $(CNIEAT_gac_whdexe) -o$@ wahoo.c
|

# Compiler configuration file
BccDos.cfg : 
   Copy &&|
-W-
-R
-v
-vi
-H
-H=gac_bj.csm
-i32
-k-
-vi
-ml
-f
-fp
-Ff
-Ff=50
-v-
-R-
-C
-Ot
-d-
-y-
-N
-3
| $@


