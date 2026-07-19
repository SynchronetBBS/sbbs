MODULE := backends/platform/synchronet

MODULE_OBJS := \
	syncscumm.o \
	video_dump.o \
	video_term.o \
	audio_term.o \
	sst_quant.o

# We don't use rules.mk but rather manually update OBJS and MODULE_DIRS.
# MODULE_OBJS is deliberately left as the bare .o list above (not reassigned
# to its $(addprefix ...) form): the Windows MSVC project generator,
# devtools/create_project, parses this module.mk directly and reads MODULE_OBJS
# literally -- a "$(addprefix ...)" reassignment would feed it those make
# functions as if they were object names. The prefixed paths go through a
# private variable instead, so the Unix make build is byte-for-byte unchanged
# while create_project sees a plain, parseable object list.
SYNCSCUMM_OBJS := $(addprefix $(MODULE)/, $(MODULE_OBJS))

OBJS := $(SYNCSCUMM_OBJS) $(OBJS)
MODULE_DIRS += $(sort $(dir $(SYNCSCUMM_OBJS)))
