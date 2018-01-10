#!/bin/sh

#
# used with buildroot
#

[ -z "$AXIOMHOME" ] && AXIOMHOME=$HOME/axiom-evi
for arg in "$@";do
    echo "$arg" | grep -q '^--with-axiomhome=' \
	&& AXIOMHOME=$(echo "$arg" | sed 's#^--with-axiomhome=##')
done
[ -f "$AXIOMHOME/scripts/start_qemu.sh" ] || {
    echo "AXIOMHOME shell variable or --with-axiomhome are improperly set!"
    echo "(axiom home used is '$AXIOMHOME')"
    exit 255;
}


TARGET_DIR=$(realpath ${ROOTFS})
#SYSROOT_DIR=$(realpath ${LINARO}/sysroot)
SYSROOT_DIR=$(realpath ${ROOTFS})
HOST_DIR=$(realpath ${LINARO}/host)
        
BUILD_ID='x86_64-unknown-linux-gnu'
TARGET_ID='aarch64-linux-gnu'
        
[ -z "$PREFIX" ] && PREFIX=$HOST_DIR

export PATH=$PATH:$HOST_DIR/usr/bin

export LDFLAGS=-Wl,--build-id=uuid

#../configure --prefix=$OUTPUT 
../configure --prefix=$PREFIX \
	     --build=$BUILD_ID --host=$TARGET_ID --target=$TARGET_ID \
	     --with-gasnet \
	     --with-gasnet-include=${SYSROOT_DIR}/usr/include/debug \
	     --with-gasnet-lib=${SYSROOT_DIR}/usr/lib/debug \
	     --disable-performance \
	     --disable-instrumentation \
	     --disable-instrumentation-debug \
             --with-axiomhome=$AXIOMHOME \
	     "$@"
