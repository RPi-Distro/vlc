# FFmpeg

#Uncomment the one you want
#USE_LIBAV ?= 1
USE_FFMPEG ?= 1

ifdef USE_FFMPEG
HASH=cdb0225
FFMPEG_SNAPURL := http://git.videolan.org/?p=ffmpeg.git;a=snapshot;h=$(HASH);sf=tgz
FFMPEG_GITURL := git://git.videolan.org/ffmpeg.git
else
HASH=c457bde
FFMPEG_SNAPURL := http://git.libav.org/?p=libav.git;a=snapshot;h=$(HASH);sf=tgz
FFMPEG_GITURL := git://git.libav.org/libav.git
endif

FFMPEGCONF = \
	--cc="$(CC)" \
	--pkg-config="$(PKG_CONFIG)" \
	--disable-doc \
	--disable-encoder=vorbis \
	--enable-libgsm \
	--enable-libopenjpeg \
	--disable-debug \
	--disable-avdevice \
	--disable-devices \
	--disable-avfilter \
	--disable-filters \
	--disable-protocol=concat \
	--disable-bsfs \
	--disable-bzlib \
	--disable-avresample

ifdef USE_FFMPEG
FFMPEGCONF += \
	--disable-swresample \
	--disable-iconv
endif

DEPS_ffmpeg = zlib gsm openjpeg

# Optional dependencies
ifndef BUILD_NETWORK
FFMPEGCONF += --disable-network
endif
ifdef BUILD_ENCODERS
FFMPEGCONF += --enable-libmp3lame --enable-libvpx --disable-decoder=libvpx --disable-decoder=libvpx_vp8 --disable-decoder=libvpx_vp9
DEPS_ffmpeg += lame $(DEPS_lame) vpx $(DEPS_vpx)
else
FFMPEGCONF += --disable-encoders --disable-muxers
endif

# Small size
ifdef ENABLE_SMALL
FFMPEGCONF += --enable-small
endif
ifeq ($(ARCH),arm)
ifdef HAVE_ARMV7A
FFMPEGCONF += --enable-thumb
endif
endif

ifdef HAVE_CROSS_COMPILE
FFMPEGCONF += --enable-cross-compile --disable-programs
ifndef HAVE_DARWIN_OS
FFMPEGCONF += --cross-prefix=$(HOST)-
endif
endif

# ARM stuff
ifeq ($(ARCH),arm)
ifndef HAVE_DARWIN_OS
FFMPEGCONF += --arch=arm
endif
ifdef HAVE_NEON
FFMPEGCONF += --enable-neon
endif
ifdef HAVE_ARMV7A
FFMPEGCONF += --cpu=cortex-a8
endif
ifdef HAVE_ARMV6
FFMPEGCONF += --cpu=armv6 --disable-neon
endif
endif

# MIPS stuff
ifeq ($(ARCH),mipsel)
FFMPEGCONF += --arch=mips
endif

# x86 stuff
ifeq ($(ARCH),i386)
ifndef HAVE_DARWIN_OS
FFMPEGCONF += --arch=x86
endif
endif

# Darwin
ifdef HAVE_DARWIN_OS
FFMPEGCONF += --arch=$(ARCH) --target-os=darwin
ifdef USE_FFMPEG
FFMPEGCONF += --disable-lzma
endif
ifeq ($(ARCH),x86_64)
FFMPEGCONF += --cpu=core2
endif
endif
ifdef HAVE_IOS
FFMPEGCONF += --enable-pic --extra-ldflags="$(EXTRA_CFLAGS)"
ifdef HAVE_NEON
FFMPEGCONF += --as="$(AS)"
endif
endif
ifdef HAVE_MACOSX
FFMPEGCONF += --enable-vda
endif

# Linux
ifdef HAVE_LINUX
FFMPEGCONF += --target-os=linux --enable-pic

endif

# Windows
ifdef HAVE_WIN32
ifndef HAVE_MINGW_W64
DEPS_ffmpeg += directx
endif
FFMPEGCONF += --target-os=mingw32 --enable-memalign-hack
FFMPEGCONF += --enable-w32threads --enable-dxva2 \
	--disable-decoder=dca

ifdef HAVE_WIN64
FFMPEGCONF += --cpu=athlon64 --arch=x86_64
else # !WIN64
FFMPEGCONF+= --cpu=i686 --arch=x86
endif

else # !Windows
FFMPEGCONF += --enable-pthreads
endif

# Solaris
ifdef HAVE_SOLARIS
ifeq ($(ARCH),x86_64)
FFMPEGCONF += --cpu=core2
endif
FFMPEGCONF += --target-os=sunos --enable-pic
endif

# Build
PKGS += ffmpeg
ifeq ($(call need_pkg,"libavcodec >= 54.25.0 libavformat >= 53.21.0 libswscale"),)
PKGS_FOUND += ffmpeg
endif

$(TARBALLS)/ffmpeg-$(HASH).tar.xz:
	$(call download_git,$(FFMPEG_GITURL),,$(HASH))

.sum-ffmpeg: $(TARBALLS)/ffmpeg-$(HASH).tar.xz
	$(warning Not implemented.)
	touch $@

ffmpeg: ffmpeg-$(HASH).tar.xz .sum-ffmpeg
	rm -Rf $@ $@-$(HASH)
	mkdir -p $@-$(HASH)
	$(XZCAT) "$<" | (cd $@-$(HASH) && tar xv --strip-components=1)
	$(APPLY) $(SRC)/ffmpeg/xwd-8431629dd112874293380a6d8a852459fc1a76b6.patch
	$(MOVE)

.ffmpeg: ffmpeg
	cd $< && $(HOSTVARS) ./configure \
		--extra-ldflags="$(LDFLAGS)" $(FFMPEGCONF) \
		--prefix="$(PREFIX)" --enable-static --disable-shared
	cd $< && $(MAKE) install-libs install-headers
	touch $@
