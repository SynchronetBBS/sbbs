bin_PROGRAMS =
if COMPILE_UICMDLINE
bin_PROGRAMS += 1oom_cmdline
endif

if COMPILE_UICLASSIC
if COMPILE_UICLASSIC_SDL1
bin_PROGRAMS += 1oom_classic_sdl1
endif
if COMPILE_UICLASSIC_SDL2
bin_PROGRAMS += 1oom_classic_sdl2
endif
if COMPILE_UICLASSIC_ALLEG4
bin_PROGRAMS += 1oom_classic_alleg4
endif
endif

if COMPILE_TOOLS
bin_PROGRAMS += 1oom_pbxdump 1oom_pbxmake 1oom_gfxconv 1oom_saveconv 1oom_lbxedit
if COMPILE_TOOLS_SDL1
bin_PROGRAMS += 1oom_lbxview_sdl1
endif
if COMPILE_TOOLS_SDL2
bin_PROGRAMS += 1oom_lbxview_sdl2
endif
if COMPILE_TOOLS_ALLEG4
bin_PROGRAMS += 1oom_lbxview_alleg4
endif
endif

SUBDIRS = hw os ui game
DIST_SUBDIRS = hw os ui game

util_sources = \
    bits.h \
    boolvec.h \
    cfg.c \
    cfg.h \
    comp.h \
    gameapi.h \
    gfxaux.c \
    gfxaux.h \
    gfxscale.h \
    hw.h \
    lbx.c \
    lbx.h \
    lbxfont.c \
    lbxfont.h \
    lbxgfx.c \
    lbxgfx.h \
    lbxpal.c \
    lbxpal.h \
    lib.c \
    lib.h \
    log.c \
    log.h \
    main.c \
    main.h \
    menu.c \
    menu.h \
    options.c \
    options.h \
    os.h \
    pbx.c \
    pbx.h \
    rnd.c \
    rnd.h \
    types.h \
    ui.h \
    util.c \
    util.h \
    util_cstr.c \
    util_cstr.h \
    util_math.c \
    util_math.h \
    version.h

audio_sources = \
    fmt_id.h \
    fmt_mus.c \
    fmt_mus.h \
    fmt_sfx.c \
    fmt_sfx.h

ui_sources = \
    font8x8.c \
    font8x8.h \
    font8x8_draw.c \
    font8x8_draw.h \
    kbd.c \
    kbd.h \
    mouse.c \
    mouse.h \
    palette.c \
    palette.h

pic_sources = \
    fmt_pic.c \
    fmt_pic.h

BUILT_SOURCES = version.inc

.PHONY: version.inc.indirect

version.inc: version.inc.indirect

version.inc.indirect:
	$(SHELL) $(srcdir)/version.sh $(top_srcdir) $(srcdir)/version.inc @USE_GIT@ @VERSION_1OOM@

EXTRA_DIST = version.sh

AM_CPPFLAGS = \
    @OS_INCLUDES@ \
    -I$(top_srcdir)/src

common_libs = @OS_LIBS@
game_lib = $(top_builddir)/src/game/libgame.a
gamemain_lib = $(top_builddir)/src/game/libgamemain.a
gamesaveconv_lib = $(top_builddir)/src/game/libgamesaveconv.a

uicmdline_lib = $(top_builddir)/src/ui/cmdline/libuicmdline.a
hwnop_lib = $(top_builddir)/src/hw/nop/libhwnop.a
1oom_cmdline_libs = $(gamemain_lib) $(game_lib) $(uicmdline_lib) $(game_lib) $(hwnop_lib) $(common_libs)
1oom_cmdline_SOURCES = $(util_sources)
1oom_cmdline_LDADD = $(1oom_cmdline_libs) @READLINE_LIBS@
1oom_cmdline_DEPENDENCIES = $(1oom_cmdline_libs)

uiclassic_lib = $(top_builddir)/src/ui/classic/libuiclassic.a
hwsdl1_lib = $(top_builddir)/src/hw/sdl/1/libhwsdl1.a
1oom_classic_sdl1_libs = $(gamemain_lib) $(game_lib) $(uiclassic_lib) $(game_lib) $(uiclassic_lib) $(hwsdl1_lib) $(common_libs)
1oom_classic_sdl1_SOURCES = $(audio_sources) $(util_sources) $(ui_sources)
1oom_classic_sdl1_LDADD = $(1oom_classic_sdl1_libs) @HW_SDL1_LIBS@ @EXTRA_SDL1_LIBS@ @SAMPLERATE_LIBS@
1oom_classic_sdl1_DEPENDENCIES = $(1oom_classic_sdl1_libs)

uinop_lib = $(top_builddir)/src/ui/nop/libuinop.a
1oom_lbxview_sdl1_libs = $(hwsdl1_lib) $(uinop_lib) $(common_libs)
1oom_lbxview_sdl1_SOURCES = lbxview.c $(audio_sources) $(util_sources) $(ui_sources) $(pic_sources)
1oom_lbxview_sdl1_LDADD = $(1oom_lbxview_sdl1_libs) @HW_SDL1_LIBS@ @EXTRA_SDL1_LIBS@ @SAMPLERATE_LIBS@
1oom_lbxview_sdl1_DEPENDENCIES = $(1oom_lbxview_sdl1_libs)

