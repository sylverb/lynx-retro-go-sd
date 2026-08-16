# Retro-Go SD — Atari Lynx (Handy / handy-go) standalone dynamic core.
#
#   make                  — build + pack → lynx.bin
#   make docker           — same inside the firmware builder image
#
# Memory: hot Handy .text (system/mikie/susie) in ITCM; 64K WRAM + frame/
# audio scratch in DTCM; cart ROM banks in RAM_EMU / XIP. ITCM is not used
# for heap data.

#######################################
# Project identity
#######################################
PROJECT_KIND ?= core

CORE_NAME  := lynx
CORE_ENTRY := app_main_lynx

CORE_HANDY := external/handy-go

CORE_CXX_SOURCES := \
$(CORE_HANDY)/cart.cpp \
$(CORE_HANDY)/eeprom.cpp \
$(CORE_HANDY)/lynxdec.cpp \
$(CORE_HANDY)/mikie.cpp \
$(CORE_HANDY)/susie.cpp \
$(CORE_HANDY)/system.cpp \
src/main_lynx.cpp

CORE_C_SOURCES :=

CORE_C_INCLUDES := \
-I$(CORE_HANDY)

# TARGET_GNW: handy-go G&W paths (log_printf, LSS_FILE→FILE, wdog in UpdateFrame)
CORE_LDSCRIPT := lynx_core.ld
CORE_EXTRA_SEGMENTS := itcm:core_itcm

GNW_CORE_SDK ?= sdk
BUILD_DIR ?= build/$(PROJECT_KIND)

#######################################
# Kind-specific compile defs + packing
#######################################
ifeq ($(PROJECT_KIND),core)
CORE_C_DEFS := \
-DPROJECT_KIND_CORE=1 \
-DCOVERFLOW=1 \
-DCHEAT_CODES=0 \
-DTARGET_GNW

PACKED_BIN  := $(CORE_NAME).bin
PAD_LOGO    := src/assets/pad.bmp
HEADER_LOGO := src/assets/header.bmp

else ifeq ($(PROJECT_KIND),homebrew)
$(error Lynx is a dynamic core only — use PROJECT_KIND=core)
else
$(error PROJECT_KIND must be 'core' (got '$(PROJECT_KIND)'))
endif

include $(GNW_CORE_SDK)/Makefile

PACK_CORE := $(GNW_CORE_SDK)/tools/pack_core.py

#######################################
# Pack
#######################################
.PHONY: pack

pack: $(TARGET_BIN) $(PAD_LOGO) $(HEADER_LOGO)
	$(V)$(ECHO) [ PACK CORE ] $(PACKED_BIN)
	$(V)python3 $(PACK_CORE) \
		--elf $(TARGET_ELF) --bin $(TARGET_BIN) \
		--system-name "Atari Lynx" --dirname lynx \
		--extensions "lnx lyx" \
		--core-name "Handy" \
		--version 1.0.0 \
		--pad-logo $(PAD_LOGO) \
		--header-logo $(HEADER_LOGO) \
		--logo-invert \
		--out $(PACKED_BIN)

all: pack

.PHONY: print-PROJECT_KIND print-PACKED_BIN print-CORE_NAME print-DOCKER_IMAGE
print-PROJECT_KIND:
	@echo $(PROJECT_KIND)
print-PACKED_BIN:
	@echo $(PACKED_BIN)
print-CORE_NAME:
	@echo $(CORE_NAME)
print-DOCKER_IMAGE:
	@echo $(DOCKER_IMAGE)

clean::
	$(V)rm -f $(PACKED_BIN)

#######################################
# Docker
#######################################
.PHONY: docker docker_pull docker_shell

RELEASE_VERSION ?= v1.5
DOCKER_REPOSITORY ?= sylverb/retro-go-sd-builder
DOCKER_IMAGE ?= $(DOCKER_REPOSITORY):$(RELEASE_VERSION)

DOCKER_TTY_FLAG := $(shell if [ -t 0 ]; then echo -it; else echo; fi)
DOCKER_USER := $(shell id -u):$(shell id -g)
DOCKER_RUN := docker run --rm $(DOCKER_TTY_FLAG) \
	--user $(DOCKER_USER) \
	-v "$(CURDIR):/opt/workdir" \
	-w /opt/workdir \
	$(DOCKER_IMAGE)

docker:
	$(V)$(ECHO) "[ DOCKER ]" $(DOCKER_IMAGE) "PROJECT_KIND=$(PROJECT_KIND)"
	$(V)$(DOCKER_RUN) make --no-print-directory -j$$(nproc) PROJECT_KIND=$(PROJECT_KIND)

docker_pull:
	$(V)$(ECHO) "[ PULL ]" $(DOCKER_IMAGE)
	$(V)docker pull $(DOCKER_IMAGE)

docker_shell:
	$(DOCKER_RUN) bash
