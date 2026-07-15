MODULE := backends/platform/synchronet

MODULE_OBJS := \
	syncscumm.o \
	video_dump.o

# We don't use rules.mk but rather manually update OBJS and MODULE_DIRS.
MODULE_OBJS := $(addprefix $(MODULE)/, $(MODULE_OBJS))

# backends/module.mk only pulls in the null mixer for BACKEND=null/SDL --
# except under ENABLE_EVENTRECORDER, where it adds it for every other
# backend (including ours).  Add it only when that block won't.
ifndef ENABLE_EVENTRECORDER
MODULE_OBJS += backends/mixer/null/null-mixer.o
endif

OBJS := $(MODULE_OBJS) $(OBJS)
MODULE_DIRS += $(sort $(dir $(MODULE_OBJS)))
