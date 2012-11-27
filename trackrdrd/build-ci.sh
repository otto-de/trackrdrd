#/bin/sh

GLOBAL_STATUS=0
LHOTSE_VARNISH_PREFIX=/var/opt/varnish
LHOTSE_TRACKING_PREFIX=/var/opt/varnish_tracking

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

CFLAGS=-m64 CXXFLAGS=-m64 LDFLAGS=-Wl,-rpath=$LHOTSE_VARNISH_PREFIX/lib/varnish:$LHOTSE_VARNISH_PREFIX/lib ./configure --prefix=$LHOTSE_TRACKING_PREFIX VARNISHSRC=$WORKSPACE/lhotse-varnish-cache
[[ $? -ne 0 ]] && exit 1

make clean
make
[[ $? -ne 0 ]] && exit 1

make check
[[ $? -ne 0 ]] && exit 1

VERSION=$($WORKSPACE/trackrdrd/src/.libs/trackrdrd -V | awk '{print $2}')
REVISION=$($WORKSPACE/trackrdrd/src/.libs/trackrdrd -V | awk '{print $4}')
BUILDPATH=$WORKSPACE/trackrdrd/rpmbuild/BUILDROOT/trackrdrd-$VERSION-rev${REVISION}_build$JENKINS_BUILD_NUMBER.$(uname -m)
DESTDIR=$BUILDPATH make install
[[ $? -ne 0 ]] && exit 1

mkdir -p $BUILDPATH/etc/init.d
mkdir $BUILDPATH/$LHOTSE_TRACKING_PREFIX/etc
cp $WORKSPACE/trackrdrd/etc/sample.conf $BUILDPATH/etc/trackrdrd.conf
cp $WORKSPACE/trackrdrd/etc/sample.conf $BUILDPATH/$LHOTSE_TRACKING_PREFIX/etc/
cp $WORKSPACE/trackrdrd/etc/trackrdrd $BUILDPATH/etc/init.d/

cd $WORKSPACE/trackrdrd/rpmbuild
rpmbuild --define '_topdir '$WORKSPACE/trackrdrd/rpmbuild --define 'version '$VERSION --define 'revision '$REVISION --define 'build_number '$JENKINS_BUILD_NUMBER --define 'prefix '$LHOTSE_TRACKING_PREFIX -bb SPECS/trackrdrd.spec
[[ $? -ne 0 ]] && exit 1
