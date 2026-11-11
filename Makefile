# -x assembler-with-cpp
# gpSP makefile
# Gilead Kutnick - Exophase

# Global definitions

include version.mk

all: systembuttons_prx dvemgr homehookprx main

.PHONY: all clean main dvemgr homehookprx systembuttons_prx package check release testpkg version

# Ready-to-copy Memory Stick folder: copy $(PACKAGE_DIR) to PSP/GAME/ and add
# your own gba_bios.bin. Its name has no version, so an update replaces the
# old files and keeps the saves in GBA/. PACKAGE_PRX=prebuilt ships the hardware-tested modules
# from prx/prebuilt/; PACKAGE_PRX=built ships the ones just built in out/.
PACKAGE_DIR   = out/gpSP-kai
PACKAGE_PRX  ?= prebuilt
PACKAGE_FONTS = gpsp10.fbm knj10.fbm batang10.fbm

package: all
	rm -rf $(PACKAGE_DIR)
	mkdir -p $(PACKAGE_DIR)/cfg $(PACKAGE_DIR)/font $(PACKAGE_DIR)/licenses
	cp out/EBOOT.PBP $(PACKAGE_DIR)/
	cp $(if $(filter built,$(PACKAGE_PRX)),out,prx/prebuilt)/*.prx $(PACKAGE_DIR)/
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
# by side. Every path in cfg/dir.cfg is relative to that folder. Files in the
# git-ignored testdata/ folder are added: gba_bios.bin next to the EBOOT, and
# ROM/ SAV/ CFG/ CHT/ SS/ into GBA/. Never used for releases or CI.
TEST_DATA = testdata
TEST_NAME = gpSP-kai-$(GPSP_VERSION_STRING)
TEST_DIR  = out/test/$(TEST_NAME)

testpkg: check package
	rm -rf $(TEST_DIR) $(TEST_DIR).zip
	mkdir -p out/test
	cp -r $(PACKAGE_DIR) $(TEST_DIR)
	if [ -f $(TEST_DATA)/gba_bios.bin ]; then cp $(TEST_DATA)/gba_bios.bin $(TEST_DIR)/; fi
	for d in ROM SAV CFG CHT SS; do \
	  if [ -d $(TEST_DATA)/$$d ]; then cp -r $(TEST_DATA)/$$d/. $(TEST_DIR)/GBA/$$d/; fi; \
	done
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
	