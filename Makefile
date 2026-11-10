# -x assembler-with-cpp
# gpSP makefile
# Gilead Kutnick - Exophase

# Global definitions

all: systembuttons_prx dvemgr homehookprx main

.PHONY: all clean main dvemgr homehookprx systembuttons_prx package check release

# Ready-to-copy Memory Stick folder: copy $(PACKAGE_DIR) to PSP/GAME/ and add
# your own gba_bios.bin. PACKAGE_PRX=prebuilt ships the hardware-tested modules
# from prx/prebuilt/; PACKAGE_PRX=built ships the ones just built in out/.
PACKAGE_DIR   = out/gpSP
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

# Package plus a zip named after the version, e.g. out/gpSP-kai-3.5.0.zip
RELEASE_ZIP = out/gpSP-kai-$(strip $(shell cat version.txt)).zip

release: check package
	rm -f $(RELEASE_ZIP)
	cd out && zip -qr $(notdir $(RELEASE_ZIP)) gpSP
	@echo "Release zip: $(RELEASE_ZIP)"

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
	