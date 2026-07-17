MODULE := backends/platform/synchronet

MODULE_OBJS := \
	syncscumm.o \
	video_dump.o \
	video_term.o \
	audio_term.o \
	sst_io.o \
	sst_quant.o

# We don't use rules.mk but rather manually update OBJS and MODULE_DIRS.
MODULE_OBJS := $(addprefix $(MODULE)/, $(MODULE_OBJS))

OBJS := $(MODULE_OBJS) $(OBJS)
MODULE_DIRS += $(sort $(dir $(MODULE_OBJS)))
