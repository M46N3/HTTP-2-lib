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
#wget https://dl.bintray.com/boostorg/release/1.70.0/source/boost_1_70_0.zip
#unzip boost_1_70_0.zip && cd boost_1_70_0
#chmod +x bootstrap.sh
#./