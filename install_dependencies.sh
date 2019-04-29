#! /bin/bash

sudo apt-get update
sudo apt-get install g++ make binutils autoconf automake autotools-dev cmake libtool pkg-config zlib1g-dev libcunit1-dev libssl-dev libxml2-dev libev-dev libnghttp2-devlibevent-dev -y


# Download, build, and install nghttp2
git clone https://github.com/nghttp2/nghttp2.git && cd nghttp2
git submodule update --init
autoreconf -i
automake
autoconf
./configure --prefix=/usr \
            --disable-static \
            --enable-lib-only \
            --docdir=/usr/share/doc/nghttp2-1.48.0
make
sudo make install
ls /usr/inlcude

# Download, build, and install boost
wget -O boost_1_68_0.tar.bz2 https://sourceforge.net/projects/boost/files/boost/1.68.0/boost_1_68_0.tar.bz2/download
tar --bzip2 -xf ./boost_1_68_0.tar.bz2
ls -al
cp -r boost_1_68_0 /usr/local/
rm -r boost_1_68_0