# -x assembler-with-cpp
# gpSP makefile
# Gilead Kutnick - Exophase

# Global definitions

include ../Makefile.set

PSPSDK          = ${shell psp-config --pspsdk-path}
PREFIX          = ${shell psp-config --psp-prefix}

OBJS            = main.o cpu_common.o cpu_asm.o cpu_c.o video.o memory.o sound.o input.o gu.o
OBJS           += gui.o zip.o cheats.o mips_stub.o fbm_print.o bios.o draw.o video_setting.o
OBJS           += pspDveManager.o homehook.o unicode.o

ifeq ($(USE_ADHOC), 1)
OBJS           += adhoc.o
COMMON_FILE += adhoc.h
endif

TARGET          = UO_gpSP_kai

PSP_FW_VERSION=300
VERSION_MODE = user

include ../version.mk
GPSP_VER_WORDS  := $(subst ., ,$(GPSP_VERSION))
GPSP_VER_MAJOR  := $(word 1,$(GPSP_VER_WORDS))
GPSP_VER_MINOR  := $(word 2,$(GPSP_VER_WORDS))

PSP_EBOOT_TITLE = gpSP kai $(GPSP_VERSION_STRING)

# XMB art. A release gets the icon and the background, plus the animated
# icon (ICON1.PMF, played when the game is highlighted) if image/ has one; it
# is optional, and without it the XMB shows the still icon. A dev build
# (-dev+<commit>) gets only the gray "DEV" icon: the XMB hides the title of
# any game that has a background, and dev builds need the title to show which
# commit they are; the color animation would undo the gray look.
# ?= keeps command-line overrides working.
ifneq ($(findstring -dev+,$(GPSP_VERSION_STRING)),)
PSP_EBOOT_ICON  ?= ../image/ICON0_dev.png
PSP_EBOOT_ICON1 ?= NULL
PSP_EBOOT_PIC1  ?= NULL
else
PSP_EBOOT_ICON  ?= ../image/ICON0.png
PSP_EBOOT_ICON1 ?= $(or $(wildcard ../image/ICON1.PMF),NULL)
PSP_EBOOT_PIC1  ?= ../image/PIC1.png
endif
# The XMB information screen shows only APP_VER, in Sony's XX.YY format,
# which cannot express MAJOR.MINOR.PATCH (3.5.1 would read as 3.51), so
# APP_VER is left out and the XMB shows "-". DISC_VERSION is not shown by
# the XMB; it keeps the full version for tools that read PARAM.SFO.
SFOFLAGS        = -s DISC_VERSION=$(GPSP_VERSION)

VPATH           += ..
CFLAGS          += -O3 -DPSP_BUILD -G0 -funsigned-char
CFLAGS          += -Wall
#CFLAGS          += -W -Wno-sign-compare
#CFLAGS          += -Wshadow -Wbad-function-cast -Wconversion -Wstrict-prototypes -Wmissing-declarations -Wmissing-prototypes -Wstrict-prototypes -Wnested-externs -Wredundant-decls 
CFLAGS          +=  -funsigned-char -ffast-math -fmerge-all-constants -funsafe-loop-optimizations

CFLAGS          += -DVERSION_MAJOR=$(GPSP_VER_MAJOR)
CFLAGS          += -DVERSION_MINOR=$(GPSP_VER_MINOR)
CFLAGS          += -DVERSION_STRING='"$(GPSP_VERSION_STRING)"'

ifeq ($(USE_DEBUG), 1)
CFLAGS          += -DUSE_DEBUG
endif

ifeq ($(USE_ADHOC), 1)
CFLAGS          += -DUSE_ADHOC
endif

ifeq ($(USE_C_CORE), 1)
CFLAGS          += -DUSE_C_CORE
endif

ifeq ($(PSP_LARGE_MEMORY), 0)
CFLAGS          += -DUSE_EXT_MEM
endif

# Write each object's header dependencies to a .d file next to it. Every
# source includes common.h, which includes almost every header, so a list
# kept by hand misses some: a stale object built against an old message.h
# shows the wrong messages.
CFLAGS          += -MMD -MP
EXTRA_CLEAN     += $(OBJS:.o=.d)

ASFLAGS         = ${CFLAGS}
EXTRA_TARGETS   = EBOOT.PBP

