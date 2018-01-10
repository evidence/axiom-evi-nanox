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

export LDFLAGS=-Wl,--build-id=uuid

../configure --prefix=$PREFIX \
	     --with-gasnet \
	     --with-gasnet-include=/usr/local/include/debug \
	     --with-gasnet-lib=/usr/local/lib/debug \
	     --disable-performance \
	     --disable-instrumentation \
	     --disable-instrumentation-debug \
             --with-axiomhome=$AXIOMHOME \
	     "$@"
