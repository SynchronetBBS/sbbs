SUBDIRS =
if COMPILE_HW_SDL1
SUBDIRS += 1
endif
if COMPILE_HW_SDL2
SUBDIRS += 2
endif

DIST_SUBDIRS = 1 2

noinst_HEADERS = \
    hwsdl_audio.h \
    hwsdl_mouse.h \
    hwsdl_opt.h \
    hwsdl_video.h

EXTRA_DIST = \
    hwsdl.c \
    hwsdl_audio.c \
    hwsdl_mouse.c \
    hwsdl_opt.c \
    hwsdl_video.c
