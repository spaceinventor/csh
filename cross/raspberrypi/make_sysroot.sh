#! /usr/bin/bash

SYSROOT=/sysroot
DOWNLOAD=/download

cd $DOWNLOAD

# apt install --download-only $LIST_OF_PACKAGES
# Download packages only
apt-get download $(apt-cache depends --recurse --no-recommends --no-suggests \
  --no-conflicts --no-breaks --no-replaces --no-enhances \
  --no-pre-depends ${LIST_OF_PACKAGES} | grep "^\w")

cd $SYSROOT

# unpack all the packages downloaded above in that sysroot root
for deb in $DOWNLOAD/*.deb; do dpkg-deb -xv $deb .; done

# Create a tarball
#cd ..
#tar -cazf sysroot.tar.gz $SYSROOT

# bash -i
# # Get it and unpack it where you want that sysroot
# scp pi@raspberrypi.local:~/sysroot.tar.gz .
# tar -xf sysroot.tar.gz
