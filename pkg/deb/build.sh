#!/bin/bash

echo 'Building Slate package...'

rm -rf slate/

mkdir -p slate/DEBIAN
mkdir -p slate/usr/bin
mkdir -p slate/usr/lib/slate
mkdir -p slate/usr/share/man/man1
mkdir -p slate/usr/share/doc/slate


cp control slate/DEBIAN/
cp ../../LICENSE slate/usr/share/doc/slate/copyright
cp changelog slate/usr/share/doc/slate/changelog
cp ../../slate slate/usr/bin
cp ../../etc/slate.1 slate/usr/share/man/man1
cp ../../README slate/usr/share/doc/slate/README.Debian
cp ../../full.image slate/usr/lib/slate/slate.image


gzip --best slate/usr/share/man/man1/slate.1
gzip --best slate/usr/share/doc/slate/changelog
gzip --best slate/usr/share/doc/slate/README.Debian

strip slate/usr/bin/*

fakeroot dpkg-deb -b slate
lintian --pedantic -v slate.deb
