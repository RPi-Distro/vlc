# sparkle

SPARKLE_VERSION := 1.6.1
SPARKLE_URL := https://github.com/sparkle-project/Sparkle/archive/$(SPARKLE_VERSION).zip

ifdef HAVE_MACOSX
PKGS += sparkle
endif

$(TARBALLS)/Sparkle-$(SPARKLE_VERSION).zip:
	$(call download_pkg,$(SPARKLE_URL),sparkle)

.sum-sparkle: Sparkle-$(SPARKLE_VERSION).zip

sparkle: Sparkle-$(SPARKLE_VERSION).zip .sum-sparkle
	$(UNPACK)
	$(APPLY) $(SRC)/sparkle/sparkle-fix-compilation-on-snowleopard.patch
	$(APPLY) $(SRC)/sparkle/sparkle-disable-redirects-to-non-HTTP-URLs-in-release-notes.patch
	$(APPLY) $(SRC)/sparkle/sparkle-prevent-inclusion-of-local-files-via-file-XML-entiti.patch
	$(APPLY) $(SRC)/sparkle/sparkle-macosx106.patch
	$(MOVE)

.sparkle: sparkle
	cd $< && xcodebuild $(XCODE_FLAGS) WARNING_CFLAGS=-Wno-error
	cd $< && install_name_tool -id @executable_path/../Frameworks/Sparkle.framework/Versions/A/Sparkle build/Release/Sparkle.framework/Sparkle
	install -d $(PREFIX)
	cd $< && cp -R build/Release/Sparkle.framework "$(PREFIX)"
	touch $@
