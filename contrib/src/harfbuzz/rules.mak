# HARFBUZZ

HARFBUZZ_VERSION := 2.6.4
HARFBUZZ_URL := http://www.freedesktop.org/software/harfbuzz/release/harfbuzz-$(HARFBUZZ_VERSION).tar.xz
PKGS += harfbuzz
ifeq ($(call need_pkg,"harfbuzz"),)
PKGS_FOUND += harfbuzz
endif

$(TARBALLS)/harfbuzz-$(HARFBUZZ_VERSION).tar.xz:
	$(call download_pkg,$(HARFBUZZ_URL),harfbuzz)

.sum-harfbuzz: harfbuzz-$(HARFBUZZ_VERSION).tar.xz

harfbuzz: harfbuzz-$(HARFBUZZ_VERSION).tar.xz .sum-harfbuzz
	$(UNPACK)
	$(APPLY) $(SRC)/harfbuzz/harfbuzz-aarch64.patch
	$(MOVE)

DEPS_harfbuzz = freetype2 $(DEPS_freetype2)

HARFBUZZ_CONF := --with-freetype \
	--without-glib

ifdef HAVE_DARWIN_OS
HARFBUZZ_CONF += --with-coretext
endif

.harfbuzz: harfbuzz
	$(RECONF)
	cd $< && $(HOSTVARS_PIC) ./configure $(HOSTCONF) $(HARFBUZZ_CONF)
	cd $< && $(MAKE) install
	touch $@
