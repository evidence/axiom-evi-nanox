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

export LDFLAGS=-Wl,--build-id=uuid

../configure --prefix=$PREFIX \
	     --with-gasnet \
	     --with-gasnet-include=/usr/local/include/performance \
	     --with-gasnet-lib=/usr/local/lib/performance \
	     $EXTRA_PARAMS \
	     --disable-debug \
	     --disable-performance \
	     --enable-instrumentation \
	     --with-extrae=/usr/local \
	     --disable-instrumentation-debug \
             --with-axiomhome=$AXIOMHOME \
	     "$@"
