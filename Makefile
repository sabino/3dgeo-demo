# SPDX-License-Identifier: GPL-3.0-or-later

PROJECT := 3dgeo
GAMEROM ?= 3dgeo_demo
GAMETITLE ?= 3Dgeo Demo / Template

BUILDDIR ?= build/viewer
VIEWER_CAPTURE_DIR ?= $(BUILDDIR)/captures
ROM := $(BUILDDIR)/rom
ASSETDIR := $(BUILDDIR)/assets
GENERATEDDIR := $(BUILDDIR)/generated
PRIMITIVE_OBJS := assets/models/ball.obj assets/models/donut.obj assets/models/cone.obj assets/models/cylinder.obj
MODEL_OBJS := assets/models/cube.obj assets/models/pyramid.obj assets/models/tetrahedron.obj $(PRIMITIVE_OBJS)
MODEL_DATA := $(GENERATEDDIR)/model_data.h

M68KGCC ?= m68k-neogeo-elf-gcc
M68KOBJCOPY ?= m68k-neogeo-elf-objcopy
Z80SDOBJCOPY ?= z80-neogeo-ihx-sdobjcopy
PKGCONFIG ?= pkg-config
PYTHON ?= python3
TILETOOL ?= tiletool.py
ROMTOOL ?= romtool.py
GNGEO ?= ngdevkit-gngeo
GNGEO_P1CONTROL ?= A=K122,B=K120,C=K97,D=K115,START=K49,COIN=K51,UP=K82,DOWN=K81,LEFT=K80,RIGHT=K79,MENU=K27

NGDEVKIT_PREFIX ?= $(shell dirname "$$(dirname "$$(readlink -f "$$(command -v $(M68KGCC) 2>/dev/null)")")")
NGPKGCONFIG = PKG_CONFIG_PATH="$(NGDEVKIT_PREFIX)/lib/pkgconfig:$(NGDEVKIT_PREFIX)/share/pkgconfig:$(PKG_CONFIG_PATH)" $(PKGCONFIG)
NGCFLAGS := $(shell $(NGPKGCONFIG) --cflags ngdevkit 2>/dev/null)
NGLDFLAGS := $(shell $(NGPKGCONFIG) --libs ngdevkit 2>/dev/null)
NGSHAREDIR := $(shell $(NGPKGCONFIG) --variable=sharedir ngdevkit 2>/dev/null)
GNGEO_DATA ?= $(NGSHAREDIR)/../ngdevkit-gngeo/gngeo_data.zip

CFLAGS ?= -std=c99 -fomit-frame-pointer -O3 -flto -g -Wall -Wextra
CFLAGS += $(NGCFLAGS) -Isrc -I$(GENERATEDDIR) -MMD -MP
LDFLAGS += $(NGLDFLAGS) -O3 -flto

PROMSIZE := 1048576
SROMSIZE := 131072
CROMSIZE := 2097152
MROMSIZE := 131072
VROMSIZE := 524288

ELF := $(BUILDDIR)/$(PROJECT).elf
PROM1 := $(ROM)/$(GAMEROM)-p1.p1
SROM1 := $(ROM)/$(GAMEROM)-s1.s1
CROM1 := $(ROM)/$(GAMEROM)-c1.c1
CROM2 := $(ROM)/$(GAMEROM)-c2.c2
MROM1 := $(ROM)/$(GAMEROM)-m1.m1
VROM1 := $(ROM)/$(GAMEROM)-v1.v1
CART := $(ROM)/$(GAMEROM).zip
GNGEO_HASH := $(ROM)/gngeo_data.zip
MAME_HASH := $(ROM)/neogeo.xml

.PHONY: all cart assets check-ngdevkit gngeo run test hosttest native-test

all: cart

cart: check-ngdevkit $(CART) $(GNGEO_HASH) $(MAME_HASH)

check-ngdevkit:
	@command -v $(M68KGCC) >/dev/null 2>&1 || { echo "missing $(M68KGCC)"; exit 1; }
	@command -v $(TILETOOL) >/dev/null 2>&1 || { echo "missing $(TILETOOL)"; exit 1; }
	@command -v $(ROMTOOL) >/dev/null 2>&1 || { echo "missing $(ROMTOOL)"; exit 1; }
	@$(NGPKGCONFIG) --exists ngdevkit || { echo "pkg-config cannot find ngdevkit; set NGDEVKIT_PREFIX"; exit 1; }
	@test -f "$(NGSHAREDIR)/nullsound_driver.ihx" || { echo "missing ngdevkit nullsound_driver.ihx"; exit 1; }

assets: $(SROM1)

$(BUILDDIR) $(ROM) $(ASSETDIR) $(GENERATEDDIR):
	mkdir -p $@

$(PRIMITIVE_OBJS) &: tools/make_primitive_objs.py
	$(PYTHON) tools/make_primitive_objs.py

$(MODEL_DATA): tools/obj_to_model.py $(MODEL_OBJS) | $(GENERATEDDIR)
	$(PYTHON) tools/obj_to_model.py $(MODEL_OBJS) $@

$(ASSETDIR)/shade-fix.gif: tools/make_fix_tiles.py | $(ASSETDIR)
	$(PYTHON) $< $@

