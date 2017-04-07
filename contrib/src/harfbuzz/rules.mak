# HARFBUZZ

HARFBUZZ_VERSION := 1.0.6
HARFBUZZ_URL := http://www.freedesktop.org/software/harfbuzz/release/harfbuzz-$(HARFBUZZ_VERSION).tar.bz2
PKGS += harfbuzz
ifeq ($(call need_pkg,"harfbuzz"),)
PKGS_FOUND += harfbuzz
endif

$(TARBALLS)/harfbuzz-$(HARFBUZZ_VERSION).tar.bz2:
	$(call download_pkg,$(HARFBUZZ_URL),harfbuzz)

.sum-harfbuzz: harfbuzz-$(HARFBUZZ_VERSION).tar.bz2

harfbuzz: harfbuzz-$(HARFBUZZ_VERSION).tar.bz2 .sum-harfbuzz
	$(UNPACK)
	$(UPDATE_AUTOCONFIG)
	$(APPLY) $(SRC)/harfbuzz/harfbuzz-aarch64.patch
	$(APPLY) $(SRC)/harfbuzz/harfbuzz-clang.patch
	$(MOVE)

DEPS_harfbuzz = freetype2 $(DEPS_freetype2)

.harfbuzz: harfbuzz
	cd $< && env NOCONFIGURE=1 sh autogen.sh
	cd $< && $(HOSTVARS) CFLAGS="$(CFLAGS)" ./configure $(HOSTCONF) --with-icu=no --with-glib=no --with-fontconfig=no
	cd $< && $(MAKE) install
	touch $@
