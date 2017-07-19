#!/bin/sh

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

if [ "$FS" = "seco" ]; then

    TARGET_DIR=$(realpath ${ROOTFS})
#    SYSROOT_DIR=$(realpath ${LINARO}/sysroot)
    SYSROOT_DIR=$(realpath ${ROOTFS})
    HOST_DIR=$(realpath ${LINARO}/host)
        
    BUILD_ID='x86_64-unknown-linux-gnu'
    TARGET_ID='aarch64-linux-gnu'
        
    [ -z "$PREFIX" ] && PREFIX=$HOST_DIR

    export PATH=$PATH:$HOST_DIR/usr/bin

else

    OUTPUT_DIR=$AXIOMHOME/output
    TARGET_DIR=$(realpath ${OUTPUT_DIR}/target)
    SYSROOT_DIR=$(realpath ${OUTPUT_DIR}/staging)
    HOST_DIR=$(realpath ${OUTPUT_DIR}/host)

    BUILD_ID='x86_64-unknown-linux-gnu'
    TARGET_ID='aarch64-buildroot-linux-gnu'

    CC="$HOST_DIR/usr/bin/aarch64-buildroot-linux-gnu-gcc" ; export CC
    CXX="$HOST_DIR/usr/bin/aarch64-buildroot-linux-gnu-g++" ; export CXX

fi

#PKG_CONFIG_PATH=$OUTPUT/lib/pkgconfig
#
#if pkg-config --exists ompi; then
#    EXTRA_PARAMS="$EXTRA_PARAMS --with-mpi=$OUTPUT"
#fi

[ -z "$PREFIX" ] && PREFIX=$SYSROOT_DIR

../configure --prefix=$PREFIX \
	     --build=$BUILD_ID --host=$TARGET_ID --target=$TARGET_ID \
	     --with-gasnet \
	     --with-gasnet-include=${SYSROOT_DIR}/usr/include/performance \
	     --with-gasnet-lib=${SYSROOT_DIR}/usr/lib/performance \
	     $EXTRA_PARAMS \
	     --disable-debug \
	     --disable-performance \
	     --enable-instrumentation \
	     --with-extrae=${SYSROOT_DIR}/usr \
	     --disable-instrumentation-debug \
             --with-axiomhome=$AXIOMHOME \
	     "$@"