#LDFLAGS += -Wl,-Map gpSP.map

LIBS            += -lpspgu -lpsprtc -lpspaudio -lz -lpsppower -lpspkubridge -lpspgum -lpspsystemctrl_user -lm

ifdef USE_ADHOC
LIBS            += -lpspnet -lpspwlan -lpspnet_adhoc -lpspnet_adhocctl -lpspnet_adhocmatching
endif

include ${PSPSDK}/lib/build.mak

SRC_DIR = ../src/

COMMON_FILE       = ${SRC_DIR}common.h \
                                 ${SRC_DIR}cpu.h \
                                 ${SRC_DIR}memory.h \
                                 ${SRC_DIR}video.h \
                                 ${SRC_DIR}gu.h \
                                 ${SRC_DIR}input.h \
                                 ${SRC_DIR}sound.h \
                                 ${SRC_DIR}main.h \
                                 ${SRC_DIR}gui.h \
                                 ${SRC_DIR}zip.h \
                                 ${SRC_DIR}cheats.h \
                                 ${SRC_DIR}unicode.h

UI_COMMON_FILE  = ${SRC_DIR}fbm_print.h \
                                 ${SRC_DIR}draw.h \
                                 ${SRC_DIR}message.h

main.o			: ${COMMON_FILE} ${UI_COMMON_FILE} ${SRC_DIR}main.c
cpu_common.o	: ${COMMON_FILE} ${SRC_DIR}cpu_common.c
cpu_asm.o		: ${COMMON_FILE} ${SRC_DIR}cpu_asm.c
cpu_c.o			: ${COMMON_FILE} ${SRC_DIR}cpu_c.c
video.o			: ${COMMON_FILE} ${SRC_DIR}video.c 
gu.o			: ${COMMON_FILE} ${SRC_DIR}dvemgr.h ${SRC_DIR}gu.c 
memory.o		: ${COMMON_FILE} ${SRC_DIR}memory.c
sound.o			: ${COMMON_FILE} ${SRC_DIR}sound.c
input.o			: ${COMMON_FILE} ${UI_COMMON_FILE} ${SRC_DIR}homehook.h ${SRC_DIR}input.c
gui.o			: ${COMMON_FILE} ${UI_COMMON_FILE} ${SRC_DIR}homehook.h ${SRC_DIR}gui.c
zip.o			: ${COMMON_FILE} ${SRC_DIR}zip.c
cheats.o		: ${COMMON_FILE} ${SRC_DIR}cheats.c
mips_stub.o		: ${SRC_DIR}mips_stub.S
fbm_print.o		: ${COMMON_FILE} ${UI_COMMON_FILE} ${SRC_DIR}fbm_print.c
bios.o			: ${COMMON_FILE} ${SRC_DIR}bios.c ${SRC_DIR}bios.h
draw.o			: ${COMMON_FILE} ${UI_COMMON_FILE} ${SRC_DIR}draw.c ${SRC_DIR}draw.h
pspDveManager.o : ${SRC_DIR}pspDveManager.S
homehook.o		: ${SRC_DIR}homehook.S
video_setting.o	: ${COMMON_FILE} ${SRC_DIR}video_setting.c
unicode.o		: ${COMMON_FILE} ${SRC_DIR}unicode.c

ifdef USE_ADHOC
adhoc.o					: ${COMMON_FILE} ${SRC_DIR}adhoc.c ${SRC_DIR}adhoc.h
endif

# version.txt and Makefile.set set compiler flags for every object (version
# numbers, USE_* options), so a change to either must recompile everything.
$(OBJS): ../version.txt ../version.mk ../Makefile.set

-include $(OBJS:.o=.d)

%.o: ../src/%.c
	@echo Compiling $<...
	@$(CC) $(CDEFS) $(CFLAGS) -c $< -o$@

%.o: ../src/%.S
	@echo Assembling $<...
	@$(CC) $(CDEFS) $(CFLAGS) -c $< -o$@

%.o: ../src/%.s
	@echo Assembling $<...
	@$(AS) $(ASDEFS) $(ASFLAGS) -c $< -o$@

%.a:
	@echo Archiving $@...
	@$(AR) -r $@ $^

all:
	rm -f ./PARAM.SFO
	rm -f ./gui.o
	mkdir -p ../out
	cp ./EBOOT.PBP ../out/
