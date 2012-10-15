# ASS
ASS_VERSION := 0.10.0
ASS_URL := http://libass.googlecode.com/files/libass-$(ASS_VERSION).tar.gz

PKGS += ass
ifeq ($(call need_pkg,"libass"),)
PKGS_FOUND += ass
endif

$(TARBALLS)/libass-$(ASS_VERSION).tar.gz:
	$(call download,$(ASS_URL))

.sum-ass: libass-$(ASS_VERSION).tar.gz

libass: libass-$(ASS_VERSION).tar.gz .sum-ass
	$(UNPACK)
ifdef HAVE_MACOSX
	$(APPLY) $(SRC)/ass/ass-macosx.patch
endif
	$(MOVE)

DEPS_ass = freetype2 $(DEPS_freetype2) fontconfig $(DEPS_fontconfig) fribidi

.ass: libass
	#$(RECONF)
	cd $< && $(HOSTVARS) CFLAGS="$(CFLAGS) -O3" ./configure $(HOSTCONF) --disable-enca
	cd $< && $(MAKE) install
	touch $@
