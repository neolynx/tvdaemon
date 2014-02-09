SCRIPT_FILE=`readlink -f @0`
SCRIPT_DIR=`dirname $SCRIPT_FILE`

set -x

die()
{
  echo $1
  exit -1
}

git submodule update --init --recursive || die "submodule update failed"

cd v4l-utils                            || die "change to v4l-utils failed"
git checkout master                     || die "checkout v4l-utils failed"
git reset --hard origin/master          || die "reset v4l-utils failed"
git am -3 ../patches/*                  || die "patching v4l-utils failed"
cd $SCRIPT_DIR                          || die "change to home failed"

cd tsdecrypt                            || die "change to tsdecrypt failed"
git checkout master                     || die "checkout tsdecrypt failed"
git submodule update --init --recursive || die "tsdecrypt submodule update failed"
cd $SCRIPT_DIR                          || die "change to home failed"

cd v4l-utils                            || die "change to v4l-utils failed"
autoreconf -vfis                        || die "autoreconf v4l-utils failed"
./configure                             || die "configure v4l-utils failed"
cd lib/libdvbv5                         || die "change to libdvbv5 failed"
make                                    || die "make libdvbv5 failed"
cd $SCRIPT_DIR                          || die "change to home failed"

autoreconf -vfis                        || die "autoreconf tvdaeon failed"
./configure                             || die "configure tvdaemon failed"
make                                    || die "make tvdaemon failed"

