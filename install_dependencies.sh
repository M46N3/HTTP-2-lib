#! /bin/bash

sudo apt-get update
sudo apt-get install g++ make binutils autoconf automake autotools-dev cmake libtool pkg-config zlib1g-dev libcunit1-dev libssl-dev libxml2-dev libev-dev libevent-dev libboost-dev -y


# Download, build, and install nghttp2
git clone https://github.com/nghttp2/nghttp2.git && cd nghttp2
git submodule update --init
autoreconf -i
automake
autoconf
./configure --enable-apps
make && sudo make install

# Download, build, and install boost
wget -O boost_1_68_0.tar.gz https://sourceforge.net/projects/boost/files/boost/1.68.0/boost_1_68_0.tar.gz/download
tar xzvf boost_1_68_0.tar.gz && cd boost_1_68_0
chmod +x bootstrap.sh
./bootstrap.sh --prefix=/usr/
./b2
sudo ./b2 install