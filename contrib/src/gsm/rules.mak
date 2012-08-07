# GSM
GSM_VERSION := 1.0.13
GSM_URL := $(CONTRIB_VIDEOLAN)/libgsm_$(GSM_VERSION).tar.gz

$(TARBALLS)/libgsm_$(GSM_VERSION).tar.gz:
	$(call download,$(GSM_URL))

.sum-gsm: libgsm_$(GSM_VERSION).tar.gz

gsm: libgsm_$(GSM_VERSION).tar.gz .sum-gsm
	$(UNPACK)
	mv gsm-1.0-* libgsm_$(GSM_VERSION)
	$(APPLY) $(SRC)/gsm/gsm-cross.patch
	$(MOVE)
ifdef HAVE_MACOSX
	(cd $@; sed -e 's%-O2%-O2\ $(EXTRA_CFLAGS)\ $(EXTRA_LDFLAGS)%' -e 's%# LDFLAGS >=%LDFLAGS >-= $(EXTRA_LDFLAGS)%' -e 's%gcc%$(CC)%' -i.orig  Makefile)
endif
	(cd $@; sed -i -e 's%-O2%-O2 -fPIC%' Makefile)

.gsm: gsm
	cd $< && $(HOSTVARS) $(MAKE)
	mkdir -p "$(PREFIX)/include/gsm" "$(PREFIX)/lib"
	cp $</inc/gsm.h "$(PREFIX)/include/gsm/"
	cp $</lib/libgsm.a "$(PREFIX)/lib/"
	touch $@
