#!/bin/bash

./configure
./build
./gendoc.py

IFS='-' read -ra a <<< `git describe --long --always`
REVID="${a[0]}-${a[1]}"

rm -R si-csh_$REVID
mkdir si-csh_$REVID
mkdir si-csh_$REVID/usr
mkdir si-csh_$REVID/usr/bin
cp builddir/csh si-csh_$REVID/usr/bin

mkdir si-csh_$REVID/usr/share
mkdir si-csh_$REVID/usr/share/si-csh
cp build-doc/CSH_MAN.pdf si-csh_$REVID/usr/share/si-csh

mkdir si-csh_$REVID/DEBIAN

echo "Package: si-csh" >  si-csh_$REVID/DEBIAN/control
echo "Version: $REVID" >> si-csh_$REVID/DEBIAN/control
echo "Section: base" >> si-csh_$REVID/DEBIAN/control
echo "Priority: optional" >> si-csh_$REVID/DEBIAN/control
echo "Architecture: amd64" >> si-csh_$REVID/DEBIAN/control
echo "Depends: can-utils (>= 2020.11.0-1), fonts-powerline (>= 2.8.2-1)" >> si-csh_$REVID/DEBIAN/control
echo "Maintainer: Space Inventor A/S <sales@space-inventor.com>" >> si-csh_$REVID/DEBIAN/control
echo "Description: CSP shell client application" >> si-csh_$REVID/DEBIAN/control
echo " For easy operation of Space Inventor" >> si-csh_$REVID/DEBIAN/control
echo " satellites and modules based on CSP and libparam" >> si-csh_$REVID/DEBIAN/control

#!/bin/sh

echo 'USER_FOLDER=`eval echo ~$SUDO_USER`' >  si-csh_$REVID/DEBIAN/postinst
echo 'if [ -f "$USER_FOLDER/.local/bin/csh" ]' >>  si-csh_$REVID/DEBIAN/postinst
echo 'then' >>  si-csh_$REVID/DEBIAN/postinst
echo '    echo "Removing CSH from local installation"' >>  si-csh_$REVID/DEBIAN/postinst
echo '    rm $USER_FOLDER/.local/bin/csh' >>  si-csh_$REVID/DEBIAN/postinst
echo 'fi' >>  si-csh_$REVID/DEBIAN/postinst

chmod +x si-csh_$REVID/DEBIAN/postinst

dpkg -b si-csh_$REVID

rm -R si-csh_$REVID
