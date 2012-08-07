#!/bin/sh
set -e
set -x

info()
{
    local green="\033[1;32m"
    local normal="\033[0m"
    echo "[${green}build${normal}] $1"
}

ARCH="x86_64"
SDK="10.6"

usage()
{
cat << EOF
usage: $0 [options]

Build vlc in the current directory

OPTIONS:
   -h            Show some help
   -q            Be quiet
   -k <sdk>      Use the specified sdk (default: $SDK)
   -a <arch>     Use the specified arch (default: $ARCH)
EOF

}

spushd()
{
    pushd "$1" > /dev/null
}

spopd()
{
    popd > /dev/null
}

while getopts "hvk:a:" OPTION
do
     case $OPTION in
         h)
             usage
             exit 1
             ;;
         q)
             set +x
             QUIET="yes"
         ;;
         a)
             ARCH=$OPTARG
         ;;
         k)
             SDK=$OPTARG
         ;;
     esac
done
shift $(($OPTIND - 1))

if [ "x$1" != "x" ]; then
    usage
    exit 1
fi

#
# Various initialization
#

out="/dev/stdout"
if [ "$QUIET" = "yes" ]; then
    out="/dev/null"
fi

info "Building VLC for the Mac OS X"

spushd `dirname $0`/../../..
vlcroot=`pwd`
spopd

builddir=`pwd`

info "Building in \"$builddir\""

export CC=/Developer/usr/bin/clang
export CXX="/Developer/usr/bin/clang++"
export OBJC=/Developer/usr/bin/clang
export OSX_VERSION=$SDK
export PATH="${vlcroot}/extras/tools/build/bin:$PATH"

TRIPLET=$ARCH-apple-darwin10


#
# vlc/extras/tools
#

info "Building building tools"
spushd "${vlcroot}/extras/tools"
./bootstrap > $out
make > $out
spopd


#
# vlc/contribs
#

info "Building contribs"
spushd "${vlcroot}/contrib"
mkdir -p build && cd build
../bootstrap --build=$TRIPLET --host=$TRIPLET > $out
if [ ! -e "../$TRIPLET" ]; then
    make prebuilt > $out
fi
spopd


#
# vlc/bootstrap
#

info "Bootstrap-ing configure"
spushd "${vlcroot}"
if ! [ -e "${vlcroot}/configure" ]; then
    ${vlcroot}/bootstrap > $out
fi
spopd


#
# vlc/configure
#

if [ "${vlcroot}/configure" -nt Makefile ]; then

  ${vlcroot}/extras/package/macosx/configure.sh \
      --build=$TRIPLET \
      --host=$TRIPLET \
      --with-macosx-version-min=$OSX_VERSION \
      --with-macosx-sdk=/Developer/SDKs/MacOSX$OSX_VERSION.sdk > $out
fi


#
# make
#

core_count=`sysctl -n machdep.cpu.core_count`
let jobs=$core_count+1

info "Running make -j$jobs"
make -j$jobs
