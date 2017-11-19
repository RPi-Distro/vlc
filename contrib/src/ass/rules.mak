# ASS
ASS_VERSION := 0.13.2
ASS_URL := https://github.com/libass/libass/releases/download/$(ASS_VERSION)/libass-$(ASS_VERSION).tar.gz

PKGS += ass
ifeq ($(call need_pkg,"libass"),)
PKGS_FOUND += ass
endif

ifdef HAVE_ANDROID
WITH_FONTCONFIG = 0
WITH_HARFBUZZ = 0
else
ifdef HAVE_DARWIN_OS
WITH_FONTCONFIG = 0
WITH_HARFBUZZ = 1
else
ifdef HAVE_WINRT
WITH_FONTCONFIG = 0
WITH_HARFBUZZ = 1
else
WITH_FONTCONFIG = 1
WITH_HARFBUZZ = 1
endif
endif
endif

$(TARBALLS)/libass-$(ASS_VERSION).tar.gz:
	$(call download_pkg,$(ASS_URL),ass)

.sum-ass: libass-$(ASS_VERSION).tar.gz

libass: libass-$(ASS_VERSION).tar.gz .sum-ass
	$(UNPACK)
	$(APPLY) $(SRC)/ass/ass-macosx.patch
	$(APPLY) $(SRC)/ass/ass-macosx-coretext.patch
	$(APPLY) $(SRC)/ass/ass-solaris.patch
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

DEPS_ass = freetype2 $(DEPS_freetype2) fribidi

ASS_CONF=--disable-enca

ifneq ($(WITH_FONTCONFIG), 0)
DEPS_ass += fontconfig $(DEPS_fontconfig)
else
ASS_CONF += --disable-fontconfig --disable-require-system-font-provider
endif

ifneq ($(WITH_HARFBUZZ), 0)
DEPS_ass += harfbuzz $(DEPS_harfbuzz)
else
ASS_CONF += --disable-harfbuzz
endif

ifdef WITH_OPTIMIZATION
ASS_CFLAGS += -O3
else
ASS_CFLAGS += -g
endif

.ass: libass
	$(RECONF)
	cd $< && $(HOSTVARS) CFLAGS="$(CFLAGS) $(ASS_CFLAGS)" ./configure $(HOSTCONF) $(ASS_CONF)
	cd $< && $(MAKE) install
	touch $@
