set -x

git submodule init
git submodule update

cd v4l-utils
git checkout master
git reset --hard origin/master
git am -3 ../patches/*
cd -

cd tsdecrypt
git checkout master
git submodule init
git submodule update
cd -

cd v4l-utils
autoreconf -vfis
./configure
cd lib/libdvbv5
make
cd ../../..

autoreconf -vfis
./configure
make

