# modplug

MODPLUG_VERSION := 0.8.9.0
MODPLUG_URL := $(SF)/modplug-xmms/libmodplug-$(MODPLUG_VERSION).tar.gz

PKGS += modplug
ifeq ($(call need_pkg,"libmodplug >= 0.8.9.0"),)
PKGS_FOUND += modplug
endif

$(TARBALLS)/libmodplug-$(MODPLUG_VERSION).tar.gz:
	$(call download_pkg,$(MODPLUG_URL),modplug)

.sum-modplug: libmodplug-$(MODPLUG_VERSION).tar.gz

libmodplug: libmodplug-$(MODPLUG_VERSION).tar.gz .sum-modplug
	$(UNPACK)
	$(APPLY) $(SRC)/modplug/modplug-win32-static.patch
	$(APPLY) $(SRC)/modplug/macosx-do-not-force-min-version.patch
	$(call pkg_static,"libmodplug.pc.in")
	$(MOVE)

.modplug: libmodplug
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
