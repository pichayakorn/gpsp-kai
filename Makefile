# -x assembler-with-cpp
# gpSP makefile
# Gilead Kutnick - Exophase

# Global definitions

all: systembuttons_prx dvemgr homehookprx main

.PHONY: all clean main dvemgr homehookprx systembuttons_prx bump_build

clean: systembuttons_prx_clean  dvemgr_clean homehookprx_clean main_clean

dvemgr:
	make -C ./prx/dvemgr/

main:
	make -C ./gpsp/

# Increment BUILD_COUNT in build_count.inc (run explicitly, not on every build)
bump_build:
	awk '{ print $$1, $$2 + 1 }' build_count.inc > build_count.inc.tmp
	mv build_count.inc.tmp build_count.inc

homehookprx:
	make -C ./prx/homehookprx/

systembuttons_prx:
	make -C ./prx/systembuttons_prx/

dvemgr_clean:
	make -C ./prx/dvemgr/ clean

main_clean:
	make -C ./gpsp/ clean

homehookprx_clean:
	make -C ./prx/homehookprx/ clean

systembuttons_prx_clean:
	make -C ./prx/systembuttons_prx/ clean
	