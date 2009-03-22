#!/bin/bash

echo 'Building Slate package...'

mkdir out/
cp -a debian out/
dpkg-deb -b out/

