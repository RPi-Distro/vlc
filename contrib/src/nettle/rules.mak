# Nettle

NETTLE_VERSION := 3.3
NETTLE_URL := ftp://ftp.gnu.org/gnu/nettle/nettle-$(NETTLE_VERSION).tar.gz

ifeq ($(call need_pkg,"nettle >= 2.7"),)
PKGS_FOUND += nettle
endif

$(TARBALLS)/nettle-$(NETTLE_VERSION).tar.gz:
	$(call download_pkg,$(NETTLE_URL),nettle)

.sum-nettle: nettle-$(NETTLE_VERSION).tar.gz

nettle: nettle-$(NETTLE_VERSION).tar.gz .sum-nettle
	$(UNPACK)
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

DEPS_nettle = gmp $(DEPS_gmp)

# GMP requires either GPLv2 or LGPLv3
.nettle: nettle
ifndef GPL
	$(REQUIRE_GNUV3)
endif
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
