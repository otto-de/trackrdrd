#/bin/sh

GLOBAL_STATUS=0
VERSION=3.8.2

URL=http://apache.openmirror.de/activemq/activemq-cpp/$VERSION
TARBALL=activemq-cpp-library-$VERSION-src.tar.bz2

for DIR in BUILD BUILDROOT RPMS SOURCES SRPMS SPECS
do
    RPMDIR=$WORKSPACE/libactivemq/rpmbuild/$DIR
    [[ -d $RPMDIR ]] && rm -rf $RPMDIR
    mkdir -p $RPMDIR
done

cd $WORKSPACE/libactivemq/rpmbuild/SOURCES
wget $URL/$TARBALL
[[ $? -ne 0 ]] && exit 1

cp $WORKSPACE/libactivemq/activemq-cpp.spec $WORKSPACE/libactivemq/rpmbuild/SPECS
BUILDROOTPREFIX=$WORKSPACE/libactivemq/rpmbuild/BUILDROOT
PKGNAME=activemq-cpp-library-$VERSION.$(uname -m)
BUILDROOTPATH=$BUILDROOTPREFIX/$PKGNAME

cd $WORKSPACE/libactivemq/rpmbuild
rpmbuild \
    --define '_topdir '$WORKSPACE/libactivemq/rpmbuild \
    --define 'version '$VERSION \
    --define 'build_number '$BUILD_NUMBER \
    --buildroot $BUILDROOTPATH -bb SPECS/activemq-cpp.spec
