#!/bin/bash

echo 'Building Slate package...'

rm -rf slate/
mkdir slate/
cp -a debian slate/DEBIAN
dpkg-deb -b slate

