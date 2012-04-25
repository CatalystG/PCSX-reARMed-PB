#CROSS_COMPILE=
AS  = $(CROSS_COMPILE)as
GCC = $(CROSS_COMPILE)gcc
CC  = $(CROSS_COMPILE)gcc
LD  = $(CROSS_COMPILE)ld
CC_OVERRIDE = C:/bbndk-2.0.0/host/win32/x86/usr/bin/qcc -V4.4.2,gcc_ntoarmv7le
ifdef CC_OVERRIDE
CC = $(CC_OVERRIDE)
endif

ARM926 ?= 0
ARM_CORTEXA8 ?= 1
PLATFORM ?= pandora
USE_OSS ?= 1
RAM_FIXED ?= 1
#USE_ALSA = 1
#DRC_DBG = 1
#PCNT = 1
TARGET ?= pcsx

-include Makefile.local

ARCH ?= $(shell $(GCC) -v 2>&1 | grep -i 'target:' | awk '{print $$2}' | awk -F '-' '{print $$1}')

CFLAGS += -Wall -ggdb -Ifrontend -ffast-math
LDFLAGS += -lpng -lz -lm
ifndef DEBUG
CFLAGS += -O2 -DNDEBUG
endif
CFLAGS += $(EXTRA_CFLAGS)

ifeq "$(ARCH)" "arm"
ifeq "$(ARM_CORTEXA8)" "1"
GCC_CFLAGS += -mcpu=cortex-a8 -mtune=cortex-a8 -mfpu=neon -mfloat-abi=softfp
ASFLAGS += -mcpu=cortex-a8 -mfpu=neon
endif
ifeq "$(ARM926)" "1"
GCC_CFLAGS += -mcpu=arm926ej-s -mtune=arm926ej-s
ASFLAGS += -mcpu=arm926ej-s -mfloat-abi=softfp
endif
endif
CFLAGS += $(GCC_CFLAGS)

# detect armv7 and NEON from the specified CPU
HAVE_NEON ?= $(shell $(GCC) -E -dD $(GCC_CFLAGS) frontend/config.h | grep -q '__ARM_NEON__ 1' && echo 1)
HAVE_ARMV7 ?= $(shell $(GCC) -E -dD $(GCC_CFLAGS) frontend/config.h | grep -q '__ARM_ARCH_7A__ 1' && echo 1)

all: $(TARGET)

# core
OBJS += libpcsxcore/cdriso.o libpcsxcore/cdrom.o libpcsxcore/cheat.o libpcsxcore/debug.o \
	libpcsxcore/decode_xa.o libpcsxcore/disr3000a.o libpcsxcore/mdec.o \
	libpcsxcore/misc.o libpcsxcore/plugins.o libpcsxcore/ppf.o libpcsxcore/psxbios.o \
	libpcsxcore/psxcommon.o libpcsxcore/psxcounters.o libpcsxcore/psxdma.o libpcsxcore/psxhle.o \
	libpcsxcore/psxhw.o libpcsxcore/psxinterpreter.o libpcsxcore/psxmem.o libpcsxcore/r3000a.o \
	libpcsxcore/sio.o libpcsxcore/socket.o libpcsxcore/spu.o
OBJS += libpcsxcore/gte.o libpcsxcore/gte_nf.o libpcsxcore/gte_divider.o
ifeq "$(ARCH)" "arm"
OBJS += libpcsxcore/gte_arm.o
endif
ifeq "$(HAVE_NEON)" "1"
OBJS += libpcsxcore/gte_neon.o
endif
libpcsxcore/cdrom.o libpcsxcore/misc.o: CFLAGS += -Wno-pointer-sign
libpcsxcore/misc.o libpcsxcore/psxbios.o: CFLAGS += -Wno-nonnull

# dynarec
ifndef NO_NEW_DRC
OBJS += libpcsxcore/new_dynarec/new_dynarec.o libpcsxcore/new_dynarec/linkage_arm.o
OBJS += libpcsxcore/new_dynarec/pcsxmem.o
endif
OBJS += libpcsxcore/new_dynarec/emu_if.o
libpcsxcore/new_dynarec/new_dynarec.o: libpcsxcore/new_dynarec/assem_arm.c \
	libpcsxcore/new_dynarec/pcsxmem_inline.c
libpcsxcore/new_dynarec/new_dynarec.o: CFLAGS += -Wno-all -Wno-pointer-sign
ifdef DRC_DBG
libpcsxcore/new_dynarec/emu_if.o: CFLAGS += -D_FILE_OFFSET_BITS=64
CFLAGS += -DDRC_DBG
endif
ifeq "$(RAM_FIXED)" "1"
CFLAGS += -DRAM_FIXED
endif

# spu
OBJS += plugins/dfsound/dma.o plugins/dfsound/freeze.o \
	plugins/dfsound/registers.o plugins/dfsound/spu.o
plugins/dfsound/spu.o: plugins/dfsound/adsr.c plugins/dfsound/reverb.c \
	plugins/dfsound/xa.c
ifeq "$(ARCH)" "arm"
OBJS += plugins/dfsound/arm_utils.o
endif
ifeq "$(USE_OSS)" "1"
plugins/dfsound/%.o: CFLAGS += -DUSEOSS
OBJS += plugins/dfsound/oss.o
endif
ifeq "$(USE_ALSA)" "1"
plugins/dfsound/%.o: CFLAGS += -DUSEALSA
ifeq "$(USE_SCREEN)" "1"
OBJS += qnx/alsa.o
else
OBJS += plugins/dfsound/alsa.o
endif
LDFLAGS += -lasound
endif

