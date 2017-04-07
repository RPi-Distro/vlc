# freetype2

FREETYPE2_VERSION := 2.6.2
FREETYPE2_URL := $(SF)/freetype/freetype2/$(FREETYPE2_VERSION)/freetype-$(FREETYPE2_VERSION).tar.gz

PKGS += freetype2
ifeq ($(call need_pkg,"freetype2"),)
PKGS_FOUND += freetype2
endif

$(TARBALLS)/freetype-$(FREETYPE2_VERSION).tar.gz:
	$(call download_pkg,$(FREETYPE2_URL),freetype2)

.sum-freetype2: freetype-$(FREETYPE2_VERSION).tar.gz

freetype: freetype-$(FREETYPE2_VERSION).tar.gz .sum-freetype2
	$(UNPACK)
	$(call pkg_static, "builds/unix/freetype2.in")
	$(MOVE)
	cd $@ && cp builds/unix/install-sh .

DEPS_freetype2 = zlib $(DEPS_zlib)

.freetype2: freetype
	sed -i.orig s/-ansi// $</builds/unix/configure
	cd $< && GNUMAKE=$(MAKE) $(HOSTVARS) ./configure --with-harfbuzz=no --with-zlib=yes --without-png $(HOSTCONF)
	cd $< && $(MAKE) && $(MAKE) install
	touch $@
