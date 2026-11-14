# -x assembler-with-cpp
# gpSP makefile
# Gilead Kutnick - Exophase

# Global definitions

include version.mk

all: systembuttons_prx dvemgr homehookprx main

.PHONY: all clean main dvemgr homehookprx systembuttons_prx package check release testpkg version

# Ready-to-copy Memory Stick folder: copy $(PACKAGE_DIR) to PSP/GAME/ and add
# your own gba_bios.bin. Its name has no version, so an update replaces the
# old files and keeps the saves in GBA/.
PACKAGE_DIR   = out/gpSP-kai
PACKAGE_FONTS = gpsp10.fbm knj10.fbm batang10.fbm

# PRX modules the EBOOT loads. By default homehook.prx is the one built from
# source (tested on a PSP-3000, #11) and dvemgr.prx (TV output) is takka's
# build from prx/prebuilt/ until TV-out is tested with the built one.
# PACKAGE_PRX=built ships both built modules, PACKAGE_PRX=prebuilt both
# prebuilt ones. SystemButtons.prx is never loaded, so it is not shipped.
PACKAGE_PRX  ?= default
ifeq ($(PACKAGE_PRX),built)
PRX_FROM_OUT      = homehook.prx dvemgr.prx
PRX_FROM_PREBUILT =
else ifeq ($(PACKAGE_PRX),prebuilt)
PRX_FROM_OUT      =
PRX_FROM_PREBUILT = homehook.prx dvemgr.prx
else
PRX_FROM_OUT      = homehook.prx
PRX_FROM_PREBUILT = dvemgr.prx
endif

package: all
	rm -rf $(PACKAGE_DIR)
	mkdir -p $(PACKAGE_DIR)/cfg $(PACKAGE_DIR)/font $(PACKAGE_DIR)/licenses
	cp out/EBOOT.PBP $(PACKAGE_DIR)/
	$(if $(PRX_FROM_OUT),cp $(addprefix out/,$(PRX_FROM_OUT)) $(PACKAGE_DIR)/)
	$(if $(PRX_FROM_PREBUILT),cp $(addprefix prx/prebuilt/,$(PRX_FROM_PREBUILT)) $(PACKAGE_DIR)/)
	cp data/cfg/* $(PACKAGE_DIR)/cfg/
	cp $(addprefix data/font/,$(PACKAGE_FONTS)) $(PACKAGE_DIR)/font/
	cp data/game_config.txt $(PACKAGE_DIR)/
	cp COPYING $(PACKAGE_DIR)/licenses/
	cp -r docs/licenses/. $(PACKAGE_DIR)/licenses/
	for d in ROM SAV CFG SS CHT; do mkdir -p $(PACKAGE_DIR)/GBA/$$d; done
	@echo "Package ready: $(PACKAGE_DIR) (add gba_bios.bin)"

clean: systembuttons_prx_clean  dvemgr_clean homehookprx_clean main_clean

dvemgr:
	$(MAKE) -C ./prx/dvemgr/

# The PRX builds regenerate src/homehook.S and src/SystemButtons.*, which the
# EBOOT build uses, so with make -j the EBOOT has to wait for them.
main: systembuttons_prx homehookprx
	$(MAKE) -C ./eboot/

# Check the UI message files against the code (entry order, printf codes)
check:
	python3 tools/check_messages.py

# Package plus a zip named after the version: out/gpSP-kai-3.5.2.zip for a
# release, out/gpSP-kai-3.5.2-dev+<commit>.zip for any other build
RELEASE_ZIP = out/gpSP-kai-$(GPSP_VERSION_STRING).zip

release: check package
	rm -f $(RELEASE_ZIP)
	cd out && zip -qr $(notdir $(RELEASE_ZIP)) $(notdir $(PACKAGE_DIR))
	@echo "Release zip: $(RELEASE_ZIP)"

# Local PSP test build: out/test/gpSP-kai-<version>.zip unzips to a folder
# gpSP-kai-<version>/ to paste into PSP/GAME/, so several builds can sit side
# by side. Every path in cfg/dir.cfg is relative to that folder. The
# git-ignored testdata/ folder has the same layout as that folder
# (gba_bios.bin, GBA/ROM, GBA/SAV, ...) and is copied over it. Never used
# for releases or CI.
TEST_DATA = testdata
TEST_NAME_DEFAULT = gpSP-kai-$(GPSP_VERSION_STRING)
TEST_NAME = $(TEST_NAME_DEFAULT)
TEST_DIR  = out/test/$(TEST_NAME)
# A custom TEST_NAME (e.g. two variants of one build) is also the XMB title,
# so the variants can be told apart on the PSP.
TEST_TITLE = $(if $(filter $(TEST_NAME_DEFAULT),$(TEST_NAME)),,$(TEST_NAME))

# The XMB shows "Corrupted Data" for a PSP/GAME folder name longer than 31
# characters (tested on a PSP-3000 with ARK-5: 31 shows, 32 does not), so
# testpkg refuses a longer TEST_NAME before building anything.
TEST_NAME_MAX = 31
ifneq ($(filter testpkg,$(MAKECMDGOALS)),)
ifeq ($(shell [ $$(printf '%s' '$(TEST_NAME)' | wc -c) -gt $(TEST_NAME_MAX) ] && echo long),long)
$(error TEST_NAME "$(TEST_NAME)" is longer than $(TEST_NAME_MAX) characters; the PSP would show the folder as Corrupted Data)
endif
endif

testpkg: check package
	rm -rf $(TEST_DIR) $(TEST_DIR).zip
	mkdir -p out/test
	cp -r $(PACKAGE_DIR) $(TEST_DIR)
	if [ -d $(TEST_DATA) ]; then cp -r $(TEST_DATA)/. $(TEST_DIR)/; fi
	$(if $(TEST_TITLE),tools/retitle_eboot.sh $(TEST_DIR)/EBOOT.PBP '$(TEST_TITLE)' $(GPSP_VERSION))
	find $(TEST_DIR) -name .DS_Store -delete
	cd out/test && zip -qr $(TEST_NAME).zip $(TEST_NAME)
	@echo "Test zip: $(TEST_DIR).zip"

# Print the version string, e.g. for naming the CI artifact
version:
	@echo $(GPSP_VERSION_STRING)

homehookprx:
	$(MAKE) -C ./prx/homehookprx/

systembuttons_prx:
	$(MAKE) -C ./prx/systembuttons_prx/

dvemgr_clean:
	$(MAKE) -C ./prx/dvemgr/ clean

main_clean:
	$(MAKE) -C ./eboot/ clean

homehookprx_clean:
	$(MAKE) -C ./prx/homehookprx/ clean

systembuttons_prx_clean:
	$(MAKE) -C ./prx/systembuttons_prx/ clean
	