# gpu
OBJS += plugins/gpulib/gpu.o
ifeq "$(HAVE_NEON)" "1"
OBJS += plugins/gpulib/cspace_neon.o
OBJS += plugins/gpu_neon/psx_gpu_if.o plugins/gpu_neon/psx_gpu/psx_gpu_arm_neon.o
plugins/gpu_neon/psx_gpu_if.o: CFLAGS += -DNEON_BUILD -DTEXTURE_CACHE_4BPP -DTEXTURE_CACHE_8BPP
plugins/gpu_neon/psx_gpu_if.o: plugins/gpu_neon/psx_gpu/*.c
else
OBJS += plugins/gpulib/cspace.o
# note: code is not safe for strict-aliasing? (Castlevania problems)
plugins/dfxvideo/gpulib_if.o: CFLAGS += -fno-strict-aliasing
plugins/dfxvideo/gpulib_if.o: plugins/dfxvideo/prim.c plugins/dfxvideo/soft.c
OBJS += plugins/dfxvideo/gpulib_if.o
endif
ifdef X11
LDFLAGS += -lX11 `sdl-config --libs`
OBJS += plugins/gpulib/vout_sdl.o
plugins/gpulib/vout_sdl.o: CFLAGS += `sdl-config --cflags`
else
OBJS += plugins/gpulib/vout_fb.o
endif

# cdrcimg
OBJS += plugins/cdrcimg/cdrcimg.o

# dfinput
OBJS += plugins/dfinput/main.o plugins/dfinput/pad.o plugins/dfinput/guncon.o

# gui
OBJS += frontend/main.o frontend/plugin.o
OBJS += frontend/plugin_lib.o frontend/common/readpng.o
OBJS += frontend/common/fonts.o frontend/linux/plat.o
ifeq "$(USE_GTK)" "1"
OBJS += maemo/hildon.o maemo/main.o
maemo/%.o: maemo/%.c
else
ifeq "$(USE_SCREEN)" "1"
OBJS += qnx/screen.o qnx/external_main.o
qnx/%.o: qnx/%.c
else
OBJS += frontend/menu.o frontend/linux/in_evdev.o
OBJS += frontend/common/input.o frontend/linux/xenv.o

ifeq "$(PLATFORM)" "pandora"
frontend/%.o: CFLAGS += -DVOUT_FBDEV
OBJS += frontend/linux/fbdev.o
OBJS += frontend/plat_omap.o
OBJS += frontend/plat_pandora.o
else
ifeq "$(PLATFORM)" "caanoo"
OBJS += frontend/plat_pollux.o frontend/in_tsbutton.o frontend/blit320.o
OBJS += frontend/gp2x/in_gp2x.o frontend/warm/warm.o
else
OBJS += frontend/plat_dummy.o
endif
endif
endif

endif # !USE_GTK

ifdef X11
frontend/%.o: CFLAGS += -DX11
OBJS += frontend/xkb.o
endif
ifdef PCNT
CFLAGS += -DPCNT
endif
ifndef NO_TSLIB
frontend/%.o: CFLAGS += -DHAVE_TSLIB
OBJS += frontend/pl_gun_ts.o
endif
%.o: ASFLAGS += -Wa,--defsym,HAVE_ARMV7=$(HAVE_ARMV7)
frontend/%.o: CFLAGS += -DIN_EVDEV
frontend/menu.o: frontend/revision.h

libpcsxcore/gte_nf.o: libpcsxcore/gte.c
	$(CC) -c -o $@ $^ $(CFLAGS) -DFLAGLESS

frontend/revision.h: FORCE
	@(git describe || echo) | sed -e 's/.*/#define REV "\0"/' > $@_
	@diff -q $@_ $@ > /dev/null 2>&1 || cp $@_ $@
	@rm $@_
.PHONY: FORCE

%.o: %.S
	$(CC) $(CFLAGS) -c $^ -o $@
	
%.o: %.s
	$(CC) $(ASFLAGS) -c $^ -o $@

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) -Wl,-Map=$@.map

PLUGINS ?= plugins/spunull/spunull.so plugins/gpu-gles/gpu_gles.so \
	plugins/gpu_unai/gpu_unai.so plugins/dfxvideo/gpu_peops.so

$(PLUGINS):
	make -C plugins/gpulib/ clean
	make -C $(dir $@)

clean: $(PLAT_CLEAN)
	$(RM) $(TARGET) $(OBJS) $(TARGET).map

clean_plugins:
	make -C plugins/gpulib/ clean
	for dir in $(PLUGINS) ; do \
		$(MAKE) -C $$(dirname $$dir) clean; done

# ----------- release -----------

PND_MAKE ?= $(HOME)/dev/pnd/src/pandora-libraries/testdata/scripts/pnd_make.sh

VER ?= $(shell git describe master)

rel: pcsx $(PLUGINS) \
		frontend/pandora/pcsx.sh frontend/pandora/pcsx.pxml.templ frontend/pandora/pcsx.png \
		frontend/pandora/picorestore frontend/pandora/skin readme.txt COPYING
	rm -rf out
	mkdir -p out/plugins
	cp -r $^ out/
	sed -e 's/%PR%/$(VER)/g' out/pcsx.pxml.templ > out/pcsx.pxml
	rm out/pcsx.pxml.templ
	mv out/*.so out/plugins/
	mv out/plugins/gpu_unai.so out/plugins/gpuPCSX4ALL.so
	mv out/plugins/gpu_gles.so out/plugins/gpuGLES.so
	mv out/plugins/gpu_peops.so out/plugins/gpuPEOPS.so
	$(PND_MAKE) -p pcsx_rearmed_$(VER).pnd -d out -x out/pcsx.pxml -i frontend/pandora/pcsx.png -c