$(ASSETDIR)/shade-fix.fix: $(ASSETDIR)/shade-fix.gif
	$(TILETOOL) --fix -c $< -o $@

$(ASSETDIR)/shade-sprite.gif: tools/make_sprite_tiles.py | $(ASSETDIR)
	$(PYTHON) $< $@

$(ASSETDIR)/shade-sprite.c1 $(ASSETDIR)/shade-sprite.c2 &: $(ASSETDIR)/shade-sprite.gif
	$(TILETOOL) --sprite -c $< -o $(ASSETDIR)/shade-sprite.c1 $(ASSETDIR)/shade-sprite.c2 -s $(CROMSIZE)

$(BUILDDIR)/main.o: $(MODEL_DATA) src/viewer_core.h

$(BUILDDIR)/%.o: src/%.c | $(BUILDDIR)
	mkdir -p $(dir $@)
	$(M68KGCC) $(CFLAGS) -c $< -o $@

$(ELF): $(BUILDDIR)/main.o
	$(M68KGCC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(PROM1): $(ELF) | $(ROM)
	$(M68KOBJCOPY) -O binary -S -R .text2 --gap-fill 0xff --pad-to $(PROMSIZE) $< $@
	dd if=$@ of=$@ conv=notrunc,swab status=none

$(SROM1): $(ASSETDIR)/shade-fix.fix | $(ROM)
	cp $< $@
	truncate -s $(SROMSIZE) $@

$(CROM1): $(ASSETDIR)/shade-sprite.c1 | $(ROM)
	cp $< $@

$(CROM2): $(ASSETDIR)/shade-sprite.c2 | $(ROM)
	cp $< $@

$(MROM1): | $(ROM)
	$(Z80SDOBJCOPY) -I ihex -O binary "$(NGSHAREDIR)/nullsound_driver.ihx" $@ --pad-to $(MROMSIZE)

$(VROM1): | $(ROM)
	dd if=/dev/zero of=$@ bs=$(VROMSIZE) count=1 status=none

$(CART): $(PROM1) $(SROM1) $(CROM1) $(CROM2) $(MROM1) $(VROM1) | $(ROM)
	$(ROMTOOL) -b cartridge -f zip \
		-p $(PROM1) -s $(SROM1) -c $(CROM1) $(CROM2) -m $(MROM1) -v $(VROM1) \
		-n $(GAMEROM) -l "$(GAMETITLE)" -y 2026 --publisher homebrew \
		-x "zip.comment=$(GAMETITLE)" -o $@

$(GNGEO_HASH): $(CART)
	$(ROMTOOL) -b hash -f gngeo \
		-p $(PROM1) -s $(SROM1) -c $(CROM1) $(CROM2) -m $(MROM1) -v $(VROM1) \
		-n $(GAMEROM) -l "$(GAMETITLE)" -x gngeo.data=$(GNGEO_DATA) -o $@

$(MAME_HASH): $(CART)
	$(ROMTOOL) -b hash -f mame \
		-p $(PROM1) -s $(SROM1) -c $(CROM1) $(CROM2) -m $(MROM1) -v $(VROM1) \
		-n $(GAMEROM) -l "$(GAMETITLE)" -o $@

$(ROM)/neogeo.zip: | $(ROM)
	cp "$(NGSHAREDIR)/neogeo.zip" $@

gngeo run: cart $(ROM)/neogeo.zip
	$(GNGEO) --scale 3 --screen320 --no-resize --system home --p1control="$(GNGEO_P1CONTROL)" -i $(ROM) -d $(GNGEO_HASH) $(GAMEROM)

hosttest: $(MODEL_DATA) tests/test_viewer.c tests/viewer_platform.h src/main.c src/viewer_core.h | $(BUILDDIR)
	cc -std=c99 -O2 -g -fsanitize=address,undefined -DVIEWER_HOST_TEST -Itests -Isrc -I$(GENERATEDDIR) tests/test_viewer.c -o $(BUILDDIR)/viewer-host-test
test: hosttest
	mkdir -p "$(VIEWER_CAPTURE_DIR)"
	VIEWER_CAPTURE_DIR="$(VIEWER_CAPTURE_DIR)" UBSAN_OPTIONS=halt_on_error=1 $(BUILDDIR)/viewer-host-test

$(ROM)/aes.zip: | $(ROM)
	cp "$(NGSHAREDIR)/aes.zip" $@

native-test: cart $(ROM)/aes.zip
	mkdir -p "$${VIEWER_CAPTURE_DIR:-$(BUILDDIR)/native-captures}"
	xvfb-run -a env VIEWER_ELF="$(ELF)" VIEWER_CAPTURE_DIR="$${VIEWER_CAPTURE_DIR:-$(BUILDDIR)/native-captures}" SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy \
		mame aes -cart $(GAMEROM) -rompath $(ROM) -hashpath $(ROM) \
		-video soft -sound none -window -skip_gameinfo -noplugins -seconds_to_run 16 \
		-nothrottle -autoboot_delay 0 -autoboot_script tools/viewer_probe.lua \
		-snapshot_directory "$${VIEWER_CAPTURE_DIR:-$(BUILDDIR)/native-captures}" > $(BUILDDIR)/native-test.log 2>&1
	grep -q '^VIEWER PASS:' $(BUILDDIR)/native-test.log

-include $(BUILDDIR)/main.d