hwsdl2_lib = $(top_builddir)/src/hw/sdl/2/libhwsdl2.a
1oom_classic_sdl2_libs = $(gamemain_lib) $(game_lib) $(uiclassic_lib) $(game_lib) $(uiclassic_lib) $(hwsdl2_lib) $(common_libs)
1oom_classic_sdl2_SOURCES = $(audio_sources) $(util_sources) $(ui_sources)
1oom_classic_sdl2_LDADD = $(1oom_classic_sdl2_libs) @HW_SDL2_LIBS@ @EXTRA_SDL2_LIBS@ @SAMPLERATE_LIBS@
1oom_classic_sdl2_DEPENDENCIES = $(1oom_classic_sdl2_libs)

1oom_lbxview_sdl2_libs = $(hwsdl2_lib) $(uinop_lib) $(common_libs)
1oom_lbxview_sdl2_SOURCES = lbxview.c $(audio_sources) $(util_sources) $(ui_sources) $(pic_sources)
1oom_lbxview_sdl2_LDADD = $(1oom_lbxview_sdl2_libs) @HW_SDL2_LIBS@ @EXTRA_SDL2_LIBS@ @SAMPLERATE_LIBS@
1oom_lbxview_sdl2_DEPENDENCIES = $(1oom_lbxview_sdl2_libs)

hwalleg4_lib = $(top_builddir)/src/hw/alleg/4/libhwalleg4.a
1oom_classic_alleg4_libs = $(gamemain_lib) $(game_lib) $(uiclassic_lib) $(game_lib) $(uiclassic_lib) $(hwalleg4_lib) $(common_libs)
1oom_classic_alleg4_SOURCES = $(audio_sources) $(util_sources) $(ui_sources)
1oom_classic_alleg4_LDADD = $(1oom_classic_alleg4_libs) @HW_ALLEG4_LIBS@ @EXTRA_ALLEG4_LIBS@ @SAMPLERATE_LIBS@
1oom_classic_alleg4_DEPENDENCIES = $(1oom_classic_alleg4_libs)

1oom_lbxview_alleg4_libs = $(hwalleg4_lib) $(uinop_lib) $(common_libs)
1oom_lbxview_alleg4_SOURCES = lbxview.c $(audio_sources) $(util_sources) $(ui_sources) $(pic_sources)
1oom_lbxview_alleg4_LDADD = $(1oom_lbxview_alleg4_libs) @HW_ALLEG4_LIBS@ @EXTRA_ALLEG4_LIBS@ @SAMPLERATE_LIBS@
1oom_lbxview_alleg4_DEPENDENCIES = $(1oom_lbxview_alleg4_libs)

1oom_lbxedit_libs = $(hwnop_lib) $(common_libs)
1oom_lbxedit_SOURCES = lbxedit.c util.c util_cstr.c lib.c log.c
1oom_lbxedit_LDADD = $(1oom_lbxedit_libs)
1oom_lbxedit_DEPENDENCIES = $(1oom_lbxedit_libs)

1oom_pbxdump_libs = $(hwnop_lib) $(common_libs)
1oom_pbxdump_SOURCES = pbxdump.c pbx.c util.c lib.c log.c
1oom_pbxdump_LDADD = $(1oom_pbxdump_libs)
1oom_pbxdump_DEPENDENCIES = $(1oom_pbxdump_libs)

1oom_pbxmake_libs = $(hwnop_lib)
1oom_pbxmake_SOURCES = pbxmake.c util.c util_cstr.c lib.c log.c
1oom_pbxmake_LDADD = $(1oom_pbxmake_libs)
1oom_pbxmake_DEPENDENCIES = $(1oom_pbxmake_libs)

1oom_gfxconv_libs = $(hwnop_lib)
1oom_gfxconv_SOURCES = gfxconv.c util.c lib.c log.c $(pic_sources)
1oom_gfxconv_LDADD = $(1oom_gfxconv_libs)
1oom_gfxconv_DEPENDENCIES = $(1oom_gfxconv_libs)

1oom_saveconv_libs = $(gamesaveconv_lib) $(hwnop_lib) $(uinop_lib) $(common_libs)
1oom_saveconv_SOURCES = saveconv.c lib.c log.c options.c util.c
1oom_saveconv_LDADD = $(1oom_saveconv_libs)
1oom_saveconv_DEPENDENCIES = $(1oom_saveconv_libs)

.PHONY: libui libhw libos libgame libgamemain

libos:
	(cd os && $(MAKE))
libhw:
	(cd hw && $(MAKE))
libui:
	(cd ui && $(MAKE))
libgame:
	(cd game && $(MAKE) libgame.a)
libgamemain:
	(cd game && $(MAKE) libgamemain.a)
libgamesaveconv:
	(cd game && $(MAKE) libgamesaveconv.a)
