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

AXIOMEVI_DIR=$AXIOMHOME
AXIOMEVI_GASNET_DIR=$AXIOMEVI_DIR/axiom-evi-gasnet
AXIOMEVI_OMPSS_DIR=$AXIOMEVI_DIR/ompss
AXIOMEVI_BR_DIR=$AXIOMEVI_DIR/axiom-evi-buildroot
AXIOMEVI_BR_HOST_DIR=$AXIOMEVI_BR_DIR/output/host
AXIOMEVI_BR_TRG_DIR=$AXIOMEVI_BR_DIR/output/target

OUTPUT=$AXIOMHOME/output

BUILD_ID='x86_64-unknown-linux-gnu'
TARGET_ID='aarch64-buildroot-linux-gnu'

CC="$AXIOMEVI_BR_HOST_DIR/usr/bin/aarch64-buildroot-linux-gnu-gcc" ; export CC
CXX="$AXIOMEVI_BR_HOST_DIR/usr/bin/aarch64-buildroot-linux-gnu-g++" ; export CXX

PKG_CONFIG_PATH=$OUTPUT/lib/pkgconfig

if pkg-config --exists ompi; then
    EXTRA_PARAMS="$EXTRA_PARAMS --with-mpi=$OUTPUT"
fi

../configure --prefix=$OUTPUT \
	     --build=$BUILD_ID --host=$TARGET_ID --target=$TARGET_ID \
	     --with-gasnet \
	     --with-gasnet-include=$AXIOMHOME/output/include/performance \
	     --with-gasnet-lib=$AXIOMHOME/output/lib/performance \
	     $EXTRA_PARAMS \
	     --disable-debug \
	     --disable-instrumentation \
	     --disable-instrumentation-debug \
             --with-axiomhome=$AXIOMHOME \
	     "$@"
