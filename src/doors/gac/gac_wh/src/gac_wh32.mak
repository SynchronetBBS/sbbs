OPT =  @turbo32.cfg

OBJDIR = 32\

OBJLIST = $(OBJDIR)\bbscur.obj \
          $(OBJDIR)\plycur.obj \
          $(OBJDIR)\plylst.obj \
          $(OBJDIR)\bbslst.obj \
          $(OBJDIR)\gamesdki.obj \
          $(OBJDIR)\gamesdko.obj \
          $(OBJDIR)\gamesdk.obj \
          $(OBJDIR)\netmail.obj \
          $(OBJDIR)\gregedit.obj \
          $(OBJDIR)\ibbscfg.obj \
          $(OBJDIR)\gac_wh_s.obj \
          $(OBJDIR)\wahoo.obj 
          


.c.obj:

ALL: $(OBJLIST) 

$(OBJDIR)\bbscur.obj: \gac_cs\gamesdk\bbscur.c
        bcc32 /c $(OPT) /o$(OBJDIR)\bbscur.obj \gac_cs\gamesdk\bbscur.c
$(OBJDIR)\bbslst.obj: \gac_cs\gamesdk\bbslst.c
        bcc32 /c $(OPT) /o$(OBJDIR)\bbslst.obj \gac_cs\gamesdk\bbslst.c
$(OBJDIR)\plycur.obj: \gac_cs\gamesdk\plycur.c
        bcc32 /c $(OPT) /o$(OBJDIR)\plycur.obj \gac_cs\gamesdk\plycur.c
$(OBJDIR)\plylst.obj: \gac_cs\gamesdk\plylst.c 
        bcc32 /c $(OPT) /o$(OBJDIR)\plylst.obj \gac_cs\gamesdk\plylst.c
$(OBJDIR)\gamesdki.obj: \gac_cs\gamesdk\gamesdki.c \gac_cs\gamesdk\gamesdk.h
        bcc32 /c $(OPT) /o$(OBJDIR)\gamesdki.obj \gac_cs\gamesdk\gamesdki.c
$(OBJDIR)\gamesdko.obj: \gac_cs\gamesdk\gamesdko.c \gac_cs\gamesdk\gamesdk.h
        bcc32 /c $(OPT) /o$(OBJDIR)\gamesdko.obj \gac_cs\gamesdk\gamesdko.c
$(OBJDIR)\gamesdk.obj: \gac_cs\gamesdk\gamesdk.c \gac_cs\gamesdk\gamesdk.h
        bcc32 /c $(OPT) /o$(OBJDIR)\gamesdk.obj \gac_cs\gamesdk\gamesdk.c
$(OBJDIR)\netmail.obj: \gac_cs\gamesdk\netmail.c \gac_cs\gamesdk\netmail.h
        bcc32 /c $(OPT) /o$(OBJDIR)\netmail.obj \gac_cs\gamesdk\netmail.c
$(OBJDIR)\gregedit.obj: \gac_cs\gamesdk\gregedit.c
        bcc32 /c $(OPT) /o$(OBJDIR)\gregedit.obj \gac_cs\gamesdk\gregedit.c
$(OBJDIR)\ibbscfg.obj: \gac_cs\gamesdk\ibbscfg.c gac_wh.h
        bcc32 /c $(OPT) /o$(OBJDIR)\ibbscfg.obj \gac_cs\gamesdk\ibbscfg.c


$(OBJDIR)\wahoo.obj: wahoo.c gac_wh.h
        bcc32 /c $(OPT) /o$(OBJDIR)\wahoo.obj wahoo.c
$(OBJDIR)\gac_wh_s.obj: gac_wh_s.c gac_wh.h
        bcc32 /c $(OPT) /o$(OBJDIR)\gac_wh_s.obj gac_wh_s.c
