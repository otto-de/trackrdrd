#/bin/sh

GLOBAL_STATUS=0

if [ ! -d "$WORKSPACE/lhotse-varnish-cache" ]; then
  git clone git@git.lhotse.ov.otto.de:lhotse-varnish-cache
fi
cd $WORKSPACE/lhotse-varnish-cache
git pull origin 3.0_bestats | head -1 | grep 'Already up-to-date'
if [ $? -eq 1 ]; then
    ./autogen.sh
    [[ $? -ne 0 ]] && exit 1
    CFLAGS=-m64 ./configure
    [[ $? -ne 0 ]] && exit 1
    make
    [[ $? -ne 0 ]] && exit 1
fi

cd $WORKSPACE/trackrdrd
./autogen.sh
[[ $? -ne 0 ]] && exit 1

CFLAGS=-m64 ./configure VARNISHSRC=$WORKSPACE/lhotse-varnish-cache
[[ $? -ne 0 ]] && exit 1

make clean
make
[[ $? -ne 0 ]] && exit 1

make check
[[ $? -ne 0 ]] && exit 1

make clean
