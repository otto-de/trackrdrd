#/bin/sh

GLOBAL_STATUS=0

if [ ! -d "$WORKSPACE/lhotse-varnish-cache" ]; then
  git clone git@git.lhotse.ov.otto.de:lhotse-varnish-cache
fi
cd $WORKSPACE/lhotse-varnish-cache
git pull origin 3.0_bestats | head -1 | grep 'Already up-to-date'
if [ $? -eq 1 ]; then
    ./autogen.sh
    CFLAGS=-m64 ./configure
    make
fi

cd $WORKSPACE/trackrdrd
./autogen.sh
CFLAGS=-m64 ./configure VARNISHSRC=$WORKSPACE/lhotse-varnish-cache
make clean
make
[[ $? -ne 0 ]] && GLOBAL_STATUS=1

make check
[[ $? -ne 0 ]] && GLOBAL_STATUS=1

make clean

exit $GLOBAL_STATUS
